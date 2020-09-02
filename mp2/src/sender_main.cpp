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
#include <netdb.h>
#include <math.h>
#include <errno.h>

#include <iostream>
#include <deque>
#include <string>

using namespace std;

#define MSS 1000
#define SWND 150 * 1000

enum MsgType{
    SYN = 0,
    SEQ,
    ACK,
    FIN,
    FIN_ACK
};

enum CWndStatus{
    SLOW_START = 10,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

//packet structure used for transfering
typedef struct{
	int 	    data_size;
	uint64_t 	seq_num;
	uint64_t    ack_num;
	MsgType     msg_type;
	char        data[MSS];
}packet;

int socket_fd;
struct sockaddr_in bind_addr, sendto_addr;
struct sockaddr_storage their_addr;
socklen_t addr_len = sizeof their_addr;

FILE* fp;
uint64_t total_packet_cnt;
uint64_t bytes_to_read;

uint64_t sent_packet_cnt;
uint64_t received_packet_cnt;

int dup_ack_cnt;
CWndStatus status;

uint64_t seq_num;
deque<packet>msg_queue;
deque<packet>msg_send_queue;

float cwnd;
float ssthresh;

int64_t timeout = 100000;

void setTimeout(timeval t)
{
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0) {
        perror("sender: setsockopt");
    }
}

