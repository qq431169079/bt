#ifndef _BT_CONNECT_H
#define _BT_CONNECT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "bt_setup.h"
#include "bt_lib.h"
#include "bencode.h"

void extract_attributes(be_node *node, bt_info_t *info);
//int receive_handshake(int sockfd, bt_args_t *args);
//int send_handshake(int sockfd, bt_args_t *args);
int leecher_handshake(int sockfd, char* fname, char *id, struct sockaddr_in* sockaddr);
int seeder_handshake(int sockfd, char* fname, char *id,struct sockaddr_in sockaddr);
void handshake_all(bt_args_t *args);

#endif
