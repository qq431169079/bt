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
  unsigned short port;
  char id[ID_SIZE];
  fd_set listen_set;
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

  //send handshake
  handshake_all(&bt_args);

  //main client loop
  printf("Starting Main Loop\n");
  tv.tv_sec = 0;
  tv.tv_usec = 50;
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
        index = add_peer(newpeer, &bt_args, ip, port);
        if (index != -1){
          //store the sockfd at that index
          bt_args.sockets[index] = client_sock;
        }
        else{
          //reclaim the memory
          free(newpeer);
        }

      }
    }
    
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
