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
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )


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

//initialize the listening socket
int init_socket(bt_args_t *bt_args){
  socklen_t addr_size;
  struct sockaddr_in serv_addr;
  int sockfd;
  char id[ID_SIZE];

  //initialize listening socket
  addr_size = sizeof(struct sockaddr);
  //open the socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("socket");
    exit(1);
  }

  //get my own IP
  fill_listen_buff(&serv_addr, bt_args->port);
  bt_args->ip = inet_ntoa(serv_addr.sin_addr); //get the IP
  
  char init_stats[80];
  LOGGER(bt_args->log_file, 0, "Starting the BitTorrent Client\n");
  sprintf(init_stats, "Listening on IP:port is %s:%d\n", bt_args->ip, bt_args->port);
  LOGGER(bt_args->log_file, 1, init_stats);
  //get own ID
  calc_id(bt_args->ip, bt_args->port, id);
  memcpy(bt_args->id, id, 20);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, addr_size) == -1){
    perror("bind");
    exit(1);
  }
  printf("Bind Succeeded\n");
   
  //set listen to up to 5 queued connections
  if (listen(sockfd, MAX_CONNECTIONS) == -1){
    perror("listen");
    exit(1);
  }
  return sockfd;

}

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
int init_peer(peer_t *peer, char *id, char *ip, unsigned short port){
    
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

//print out a peer's information
//more of a debug function than anything else
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

//checks the alive status of all the peers we have in our collection.
//Kills of any peers that we think might be dead or we have lost
//connection with
int check_all(bt_args_t *args){
  int i;
  for (i=0;i<MAX_CONNECTIONS;i++){
    if(args->peers[i]){ //is this a valid peer
      check_peer(args->peers[i]);
    }
  }
  return 0;
}

//check whether a peer is alive or not, make use of the tv field within each
//peer that is updated whenever we hear something new from peer or whenever
//we hear a keep alive message from them
int check_peer(peer_t *peer) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0){
    perror("gettimeofday");
    return TRUE; //since this is our problem, give benefit of the doubt
  }

  if ((tv.tv_sec - peer->tv.tv_sec) > LIFEPERIOD){ //more than 3 min since last
    return FALSE; //uh-oh dead
  }
  
  return TRUE; //haha, still alive suckers
}

//send a message to peer
int send_to_peer(peer_t *peer, bt_msg_t *msg) {
  int bytes;
  int len = HDRLEN + msg->length; //length of the message
  char sendbuf[len];
  memcpy(sendbuf, msg, len);
  bytes = write(peer->sockfd, sendbuf, len);
  //if (bytes < 0)
  //  perror("send_to_peer: write()");
  return bytes;
}

//send blocks, return -1 on error essentially instructing application
//to resend the entire block sequence
int send_blocks(peer_t *peer, bt_request_t request, bt_args_t *args){
  bt_msg_t response; //response message
  bt_piece_t piece;
  int length;;
  
  //build the piece
  piece.index = request.index;
  piece.begin = request.begin;
  load_piece(args, &piece, request.length);
  length = sizeof(bt_piece_t);

  //build the message
  response.length = length;
  response.bt_type = BT_PIECE;
  response.payload.piece = piece;
  send_to_peer(peer, &response);
  if (args->verbose)
    printf("sent out %d bytes on the line\n", response.length);
  return 0; //on success
}

