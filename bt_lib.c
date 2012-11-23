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

//send message to all peers
int send_all(bt_args_t *args, bt_msg_t *msg){
  int i;
  for (i=0;i<MAX_CONNECTIONS;i++){
    //check if peer is good and then send out to that peer
    //TODO check for all that choke and interested shit
    if(args->peers[i]){ //is this a valid peer
      send_to_peer(args->peers[i], msg);
    }
  }
  return 0;
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

//send a message to peer
int send_to_peer(peer_t *peer, bt_msg_t *msg) {
  int bytes;
  bytes = write(peer->sockfd, 
                msg,
                sizeof(bt_msg_t));
  if (bytes < 0)
    perror("send_to_peer: write()");
  return bytes;
}

int read_from_peer(peer_t *peer, bt_msg_t *msg, bt_args_t *args) {
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
  bt_msg_t response; //reusable response message
  char *logfile = args->log_file; 
  char log_msg[80]; //log message buffer
  char *ip = inet_ntoa(peer->sockaddr.sin_addr); 
  read(peer->sockfd, msg, sizeof(bt_msg_t)); 
  int piece_index;
  bt_piece_t piece; //this little piece of ....
  if(msg->length == 0){
    sprintf(log_msg, "MESSAGE RECEIVED :{KEEP_ALIVE} from %s\n", ip);
    LOGGER(logfile, 1, log_msg);
    //just a keep alive message
    return 0;
  }
  else{
    switch(msg->bt_type){
      case BT_BITFIELD:
        printf("received bitfield message\n");
        sprintf(log_msg, "MESSAGE RECEIVED :{BITFIELD} from %s\n", ip);
        LOGGER(logfile, 1, log_msg);
        peer->bitfield = msg->payload.bitfield; //backup the bitfield
        peer->choked = 0;
        peer->interested = 1;
        //TODO: if peer has pieces we dont have, set the peer to be unchoked
        //      and interested
        break;
      case BT_REQUEST:
        piece_index = msg->payload.request.index;
        printf("received a request for a piece\n");
        sprintf(log_msg, "MESSAGE RECEIVED :{REQUEST} for piece:%d from %s\n",
                piece_index, ip);
        LOGGER(logfile, 1, log_msg);
        
        //TODO: if we have the piece, send out the piece
        if (own_piece(args, piece_index)){
          printf("sending out response for piece\n");
          sprintf(log_msg, "MESSAGE RESPONSE :{PIECE} for piece:%d to %s\n",
                  piece_index, ip);
          LOGGER(logfile, 1, log_msg);
          load_piece(args, &piece); //load the piece if we have it

          response.length = 2 + sizeof(bt_piece_t);
          response.bt_type = BT_PIECE;
          response.payload.piece = piece;
          send_to_peer(peer, &response);
        }
        break;
      
      case BT_PIECE:
        piece_index = msg->payload.piece.index;
        printf("received a piece of the cake\n");
        sprintf(log_msg, "MESSAGE RECEIVED :{PIECE} received file piece:%d from %s\n",
                piece_index, ip);
        LOGGER(logfile, 1, log_msg);
        
        //if we already have the piece, just ignore the message
        if (own_piece(args, piece_index)){
           sprintf(log_msg, "MESSAGE RECEIVED :{PIECE} received file piece:%d from %s\n",
                piece_index, ip);
           LOGGER(logfile, 1, log_msg);
           //TODO print stats here
           piece = msg->payload.piece;
           save_piece(args, &piece);

        }
        break;

      default:
        printf("unknown message type\n");
        break;
    }

      /*
      case '0':
        //choked

      case '1':
        //unchoked
      case '2':
        //interested
      case '3':
        //not interested
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
        //peer indicating it no longers wants a piece*/
    }
    
  return 0;
}

//local implementation of ceil 
//The real ceil kinda sucks, so we opted to write our own
int ceiling(int dividend, int divisor){
  int result = ((float)dividend)/divisor;
  float f_result = ((float)dividend)/divisor;
  if (f_result > result)
    result++;
  return result;

}

//get the piece to start downloading
//return -1 when we have every piece
int select_download_piece(bt_args_t *args){
  int i,j, index;
  int bytes = ceiling(args->bitfield.size, 8); //number of character bytes
  char *bitfield = args->bitfield.bitfield;
  for(i=0;i< bytes;i++){
    for(j=0;j<8;j++){
      index = j + (i*8);
      if (index >= args->bitfield.size) //stopping condition
        continue;

      //search for a zero bit and use that as index
      if ((bitfield[i] & (1 << (7-j))) == 0)
        return index;
    }
  }
  return -1; //we now have all the pieces we need
}

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
  char *pieces;
  char **hashes; //array of the hashes for each piece

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
            pieces = (char *)node->val.d[i].val->val.d[j].val->val.s;
          j++;
        }
        
      }
      i++;
    }
    
    //poor man's implementation of ceil
    int num_pieces = ((float)info->length)/info->piece_length;
    float f_num_pieces = ((float)info->length)/info->piece_length;
    if (f_num_pieces > num_pieces)
      num_pieces += 1;
    info->num_pieces = num_pieces;
    //TODO reclaim all this dynamic memory when we close the application
    hashes = (char **)malloc(sizeof(char *)*num_pieces);
    for(i=0;i<num_pieces;i++){
      hashes[i] = (char *)malloc(HASH_UNIT);
      strncpy(hashes[i], (pieces + i*HASH_UNIT), HASH_UNIT);
    }
    info->piece_hashes = hashes;

  }
  return 0;

}


