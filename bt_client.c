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

int main (int argc, char * argv[]){

  bt_args_t bt_args;
  be_node * node; // top node in the bencoding
  bt_info_t bt_info; //info be parsed from the be_node
  int i=0;
  int sockfd, clientfd;
  struct sockaddr_in serv_addr, client_addr;
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
  if(bt_args.verbose){
    be_dump(node);
  }

  //main client loop
  printf("Starting Main Loop\n");

  //open up the server connection
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
      perror("socket");
      exit(1);
  }

  //bind on the socket

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