//read from peers, generic function for handling all cases when we
//might need to read from a peer. Has functionality to handle all
//message types from here.
int read_from_peer(peer_t *peer, bt_msg_t *msg, bt_args_t *args) {
  //bt_msg_t response; //reusable response message
  char *logfile = args->log_file; 
  char log_msg[80]; //log message buffer
  char *ip = inet_ntoa(peer->sockaddr.sin_addr); 
  char readbuf[MAXMSG];
  int bytes;
  
  if ((bytes = read(peer->sockfd, readbuf, HDRLEN)) < 0){ //get message header
    perror("read");
    return ERR;
  }

  msg = (bt_msg_t *)readbuf;
  if (msg->length > 0){//there is more data to read on the line
    if (read(peer->sockfd, (readbuf + bytes), msg->length)<0){ //done reading
      perror("read");
      return ERR;
    }
  }

  msg = (bt_msg_t *)readbuf;
  int piece_index, block, have;
  bt_piece_t piece; //this little piece of ....

  //for every message we receive, update the timestamp in the peer
  if (gettimeofday(&(peer->tv), NULL) < 0)
    perror("gettimeofday");


  if(msg->length == 0){
    if (args->verbose)
      printf("received keep alive message\n");
    sprintf(log_msg, "MESSAGE RECEIVED :{KEEP_ALIVE} from %s\n", ip);
    LOGGER(logfile, 1, log_msg);
  }
  
  else{
    switch(msg->bt_type){
      case BT_BITFIELD:
        if (args->verbose)
          printf("received bitfield message\n");
        sprintf(log_msg, "MESSAGE RECEIVED :{BITFIELD} from %s\n", ip);
        LOGGER(logfile, 1, log_msg);
        peer->bitfield = msg->payload.bitfield; //backup the bitfield
        break;
      
      case BT_REQUEST:
        piece_index = msg->payload.request.index;
        block = msg->payload.request.begin;
        msg->payload.request.length = MAXBLOCK; //hack
        if (args->verbose)
          printf("received a request for a piece %d length %d\n",
                 piece_index, msg->payload.request.length);
        sprintf(log_msg, "MESSAGE RECEIVED :{REQUEST} for piece:%d.(%d) from %s\n",
                piece_index, block, ip);
        LOGGER(logfile, 1, log_msg);
        
        if (own_piece(args, piece_index) && peer->interested && !(peer->choked)){
          sprintf(log_msg, "MESSAGE RESPONSE :{PIECE} for piece:%d(%d) to %s\n",
                  piece_index, block, ip);
          LOGGER(logfile, 1, log_msg);
          if (args->verbose)
            printf("sending out response for piece %d, begin %d\n", piece_index, block);
          send_blocks(peer, msg->payload.request, args);
        }
        break;
      
      case BT_PIECE:
        piece_index = msg->payload.piece.index;
        block = msg->payload.piece.begin;
        if (args->verbose)
          printf("received a piece of length %d\n", msg->length);
        sprintf(log_msg, "MESSAGE RECEIVED :{PIECE} received file piece:%d.(%d) from %s\n",
                piece_index, block, ip);
        LOGGER(logfile, 1, log_msg);
        
        //if we already have the piece, just ignore the message
        if (!(own_piece(args, piece_index))){
           sprintf(log_msg, "MESSAGE RECEIVED :{PIECE} received file piece:%d from %s\n",
                piece_index, ip);
           LOGGER(logfile, 1, log_msg);
           piece = msg->payload.piece;
           save_piece(args, &piece, msg->length - 8);
        }
        break;

      case BT_INTERESTED:
        if (args->verbose)
          printf("received interested message from %s\n", ip );
        sprintf(log_msg, "MESSAGE RECEIVED :{INTERESTED} from %s\n",ip);
        LOGGER(logfile, 1, log_msg);
        peer->interested = 1;
        break;

      case BT_NOT_INTERESTED:
        if (args->verbose)
          printf("received not interested message from %s\n", ip );
        sprintf(log_msg, "MESSAGE RECEIVED :{NOT_INTERESTED} from %s\n",ip);
        LOGGER(logfile, 1, log_msg);
        peer->interested = 0;
        break;
      
      case BT_CHOKE:
        if (args->verbose)
          printf("received choke message from %s\n", ip );
        sprintf(log_msg, "MESSAGE RECEIVED :{CHOKE} from %s\n",ip);
        LOGGER(logfile, 1, log_msg);
        peer->choked = 1;
        break;
      
      case BT_UNCHOKE:
        if (args->verbose)
          printf("received unchoke message from %s\n", ip );
        sprintf(log_msg, "MESSAGE RECEIVED :{UNCHOKE} from %s\n",ip);
        LOGGER(logfile, 1, log_msg);
        peer->choked = 0;
        break;

      case BT_CANCEL:
        if (args->verbose)
          printf("received cancel message from %s\n", ip );
        sprintf(log_msg, "MESSAGE RECEIVED :{CANCEL} from %s\n",ip);
        LOGGER(logfile, 1, log_msg);
        break;

      case BT_HAVE:
        have = msg->payload.have;  
        if (args->verbose)
          printf("received have message from %s for piece %d\n", ip, have );
        sprintf(log_msg, "MESSAGE RECEIVED :{HAVE} for piece %d from %s\n",have,ip);
        LOGGER(logfile, 1, log_msg);
        update_bitfield(peer, have); //update the peers bitfield
        break;

      default:
        //printf("unknown message type %d\n", msg->bt_type);
        //herp derp
        break;
    }
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
  int bytes = args->bitfield.size; //number of character bytes
  char *bitfield = args->bitfield.bitfield;
  int numpieces = args->bt_info->num_pieces;
  for(i=0;i< bytes;i++){
    for(j=0;j<8;j++){
      index = j + (i*8);
      if ((index >= numpieces)||(index >= (args->bitfield.size*8))) //stopping condition
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
    info->num_pieces = max(1,num_pieces);
    
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
int save_piece(bt_args_t *args, bt_piece_t *piece, int size){
  int base; //base offset
  int offset, length;
  int bytes = 0;
  FILE *fp = args->fp;

  //get location within file
  length = args->bt_info->piece_length;
  base = length * piece->index;
  offset = base + piece->begin;

  if (fseek(fp, offset, SEEK_SET) != 0){
    //perror("fseek");
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }
  bytes = fwrite(piece->piece, 1, size, fp);
  //bytes = fwrite(piece->piece, 1, strlen(piece->piece), fp);
  rewind(fp); //courtesy rewind
  //printf("%s\n", piece->piece);
  fflush(fp);
  return bytes;
}

//load the piece in the bt_piece_t struct
int load_piece(bt_args_t *args, bt_piece_t *piece, int length){
  int base; //base offset
  int offset, bytes, piece_length;
  FILE *fp = args->fin;
  char buff[length];
  
  //get location within file
  piece_length = args->bt_info->piece_length;
  base = piece_length * piece->index;
  offset = base + piece->begin;
 
  if (fseek(fp, offset, SEEK_SET) != 0){
    fprintf(stderr, "failed to offset to correct position\n");
    return ERR;
  }

  bytes = fread(buff, 1, length, fp);
  memcpy(piece->piece, buff, length);
  rewind(fp); //courtesy rewind
  return bytes;
}

void print_bits(char byte){
  int i, val;
  for(i=0;i<8;i++){
    val = byte & (1 << (7-i)) ? 1: 0;
    printf("%d", val);
  }
  printf("\n");
}

//comment on whether current piece has been downloaded
//successfully. Check the download file and read in the 
//piece data from there
int piece_bytes_left(bt_args_t *args, int index, int bytes){
  int flen = args->bt_info->length;
  int piece_len = args->bt_info->piece_length;

  if (flen < piece_len) //the special case
    return (flen - bytes);
  return (piece_len - bytes); 
}

//comment on whether current piece has been downloaded
//successfully. Check the download file and read in the 
//piece data from there
int piece_download_complete_foo(bt_args_t *args, int index, int bytes){
  int len;
  int length = args->bt_info->piece_length;
  unsigned char result_hash[20];
  char piece[length];
  FILE *fp = args->fp;

  if (fseek(fp, index*length, SEEK_SET) < 0){
    perror("fseek");
    return FALSE;
  }

  len = fread(piece, 1, length, fp);
  rewind(fp); //always rewind after use
  SHA1((unsigned char *) piece, len, result_hash);
  if (strncmp((const char *)result_hash,
        args->bt_info->piece_hashes[index], HASH_UNIT) == 0)
    return TRUE;
  return FALSE;
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
  int len; //length of the piece read
  int i,j, bytes, count;
  int index = 0; //index in the char array
  int length = args->bt_info->piece_length;
  char piece[length];
  char *bits;
  unsigned char result_hash[20];

  count = args->bt_info->num_pieces; //get number of pieces
  bytes = ceiling(count, 8); //get the number of char needed to hold bits 
  bits = (char *)malloc(bytes);
  
  //opening file for reading
  fp = args->fin;
  //check hashed pieces of file against piece hashes of torrent file
  for (i=0; i<bytes; i++){
    bits[i] = 0; //zero out the char
    for (j=0;j<8;j++){
      index = j + (i*8);
      if (index >= count) //stopping condition
        continue; 
      fseek(fp, index*length, SEEK_SET);
      len = fread(piece, 1, length, fp);
      rewind(fp); //always rewind after use
      memset(result_hash, 0, 20); //zero out the result_hash
      SHA1((unsigned char *) piece, len, result_hash);
      if (strncmp((const char *)result_hash,
            args->bt_info->piece_hashes[index], HASH_UNIT) == 0){
        bits[i] = bits[i] | (1<<(7-j)); //set the correct bit
       // printf("[%d] available\n", index);
      }
    }
    
    if (args->verbose)
      print_bits(bits[i]);
  }
  
  
  //set the appropriate variables
  memcpy(bitfield->bitfield, bits, bytes);
  bitfield->size = bytes; //number of character bytes
  return 0;
}

int update_bitfield(peer_t *peer, int piece){
  int base, offset;
  base = piece/8;
  offset = piece - (base * 8);
  peer->bitfield.bitfield[base] = peer->bitfield.bitfield[base] | (1 << (7-offset));
  return base;
}

int sha1_piece(char *piece, int length,  unsigned char *hash) {
  SHA1((unsigned char *)piece, length, hash);
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

//print out the statistics when we are downloading anything
//implement a small progress bar in there with the percentage
//of the file downloaded
void print_stats(bt_args_t *args, int blocks){
  int peers = 0;
  float percentage; //percentage of the file downloaded
  char *fname = args->save_file; //download file name
  int downloaded = (pieces_count(args) * args->bt_info->piece_length) + blocks;
  int uploaded = 0;
  int i, bars;

  percentage = (100.0 * downloaded)/args->bt_info->length;
  bars = percentage/10;
  char *progress = malloc(10);
  memset(progress, '|', bars);
  progress[bars] = '\0'; //null terminate the string

  for (i=0; i < MAX_CONNECTIONS; i++){
    if (args->peers[i])
      peers++;
  }

  printf("File: %s Progress [%-10s] %.1f %% Peers %d Downloaded %d KB Uploaded %d KB\n",
        fname, progress, percentage, peers, downloaded, uploaded);
  printf("\r"); //carriage return
  fflush(stdout);
  free(progress);
}

//return a count of all the pieces we currently have
//utilize our bitfield to check what we have and what 
//we dont have
int pieces_count(bt_args_t *args){
  int i,j;
  int count = 0;
  int bytes = args->bitfield.size;
  char *bits = args->bitfield.bitfield; 

  for (i=0; i< bytes;i++){
    for (j=0;j<8;j++){
      if (bits[i] & (1 << j))
        count++;
    }
  }

 return count;
}

//return the index of the peer from the args list
//return -1 if the peer doesn't exist
int peer_index(peer_t *peer, bt_args_t *args){
  int i;
  int index = 1;
  for (i=0;i<MAX_CONNECTIONS;i++){
    if (args->peers[i]){
      if(strncmp((const char *)args->peers[i]->id, (const char *)peer->id, ID_SIZE) == 0){
        index = i;
        break;
      }
    }
  }

  return index; //return -1 if entry not found
}

//drop peers from the bt_args structure
//reset the pollfd in that structure
int drop_peer(peer_t *peer, bt_args_t *args){
  int index = peer_index(peer, args);
  if (index >= 0){
    free(args->peers[index]); //free the memory
    args->peers[index] = NULL; //remove the entry
    args->poll_sockets[index].events = 0; //no activity on this socket
  }
  return index;
}

//add a new peer to the swarm
//calculate peer id and all here
int add_peer(peer_t *peer, bt_args_t *args, char *ip, unsigned short port){
  int i;
  char id[ID_SIZE];
  calc_id(ip, INIT_PORT, id);
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

//print out the intro
void intro(){
  printf("===================================================================\n");
  printf("                      myTORRENT Version 1.0\n");
  printf("                   (c) 2012 Swarthmore College\n");
  printf("                         CS43 Final Lab\n");
  printf("===================================================================\n");
  return;
}

//finalize the application, reclaim all dynamic memory 
//and stuff. Close all sockets and file descriptors
void __CLEANUP__(bt_args_t *args){
  int i;
  for (i=0;i<MAX_CONNECTIONS;i++){
    if (args->peers[i]){
      close(args->peers[i]->sockfd); //close the socket
      free(args->peers[i]); //reclaim the dynamic memory
    }
  }
 
  for (i=0;i<args->bt_info->num_pieces;i++){
    free(args->bt_info->piece_hashes[i]);
  }
  free(args->bt_info->piece_hashes); //free the array
  
  //close the file descriptors
  fclose(args->fp);
  fclose(args->fin);
}

/* local copy function. Used only for a restart when we want to
 * copy the previously saved as file to the current file we will use
 */
int __fcopy__(char *source, char *dest){
    FILE *d, *s;
    char *buffer;
    size_t incount;
    long totcount = 0L;

    s = fopen(source, "rb");
    if(s == NULL)
            return -1L;

    d = fopen(dest, "wb");
    if(d == NULL)
    {
            fclose(s);
            return -1L;
    }

    buffer = malloc(MAXSIZE);
    if(buffer == NULL)
    {
            fclose(s);
            fclose(d);
            return -1L;
    }

    incount = fread(buffer, sizeof(char), MAXSIZE, s);

    while(!feof(s))
    {
            totcount += (long)incount;
            fwrite(buffer, sizeof(char), incount, d);
            incount = fread(buffer, sizeof(char), MAXSIZE, s);
    }

    totcount += (long)incount;
    fwrite(buffer, sizeof(char), incount, d);

    free(buffer);
    fclose(s);
    fclose(d);

    return totcount;
}


