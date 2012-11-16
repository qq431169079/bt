#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h> //internet address library
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <arpa/inet.h>

#include <openssl/sha.h> //hashing pieces

#include "bencode.h"
#include "bt_lib.h"
#include "bt_setup.h"



void calc_id(char * ip, unsigned short port, char *id){
  char data[256];
  int len;
  
  //format print
  len = snprintf(data,256,"%s%u",ip,port);

  //id is just the SHA1 of the ip and port string
  SHA1((unsigned char *) data, len, (unsigned char *) id); 

  return;
}


/**
 * init_peer(peer_t * peer, int id, char * ip, unsigned short port) -> int
 *
 *
 * initialize the peer_t structure peer with an id, ip address, and a
 * port. Further, it will set up the sockaddr such that a socket
 * connection can be more easily established.
 *
 * Return: 0 on success, negative values on failure. Will exit on bad
 * ip address.
 *   
 **/
int init_peer(peer_t *peer, char * id, char * ip, unsigned short port){
    
  struct hostent * hostinfo;
  //set the host id and port for referece
  memcpy(peer->id, id, ID_SIZE);
  peer->port = port;
    
  //get the host by name
  if((hostinfo = gethostbyname(ip)) ==  NULL){
    perror("gethostbyname failure, no such host?");
    herror("gethostbyname");
    exit(1);
  }
  
  //zero out the sock address
  bzero(&(peer->sockaddr), sizeof(peer->sockaddr));
      
  //set the family to AF_INET, i.e., Iternet Addressing
  peer->sockaddr.sin_family = AF_INET;
    
  //copy the address to the right place
  bcopy((char *) (hostinfo->h_addr), 
        (char *) &(peer->sockaddr.sin_addr.s_addr),
        hostinfo->h_length);
    
  //encode the port
  peer->sockaddr.sin_port = htons(port);
  
  return 0;

}

/**
 * print_peer(peer_t *peer) -> void
 *
 * print out debug info of a peer
 *
 **/
void print_peer(peer_t *peer){
  int i;

  if(peer){
    printf("peer: %s:%u ",
           inet_ntoa(peer->sockaddr.sin_addr),
           peer->port);
    printf("id: ");
    for(i=0;i<ID_SIZE;i++){
      printf("%02x",peer->id[i]);
    }
    printf("\n");
  }
}

//save the piece to the file
int save_piece(bt_args_t *args, bt_piece_t *piece){
  int base; //base offset
  int offset, length, bytes;

  //get location within file
  length = args->bt_info->piece_length;
  base = length * piece->index;
  offset = base + piece->begin;

  if (fseek(args->f_save, offset, SEEK_SET) != 0){
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }

  bytes = fwrite(piece->piece, 1, length, args->f_save);
  return bytes;
}

//load the piece in the bt_piece_t struct
int load_piece(bt_args_t *args, bt_piece_t *piece){
  int base; //base offset
  int offset, length, bytes;

  //get location within file
  length = args->bt_info->piece_length;
  base = length * piece->index;
  offset = base + piece->begin;

  if (fseek(args->f_save, offset, SEEK_SET) != 0){
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }

  bytes = fread(piece->piece, 1, length, args->f_save);
  return bytes;
}

//get the bitfield
int get_bitfield(bt_args_t *args, bt_bitfield_t * bitfield){
  /*
  FILE *fp;
  int i;
  int length = args->bt_info->piece_length;
  char *piece = NULL;
  char *fname = args->bt_info->name;
  fp = fopen(fname, "r");

  for (i=0;i<args->bt_info->num_pieces;i++){
    fseek(fp, i*length, SEEK_SET);
    fread(piece, 1, length, fp);
    //TODO compare the hashes
  }*/
  return 0;

}

