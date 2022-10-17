/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>

#define MSS 4088
#define packet_buf_size 512


struct sockaddr_in si_me, si_other;
socklen_t addr_len;
int s, slen;

typedef struct TCP_packet_t
{
  int seq_num;
  unsigned int data_size;
  char data[MSS]; 

  void init(){
    this->seq_num = -1;
    this->data_size = 0;
    // memset(this->data, 0, MSS);
  }
  
  // void copy(TCP_packet_t src_pck){
  //   this->seq_num = src_pck.seq_num;
  //   memcpy(this->data, src_pck.data, src_pck.data_size);
  //   this->data_size = src_pck.data_size;
  // }
}TCP_packet_t;

void diep(const char *s) {
    perror(s);
    exit(1);
}

void writeFile(unsigned int data_size, char data[], FILE* file_ptr){
  size_t wr_bytes = 0;
  wr_bytes = fwrite(data, 1, data_size, file_ptr);
  if (wr_bytes != data_size){
    diep("fwrite");
  }
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {

    int rcv_numbytes; /* check if valid packet received*/
    int send_numbytes; /* check if valid ack send*/
    // char recv_buf[MSS]; /* buffer for packet data*/
    struct sockaddr_storage their_addr;
	  // addr_len = sizeof (si_other);
    addr_len = sizeof (their_addr);

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
      diep("socket");
    }

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1){
      printf("failed to bind");
      diep("bind");
    }

    TCP_packet_t place_holder;
    place_holder.init();
    // place_holder.data_size = 0;
    // place_holder.seq_num = -1;
    // memset(place_holder.data, 0, MSS);

    /* Packet buffer for storing unconsecutive packets */
    std::vector <TCP_packet_t> packet_buf;
    /* Initialize */
    for (int i = 0; i < packet_buf_size; i++)
    {
       packet_buf.emplace_back(place_holder);
       // packet_buf[packet_buf_size] = place_holder;
    }
    std::vector<TCP_packet_t>::iterator next_start;
    next_start = packet_buf.begin();
    printf("Finish initializing packet buffer.");

    FILE* file_ptr = fopen(destinationFile, "wb");
    if (file_ptr == NULL) { diep("fopen");}
    int cu_ack = -1;  /* cumulative ack. to keep track of unreceived consecutive packets*/

    while (1)
    {
      TCP_packet_t recv_packet;
      if ((rcv_numbytes = recvfrom(s, &recv_packet, sizeof(recv_packet) , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        diep("recvfrom");
	    }
      printf("Receive packet: %d, Cumulative Ack: %d", recv_packet.seq_num, cu_ack);
      // if(recv_packet.seq_num < 0){
      //   printf("error receive");
      // }
      /* check if receive end signal*/
      if(recv_packet.seq_num == -1){
        int end_ack = -1;
        if ((send_numbytes = sendto(s, &end_ack, sizeof(int), 0, (struct sockaddr *)&their_addr, addr_len)) == -1){
          diep("endackSend");
        }
        break;
      }

      /* if duplicate packet received */
      else if(recv_packet.seq_num <= cu_ack){
        /* send cumulative ack back to indicate the duplicate packet has once received */
        if ((send_numbytes = sendto(s, &cu_ack, sizeof(int), 0, (struct sockaddr *)&their_addr, addr_len)) == -1){
          diep("endackSend");
       }
      }

      /* if consecutive packet received */
      else if(recv_packet.seq_num == cu_ack + 1){
        // memcpy(recv_buf, recv_packet.data, recv_packet.data_size);
        // writeFile(recv_packet.data_size, recv_buf, file_ptr);
        // writeFile(recv_packet.data_size, recv_packet.data, file_ptr);
        // // recv_packet.init();
        // cu_ack++;
        // (*next_start) = place_holder;
        // next_start++;
        *next_start = recv_packet;

        /* Find if consecutive packets stored in vector, write all in file*/
        while ((*next_start).seq_num != -1)
        {
          /* write the packet's data to file */
          writeFile((*next_start).data_size, (*next_start).data, file_ptr);

          /* update cumulative ack and clear the position in the vector */
          cu_ack = (*next_start).seq_num;
          // (*next_start).init();
          (*next_start) = place_holder;

          next_start++;
          /* turn to the next position, the vector can be seen as circulated, i.e. end is connected to the start */
          if(next_start == packet_buf.end())
          {
            next_start = packet_buf.begin();
          }
        }

        /* after finish writing, send ack*/
        if ((send_numbytes = sendto(s, &cu_ack, sizeof(int), 0, (struct sockaddr *)&their_addr, addr_len)) == -1){
          diep("cuAckSend");
        }
      }

      /* last case: nonconsecutive packet arrive, store in relevant position if have space*/
      else{
        /* find the position to store the packet */
        int base_offset = recv_packet.seq_num - cu_ack - 1; // -1 for we should get the difference between the lost/delayed packet and this received one
        std::vector<TCP_packet_t>::iterator temp_pck_pos = next_start;
        int checkfull = 0;

        /* find position */
        while (base_offset-- != 0)
        {
          if(++temp_pck_pos == packet_buf.end()){
            temp_pck_pos = packet_buf.begin();
          }

          if (temp_pck_pos == next_start)
          {
            checkfull = 1;
          }
        }
        
        /* no space for storing extra packet, just drop it, do nothing*/
        if(checkfull){  continue; }
        /* copy packet content to the position ? */
        if((*temp_pck_pos).seq_num == -1){
          /* check if duplicate */
          // (*temp_pck_pos).seq_num = recv_packet.seq_num;
          // memcpy((*temp_pck_pos).data, recv_packet.data, recv_packet.data_size);
          // (*temp_pck_pos).data_size = recv_packet.data_size;
          (*temp_pck_pos) = recv_packet;
        }
        // recv_packet.init();

        /* send the latest cumulative ack */
        if ((send_numbytes = sendto(s, &cu_ack, sizeof(int), 0, (struct sockaddr *)&their_addr, addr_len)) == -1){
          diep("cuAckSend");
        }
      }
    }


	/* Now receive data and send acknowledgements */    

    close(s);
    fclose(file_ptr);
	  printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

