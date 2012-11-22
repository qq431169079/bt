#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h> //internet address library
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

#include <sys/stat.h>
#include <arpa/inet.h>

#include <openssl/sha.h> //hashing pieces

#include "bencode.h"
#include "bt_lib.h"
#include "bt_setup.h"


//NOTE: parse_bt_info is in bt_connect i believe.. called by different name

void calc_id(char * ip, unsigned short port, char *id){
  char data[256];
  int len;
  
  //format print
  len = snprintf(data,256,"%s%u",ip,port);

  //id is just the SHA1 of the ip and port string
  SHA1((unsigned char *) data, len, (unsigned char *) id); 

  return;
}


unsigned int select_id() {
  //TODO
  return 0;
}

int add_peer(peer_t *peer, bt_args_t *bt_args, char * hostname, unsigned short port) {
  //TODO
  return 0;
}

int drop_peer(peer_t *peer, bt_args_t *bt_args) {
  //TODO
  return 0;
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

int check_peer(peer_t *peer) {
  //TODO
  //need to figure out how this is going to work
  // -- new type of message perhaps?
  // -- respond immediately when you get this...
  // -- timeout indicates they are offline
  return 0;
}

int poll_peers(bt_args_t *bt_args) {
  //TODO
  return 0;
}

int send_to_peer(peer_t * peer, bt_msg_t * msg) {
  //TODO
  return 0;
}

int read_from_peer(peer_t * peer, bt_msg_t *msg) {
  //TODO
  //note: when writing the pseudocode here I basically
  //unintenionally ignored the fact that some peers
  //are choked and others aren't
  //
  //need to address this...
  //
  //we have list of peer pointers in bt_args_t
  //
  //Question: How do we keep track of which peers are choked
  //and which ones are unchoked???
  //
  //IN THE STRUCT PEER!!!  type peer_t
  //
  //
   
  if(msg->length == 0)
    //just a keep alive message
    // 
  else:
    switch(atoi(msg->bt_type))
    {
      case BT_CHOKE:
        //choke
        peer->choked = 1;
      case BT_UNCHOKE:
        //unchoke
        peer->choked = 0;
      case BT_INTEREST:
        //interest
        peer->interested = 1;
      case BT_NOT_INTERESTED:
        //not interested
        peer->interested = 0;
      case '4':
        //have
        //message indicating a peer has completed downloading a piece
        //message is just an int indicating which piece
        
        //check to see if we want this piece
        //      -- if so what offset??
        //      int offset;
        //      SEE CHANGE LATER BELOW

        //if we want this piece, create request msg 
        bt_msg_t newMsg;
        
        //length in bytes (bytes right?)
        newMsg.length = 1 + sizeof(bt_request_t);
        newMsg.bt_type = (unsigned char) "6";
        
        //creating payload
        bt_request_t reqMsg;
        reqMsg.index = msg->payload.have;
        reqMsg.begin = 0;                    //CHANGE LATER....
                                             //pull out of above check

        reqMsg.length = 0;                   //CHANGE LATER 2...
                                             //it will be this except names instead of types
                                             //bt_args_t.bt_info->piece_length - offset;

        //addeding reqMsg to newMsg
        newMsg.payload.request = reqMsg; 
        
        //send request message to that peer
        send_to_peer(peer, newMsg);

        //if we don't want this piece, otherwise ignore??
      
      case '5':
        //bitfield
        //sent after a completed handshake
        //indicates which pieces a peer has available

        //decide which if any pieces we want
        //    - mark ourselves as interested in that peer
              // two options:
              //  --- peer->interested = BT_INTERESTED
              //  --- peer->interested = BT_NOT_INTERESTED
        //    - make request message(s) for wanted piece(s)
        //
      case '6':
        //request
        //asking for download
        
        //is this peer choked/unchoked?
        if (peer->choked == BT_UNCHOKE) {
                //construct bt_piece_t message
                //send_to_peer(peer, pieceMsg)
        }
        else {
          //do we want to unchoke them??
        }

      case '7':
        //piece
        //piece/block of a piece
        
        //check to see if we wanted it (is that necessary?)
        
        //validate the piece against torrent file?
        
        //call save_piece and save it
        
        //announce to all other peers via have msg
        //      - construct have msg
        //      - for loop that run # of peer times and sends
        //      have msg to all of them via send_to_peer

      case '8':
        //cancel
        //peer indicating it no longers wants a piece
    }
      
  return 0;
/***********************************************************
 * pass in a be_node and extract necessary attributes
 * including the url of the torrent tracker and values for 
 * suggested name of the file, piecelength, file size and hash
 * for each of the pieces to check for corruption
 * **********************************************************/
int parse_bt_info(bt_info_t *info, be_node *node){
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
  return 0;

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
int get_bitfield(bt_args_t *args, bt_bitfield_t * bfield){
  FILE *fp;
  int i;
  int length = args->bt_info->piece_length;
  char *piece = NULL;
  char *fname = args->bt_info->name;
  //need place to store piece hash of current file
  unsigned char result_hash[20];
  int bSize;

  //determining bitfield size
  //note: assuming size is the number of pieces, rather than memory size
  bfield->size = args->bt_info->num_pieces;
  
  //calculating space necessary for bitfield (using num_pieces instead
  //of size because of types, size is size_t)
  bSize = (args->bt_info->num_pieces)/8; //integer division
  if (((args->bt_info->num_pieces) % 8) > 0) {
    bSize += 1;
  }

  //allocating memory for bitfield array within bitfield strcut
  bfield->bitfield = (char *) malloc(bSize);
  
  //make sure all initial entries in bitfield are 0
  for (i=0; i<bSize; i++) {
    bfield->bitfield[i] == bfield->bitfield[i] && 0x0;
  }
  //opening file for reading
  fp = fopen(fname, "r");
  
  //check hashed pieces of file against piece hashes of torrent file
  for (i=0; i<bitfield->size; i++){
    fseek(fp, i*length, SEEK_SET);
    fread(piece, 1, length, fp);
    //compare the hashes
    SHA1((unsigned char *) piece, length, result_hash);
    if (memcmp(result_hash, args->bt_info->piece_hashes[i], 20) == 0){
      bfield->bitfield[i/8] = (bitfield[i/8] << 1) + 1;  //there is an unnecessary left shift 
                                                         //that occurs for first piece check
                                                         //of each new element in bitfield array
                                                         //TODO worth fixing???
                                                         //same for else statement
    } 
    else {
      bfield->bitfield[i/8] = (bitfield[i/8] << 1);
    }
  }
  return 0;
}

int sha1_piece(bt_args_t *args, bt_piece_t* piece, unsigned char * hash) {
  SHA1((unsigned char *) piece, sizeof(bt_piece_t), hash);
  return 0;
}

int contact_tracker(bt_args_t  * bt_args) {
  //TODO
  return 0;

//build the log message and write it out to the 
//logfile
void LOGGER(char *log, int type, char *msg){
  time_t rawtime;
  struct tm * timeinfo;
  char *tv;
  FILE *fp;

  time( &rawtime );
  timeinfo = localtime ( &rawtime );
  tv = asctime(timeinfo);
  tv[strlen(tv)-1] = '\0'; //get rid of trailing \n
  switch(type){
    case 0: //this is an initialization
      fp = fopen(log, "w+");
      fprintf(fp, "[%s] %s", tv, msg);
      break;
    default:
      fp = fopen(log, "a+");
      fprintf(fp, "[%s] %s", tv, msg);
      break;



  }
  fclose(fp); //close the file
  return;
}

//add a new peer to the swarm
//calculate peer id and all here
int add_peer(peer_t *peer, bt_args_t *args, char *ip, unsigned short port){
  int i;
  char id[ID_SIZE];
  calc_id(ip, port, id);
  init_peer(peer, id, ip, port);
  
  //look for free spot
  for (i=0;i<MAX_CONNECTIONS;i++){
    if (args->peers[i] == NULL){ //use this slot
      args->peers[i] = peer;
      return i;
    }
  }

  return -1; //on failure
}
