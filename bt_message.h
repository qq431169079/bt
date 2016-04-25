#ifndef _BT_MESSAGE_H
#define _BT_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bt_lib.h"

void have_message(bt_msg_t *msg, int piece); //build the have message;
void send_interested(bt_args_t *args); //send interested or not interested message
int interested(bt_args_t *args, peer_t *peer); //are we interested in peer
int send_all(bt_args_t *args, bt_msg_t *msg);//send message to all your peers
void choke_message(bt_msg_t *msg);
void unchoke_message(bt_msg_t *msg);
void interested_message(bt_msg_t *msg);
void uninterested_message(bt_msg_t *msg);
void cancel_message(bt_msg_t *msg, bt_request_t *request, int piece);
#endif
