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

//fill the listen buffer with the necessary info
//set the listening port to the current default 
void fill_listen_buff(struct sockaddr_in *destaddr, int port){
  struct hostent *hostinfo;
  char ip[NAME_MAX];
  if (gethostname(ip, NAME_MAX) < 0){
    perror("gethostbyname");
    exit(1);
  }
  
  if(!(hostinfo = gethostbyname(ip))){  
    fprintf(stderr,"ERROR: Invalid host name %s",ip);
    usage(stderr);
    exit(1);
  }
  
  destaddr->sin_family = hostinfo->h_addrtype;
  bcopy((char *) hostinfo->h_addr,
        (char *) &(destaddr->sin_addr.s_addr),
        hostinfo->h_length);
  
  destaddr->sin_port = htons(port);
  
}

/***********************************************************
 * pass in a be_node and extract necessary attributes
 * including the url of the torrent tracker and values for 
 * suggested name of the file, piecelength, file size and hash
 * for each of the pieces to check for corruption
 * **********************************************************/
void extract_attributes(be_node *node, bt_info_t *info){
  int i=0, j=0;
  char *key;
  char *k;
  if (node->type == BE_DICT){
    while (node->val.d[i].key){
      key = node->val.d[i].key;
      if (strcmp(key, "announce") == 0){
        strcpy(info->announce,node->val.d[i].val->val.s);
      }

      else if (strcmp(key, "info") == 0){
        while (node->val.d[i].val->val.d[j].key){
          k = node->val.d[i].val->val.d[j].key;
          if (strcmp(k, "length") == 0)
            info->length = node->val.d[i].val->val.d[j].val->val.i;
          else if (strcmp(k, "name") == 0)
            strcpy(info->name, node->val.d[i].val->val.d[j].val->val.s);
          else if (strcmp(k, "piece length") == 0)
            info->piece_length = node->val.d[i].val->val.d[j].val->val.i;
          else if (strcmp(k, "pieces") == 0)
            info->piece_hashes = (char **)node->val.d[i].val->val.d[j].val->val.s;
          j++;
        }
        
        //poor man's implementation of ceil
        int num_pieces = ((float)info->length)/info->piece_length;
        float f_num_pieces = ((float)info->length)/info->piece_length;
        if (f_num_pieces > num_pieces)
          num_pieces += 1;
        info->num_pieces = num_pieces;
      }
      i++;
    }
  }

}

//receive the handshake
int receive_handshake(struct sockaddr_in sockaddr, int port){
   int sockfd, client_sock;
   socklen_t addr_size;
   struct sockaddr_in client_addr;
   char data[HANDSHAKE_SIZE];

   addr_size = sizeof(struct sockaddr);
   //open the socket
   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
     perror("socket");
     return ERR;
   }

   //optionally bind() the sock
   sockaddr.sin_port = htons(port);
   if (bind(sockfd, (struct sockaddr *)&sockaddr, addr_size) == -1){
     perror("bind");
     close(sockfd);
     return ERR;
   }
   
   //set listen to up to 5 queued connections
   if (listen(sockfd, 5) == -1){
     perror("listen");
     close(sockfd);
     return ERR;
   }
   
   printf("listening on port (handshake) %d\n", port);
   //accept a client connection
   if ((client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size)) < 0){
     perror("accept");
     close(sockfd);
     return ERR;
   }
   
   if(read(client_sock, data, HANDSHAKE_SIZE) < HANDSHAKE_SIZE){
     perror("Handshaking Protocol");
     //return ERR;
   }
   printf("Received: %s\n", data); 
   close(client_sock); 
   close(sockfd);
   return 0;

}


//send the handshake procedure
int send_handshake(struct sockaddr_in sockaddr, int port){
  char *msg = "Hello World";
  int len = 11; //dummy length of message
  char data[HANDSHAKE_SIZE];
  int sockfd;
  
  memcpy(data, msg, len);
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("socket");
    return ERR;
  }
  
  sockaddr.sin_port = htons(port);
  if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr)) < 0){
    //perror("socket");
    close(sockfd);
    return -2;
  }

  if (write(sockfd, data, HANDSHAKE_SIZE) < 0){
    perror("Failed to send handshake");
    close(sockfd);
    return ERR;
  }
  close(sockfd);
  return 0;
}

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
