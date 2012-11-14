#ifndef _BT_CONNECT_H
#define _BT_CONNECT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bt_setup.h"
#include "bt_lib.h"
#include "bencode.h"

void fill_listen_buff(struct sockaddr_in *destaddr, int port);
void extract_attributes(be_node *node, bt_info_t *info);
int receive_handshake(struct sockaddr_in sockaddr, int port);
int send_handshake(struct sockaddr_in sockaddr, int port);

#endif
