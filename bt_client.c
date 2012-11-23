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



int main(int argc, char * argv[]){
  bt_args_t bt_args;
  be_node * node; // top node in the bencoding
  bt_info_t bt_info; //info be parsed from the be_node
  int i=0;
  int num_peers = 0;
  int sockfd, client_sock;
  struct sockaddr_in serv_addr, client_addr;
  socklen_t addr_size;
  struct timeval tv;
  char *ip;
  int index;
  bt_msg_t msg; //reusable message casing
  int pollrv; //return value from the call to poll()
  unsigned short port;
  char id[ID_SIZE];
  fd_set listen_set;
  int current = 0; //current piece being analyzed
  int previous;
  bt_request_t request; //reusable request message
  parse_args(&bt_args, argc, argv);
  
  //count number of initial peers
  while (bt_args.peers[i])
    i++;
  num_peers = i;

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
  if(bt_args.verbose){
    be_dump(node);
  }
  
  //initialize listening socket
  addr_size = sizeof(struct sockaddr);
  //open the socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("socket");
    exit(1);
  }

  //get my own IP
  fill_listen_buff(&serv_addr, bt_args.port);
  bt_args.ip = inet_ntoa(serv_addr.sin_addr); //get the IP
  
  char init_stats[80];
  LOGGER(bt_args.log_file, 0, "Starting the BitTorrent Client\n");
  sprintf(init_stats, "Listening on IP:port is %s:%d\n", bt_args.ip, bt_args.port);
  LOGGER(bt_args.log_file, 1, init_stats);
  //get own ID
  calc_id(bt_args.ip, bt_args.port, id);
  memcpy(bt_args.id, id, 20);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, addr_size) == -1){
    perror("bind");
    exit(1);
  }
  printf("managed to bind\n");
   
  //set listen to up to 5 queued connections
  if (listen(sockfd, MAX_CONNECTIONS) == -1){
    perror("listen");
    exit(1);
  }

  //get bitfield info
  bt_bitfield_t bitfield;
  get_bitfield(&bt_args, &bitfield);
  bt_args.bitfield = bitfield; //backup the bitfield

  //send handshake
  handshake_all(&bt_args);

  tv.tv_sec = 0;
  tv.tv_usec = 50;
  

  //main client loop
  printf("Starting Main Loop\n");
  while(1){
    //accept a client connection
    FD_ZERO(&listen_set); //initialize
    FD_SET(sockfd, &listen_set); //make sockfd a non-blocking listener
    if (select(sockfd+1, &listen_set, (fd_set *)0, (fd_set *)0, &tv) > 0){
      printf("accepted\n");
      if ((client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size))<0){
        perror("accept");
        continue;
      }
     
      else{
        ip = inet_ntoa(client_addr.sin_addr);
        sprintf(init_stats, "CONNECTION ACCEPTED from peer: %s\n", ip);
        LOGGER(bt_args.log_file, 1, init_stats);
      }
      
      if (leecher_handshake(client_sock, bt_args.bt_info->name, bt_args.id,&client_addr)){
        port = ntohs(client_addr.sin_port);
        sprintf(init_stats, "HANDSHAKE SUCCESS with peer:%s on port:%d\n", ip, port);
        LOGGER(bt_args.log_file, 1, init_stats);
        peer_t *newpeer = (peer_t *)malloc(sizeof(peer_t)); //new peer to try out 
        newpeer->sockfd = client_sock; //keep track of the communication socket
        index = add_peer(newpeer, &bt_args, ip, port);
        if (index != -1){
          //store the sockfd at that index
          bt_args.sockets[index] = client_sock;
          bt_args.poll_sockets[index].fd = client_sock; //initialize polling stuff
          bt_args.poll_sockets[index].events = POLLIN;

          //sendout the bitfield
          msg.length = 2+4+ceiling(bitfield.size, 8);
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
    
    
    previous = current; //keep track of the last piece that was downloaded
    pollrv = poll(bt_args.poll_sockets, MAX_CONNECTIONS, 50); //50ms timeout
    if (pollrv == -1){
      perror("poll"); //we got a problem from poll
    }

    else if (pollrv != 0){ //timeout didn't occur
      //check for events on all the fds
      //TODO weird behavior of peer dies out
      int j;
      for (j=0;j<MAX_CONNECTIONS;j++){
        if (bt_args.poll_sockets[j].revents & POLLIN){
          //get message and process it
          printf("Got a message from peer in index %d\n", j);
          read_from_peer(bt_args.peers[j], &msg, &bt_args);
         }
       }
     }
     
    //select piece to download and send out the message
    current = select_download_piece(&bt_args);
    //printf("previous %d -- current %d\n", previous, current);
    if (current == previous){
      //noone has this piece so move on
      set_bitfield(&bt_args, current);
      continue;
    }

    //send the request for the current piece
    if (current != -1){
      request.index = current;
      request.length = bt_args.bt_info->piece_length;
      request.begin = 0;

      msg.length = 2 + sizeof(bt_request_t);
      msg.bt_type = BT_REQUEST;
      msg.payload.request = request;
      send_all(&bt_args, &msg); //send out the request message to all peers
    }
    sleep(1); 
    
    //poll current peers for incoming traffic
    //   write pieces to files
    //   udpdate peers choke or unchoke status
    //   responses to have/havenots/interested etc.
    
    //for peers that are not choked
    //   request pieaces from outcoming traffic

    //check livelenss of peers and replace dead (or useless) peers
    //with new potentially useful peers
    
    //update peers, 
  }

  return 0;
}
