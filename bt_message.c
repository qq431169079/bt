#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#include "bt_message.h"
#include "bt_lib.h"
#include <openssl/sha.h>
#include "bencode.h"

//build the have message
void have_message(bt_msg_t *msg, int piece){
  msg->payload.have = piece;
  msg->length = sizeof(int); //int for the piece we have
  msg->bt_type = BT_HAVE;
  return;
}

//build the interested message
void interested_message(bt_msg_t *msg){
  msg->length = 1;
  msg->bt_type = BT_INTERESTED;
}

//build the uninterested message
void uninterested_message(bt_msg_t *msg){
  msg->length = 1;
  msg->bt_type = BT_NOT_INTERESTED;
}

//build the choke message
void choke_message(bt_msg_t *msg){
  msg->length = 1;
  msg->bt_type = BT_CHOKE;
}

//build the unchoke message
void unchoke_message(bt_msg_t *msg){
  msg->length = 1;
  msg->bt_type = BT_UNCHOKE;
}


//check if we are interested in this peer
//check if they have some piece we don't have
int interested(bt_args_t *args, peer_t *peer){
  int i,j, index;
  int bytes = args->bitfield.size; //number of character bytes
  char *bitfield = args->bitfield.bitfield;
  char *peerbits = peer->bitfield.bitfield;
  int num_pieces = args->bt_info->num_pieces;
  for(i=0;i< bytes;i++){
    for(j=0;j<8;j++){
      index = j + (i*8);
      if ((index >= num_pieces)||(index >= (args->bitfield.size*8))) //stopping condition
        continue;

      //search for at least one piece the peers has and we dont
      if (((bitfield[i] & (1 << (7-j))) == 0) &&
          (peerbits[i] & (1 << (7-j))))
        return TRUE;
    }
  }
  return FALSE;
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

//check the status of our relationship with a peer
//send the appropriate interested or non-interested signal
void send_interested(bt_args_t *args){
  int i;
  int state = 0;
  bt_msg_t *msg; //the message 
  for (i=0; i<MAX_CONNECTIONS;i++){
    if(args->peers[i]){ //valid peer
      msg = (bt_msg_t *)malloc(sizeof(bt_msg_t));
      state = interested(args, args->peers[i]);
      printf("state %d\n", state);
      if (state)
        interested_message(msg);
      else
        uninterested_message(msg);
      send_to_peer(args->peers[i], msg); //send out the message
      free(msg); //free the memory
    }
  }
  
}