//save the piece to the file
int save_piece(bt_args_t *args, bt_piece_t *piece){
  int base; //base offset
  int offset, length;
  int bytes = 0;
  FILE *fp = fopen(args->save_file, "a+");

  //get location within file
  length = args->bt_info->piece_length;
  base = length * piece->index;
  offset = base + piece->begin;

  if (fseek(fp, offset, SEEK_SET) != 0){
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }
 
  //TODO :resolve the segfault here
  //bytes = fwrite(piece->piece, 1, length, fp);
  fclose(fp);
  return bytes;
}

//load the piece in the bt_piece_t struct
int load_piece(bt_args_t *args, bt_piece_t *piece){
  int base; //base offset
  int offset, length, bytes;
  FILE *fp = fopen(args->bt_info->name, "a+");
  
  //get location within file
  length = args->bt_info->piece_length;
  char buffer[length]; 
  base = length * piece->index;
  offset = base + piece->begin;

  if (fseek(fp, offset, SEEK_SET) != 0){
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }

  bytes = fread(buffer, 1, length, fp);

  //backup the piece data
  piece->piece = buffer; 
  fclose(fp);
  return bytes;
}

void print_bits(char byte){
  int i, val;
  for(i=0;i<8;i++){
    val = byte & (1 << i) ? 1: 0;
    printf("%d", val);
  }
  printf("\n");
}

//given an index, set the corresponding bit to true
//called when a file piece has been received
int set_bitfield(bt_args_t *args, int index){
  int base, offset;
  base = index/8;
  offset = index - (base*8);
  args->bitfield.bitfield[base] = args->bitfield.bitfield[base] | (1 << (7-offset));
  return 0;
}

//do we own this piece?
int own_piece(bt_args_t *args, int piece){
  int base, offset;
  base = piece/8;
  offset = piece - (base * 8);
  if ((args->bitfield.bitfield[base] & (1 << (7-offset))))
    return TRUE;
  return FALSE;

}

//get the bitfield
int get_bitfield(bt_args_t *args, bt_bitfield_t *bitfield){
  FILE *fp;
  int i,j, bytes, count;
  int index = 0; //index in the char array
  int length = args->bt_info->piece_length;
  char piece[length];
  char *bits;
  char *fname = args->bt_info->name;
  //need place to store piece hash of current file
  unsigned char result_hash[20];

  count = args->bt_info->num_pieces; //get number of pieces
  bytes = ceiling(count, 8); //get the number of char needed to hold bits 
  bits = (char *)malloc(bytes);
  
  //opening file for reading
  fp = fopen(fname, "a+");
  //check hashed pieces of file against piece hashes of torrent file
  for (i=0; i<bytes; i++){
    bits[i] = 0; //zero out the char
    //printf("%d  ", bits[i]);
    for (j=0;j<8;j++){
      index = j + (i*8);
      if (index >= count) //stopping condition
        continue; 
      fseek(fp, index*length, SEEK_SET);
      fread(piece, 1, length, fp);
      rewind(fp); //always rewind after use
       
      SHA1((unsigned char *) piece, length, result_hash);
      if (memcmp(result_hash, args->bt_info->piece_hashes[index], HASH_UNIT) == 0){
        bits[i] = bits[i] | (1<<(7-j)); //set the correct bit
       // printf("[%d] available\n", index);
      }
    }
    print_bits(bits[i]);
  }
  
  
  //set the appropriate variables
  bitfield->bitfield = bits;
  bitfield->size = count;
  return 0;
}

int sha1_piece(bt_args_t *args, bt_piece_t* piece, unsigned char * hash) {
  SHA1((unsigned char *) piece, sizeof(bt_piece_t), hash);
  return 0;
}

int contact_tracker(bt_args_t  * bt_args) {
  //TODO
  return 0;
}

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
