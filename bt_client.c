#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h> //ip hdeader library (must come before ip_icmp.h)
#include <netinet/ip_icmp.h> //icmp header
#include <arpa/inet.h> //internet address library
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <math.h>
#include <sys/poll.h>

#include "bencode.h"
#include "bt_lib.h"
#include "bt_setup.h"
#include "bt_connect.h"
#include "bt_message.h"

#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )

int LIVENESS_CHECK_PENDING = 0;
int KEEP_ALIVE = 1; //keep ourselves alive
int SEEDING = FALSE; //defaukt is to leech first

//our signal handler for use with checking the current health and
//aliveness of all peers we currently know of.
//not directly calling check_all() from here since we dont want to 
//disturb any reads or writes that might be in progress
void handler(int signum){ //signal handler 
  switch(signum){
    case SIGALRM:
      LIVENESS_CHECK_PENDING = 1; //read to check!
      break;
    case SIGINT:
      if (SEEDING)
        KEEP_ALIVE = 0; //we can now exit
      else
        exit(1); //if we are not seeding yet, just die
      break;
    case SIGPIPE:
      break; //do nothing when you get a sigpipe
    default:
      printf("Unhandled Exception (%d)\n", signum);
      break;
  }
}

//leecher main loop, when we are busy leeching and not seeding
//anything
void leecher_loop(bt_args_t *args){
  //determine which pieces to request
  int current=0;
  int begin;
  int bytes, remaining;
  bt_request_t request;
  bt_msg_t msg;
  bt_msg_t response, have, cancel;
  int print_frequently = (args->bt_info->length < args->bt_info->piece_length) ? 1 : 0;
 
  printf("leecher main loop\n");
  while((current = select_download_piece(args)) != -1){
    //only send request for next once we have the current requested piece
    printf("sending out bt_request for piece %d\n", current);
    begin = 0;
    bytes = 0;
    remaining = MAXBLOCK; //initialize to some high val
    while (remaining > 0){
      //while((remaining = piece_bytes_left(args, current, bytes)) > 0){
      remaining = piece_bytes_left(args, current, bytes);
      request.index = current;
      request.begin = begin;
      request.length = min(remaining, MAXBLOCK); 

      msg.length = sizeof(bt_request_t);
      msg.bt_type = BT_REQUEST;
      msg.payload.request = request;
      send_all(args, &msg); //send out the request message to all peers
      read_from_peer(args->peers[0], &response, args); //get response
      begin += MAXBLOCK;
      bytes += min(remaining, MAXBLOCK);
      if (print_frequently)
        print_stats(args, bytes);
    }

    if (print_frequently == 0) //print less frequently
        print_stats(args, bytes);

    //done with current piece, setbitfield
    set_bitfield(args, current);
    have_message(&have, current); //populate the message struct
    send_all(args, &have); //notify everyone we now have the piece
  
  }
  cancel_message(&cancel, &request, current);
  send_all(args, &cancel); //send the end-game message
  return;

}

