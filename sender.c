#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define PACKET_SIZE 1024

typedef struct {
    int type;
    int flag;
    int seqNum;
    int ackNum;
    int length;
    char data[PACKET_SIZE - 12];
} Packet;

void log_event(const char *event) {
    printf("%s\n", event);
    sleep(1);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("%s <송신자 포트> <수신자 IP> <수신자 포트> <타임아웃 간격(ms)> <파일 이름> <ACK 드랍 확률>\n", argv[0]);
        sleep(2);
        exit(1);
    }

    int senderPort = atoi(argv[1]);
    char *receiverIP = argv[2];
    int receiverPort = atoi(argv[3]);
    int timeoutInterval = atoi(argv[4]);
    char *filename = argv[5];
    float ackDropProb = atof(argv[6]);

    srand(time(NULL));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return 1;
    }

    struct sockaddr_in senderAddr, receiverAddr;
    memset(&senderAddr, 0, sizeof(senderAddr));
    senderAddr.sin_family = AF_INET;
    senderAddr.sin_port = htons(senderPort);
    senderAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&senderAddr, sizeof(senderAddr)) < 0) {
        close(sockfd);
        return 1;
    }

    memset(&receiverAddr, 0, sizeof(receiverAddr));
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(receiverPort);
    receiverAddr.sin_addr.s_addr = inet_addr(receiverIP);

    Packet syn;
    syn.type = 0;
    syn.flag = 1;
    syn.seqNum = 0;
    syn.ackNum = 0;
    syn.length = 0;

    sendto(sockfd, &syn, sizeof(syn), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
    log_event("SYN 패킷 전송 완료");

    Packet synAck;
    socklen_t addrLen = sizeof(receiverAddr);
    if (recvfrom(sockfd, &synAck, sizeof(synAck), 0, (struct sockaddr *)&receiverAddr, &addrLen) < 0) {
        close(sockfd);
        return 1;
    }
    log_event("SYN-ACK 패킷 수신 완료");

    Packet ack;
    ack.type = 0;
    ack.flag = 2;
    ack.seqNum = 1;
    ack.ackNum = synAck.seqNum + 1;
    ack.length = 0;

    sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
    log_event("ACK 패킷 전송 완료");

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        close(sockfd);
        return 1;
    }

    int cwnd = 1;
    int ssthresh = 64;
    int seqNum = 0;
    int ackNum = 0;
    int packetsSent = 0;
    int packetsInFlight = 0;

    struct timeval timeout;
    timeout.tv_sec = timeoutInterval;
    timeout.tv_usec = timeoutInterval * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

    while (1) {
        while (packetsInFlight < cwnd) {
            Packet data;
            data.type = 1;
            data.flag = 0;
            data.seqNum = seqNum;
            data.ackNum = ackNum;
            data.length = fread(data.data, 1, PACKET_SIZE - 12, fp);
            if (data.length == 0) {
                break;
            }

            sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
            char log_msg[128];
            sprintf(log_msg, "seqNum %d번 DATA 패킷 전송 완료", data.seqNum);
            log_event(log_msg);
            seqNum++;
            packetsInFlight++;
            packetsSent++;
        }

        Packet ack;
        if (recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiverAddr, &addrLen) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_event("타임아웃! 재전송 중");
                for (int i = ackNum; i < seqNum; i++) {
                    fseek(fp, (i - ackNum) * (PACKET_SIZE - 12), SEEK_SET);
                    Packet data;
                    data.type = 1;
                    data.flag = 0;
                    data.seqNum = i;
                    data.ackNum = ackNum;
                    data.length = fread(data.data, 1, PACKET_SIZE - 12, fp);
                    if (data.length == 0) {
                        break;
                    }
                    sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
                    char log_msg[128];
                    sprintf(log_msg, "seqNum %d번 DATA 패킷 재전송 완료", data.seqNum);
                    log_event(log_msg);
                    packetsSent++;
                }
                continue;
            }
            break;
        }

        if (ack.type == 0 && ack.flag == 2) {
            char log_msg[128];
            sprintf(log_msg, "ackNum %d번 ACK 패킷 수신 완료", ack.ackNum);
            log_event(log_msg);
            if (rand() / (float)RAND_MAX < ackDropProb) {
                log_event("ACK 패킷 드랍!");
                continue;
            }
            packetsInFlight--;
            ackNum = ack.seqNum + 1;

            if (ackNum == seqNum) {
                log_event("파일 전송 완료!");
                break;
            }

            if (packetsInFlight == 0) {
                cwnd = 1;
            } else {
                if (cwnd < ssthresh) {
                    cwnd++;
                } else {
                    cwnd += 1 / cwnd;
                }
            }

            sprintf(log_msg, "CWND %d로 업데이트", cwnd);
            log_event(log_msg);
        }
    }

    Packet fin;
    fin.type = 0;
    fin.flag = 3;
    fin.seqNum = seqNum;
    fin.ackNum = ackNum;
    fin.length = 0;

    sendto(sockfd, &fin, sizeof(fin), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
    log_event("FIN 패킷 전송 완료");

    Packet finAck;
    if (recvfrom(sockfd, &finAck, sizeof(finAck), 0, (struct sockaddr *)&receiverAddr, &addrLen) < 0) {

    } else {
        log_event("FIN-ACK 패킷 수신 완료");
    }

    close(sockfd);
    fclose(fp);
    return 0;
}
