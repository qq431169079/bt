#ifndef _BT_LIB_H
#define _BT_LIB_H

//standard stuff
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <poll.h>

//networking stuff
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "bt_lib.h"
#include "bencode.h"

/*Maximum file name size, to make things easy*/
#define FILE_NAME_MAX 1024

/*Maxium number of connections*/
#define MAX_CONNECTIONS 5

/*initial port to try and open a listen socket on*/
#define INIT_PORT 6667 

/*max port to try and open a listen socket on*/
#define MAX_PORT 6699

/*Different BitTorrent Message Types*/
#define BT_CHOKE 0
#define BT_UNCHOKE 1
#define BT_INTERESTED 2
#define BT_NOT_INTERESTED 3
#define BT_HAVE 4
#define BT_BITFIELD 5
#define BT_REQUEST 6
#define BT_PIECE 7
#define BT_CANCEL 8

/*size (in bytes) of id field for peers*/
#define ID_SIZE 20

//other rand #defines
#define ERR -1 //return when error encountered
#define TRUE 1
#define FALSE 0
#define NAME_MAX 1024
#define HASH_UNIT 20
#define CHUNK_SIZE 1 //1 byte bt_piece response size
#define HDRLEN 6 //size of message header length
#define MAXBLOCK 1024 //max block size. Standard length
#define MAXMSG 1500 //size of largest message

typedef struct {
  size_t size;  //number of bytes for the bitfield
  char *bitfield; //bitfield where each bit represents a piece that
                   //the peer has or doesn't have
} bt_bitfield_t;

//holds information about a peer
typedef struct peer{
  unsigned char id[ID_SIZE]; //the peer id
  unsigned short port; //the port to connect n
  struct sockaddr_in sockaddr; //sockaddr for peer
  int sockfd; //file descriptor for communication port
  int choked; //peer choked?
  int interested; //peer interested?
  bt_bitfield_t bitfield;
}peer_t;


//holds information about a torrent file
typedef struct {
  char announce[FILE_NAME_MAX]; //url of tracker
  char name[FILE_NAME_MAX]; //name of file
  int piece_length; //number of bytes in each piece
  int length; //length of the file in bytes
  int num_pieces; //number of pieces, computed based on above two values
  char ** piece_hashes; //pointer to 20 byte data buffers containing the sha1sum of each of the pieces
} bt_info_t;


//holds all the agurments and state for a running the bt client
typedef struct {
  int verbose; //verbose level
  char save_file[FILE_NAME_MAX];//the filename to save to
  FILE *fp; //fout the fp to the file to save to
  FILE *fin; //the fin to the file to read from
  char log_file[FILE_NAME_MAX];//the log file
  char torrent_file[FILE_NAME_MAX];// *.torrent file
  peer_t * peers[MAX_CONNECTIONS]; // array of peer_t pointers
  char id[ID_SIZE]; //this bt_clients id
  int sockets[MAX_CONNECTIONS]; //Array of possible sockets
  struct pollfd poll_sockets[MAX_CONNECTIONS]; //Arry of pollfd for polling for input
  char *ip;
  int port;
  int leecher; //flag for whether I am a leecher or seeder
  int downloading; //current piece being downloaded 
  /*set once torrent is parsed*/
  bt_info_t *bt_info; //the parsed info for this torrent
  bt_bitfield_t bitfield;  

} bt_args_t;


/**
 * Message structures
 **/


typedef struct{
  int index; //which piece index
  int begin; //offset within piece
  int length; //amount wanted, within a power of two
} bt_request_t;

typedef struct{
  int index; //which piece index
  int begin; //offset within piece
  char piece[MAXBLOCK]; //pointer to start of the data for a piece
} bt_piece_t;



typedef struct bt_msg{
  int length; //length of remaining message, 
              //0 length message is a keep-alive message
  unsigned short bt_type;//type of bt_mesage

  //payload can be any of these
  union { 
    bt_bitfield_t bitfield;//send a bitfield
    int have; //what piece you have
    bt_request_t request; //request messge
    bt_request_t cancel; //cancel message, same type as request
    bt_piece_t piece; //a piece message
    char data[0];//pointer to start of payload, just incase
  }payload;

} bt_msg_t;


int parse_bt_info(bt_info_t *info, be_node *node);
int select_download_piece(bt_args_t *args); //get the piece to start downloading
int select_upload_piece(bt_args_t *args); //get the piece to start uploading
int ceiling(int dividend, int divisor); //mimic ceil()
int set_bitfield(bt_args_t *args, int index); //set bitfield
int send_all(bt_args_t *args, bt_msg_t *msg);//send message to all your peers
int own_piece(bt_args_t *args, int piece); //do we have own this piece
int send_blocks(peer_t *peer, bt_request_t request, bt_args_t *args); //send back the requested 
int init_socket(bt_args_t *args); //initialize the listening socket
int poll_peers(bt_args_t *bt_args); //poll peers for info
void fill_listen_buff(struct sockaddr_in *destaddr, int port);
int piece_download_complete(bt_args_t *args, int index);
void print_bits(char byte); //print bits in a byte
/*choose a random id for this node*/
unsigned int select_id();

/*propogate a peer_t struct and add it to the bt_args structure*/
int add_peer(peer_t *peer, bt_args_t *bt_args, char *ip, unsigned short port);

/*drop an unresponsive or failed peer from the bt_args*/
int drop_peer(peer_t *peer, bt_args_t *bt_args);

/* initialize connection with peers */
int init_peer(peer_t *peer, char * id, char * ip, unsigned short port);


int ceiling(int dividend, int divisor); //helper for getting the ceiling of a divide
/*calc the peer id based on the string representation of the ip and
  port*/
void calc_id(char * ip, unsigned short port, char * id);

/* print info about this peer */
void print_peer(peer_t *peer);

/* check status on peers, maybe they went offline? */
int check_peer(peer_t *peer);

//send the msg to a log-file
void LOGGER(char *log, int type, char *msg);
void INIT_LOGGER(char *log);
/*check if peers want to send me something*/

/*send a msg to a peer*/
int send_to_peer(peer_t *peer, bt_msg_t *msg);

/*read a msg from a peer and store it in msg*/
int read_from_peer(peer_t *peer, bt_msg_t *msg, bt_args_t *args);


/* save a piece of the file */
int save_piece(bt_args_t *bt_args, bt_piece_t *piece, int size);

/*load a piece of the file into piece */
int load_piece(bt_args_t *bt_args, bt_piece_t *piece, int length);

/*load the bitfield into bitfield*/
int get_bitfield(bt_args_t * bt_args, bt_bitfield_t *bitfield);

/*compute the sha1sum for a piece, store result in hash*/
int sha1_piece(char *piece, int length, unsigned char *hash);


/*Contact the tracker and update bt_args with info learned, 
  such as peer list*/
int contact_tracker(bt_args_t * bt_args);



#endif
