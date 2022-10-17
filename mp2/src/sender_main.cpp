/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <utility>
#include <queue>
#include <cmath>
#include <string>
#include <netdb.h>
using namespace std;

// #include "sender.h"

#define MSS 4088
// class State;
char fpReadBuf[MSS];
//enum action {waitACK, resendPkt, sendNewPkt}
struct sockaddr_in si_other;
struct addrinfo hints, *servinfo, *p;


struct sockaddr_storage their_addr;
// socklen_t addrlen = sizeof(si_other);
socklen_t addr_len = sizeof(their_addr);

enum TO_DO {WAIT_ACK, SEND_PCK, RESEND_DUP, RESEND_TO};
TO_DO to_do = SEND_PCK;

char pktBuffer[MSS+8];
// int recvack_buf;
pair<int, int> lastAckPair(-1, 0);  // first element stores the last acknowledged seq_num, second stores the times we see the seq_num
char recvBuffer[4096];

FILE* file_ptr;
// State* cur_state;


void diep(char *s) {
    perror(s);
    exit(1);
}

class TCP_packet
{
public: 
    int seq_num;
    unsigned int data_size;
    char data[MSS];
    time_t sendTime;

    TCP_packet(){} 
    TCP_packet(int seq_num_, unsigned int data_size_, char* data_buf){
        this->seq_num = seq_num_;
        this->data_size = data_size_;
        memcpy(this->data, data_buf, this->data_size);
    }

    void create_pkt_buf(char *buf){
        if(this->seq_num == -1){
            memcpy(buf, &seq_num, 4);
            return;
        }
        memcpy(buf, &seq_num, 4);
        memcpy(buf+4, &data_size, 4);
        memcpy(buf+8, data, sizeof(data));
    }
};

// State::State(){};
// State::State(Sender* sender){
//     this->tcp_sender = sender;
//     // this->state_type = state_type;
// }
// State::~State(){};

class Sender{
public:
    int sockfd;
    double CW; // initialize to 1
    int SST; // initialize to 64
    int dup_ack; // initialize to 0
    int recv_ack; /* initialize to -1 */
    // delete this, move to global int to_do; /* 0 for sending new packets, 1 for (duplicate) resend old packets & check new, and 2 for timeout resend, 3 for nothing*/
    int is_end;
    int file_end;
    unsigned long long int bytes_remain;
    struct timeval checkTimeOut;
    int state_type;

    Sender(int sockfd, unsigned long long int bytesToTransfer){
        // constructor
        this->CW = 1;
        this->SST = 64;
        this->dup_ack = 0; 
        this->recv_ack = -1;
        this->is_end = 0;
        this->file_end = 0;
        this->bytes_remain = bytesToTransfer;
        this->sockfd = sockfd;
        this->state_type = 0; /* 0 for slow start */
        checkTimeOut.tv_sec = 0;
        checkTimeOut.tv_usec = 25000;
        memset(recvBuffer, 0, sizeof(recvBuffer));
    }
    
