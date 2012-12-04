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


