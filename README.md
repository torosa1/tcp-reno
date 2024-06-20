# Term Project #3

### 프로젝트 프로그램의 목적

TCP Reno 혼잡 제어 메커니즘을 통해 네트워크 혼잡을 방지하고 공정성을 보장하며 안정적이고 효율적인 데이터 전송을 달성하는 프로그램을 만드는 것입니다.

### 프로젝트 코드 특징

각 패킷에는 타입, 플래그, 시퀀스 번호, 확인 번호, 데이터 길이 및 실제 데이터가 포함합니다.

소켓을 생성하고, 송신자와 수신자의 주소를 설정합니다. bind 함수를 사용하여 소켓을 송신자 포트에 바인딩합니다.

파일에서 데이터를 읽어 패킷으로 만들어 수신자에게 전송합니다. 데이터 패킷의 시퀀스 번호와 ACK 번호를 관리하여 정상적으로 수신될 때까지 재전송합니다.

cwnd와 ssthresh 변수를 사용하여 현재의 윈도우 크기를 관리합니다. ACK를 받을 때마다 윈도우 크기를 조정하여 네트워크 혼잡 상태에 따라 전송 속도를 조절합니다.

송신자와 수신자가 사용자 정의 확률에 따라 수신된 패킷을 폐기할 수 있습니다. 네트워크 상황에 따라 패킷 폐기 확률을 임의로 부여해 시뮬레이션 할 수 있습니다.

setsockopt 함수를 사용하여 소켓의 타임아웃을 설정합니다. ACK를 기다리는 동안 타임아웃이 발생하면 데이터 패킷을 재전송합니다.

손실된 패킷을 재전송하는 타이머에 알람 시스템 호출을 사용할 수 있습니다.




## 송신자
이 프로그램은 TCP를 사용하여 여러 기능을 추가해 파일을 안전하게 전송합니다.
크게 추가된 기능은 TCP Reno는 네트워크 혼잡을 감지하여 네트워크 자원을 효율적으로 활용하고 패킷 손실을 최소화합니다.
cwnd와 ssthresh 변수를 사용해 네트워크 상황 변화에 따라 적절하게 대응함으로써 안정적인 데이터 전송을 보장합니다. 


```c
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


```
### 사용법
./render <송신포트> <수신IP> <수신포트> <타임아웃> <파일이름> <ACK손실확률>

<송신포트>: 데이터를 송신하는 포트 번호입니다.
<수신IP>: 데이터를 수신하는 대상의 IP 주소입니다.
<수신포트>: 데이터를 수신하는 포트 번호입니다.
<타임아웃>: ACK를 기다리는 타임아웃 시간입니다.
<파일이름>: 전송할 파일의 이름입니다.
<ACK손실확률>: ACK를 손실할 확률입니다.

### 파일 구성
file_transfer.c: 메인 프로그램 소스 코드 파일입니다.
packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

### 프로그램 작동순서
1. 입력된 파일을 읽어서 데이터 패킷으로 분할합니다.
2. 데이터 패킷을 송신하고, ACK를 기다립니다. 만약 ACK를 받지 못할 경우, 타임아웃이 발생하거나 ACK가 손실될 때까지 재전송을 시도합니다.
3. 파일의 끝에 도달할 때까지 위 과정을 반복합니다.
4. 파일 전송이 완료되면, 종료를 알리는 특별한 종료 패킷(EOT)을 전송합니다.

## 수신자
이 프로그램은 여러 기능이 추가된 UDP를 사용하여 안전하게 데이터를 수신합니다.
크게 추가된 기능은 데이터를 받으면 reqNum에 따라 ACK를 송신자에게 송신하여 데이터의 수신여부를 확인합니다.


```c
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


```
### 사용법
./receiver <수신포트> <데이터손실확률>

<수신포트>: 데이터를 수신하는 포트 번호입니다.
<데이터손실확률>: 데이터를 드랍할 확률입니다.

packet.h: 데이터 패킷 구조체와 관련 함수들을 정의한 헤더 파일입니다.

### 프로그램 작동순서
1. 데이터를 송신하는 프로그램에서 전송된 데이터 패킷을 수신합니다.
2. 수신된 패킷의 시퀀스 번호를 확인하여 순서대로 수신하는지 확인합니다.
3. 데이터를 드랍할 확률에 따라 일부 패킷을 드랍하고, 나머지는 파일에 쓰여집니다.
4. 올바른 시퀀스 번호를 가진 데이터 패킷에 대해 ACK(확인)을 송신하는 패킷을 전송합니다.
5. 파일 전송이 완료되면, 특별한 종료 패킷(EOT)을 수신하여 프로그램을 종료합니다.


#### 컴파일 화면
https://github.com/torosa1/project2/assets/165176275/16523009-165b-4481-9bda-f60c05978188

#### 송신자 실행 파일
https://github.com/torosa1/project2/assets/165176275/9146a915-dd51-45cc-9a01-c3128b51a81a

#### 수신자 실행 파일
https://github.com/torosa1/project2/assets/165176275/c8456fea-f648-466a-b823-0a4c534b21e4

#### 참고 사이트

https://jackpang.tistory.com/23

https://iijw.tistory.com/3

https://hyeo-noo.tistory.com/280