    void WaitAck(){
        printf("TO_DO: Wait \n");

        if((this->waitAckQueue.size() == 0) && (this->file_end == 1)){
            this->is_end = 1;
            return;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &checkTimeOut, sizeof(checkTimeOut)) < 0) {
            perror("set timeout fail");
        }
        // int recvack_buf;
        // int numBytes = recvfrom(sockfd, &recvack_buf, sizeof(int), 0, NULL, NULL);
        int numBytes = recvfrom(sockfd, &recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr *)&their_addr, &addr_len);
        int recvack_buf = parse_recv(recvBuffer, numBytes); ;
        /* time out or invalid recv*/
        // int check_to;
        // if ((numBytes < 0) || ((check_to = check_first_to(recvack_buf)) == 1) ) { 
        // if (numBytes < 0){        
        //     timeOut();
        //     lastAckPair.second = 0; //? should we clear dup here
        //     to_do = RESEND_TO;
        //     // return;
        // }
        if(numBytes < 0){
          timeOut();
          lastAckPair.second = 0; //? should we clear dup here
          to_do = RESEND_TO;
            // return;
        }
        else{
          if(numBytes < 4){ recvack_buf = lastAckPair.first;  }
          // else{ recvack_buf = parse_recv(recvBuffer, numBytes); }

          if(recvack_buf > lastAckPair.first){
            /* new_ack receive, update latest receive ack pair, first: ack value, second: times of ack received */

            /* update parameter and state transition */
            newACK(recvack_buf-lastAckPair.first);
            lastAckPair.first = recvack_buf;
            lastAckPair.second = 1;

            /* next to_do is to send packets based on CW*/
            to_do = SEND_PCK;

            /* pop completed packets */
            printf("Pop elements when recvack_buf is %d\n", recvack_buf);
            if(waitAckQueue.size()!= 0){
                while((waitAckQueue.front()).seq_num <= recvack_buf){
                  printf("Pop packet number %d\n", waitAckQueue.front().seq_num);
                  waitAckQueue.pop();
                  if(waitAckQueue.size()== 0){ break; }
                }
            }
            // return;
          }else if(recvack_buf == lastAckPair.first){
            /* duplicate ack receive */
            lastAckPair.second++;
            if(state_type == 2){
                // cur_state is fast recovery
                to_do = SEND_PCK;
            }else if(state_type == 0){
              to_do = WAIT_ACK;
            }else{ 
                // cur_state is not fast recovery
              if(lastAckPair.second >= 3){
                to_do = RESEND_DUP;
              }
            }
            dupACK();
            // return;
          }}
        
        if((this->waitAckQueue.size() == 0) && (this->file_end == 1)){
            this->is_end = 1;
        }
    }

    void SendPackets(){
        printf("TO_DO: Send Packets \n");
        /* Load packets */
        // int get_pkts_num = getNewPktsNum();
        queue<TCP_packet> to_send = read_n_load();
        /* Send packets */
        sendpackets_(to_send);
        // return recv_ack_();
        to_do = WAIT_ACK;
        return;
    }
    
    void Resend_TO(){ /* resend for time out*/
        printf("TO_DO:Resend time out \n");
        /* load and resend base */
        // if(waitAckQueue.size() == 0){
        //     printf("error logic, check ack");
        // }

        send_pkt_(&waitAckQueue.front());
        // return recv_ack_();
        to_do = WAIT_ACK;
        return;
    }

    void Resend_DUP(){
        printf("TO_DO:Resend dup \n");

        /* load and resend base */
        // if(waitAckQueue.size() == 0){
        //     printf("error logic, check ack");
        // }

        send_pkt_(&waitAckQueue.front());
        /* Load packets */
        // int get_pkts_num = this->getNewPktsNum();
        queue<TCP_packet> to_send = this->read_n_load();
        /* Send packets */
        sendpackets_(to_send);
        // return recv_ack_();
        to_do = WAIT_ACK;
        return;
    }
    
    void dupACK(){
        /* increment numbers of duplicate ack*/
        dup_ack++;
        /* check duplicate ack num*/
        if(state_type == 2){  CW += 1;  }
        else if(dup_ack >= 3 && state_type == 1){
          /* half sst*/
          // if (state_type == 0)
          // {
          //   CW = (CW <= (SST/2))? (2*CW):SST;
          // }
          // else{
          SST = round(CW / 2) + 1;
          /* set new cw size */
          CW = SST + 3;
          /* Resend cw_base and new pkt based on cw */
          //tcp_sender->to_do = 1;

          /* swtich to fast recovery */
          state_type = 2;
        }
    }

    void newACK(int times_){
      printf("New ACK at state: %d\n", state_type);
      dup_ack = 0;
      /* increment window size*/
      switch (state_type){
        case 0:
          CW += times_;
          // while (times_-- > 0){
          //   CW *=2;
          // }
          state_type = (CW >= SST)? 1: 0;
          break;
        case 1:
          while(times_-- > 0){
            CW += 1/floor(CW);
          } 
          // CW += times_;
          break;
        case 2:
          CW = SST;
          state_type = 1;
          break;
      }
    }

    void timeOut(){
        /* half sst*/
        // SST = (state_type == 0)? (round(CW / 2) + 1): SST;
        SST = round(CW / 2) + 1;
        /* set new cw size */
        // CW = SST;
        CW = 1;
        /* clear duplicate */
        dup_ack = 0;
        state_type = 0;
        /* resend cw_base */
        //tcp_sender->to_do = 2;
    }

