#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bt_connect.h"
#include "bt_lib.h"
#include "bencode.h"

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

