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

void have_message(bt_msg_t *msg, int piece){
  msg->payload.have = piece;
  msg->length = sizeof(int); //int for the piece we have
  msg->bt_type = BT_HAVE;
  return;
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

      //search for a zero bit and use that as index
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