int initSocket(char* hostname, char* UDPport){
	uint16_t sendto_port = (uint16_t)atoi(UDPport);
	memset(&sendto_addr, 0, sizeof(sendto_addr));
	sendto_addr.sin_family = AF_INET;
	sendto_addr.sin_port = htons(sendto_port);
	inet_pton(AF_INET, hostname, &sendto_addr.sin_addr);

    struct addrinfo hints, *servinfo, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

    int rv;
	if ((rv = getaddrinfo(hostname, UDPport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("sender: socket");
			continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "sender: failed to create socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	uint16_t current_port = ((struct sockaddr_in*)p->ai_addr)->sin_port;
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(current_port);
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(socket_fd, (struct sockaddr*)&bind_addr, sizeof(struct sockaddr_in)) == -1) {
		close(socket_fd);
		perror("sender: bind");
		exit(1);
	}
    return socket_fd;
}

void initFile(char* filename){
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Open file error!");
        char *buffer;
        buffer = new char[1000];
	    buffer = getcwd(NULL, 0);
        printf("pwd is %s", buffer);
        exit(1);
    }
}

void init(char* hostname, char* UDPport, char* destinationFile, unsigned long long int bytesToTransfer){
    initSocket(hostname, UDPport);
    initFile(destinationFile);
    total_packet_cnt = ceil(1.0 * bytesToTransfer / MSS);
    bytes_to_read = bytesToTransfer;
    seq_num = 0;
    sent_packet_cnt = 0;
    received_packet_cnt = 0;
    status = SLOW_START;
    dup_ack_cnt = 0;
    cwnd = MSS;
    ssthresh = MSS * 150;

    timeval t;
    t.tv_sec = 0;
    t.tv_usec = timeout;
    setTimeout(t);
}

void sendPacket(packet* pkt){
    int numbytes = 0;
    if((numbytes = sendto(socket_fd, pkt, sizeof(packet), 0, (struct sockaddr*)&sendto_addr, sizeof(sendto_addr)))== -1){
        perror("Error: data sending");
        exit(2);
    }
}

void sendPackets(){
    while(!msg_send_queue.empty())
    {
        sendPacket(&msg_send_queue.front());
        msg_send_queue.pop_front();
        sent_packet_cnt++;
    }
}

void resendPackets(){
    for(auto it = msg_queue.begin(); it != msg_queue.end(); it++){
        sendPacket(&(*it));
    }
}

void updateSWndAndSend(int packet_cnt){
    if(bytes_to_read <= 0){
        return;
    }
    char buf[MSS];
    memset(buf, 0, MSS);
    packet pkt;
    for(int i = 0; i < ceil((cwnd - msg_queue.size() * MSS) / MSS); i++){
        int read_size = fread(buf, sizeof(char), MSS, fp);
        if(read_size > 0){
            pkt.data_size = read_size;
            pkt.seq_num = seq_num;
            pkt.msg_type = SEQ;
            memcpy(pkt.data, &buf, read_size);
            seq_num += read_size;
            bytes_to_read -= read_size;
            msg_queue.push_back(pkt);
            msg_send_queue.push_back(pkt);
            cout << "Current msg queue size is " << msg_queue.size()<< endl;
            cout << "Current msg send queue size is " << msg_send_queue.size()<< endl;
        }
    }
    sendPackets();
}

void newACKCWndHandler(){
    dup_ack_cnt = 0;
    switch (status)
    {
    case SLOW_START:
        {
            if(cwnd >= ssthresh){
                cout << "Change from  SLOW_START to CONGESTION_AVOID"<< endl;
                status = CONGESTION_AVOID;
                cwnd = cwnd + MSS;
                cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
            }else{
                cwnd = max((float)MSS, cwnd);
                cwnd = cwnd + MSS;
                cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
            }
        }
        break;
    case CONGESTION_AVOID:
        {
           cwnd = cwnd + MSS * (1.0 * MSS / cwnd); 
           cwnd = max((float)MSS, cwnd);
           cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
        }
        break;
    case FAST_RECOVERY:
        {
            cwnd = ssthresh;
            cout << "Change from  FAST_RECOVERY to CONGESTION_AVOID"<< endl;
            cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
            cwnd = cwnd + MSS;
            status = CONGESTION_AVOID;
        }
    break;
    default:
        break;
    }
}

void dupACKCWndHandler(){
    dup_ack_cnt++;
    switch (status)
    {
    case SLOW_START:
        {
            if(dup_ack_cnt >= 3){
                ssthresh = cwnd / 2;
                ssthresh = max((float)MSS, ssthresh);
                cwnd = ssthresh + 3 * MSS;
                cwnd = max((float)MSS, cwnd);

                cout << "Change from  SLOW_START to FAST_RECOVERY"<< endl;
                cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
                status = FAST_RECOVERY;
                sendPacket(&msg_queue.front());
                //resendPackets();
            }
        }
        break;
    case CONGESTION_AVOID:
        {
           if(dup_ack_cnt >= 3){
                ssthresh = cwnd / 2;
                ssthresh = max((float)MSS, ssthresh);
                cwnd = ssthresh + 3 * MSS;
                cwnd = max((float)MSS, cwnd);
                cout << "Change from  CONGESTION_AVOID to FAST_RECOVERY"<< endl;
                cout << "Current CWND is "<< cwnd << " ssthresh is " << ssthresh << endl;
                status = FAST_RECOVERY;
                sendPacket(&msg_queue.front());
                //resendPackets();
           }
        }
        break;
    case FAST_RECOVERY:
        {
        }
    break;
    default:
        break;
    }
}

void timeoutCWndHandler(){
    cout << "Change from "  << status << " to SLOW_START"<< endl;
    ssthresh = cwnd / 2;
    ssthresh = max((float)MSS, ssthresh);
    cwnd = MSS;
    status = SLOW_START;
    dup_ack_cnt = 0;
}


void ACKHandler(packet* pkt){
    if(pkt->ack_num < msg_queue.front().seq_num){
        // invaild ack
        return;
    }
    if(pkt->ack_num > msg_queue.front().seq_num){
        // new ack
        newACKCWndHandler();
        cout << "Current packet ack is "<< pkt->ack_num << endl;
        int packet_cnt = ceil((pkt->ack_num - msg_queue.front().seq_num) / (1.0 * MSS));
        received_packet_cnt += packet_cnt;
        cout << "receive "<< packet_cnt << " packets" << endl;
        int cnt = 0;
        while(!msg_queue.empty() && cnt < packet_cnt){
            msg_queue.pop_front();
            cnt++;
        }
        updateSWndAndSend(packet_cnt);
    }else if(pkt->ack_num == msg_queue.front().seq_num){
        // duplicate ack
        dupACKCWndHandler();
        if(dup_ack_cnt >= 3){
            cout << "Three dup ACK at sending " << msg_queue.front().seq_num << endl;
            if(!msg_queue.empty()){
                cout << "resend packet " << msg_queue.front().seq_num << endl;
                sendPacket(&msg_queue.front());
            }
            dup_ack_cnt = 0;
        }
    }
}

void FINHandler(packet* pkt){

}

void packetHandler(packet* pkt){
    switch (pkt->msg_type)
    {
    case ACK:
    {
        ACKHandler(pkt);
    }
    break;
    case FIN:
    {
        FINHandler(pkt);
    }    
    default:
        break;
    }
}

void endConnection(){
    cout << "File transfer finish start to close " << endl;
    packet pkt;
    char buf[sizeof(packet)];
    pkt.msg_type = FIN;
    pkt.data_size=0;
    memset(pkt.data, 0, MSS);
    sendPacket(&pkt);
    cout << "Send FIN " << endl;
    while (1) {
        int numbytes = 0;
        packet ack;
        if ((numbytes = recvfrom(socket_fd, buf, sizeof(packet), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("receive error");
                exit(2);
            }
            else{
                cout << "Send FIN timeout start to resend" << endl;
                // timeout
                pkt.msg_type = FIN;
                pkt.data_size=0;
                memset(pkt.data, 0, MSS);
                sendPacket(&pkt);
            }
        }else{
            memcpy(&ack, buf, sizeof(packet));
            if (ack.msg_type == FIN_ACK) {
                cout << "Receive FINACK , Sending FIN_ACK " << endl;
                pkt.msg_type = FIN_ACK;
                sendPacket(&pkt);
                cout << "Program Exits " << endl;
                break;
            }
        }
    }
}

void fileTransferMainLoop(){
    int default_cwnd = 1;
    int packet_cnt = min((int)ceil(1.0 * bytes_to_read / MSS), default_cwnd);
    updateSWndAndSend(packet_cnt);

    int numbytes = 0;
    packet pkt;
    char buf [sizeof(packet)];
    while (sent_packet_cnt < total_packet_cnt || received_packet_cnt < sent_packet_cnt){
            if((numbytes = recvfrom(socket_fd, &pkt, sizeof(packet), 0, NULL, NULL)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                perror("receive error");
                exit(2);
            }
            if(!msg_queue.empty()){
                cout << "Timeout at sending " << msg_queue.front().seq_num << endl;
                // Timeout
                timeoutCWndHandler();
                // resend timeout packet
                sendPacket(&msg_queue.front());
                //resendPackets();
            }
        }else{
            //memcpy(&pkt, buf, sizeof(packet));
            packetHandler(&pkt);
        }
    }
    endConnection();
}

void reliablyTransfer(char* hostname, char* UDPport, char* filename, unsigned long long int bytesToTransfer) {
    init(hostname, UDPport, filename, bytesToTransfer);
    fileTransferMainLoop();
    fclose(fp);

    return;
}

int main(int argc, char** argv) {
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    numBytes = atoll(argv[4]);
    reliablyTransfer(argv[1], argv[2], argv[3], numBytes);
    return 0;
}


