#include <string.h>
#define MSGSIZE 68



  //function for building message
int client_handshake(int sockfd, char* fname, struct sockaddr_in* sockaddr) {
  unsigned char nineteen = "19";
  unsigned char name[19] = "BitTorrent Protocol";
  
  // setting extensions to 0
  unsigned char ext[8];
  memset(ext, 0, 8);

  //SHA1 hash of filename for info-hash
  unsigned char info_hash[20];
  SHA1((unsigned char *) fname, strlen(fname), info_hash);

  //setting your peer id
  char *ip; 
  unsigned short port; 
  char id[20];

  calc_id(ip, port, id);

  return; //will be 1 or 0
}

int server_handshake(int sockfd, char* fname, peer_t* peer) {

}

//function for checking correctness


int main(){

  return 0;
}
