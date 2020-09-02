#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <iostream>
#include <vector>
#include <functional> 
#include <queue>

using namespace std;

struct addrinfo *p;
int socket_fd;
FILE* fp;
uint64_t ack_num;

struct sockaddr_storage their_addr;
socklen_t addr_len;

#define MSS 1000

enum MsgType{
    SYN = 0,
    SEQ,
    ACK,
    FIN,
    FIN_ACK
};

typedef struct{
	int 	    data_size;
	uint64_t 	seq_num;
	uint64_t    ack_num;
	MsgType     msg_type;
	char        data[MSS];
}packet;

void diep(char *s) {
    perror(s);
    exit(1);
}

struct cmp {
    bool operator()(packet a,packet b) {
        return  a.seq_num > b.seq_num; 
    }
};

priority_queue<packet, vector<packet>, cmp> msg_queue;

void setTimeout(timeval t)
{
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
        perror("sender: setsockopt");
    }
}

int initSocket(char* UDPport){
    int rv;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, UDPport, &hints, &servinfo))!= 0) {
       return -1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if (( socket_fd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("UDP server: socket");
            continue;
        }
        if(bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_fd);
            perror("UDP server:bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "UDP server: failed to bind socket");
        return 2;
    }

    freeaddrinfo(servinfo);
    return socket_fd;
}

void initFile(char* destinationFile){
    fp = fopen(destinationFile, "wb");
}

void init(char* myUDPport, char* destinationFile){
    initFile(destinationFile);
    initSocket(myUDPport);
    ack_num = 0;
    addr_len = sizeof(their_addr);
}

void sendACK(){
    packet ack;
    ack.msg_type=ACK;
    ack.ack_num = ack_num;
    sendto(socket_fd, &ack, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len);
}

void handleSEQ(packet* pkt){
    if(pkt->seq_num == ack_num){
        // inorder pkt
        //cout << "receive inorder packet with seq "<< pkt->seq_num <<endl;
        fwrite(pkt->data, sizeof(char), pkt->data_size, fp);
        ack_num += pkt->data_size;
        bool flag = false;
        int cnt = 0;
        while(msg_queue.size() && msg_queue.top().seq_num == ack_num){
            flag = true;
            fwrite(msg_queue.top().data, sizeof(char), msg_queue.top().data_size, fp);
            ack_num += msg_queue.top().data_size;
            msg_queue.pop();
            cnt++;
        }
        if(flag){
            cout << "use cached "<< cnt << "packets "<<"new ack is " <<ack_num <<endl;
        }
    }else if (pkt->seq_num > ack_num){
        cout << "higher out of order packet with seq "<< pkt->seq_num <<endl;
        cout << "target seq is "<< ack_num <<endl;
        cout << "current heap size is "<< msg_queue.size() <<endl;
        if(msg_queue.size() <= 300){
            msg_queue.push(*pkt);
        }
        else{
            cout << "cached enought packets!" << endl;
        }   
    }else{
        cout << "lower out of order packet with seq "<< pkt->seq_num << " discard" <<endl;
    }
    sendACK();
}

void handleFIN(packet* fin){
    timeval t;
    t.tv_sec = 0;
    t.tv_usec = 100000;
    setTimeout(t);

    packet fin_ack;
    fin_ack.msg_type = FIN_ACK;
    cout << "Send FINACK packet"<<endl;
    sendto(socket_fd, &fin_ack, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len);

    packet pkt;
    char buf[sizeof(packet)];
    int timeout_cnt = 0;
    while (true) {
        int numbytes = 0;
        packet ack;
        if ((numbytes = recvfrom(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("receive error");
                exit(2);
            }
            else{
                // timeout
                cout << "Timeout, resend FINACK packet"<<endl;
                if(timeout_cnt >=3){
                    cout << "Time out three times during sending FINACK, exit program"<<endl;
                    break;
                }
                sendto(socket_fd, &fin_ack, sizeof(packet), 0, (struct sockaddr *) &their_addr,addr_len);
                timeout_cnt++;
            }
        }else{
            memcpy(&ack, buf, sizeof(packet));
            if (ack.msg_type == FIN_ACK) {
                cout << "Receive FINACK, exit program"<<endl;
                break;
            }
        }

    }

}

void ReceiveMainLoop(){
    int numbytes = 0;
    char buf[sizeof(packet)];
    packet pkt;
    while (true)
    {
        memset(buf, 0, sizeof(buf));
        numbytes = recvfrom(socket_fd,&pkt,sizeof(packet),0,(struct sockaddr *) &their_addr,&addr_len);
        if(numbytes ==-1) {
            perror("can not receive data");
            exit(2);
        }

        //memcpy(&pkt,buf,sizeof(packet));
        if(pkt.msg_type == SEQ){
            handleSEQ(&pkt);
        }else if(pkt.msg_type == FIN){
            cout << "Receive FIN packet"<<endl;
            handleFIN(&pkt);
            break;
        }
    }
}

void reliablyReceive(char* myUDPport, char* destinationFile) {
    init(myUDPport, destinationFile);
    ReceiveMainLoop();
    fclose(fp);
    return;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }
    reliablyReceive(argv[1], argv[2]);
}