int main(int argc, char * argv[]){
  bt_args_t bt_args; //made global, NOT.
  be_node * node; // top node in the bencoding
  bt_info_t bt_info; //info be parsed from the be_node
  int i=0;
  int sockfd, client_sock;
  struct sockaddr_in client_addr;
  socklen_t addr_size = sizeof(struct sockaddr);
  struct timeval tv;
  char *ip;
  int index;
  bt_msg_t msg; //reusable message casing
  int pollrv; //return value from the call to poll()
  unsigned short port;
  fd_set listen_set;
  char log_entry[80];
  parse_args(&bt_args, argc, argv);
 
  //print header
  intro();
  if(bt_args.verbose){
    printf("Args:\n");
    printf("verbose: %d\n",bt_args.verbose);
    printf("save_file: %s\n",bt_args.save_file);
    printf("log_file: %s\n",bt_args.log_file);
    printf("torrent_file: %s\n", bt_args.torrent_file);

    for(i=0;i<MAX_CONNECTIONS;i++){
      if(bt_args.peers[i] != NULL)
        print_peer(bt_args.peers[i]);
    }

    
  }

  //read and parse the torent file
  node = load_be_node(bt_args.torrent_file);
  parse_bt_info(&bt_info, node);
  bt_args.bt_info = &bt_info;
  if (bt_args.restart){ //handle restarts
    __fcopy__(bt_args.saved_as, bt_args.bt_info->name); //backup the partial
    remove(TMPFILE); //get rid of tmp file
  }

  //NOTE: opening with 'a+' to take care of situation when file doesnt exist
  bt_args.fin = fopen(bt_args.bt_info->name, "a+"); //open the source file
  
  if(bt_args.verbose){
    be_dump(node);
  }
  
  //get bitfield info
  bt_bitfield_t bitfield;
  get_bitfield(&bt_args, &bitfield);
  bt_args.bitfield = bitfield;

  //initialize connections
  sockfd = init_socket(&bt_args); //initialize listening socket
  handshake_all(&bt_args); //send out the handshake
  tv.tv_sec = 0;
  tv.tv_usec = 50;
 
  //register some signals and fire them!
  signal(SIGALRM, handler);
  signal(SIGPIPE, handler);
  signal(SIGINT, handler);
  alarm(LIFEPERIOD); //set the alarm to check for liveness of peers

  //send out interested or not-interested messages
  send_interested(&bt_args);
  
  //handle anything we might want to download
  if (select_download_piece(&bt_args) != -1){ //we want to download
    leecher_loop(&bt_args);
  }
  
  //main client loop
  printf("Starting Main Loop\n"); 
  while(KEEP_ALIVE){
    SEEDING = TRUE; //we have now started seeding
    //accept a client connection
    FD_ZERO(&listen_set); //initialize
    FD_SET(sockfd, &listen_set); //make sockfd a non-blocking listener
    if (select(sockfd+1, &listen_set, (fd_set *)0, (fd_set *)0, &tv) > 0){
      if ((client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size))<0){
        perror("accept");
        continue;
      }
     
      else{
        ip = inet_ntoa(client_addr.sin_addr);
        sprintf(log_entry, "CONNECTION ACCEPTED from peer: %s\n", ip);
        LOGGER(bt_args.log_file, 1, log_entry);
      }
      
      printf("accepted new connection from %s\n", ip);
      if (leecher_handshake(client_sock, bt_args.bt_info->name, bt_args.id,&client_addr)){
        port = ntohs(client_addr.sin_port);
        sprintf(log_entry, "HANDSHAKE SUCCESS with peer:%s on port:%d\n", ip, port);
        LOGGER(bt_args.log_file, 1, log_entry);
        peer_t *newpeer = (peer_t *)malloc(sizeof(peer_t)); //new peer to try out 
        newpeer->sockfd = client_sock; //keep track of the communication socket
        index = add_peer(newpeer, &bt_args, ip, port);
        if (index != -1){
          //store the sockfd at that index
          bt_args.sockets[index] = client_sock;
          bt_args.poll_sockets[index].fd = client_sock; //initialize polling stuff
          bt_args.poll_sockets[index].events = POLLIN;

          //sendout the bitfield
          msg.length =  sizeof(bt_bitfield_t);
          msg.bt_type = BT_BITFIELD;
          msg.payload.bitfield = bitfield;
          send_to_peer(newpeer, &msg); //send out the message

        }
        else{
          free(newpeer); //reclaim the memory
          close(client_sock);//server the connection
        }

      }
    }
    
    pollrv = poll(bt_args.poll_sockets, MAX_CONNECTIONS, 50); //50ms timeout
    if (pollrv == -1){
      perror("poll"); //we got a problem from poll
    }

    else if (pollrv != 0){ //timeout didn't occur
      //check for events on all the fds
      int j;
      for (j=0;j<MAX_CONNECTIONS;j++){
        if (bt_args.poll_sockets[j].revents & POLLIN){
          //get message and process it
          read_from_peer(bt_args.peers[j], &msg, &bt_args);
         }
       }
     }
    
    //check livelenss of peers and replace dead (or useless) peers
    //with new potentially useful peers
    if (LIVENESS_CHECK_PENDING){
      check_all(&bt_args); //check status of all peers
      alarm(LIFEPERIOD); //reset the alarm
      LIVENESS_CHECK_PENDING = 0;
    }
  }
  __CLEANUP__(&bt_args);
  return 0;
}
