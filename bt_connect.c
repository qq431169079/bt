#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#include "bt_connect.h"
#include "bt_lib.h"
#include <openssl/sha.h>
#include "bencode.h"

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

int __verify__(char *fname, char first, char *name, char *chaff, char *id, char *hash, 
               char *ip, unsigned short port){
  //start verification
  if ((first  & 0x13) != 0x13){
    printf("Failed on the first byte.\n");
    return FALSE;
  }

  if (strncmp(name, "BitTorrent Protocol", 19) != 0){
    printf("Failed on Protocol Name \n");
    return FALSE;
  }

  if (strncmp(chaff, "00000000", 8) != 0){
    printf("Failed on chaff buffer\n");
    return FALSE;
  }

  //SHA1 hash of filename for info-hash
  unsigned char info_hash[20];
  SHA1((unsigned char *) fname, strlen(fname), info_hash);
  
  if (memcmp(hash, info_hash, 20) != 0){
    printf("Failed on the hash\n");
    return FALSE;
  }

  char id_val[20];
  calc_id(ip, INIT_PORT, id_val);
  
  if (memcmp(id, id_val, 20) != 0){
     printf("Failed on the hash\n");
     return ERR;
  }
  return TRUE;

}


//perform handshake from the leecher's perspective
//Compute the necessary computations, listen for incoming packet, verify it 
//and then send ours back.
int get_handshake(int sockfd, char* fname, struct sockaddr_in* sockaddr) {
  char *ip;
  char init[2]; //first byte to read
  char name[19]; //protocol name
  char chaff[8]; //8 bytes for the chaff
  char id[20]; //20 bytes for the id
  char hash[20]; //20 bytes for the SHA1 hash
  int debug = 0;
  read(sockfd, init, 1);
  read(sockfd, name, 19);
  read(sockfd, chaff, 8);
  read(sockfd, hash, 20);
  read(sockfd, id, 20);
  init[1] = '\0'; 
  if (debug){
    printf("First byte: %s\n", init);
    printf("Name %s\n", name);
    printf("Chaff %s\n", chaff);
    printf("Hash %s\n", hash);
    printf("ID %s\n", id);
  }
  ip = inet_ntoa(sockaddr->sin_addr); //get the IP 
  unsigned short port = ntohs(sockaddr->sin_port); //get the port number
  
  return __verify__(fname, init[0], name, chaff, id, hash, ip, port);
  
}

void send_handshake(int sockfd, char* fname, char *id) {
  char nineteen[2];
  //char id[20];
  char name[19] = "BitTorrent Protocol";
  
  nineteen[0] = 0x13;
  nineteen[1] = '\0';
  // setting extensions to 0
  unsigned char ext[8] = "00000000";
  //SHA1 hash of filename for info-hash
  unsigned char info_hash[20];
  SHA1((unsigned char *) fname, strlen(fname), info_hash);
  write(sockfd, nineteen, 1);
  write(sockfd, name, 19);
  write(sockfd, ext, 8);
  write(sockfd, info_hash, 20);
  write(sockfd, id, 20);
  return; 
}

//perform handshake from the leecher's perspective
//Compute the necessary computations, listen for incoming packet, verify it 
//and then send ours back.
int seeder_handshake(int sockfd, char* fname, char *id,struct sockaddr_in sockaddr) {
  send_handshake(sockfd, fname, id);  
  return get_handshake(sockfd, fname, &sockaddr);

}

int leecher_handshake(int sockfd, char *fname, char *id, struct sockaddr_in* sockaddr){
  int response;
  response = get_handshake(sockfd, fname, sockaddr);
  send_handshake(sockfd, fname, id);
  return response;
}

//send the handshake to all our peers
void handshake_all(bt_args_t *args){
  int i;
  int sockfd;
  struct sockaddr_in sockaddr, handshake_addr;
  bt_msg_t msg; //message structure
  char *fname = args->bt_info->name;
  char *ip;
  int port;
  char init_stats[80];
  for (i=0;i<MAX_CONNECTIONS;i++){
    if (args->peers[i]){
      //initiate socket and send out handshake
      //keep track of the sockfd in the args struct
      if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
      }

      sockaddr = args->peers[i]->sockaddr;
      handshake_addr = args->peers[i]->sockaddr;

      if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr)) < 0){
        perror("connect");
        continue;
      }
      
      args->peers[i]->sockfd = sockfd; //backup the socket 
      ip = inet_ntoa(handshake_addr.sin_addr);
      port = ntohs(handshake_addr.sin_port);
      sprintf(init_stats, "HANDSHAKE INIT to peer: %s on port: %d\n", ip, port);
      LOGGER(args->log_file, 1, init_stats);
      if (seeder_handshake(sockfd, fname, args->id, handshake_addr)){ 
        sprintf(init_stats, "HANDSHAKE SUCCESS from peer: %s on port: %d\n", ip, port);
        LOGGER(args->log_file, 1, init_stats);
        args->sockets[i] = sockfd;
        args->poll_sockets[i].fd = sockfd;
        args->poll_sockets[i].events = POLLIN;
        
        //send our bitfield here
        //sendout the bitfield
        msg.length = sizeof(size_t) + args->bitfield.size;
        msg.bt_type = BT_BITFIELD;
        msg.payload.bitfield = args->bitfield;
        send_to_peer(args->peers[i], &msg); //send out the message
      }
      else{
        sprintf(init_stats, "HANDSHAKE DECLINED from peer: %s on port: %d\n", ip, port);
        LOGGER(args->log_file, 1, init_stats);

      }
    }
  }
}


