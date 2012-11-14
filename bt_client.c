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

//seeder when we act as the client
void seeder(bt_args_t *args){
  int sockfd, msgsize;
  struct sockaddr_in sockaddr, handshake_addr;
  char data[BUFSIZE];
  peer_t *peer;
  //open the socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("socket");
    exit(1);
  }
  
  printf("seeder socket established\n");
  sockaddr = args->peers[1]->sockaddr;
  peer = args->peers[1];
  fill_listen_buff(&handshake_addr, 0); //misnomer, correct though :P
  //set socket address and connect
  if(connect(sockfd,(struct sockaddr *)&sockaddr, sizeof(struct sockaddr)) < 0){
      perror("connect");
      exit(1);
  }
 
  printf("commencing handshake \n");
  receive_handshake(handshake_addr, HANDSHAKE_PORT_A);
  printf("handshake received\n");
  while(send_handshake(sockaddr, HANDSHAKE_PORT_B) == -2);
  printf("handshake sent\n");

  //close the socket
  close(sockfd);

}

//leecher code, when we act as the server!
void leecher(bt_args_t *args){
   int sockfd, client_sock;
   socklen_t addr_size;
   struct sockaddr_in serv_addr, client_addr, handshake_addr;
   char data[BUFSIZE];
   int bytes; //read bytes
   int offset = 0; //offset to start writing at
   int n_bytes = 0; //bytes sent in preamble


   addr_size = sizeof(struct sockaddr);
   //open the socket
   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
     perror("socket");
     exit(1);
   }

   //optionally bind() the sock
   fill_listen_buff(&serv_addr, args->port);
   if (bind(sockfd, (struct sockaddr *)&serv_addr, addr_size) == -1){
     perror("bind");
     exit(1);
   }
   printf("managed to bind\n");
   
   //set listen to up to 5 queued connections
   if (listen(sockfd, 5) == -1){
     perror("listen");
     exit(1);
   }
   
   printf("listening\n");
   //accept a client connection
   if ((client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size)) < 0){
     perror("accept");
     exit(1);
   }
   
   printf("accepted connection\n");
   printf("sending handshake \n");
   send_handshake(client_addr, HANDSHAKE_PORT_A);
   printf("handshake sent\n");
   fill_listen_buff(&handshake_addr, 0); //misnomer, correct though :P
   receive_handshake(handshake_addr, HANDSHAKE_PORT_B);
   printf("handshake received\n");
   
   //after handshake is complete, what do we want to do?

   
   
   close(client_sock); 
}

int main(int argc, char * argv[]){

  bt_args_t bt_args;
  be_node * node; // top node in the bencoding
  bt_info_t bt_info; //info be parsed from the be_node
  int i=0;
  int num_peers = 0;

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
  extract_attributes(node, &bt_info);
  bt_args.bt_info = &bt_info;
  if(bt_args.verbose){
    be_dump(node);
  }


  //main client loop
  printf("Starting Main Loop\n");
 
  if (bt_args.leecher)
    leecher(&bt_args);
  else
    seeder(&bt_args);

  while(1){

    //try to accept incoming connection from new peer
       
    
    //poll current peers for incoming traffic
    //   write pieces to files
    //   udpdate peers choke or unchoke status
    //   responses to have/havenots/interested etc.
    
    //for peers that are not choked
    //   request pieaces from outcoming traffic

    //check livelenss of peers and replace dead (or useless) peers
    //with new potentially useful peers
    
    //update peers, 
    break;
  }

  return 0;
}
