#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    if (argc != 3) {
        printf("%s <수신자 포트> <드랍 확률>\n", argv[0]);
        sleep(2);
        exit(1);
    }

    int receiver_port = atoi(argv[1]);
    double drop_probability = atof(argv[2]);

    srand(time(NULL));

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return 1;
    }

    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);
    receiver_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        close(sockfd);
        return 1;
    }

    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    Packet packet;
    int expected_seq_num = 0;

    FILE *file = fopen("receiver_file", "wb");
    if (file == NULL) {
        close(sockfd);
        return 1;
    }

    while (1) {
        if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr, &addr_len) < 0) {
            continue;
        }

        if (packet.flag == 1 && packet.type == 0) {
            log_event("SYN 패킷 수신 완료");
            Packet syn_ack;
            syn_ack.type = 0;
            syn_ack.flag = 2;
            syn_ack.seqNum = 0;
            syn_ack.ackNum = packet.seqNum + 1;
            syn_ack.length = 0;

            sendto(sockfd, &syn_ack, sizeof(syn_ack), 0, (struct sockaddr *)&sender_addr, addr_len);
            log_event("SYN-ACK 패킷 전송 완료");
        } else if (packet.flag == 2 && packet.type == 0) {
            log_event("ACK 패킷 수신 완료");
        } else if (packet.flag == 3 && packet.type == 0) {
            log_event("FIN 패킷 수신 완료");
            Packet fin_ack;
            fin_ack.type = 0;
            fin_ack.flag = 2;
            fin_ack.seqNum = packet.seqNum;
            fin_ack.ackNum = packet.seqNum + 1;
            fin_ack.length = 0;

            sendto(sockfd, &fin_ack, sizeof(fin_ack), 0, (struct sockaddr *)&sender_addr, addr_len);
            log_event("FIN-ACK 패킷 전송 완료");
            break;
        } else if (packet.type == 1) {
            char log_msg[128];
            sprintf(log_msg, "seqNum %d인 DATA 패킷 수신 완료", packet.seqNum);
            log_event(log_msg);

            if (rand() / (double)RAND_MAX < drop_probability) {
                log_event("DATA 패킷 드랍!");
                continue;
            }

            if (packet.seqNum == expected_seq_num) {
                fwrite(packet.data, 1, packet.length, file);
                expected_seq_num++;
            }

            Packet ack;
            ack.type = 0;
            ack.flag = 2;
            ack.seqNum = packet.seqNum;
            ack.ackNum = expected_seq_num;
            ack.length = 0;

            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&sender_addr, addr_len);
            sprintf(log_msg, "ackNum %d번 ACK 패킷 전송 완료", ack.ackNum);
            log_event(log_msg);
        }
    }

    fclose(file);
    close(sockfd);
    return 0;
}