private:
    queue<TCP_packet> waitAckQueue;
    
    int check_first_to(int latest_ack){
        
        // return 0 for no timeout, 1 for timeout
        if(waitAckQueue.size() == 0){   
          return 0;   
        }
        if(waitAckQueue.front().seq_num <= latest_ack){
          return 0;
        }
        time_t sendTime_ = waitAckQueue.front().sendTime;
        time_t nowTime;
        time(&nowTime);
        double diffTime = difftime(nowTime, sendTime_);
        return (diffTime < (checkTimeOut.tv_usec/ 1000.0))? 0:1;
        // return (diffTime < (40.0))? 0:1;
    }

    // int getNewPktsNum(){
    //   // printf("Calculate number of packets shoule be added\n");
    //   // when to_do is 0 (send new packets), get the number of packets to be loaded and sent first
    //   int pktNum = floor(CW) - waitAckQueue.size();
    //   return pktNum;
    // }

    queue<TCP_packet> read_n_load(){
        int pktNum = floor(CW) - waitAckQueue.size(); 
        // printf("Loading packets from file");
        queue<TCP_packet> newPktQueue;

        if(bytes_remain == 0 || pktNum == 0){
            this->file_end = (bytes_remain == 0)? 1:0;
            return newPktQueue;
        }

        /* find the first packet's id*/
        int pktSeqNum;
        if (waitAckQueue.size() != 0){
            pktSeqNum = waitAckQueue.back().seq_num + 1;
        }else{
            pktSeqNum = lastAckPair.first + 1;
        }
        if(pktSeqNum < 0){
            printf("Error seq num cal");
        }

        while(pktNum-- > 0){
            if (bytes_remain != 0){
                // still left some bytes to read and send
                int numBytes = fread(fpReadBuf, 1, MSS, file_ptr);
                if (numBytes == 0){
                    /* file has gone over */

                    // int end_seq_num = -1;
                    // TCP_packet finalPkt(end_seq_num, 0, fpReadBuf); //
                    // newPktQueue.push(finalPkt);
                    this->file_end = 1;
                    break;
                }

                numBytes = (bytes_remain > numBytes) ? numBytes: bytes_remain; 
                bytes_remain -= numBytes;
                if (bytes_remain == 0) {
                    this->file_end = 1;
                }
                TCP_packet newPkt(pktSeqNum++, numBytes, fpReadBuf);
                newPktQueue.push(newPkt);
            }
            else{
                // send final packet
                // fpReadBuf is of no use, don't care whatever it is.
                // int end_seq_num = -1;
                // TCP_packet finalPkt(end_seq_num, 0, fpReadBuf); //
                // newPktQueue.push(finalPkt);
                this->file_end = 1;
                break;
            }
        }
        return newPktQueue;
    }

    void send_pkt_(TCP_packet *pkt){
        pkt->create_pkt_buf(pktBuffer);
        int numBytes;
        if((numBytes = sendto(sockfd, pktBuffer, sizeof(pktBuffer), 0, p->ai_addr, p->ai_addrlen))== -1){
            printf("Error sending packet: %d", pkt->seq_num);
            perror("sending packet error");
        }
        printf("Send packet: %d, Cu_ack at this time: %d \n for %d times, CW: %d\n", pkt->seq_num, lastAckPair.first, lastAckPair.second, int(floor(CW)));
        if(pkt->seq_num < 0){
            printf("error sending");
        }
        time_t nowTime;
        time(&nowTime);
        pkt->sendTime = nowTime;   
        return;
    }

    void sendpackets_(queue<TCP_packet> pktQueue){
        int queueSize = pktQueue.size();
        for (int i=0; i<queueSize; i++){
            //cout << pktQueue.front() << endl;
            send_pkt_((&pktQueue.front()));
            waitAckQueue.push(pktQueue.front());
            pktQueue.pop();
        }
        return;
    }

    int parse_recv(char* buf, int buf_size){
      int max_recv = 0;
      int parse_int;
      for (int i = 0; i + 4 <= buf_size; i += 4)
      {
        memcpy(&parse_int, buf+i, 4);
        max_recv = (parse_int > max_recv)? parse_int: max_recv;
      }
      return max_recv;
    }
};    
    /* return -1 for timeout, 1 for duplicate ack, 0 for new ack*/
    //int recv_ack_(){
//     recv_ack_buf recv_ack(){
//         if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &checkTimeOut, sizeof(checkTimeOut)) < 0) {
//             perror("set timeout fail");
//         }
//         int numBytes = recvfrom(sockfd, recvBuffer, 2048, 0, NULL, NULL);
//         /* time out or invalid recv*/
//         if (numBytes < 0) {
//             return -1;
//         }else if(numBytes >= sizeof(int)){
//             /* update ack */
//             // int temp_ack;
//             // int prev = recv;
//             // for (int i = numackBytes; i >= sizeof(int); i -= sizeof(int))
//             // {
//             //     memcpy(&temp_ack, recvBuffer+(numBytes-i), sizeof(int));
//             //     recv_ack = (temp_ack > recv_ack)? temp_ack:recv_ack;
//             // }
            
