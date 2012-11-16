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

  //function for building message
int client_handshake(int sockfd, char* fname, struct sockaddr_in* sockaddr) {
  unsigned char nineteen = "19";
  unsigned char name[19] = "BitTorrent Protocol";
  
  // setting extensions to 0
  unsigned char ext[8];
  memset(ext, 0, 8);

  //SHA1 hash of filename for info-hash
  unsigned char info_hash[20];
  SHA1((unsigned char *) fname, strlen(fname), info_hash);

  //setting your peer id
  char *ip; 
  unsigned short port; 
  char id[20];

  calc_id(ip, port, id);

  return; //will be 1 or 0
}