//             // if(waitAckQueue.size())!= 0{
//             //     while((waitAckQueue.front()).seq_num <= recv_ack){
//             //         waitAckQueue.pop();
//             //     }
//             // }
//             int temp_ack;
//             recv_ack_buf ack_buf_ = recv_ack_buf();
//             for (int i = numackBytes; i >= sizeof(int); i -= sizeof(int))
//             {
//                 memcpy(&temp_ack, recvBuffer+(numBytes-i), sizeof(int));
//                 if(temp_ack > recv_ack && temp_ack > ack_buf_.new_ack_max){
//                     ack_buf_.new_ack_max = temp_ack;
//                     ack_buf_.new_ack_num ++;
//                 }
//                 else if(){
                    
//                 }
//             }
        
//             if(waitAckQueue.size())!= 0{
//                 while((waitAckQueue.front()).seq_num <= recv_ack){
//                     waitAckQueue.pop();
//                 }
//             }
//         }      
//         return r((prev==recv_ack)? 1: 0);
//     }

// };

// class SlowStart: public State{
// public:
//     /* init */
//     //using State::State;
//     SlowStart(Sender* sender){
//         this->tcp_sender = sender;
//         this->state_type = 0;
//     }

//     /* opeartion */
//     void dupACK(){
//         /* increment numbers of duplicate ack*/
//         tcp_sender->dup_ack++;
        
//         /* check duplicate ack num*/
//         if(tcp_sender->dup_ack != 3){
//             /* continue waiting for ack*/
//             //tcp_sender->to_do = 3;
//         }
//         else{
//             /* half sst*/
//             tcp_sender->SST = round(tcp_sender->CW / 2) + 1;;
//             /* set new cw size */
//             tcp_sender->CW = tcp_sender->SST + 3;
//             /* Resend cw_base and new pkt based on cw */
//             //tcp_sender->to_do = 1;

//             /* swtich to fast recovery */
//             if(!cur_state){
//                 delete cur_state;
//             }
//             FastRecovery* state_FR = new FastRecovery(tcp_sender);
//             cur_state = state_FR;
//         }
//     }

//     void newACK(){
//         printf("New ACK at state: %d\n", cur_state->state_type);
//         /* increment window size*/
//         tcp_sender->CW += 1;
//         /* send new packets based on cw*/
//         //tcp_sender->to_do = 0;
//         /* clear duplicate */
//         tcp_sender->dup_ack = 0;

//         /* check if need to swtich state*/
//         if(tcp_sender->CW >= tcp_sender->SST){
//             if(!cur_state){
//                 delete cur_state;
//             }
//             CongestionAvoid* state_CA = new CongestionAvoid(tcp_sender);
//             cur_state = state_CA;
//         }
//     }

//     void timeOut(){
//         /* half sst*/
//         tcp_sender->SST = round(tcp_sender->CW / 2) + 1;
//         /* set new cw size */
//         tcp_sender->CW = 1;
//         /* clear duplicate */
//         tcp_sender->dup_ack = 0;
//         /* resend cw_base */
//         //tcp_sender->to_do = 2;
//     }
// };

// // class CongestionAvoid: public State{
// // public:
// //     /* init */
// //     CongestionAvoid(Sender* sender){
// //         this->tcp_sender = sender;
// //         this->state_type = 1;
// //     }

// CongestionAvoid::CongestionAvoid(Sender* sender){
//     this->tcp_sender = sender;
//     this->state_type = 1;
// }

// /* operation */
// void CongestionAvoid::dupACK(){
//     /* increment numbers of duplicate ack*/
//     tcp_sender->dup_ack++;
//     /* check duplicate ack num*/
//     if(tcp_sender->dup_ack != 3){
//         /* continue waiting for ack*/
//         //tcp_sender->to_do = 3;
//     }
//     else{
//         /* half sst*/
//         tcp_sender->SST = round(tcp_sender->CW / 2) + 1;
//         /* set new cw size */
//         tcp_sender->CW = tcp_sender->SST + 3;
//         /* Resend cw_base and new pkt based on cw */
//         //tcp_sender->to_do = 1;
//         /* swtich to fast recovery */
//         if(!cur_state){
//             delete cur_state;
//         }
//         FastRecovery* state_FR = new FastRecovery(tcp_sender);
//         cur_state = state_FR;
//     }
// }

// void CongestionAvoid::newACK(){
//     printf("New ACK at state: %d\n", cur_state->state_type);
//     /* increment window size*/
//     tcp_sender->CW += 1/(floor(tcp_sender->CW));
//     /* send new packets based on cw*/
//     //tcp_sender->to_do = 0;
//     /* clear duplicate */
//     tcp_sender->dup_ack = 0;
// }

// void CongestionAvoid::timeOut(){
//     /* half sst*/
//     tcp_sender->SST = round(tcp_sender->CW / 2) + 1;
//     /* set new cw size */
//     tcp_sender->CW = 1;
//     /* clear duplicate */
//     tcp_sender->dup_ack = 0;
//     /* resend cw_base */
//     //tcp_sender->to_do = 2;
//     /* swtich back to slow start state */
//     if(!cur_state){
//             delete cur_state;
//         }
//     SlowStart* state_SS = new SlowStart(tcp_sender);
//     cur_state = state_SS;
// }


// // class FastRecovery: public State{
// // public:
// //     /* init */
// //     FastRecovery(Sender* sender){
// //         this->tcp_sender = sender;
// //         this->state_type = 2;
// //     }

// FastRecovery::FastRecovery(Sender* sender){
//     this->tcp_sender = sender;
//     this->state_type = 2;
// }
    
// void FastRecovery::dupACK(){
//     /* increment numbers of duplicate ack*/
//     tcp_sender->dup_ack++;
//     /* incerment cw size */
//     tcp_sender->CW += 1;
//     //tcp_sender->to_do = 0;
// }

// void FastRecovery::newACK(){
//     printf("New ACK at state: %d\n", cur_state->state_type);
//     /* clear duplicate ack */
//     tcp_sender->dup_ack = 0;
//     /* set cw to SST */
//     tcp_sender->CW = tcp_sender->SST;
//     //tcp_sender->to_do = 0;
    
//     /* switch to congetion avoidance state*/
//     if(!cur_state){
//         delete cur_state;
//     }
//     CongestionAvoid* state_CA = new CongestionAvoid(tcp_sender);
//     cur_state = state_CA;
// }

// void FastRecovery::timeOut(){
//     /* half sst*/
//     tcp_sender->SST = round(tcp_sender->CW / 2) + 1;
//     /* set new cw size */
//     tcp_sender->CW = 1;
//     /* clear duplicate */
//     tcp_sender->dup_ack = 0;
//     /* resend cw_base */
//     //tcp_sender->to_do = 2;
//     /* swtich back to slow start state */
//     if(!cur_state){
//             delete cur_state;
//         }
//     SlowStart* state_SS = new SlowStart(tcp_sender);
//     cur_state = state_SS;
// }

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    // open the file to be transmitted
    // also, get the sockfd for further process
    int rv, sockfd;
    int numBytes;
    
    file_ptr = fopen(filename, "rb");
    if (file_ptr == NULL) {
        printf("file can not be open");
        exit(1);
    }
    // remainingBytes = bteysToTransfer;

    // may need to parse hostUDPport?
    // memset(&servinfo,0,sizeof servinfo);
    memset(&hints, 0, sizeof(hints));
    // hints.ai_family = AF_INET;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    for(p = servinfo; p != NULL; p = servinfo->ai_next) {
        if ((sockfd = socket(AF_INET, servinfo->ai_socktype, IPPROTO_UDP)) == -1) {
            perror("server: error socket");
            continue;
        }
        break;
    }
    // if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    //   perror("server: error socket");
    // }
    
    // if (p == NULL)  {
    //     fprintf(stderr, "server: failed to bind\n");
    //     exit(1);
    // }

    printf("Initializing tcp sender\n");
    Sender tcp_sender(sockfd, bytesToTransfer);
    // printf("Start from State: SlowStart\n");
    // cur_state = new SlowStart(&tcp_sender);
        
    while(!tcp_sender.is_end){
      switch (to_do)
        {
        case WAIT_ACK:
            tcp_sender.WaitAck();
            break;
        case SEND_PCK:
            tcp_sender.SendPackets();
            break;
        case RESEND_TO:
            tcp_sender.Resend_TO();
            break;
        case RESEND_DUP:
            tcp_sender.Resend_DUP();
            break;
        default:
            break;
        }
    }
    
    // int end_seq_num = -1;
    TCP_packet finalPkt = TCP_packet();
    finalPkt.seq_num = -1;
    finalPkt.create_pkt_buf(pktBuffer);

    if((numBytes = sendto(sockfd, pktBuffer, sizeof(pktBuffer), 0, p->ai_addr, p->ai_addrlen))== -1){
        perror("sending end packet error");
    }

    freeaddrinfo(servinfo);
    fclose(file_ptr);
    close(sockfd);
    return;
}



int main(int argc, char** argv) {

    // unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    // udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], argv[2], argv[3], numBytes);


    return (EXIT_SUCCESS);
}


