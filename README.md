# Term Project #3

## 프로젝트 프로그램의 목적

TCP Reno 혼잡 제어 메커니즘을 통해 네트워크 혼잡을 방지하고 공정성을 보장하며 안정적이고 효율적인 데이터 전송을 달성하는 프로그램을 만드는 것입니다.

## 프로젝트 코드 특징

각 패킷에는 타입, 플래그, 시퀀스 번호, 확인 번호, 데이터 길이 및 실제 데이터가 포함합니다.

소켓을 생성하고, 송신자와 수신자의 주소를 설정합니다. bind 함수를 사용하여 소켓을 송신자 포트에 바인딩합니다.

파일에서 데이터를 읽어 패킷으로 만들어 수신자에게 전송합니다. 데이터 패킷의 시퀀스 번호와 ACK 번호를 관리하여 정상적으로 수신될 때까지 재전송합니다.

cwnd와 ssthresh 변수를 사용하여 현재의 윈도우 크기를 관리합니다. ACK를 받을 때마다 윈도우 크기를 조정하여 네트워크 혼잡 상태에 따라 전송 속도를 조절합니다.

송신자와 수신자가 사용자 정의 확률에 따라 수신된 패킷을 폐기할 수 있습니다. 네트워크 상황에 따라 패킷 폐기 확률을 임의로 부여해 시뮬레이션 할 수 있습니다.

setsockopt 함수를 사용하여 소켓의 타임아웃을 설정합니다. ACK를 기다리는 동안 타임아웃이 발생하면 데이터 패킷을 재전송합니다.




## 송신자
이 프로그램은 TCP를 사용하여 여러 기능을 추가해 파일을 안전하게 전송합니다.
크게 추가된 기능은 TCP Reno는 네트워크 혼잡을 감지하여 네트워크 자원을 효율적으로 활용하고 패킷 손실을 최소화합니다.
cwnd와 ssthresh 변수를 사용해 네트워크 상황 변화에 따라 적절하게 대응함으로써 안정적인 데이터 전송을 보장합니다. 

## sender.c
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
```
### PACKET_SIZE 매크로를 정의하여 데이터 패킷의 최대 크기를 지정합니다.
### Packet 구조체를 정의하여 네트워크 패킷의 필드들을 포함합니다 (type, flag, seqNum, ackNum, length, data).
```
#define PACKET_SIZE 1024

typedef struct {
    int type;
    int flag;
    int seqNum;
    int ackNum;
    int length;
    char data[PACKET_SIZE - 12];
} Packet;
```
### log_event 함수는 주어진 이벤트 문자열을 표준 출력으로 출력하고 1초간 잠시 멈춥니다.
```
void log_event(const char *event) {
    printf("%s\n", event);
    sleep(1);
}
```
### 프로그램 실행 시 7개의 인자가 전달되지 않으면 프로그램을 종료합니다.
```
int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("%s <송신자 포트> <수신자 IP> <수신자 포트> <타임아웃 간격(ms)> <파일 이름> <ACK 드랍 확률>\n", argv[0]);
        sleep(2);
        exit(1);
    }
```
### 명령행 인자로부터 송신자 포트(senderPort), 수신자 IP 주소(receiverIP), 수신자 포트(receiverPort), 타임아웃 간격(timeoutInterval), 파일 이름(filename), ACK 드랍 확률(ackDropProb)을 파싱합니다.
```
    int senderPort = atoi(argv[1]);
    char *receiverIP = argv[2];
    int receiverPort = atoi(argv[3]);
    int timeoutInterval = atoi(argv[4]);
    char *filename = argv[5];
    float ackDropProb = atof(argv[6]);
```
### srand(time(NULL));를 호출하여 난수 발생기를 시드화합니다.
```
    srand(time(NULL));
```
### socket 함수를 사용해 소켓을 생성합니다.
```
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return 1;
    }
```
### senderAddr 구조체를 초기화하고 송신자의 주소(senderPort)로 바인딩합니다.
```
    struct sockaddr_in senderAddr, receiverAddr;
    memset(&senderAddr, 0, sizeof(senderAddr));
    senderAddr.sin_family = AF_INET;
    senderAddr.sin_port = htons(senderPort);
    senderAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&senderAddr, sizeof(senderAddr)) < 0) {
        close(sockfd);
        return 1;
    }
```
### receiverAddr 구조체를 초기화하고 수신자의 주소(receiverIP, receiverPort)를 설정합니다.
    memset(&receiverAddr, 0, sizeof(receiverAddr));
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(receiverPort);
    receiverAddr.sin_addr.s_addr = inet_addr(receiverIP);

### SYN 패킷을 생성하고 sendto 함수를 사용하여 송신자에게 전송합니다.
```
    Packet syn;
    syn.type = 0;
    syn.flag = 1;
    syn.seqNum = 0;
    syn.ackNum = 0;
    syn.length = 0;

    sendto(sockfd, &syn, sizeof(syn), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
    log_event("SYN 패킷 전송 완료");
```
### recvfrom 함수를 사용하여 SYN-ACK 패킷을 수신하고, 수신 시간 초과 시 소켓을 닫고 프로그램을 종료합니다.
```
    Packet synAck;
    socklen_t addrLen = sizeof(receiverAddr);
    if (recvfrom(sockfd, &synAck, sizeof(synAck), 0, (struct sockaddr *)&receiverAddr, &addrLen) < 0) {
        close(sockfd);
        return 1;
    }
    log_event("SYN-ACK 패킷 수신 완료");
```
### ACK 패킷을 생성하고 송신자에게 전송합니다.
```
    Packet ack;
    ack.type = 0;
    ack.flag = 2;
    ack.seqNum = 1;
    ack.ackNum = synAck.seqNum + 1;
    ack.length = 0;

    sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
    log_event("ACK 패킷 전송 완료");
```
### 지정된 파일을 읽기 모드("r")로 엽니다. 파일 열기 실패 시 소켓을 닫고 프로그램을 종료합니다.
```
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        close(sockfd);
        return 1;
    }
```
### cwnd를 1로 설정합니다. 임계치(ssthresh)를 64로 설정합니다.
```
    int cwnd = 1;
    int ssthresh = 64;
    int seqNum = 0;
    int ackNum = 0;
    int packetsSent = 0;
    int packetsInFlight = 0;
```
### 시간 초과를 설정하기 위해 setsockopt 함수를 사용하여 소켓의 수신 타임아웃 값을 설정합니다.
```
    struct timeval timeout;
    timeout.tv_sec = timeoutInterval;
    timeout.tv_usec = timeoutInterval * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
```
### 현재 송신 윈도우(cwnd) 내에서 전송할 수 있는 만큼 데이터 패킷(data)을 생성하고 송신자에게 전송합니다.
```
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
```
### 각 데이터 패킷의 전송 완료를 로그합니다.
```
            sendto(sockfd, &data, sizeof(data), 0, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));
            char log_msg[128];
            sprintf(log_msg, "seqNum %d번 DATA 패킷 전송 완료", data.seqNum);
            log_event(log_msg);
            seqNum++;
            packetsInFlight++;
            packetsSent++;
        }
```
### recvfrom 함수로 ACK 패킷을 수신하다가 타임아웃(EAGAIN 또는 EWOULDBLOCK)이 발생하면 메시지를 출력하고 아직 ACK를 받지 못한 패킷들을 재전송합니다.
```
        Packet ack;
        if (recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&receiverAddr, &addrLen) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                log_event("타임아웃! 재전송 중");
```
### for 루프를 사용하여 ackNum부터 seqNum까지의 패킷을 차례로 읽어 재전송합니다.
### 파일 포인터를 fseek 함수를 사용하여 해당 패킷 위치로 이동한 후, 데이터 패킷을 다시 읽고 전송합니다.
```
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
```
### recvfrom 함수를 통해 ACK 패킷을 수신하면 ACK 패킷의 유형과 플래그를 확인하여 로그를 출력합니다.
### ACK 패킷이 ACK 드랍 확률(ackDropProb)에 따라 드랍될 수 있습니다. 드랍되면 "ACK 패킷 드랍!" 메시지를 출력하고 다음 루프를 진행합니다. 수신한 ACK 패킷에 따라 송신 중인 패킷의 수(packetsInFlight)를 감소시키고, 다음에 기대하는 ACK 번호(ackNum)를 설정합니다.
```
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
```
### 모든 데이터 패킷이 성공적으로 전송되면 메시지를 출력하고 루프를 종료합니다.
```
            if (ackNum == seqNum) {
                log_event("파일 전송 완료!");
                break;
            }
```
### 송신 윈도우 크기(cwnd)를 업데이트합니다. 송신 윈도우가 0이 될때 1로 설정하고, 아닐 경우 슬로 스타트와 컨젼션 기반의 증가 방식으로 조절합니다.
```
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
```
### FIN 패킷을 생성하고 송신자에게 전송합니다. recvfrom 함수를 사용하여 FIN-ACK 패킷을 수신하고 수신 성공 시 메시지를 출력합니다. 
```
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
```
### 소켓과 파일을 닫고 프로그램을 종료합니다.
```
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

### 프로그램 작동순서
1. 프로그램이 시작될 때 사용자로부터 송신자 포트, 수신자 IP 주소, 수신자 포트, 타임아웃 간격(ms), 파일 이름, ACK 드랍 확률을 명령줄에서 입력받습니다.
2. socket() 함수를 사용하여 소켓을 생성하고 송신자의 주소를 바인딩합니다. 수신자의 주소도 초기화하여 설정합니다.
3. Packet 구조체를 사용하여 SYN 패킷을 생성하고 이를 sendto() 함수를 통해 수신자에게 전송합니다. recvfrom() 함수를 사용하여 수신자로부터 SYN-ACK 패킷을 대기하 SYN-ACK 패킷을 받으면 이벤트 로깅을 수행합니다.
4. ACK 패킷을 생성하고 sendto() 함수를 사용하여 수신자에게 전송합니다. 연결 완료입니다.
5. 지정된 파일을 읽기 모드로 실행합니다. 파일이 열리지 않으면 프로그램은 종료됩니다. 데이터 전송을 위해 while 루프를 돌립니다.
6. packetsInFlight가 cwnd보다 작은 동안 데이터를 읽어 패킷을 생성하고 sendto() 함수를 통해 수신자에게 전송합니다. recvfrom() 함수를 사용하여 ACK 패킷을 대기합니다. 타임아웃이 발생하면 재전송을 수행합니다.
7. ACK 패킷을 수신하면 해당 패킷의 내용을 로깅하고 ACK 드랍 확률을 고려하여 패킷을 처리합니다.
8. 모든 데이터 전송이 완료되면 FIN 패킷을 생성하고 sendto() 함수를 통해 수신자에게 전송합니다.


## 수신자
이 프로그램은 여러 기능이 추가된 TCP를 통해 데이터를 수신합니다.
성뢰성 있는 데이터 전송을 위해 ACK 패킷을 수신하여 SYN-ACK 패킷 송신자에게 전송함으로 데이터 패킷을 드랍을 방지합니다.

## receiver.c
```
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
```
### 패킷의 최대 크기를 정의하는 매크로입니다. 데이터 패킷의 최대 크기는 1024바이트로 설정됩니다.
```
#define PACKET_SIZE 1024
```
### 전송될 패킷의 형식을 나타냅니다. 다섯 개의 정수형 필드와 데이터를 저장할 문자열 배열(data)이 포함되어 있습니다.
```
typedef struct {
    int type;
    int flag;
    int seqNum;
    int ackNum;
    int length;
    char data[PACKET_SIZE - 12];
} Packet;
```
### log_event 함수는 전달된 이벤트 문자열을 출력하고, 1초간 잠시 멈춥니다. 
```
void log_event(const char *event) {
    printf("%s\n", event);
    sleep(1);
}
```
### main 함수에서 프로그램의 시작합니다. 명령행 인자의 개수가 3개가 아니면 종료합니다.
```
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("%s <수신자 포트> <드랍 확률>\n", argv[0]);
        sleep(2);
        exit(1);
    }
```
### 명령행 인자로 받은 수신자 포트 번호와 드랍 확률을 정수형(receiver_port)과 실수형(drop_probability)으로 변환합니다.
```
    int receiver_port = atoi(argv[1]);
    double drop_probability = atof(argv[2]);
```
### 난수 생성기를 초기화하기 위해 srand 함수를 호출합니다. 이 함수는 현재 시간을 시드로 사용하여 난수 시퀀스를 초기화합니다.
```
    srand(time(NULL));
```
### 소켓을 생성합니다. 생성에 실패하면 프로그램을 1로 종료합니다.
```
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return 1;
    }
```
### 수신자의 주소 정보를 저장할 receiver_addr 구조체를 초기화하고 설정합니다. 주소 체계(AF_INET), 포트 번호, IP 주소(INADDR_ANY)를 설정합니다.
```
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);
    receiver_addr.sin_addr.s_addr = INADDR_ANY;
```
### 생성한 소켓을 수신자의 주소 정보에 바인딩합니다. 바인딩에 실패하면 소켓을 닫고 프로그램을 1로 종료합니다.
```
    if (bind(sockfd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        close(sockfd);
        return 1;
    }
```
### 송신자의 주소 정보를 저장할 sender_addr 구조체와 패킷 수신 시 사용할 변수들을 선언합니다.
```
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    Packet packet;
    int expected_seq_num = 0;
```
### "receiver_file"이라는 이름으로 쓰기 모드(wb)로 파일을 엽니다. 실패하 프로그램을 1로 종료합니다
```
    FILE *file = fopen("receiver_file", "wb");
    if (file == NULL) {
        close(sockfd);
        return 1;
    }
```
### 무한 루프에서 recvfrom 함수를 사용하여 소켓에서 패킷을 수신합니다. 수신에 실패하면 다시 루프를 실행합니다
```
    while (1) {
        if (recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr, &addr_len) < 0) {
            continue;
        }
```
### SYN 패킷: "SYN 패킷 수신 완료" 로그를 출력하고 SYN-ACK 패킷을 생성하여 송신자에게 전송합니다.
```
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
```
### ACK 패킷: "ACK 패킷 수신 완료" 로그를 출력합니다.
```
        } else if (packet.flag == 2 && packet.type == 0) {
            log_event("ACK 패킷 수신 완료");
```
### FIN 패킷: "FIN 패킷 수신 완료" 로그를 출력하고, FIN-ACK 패킷을 생성하여 송신자에게 전송하고 루프를 종료합니다.
```
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
```
### DATA 패킷: 수신된 데이터 패킷의 시퀀스 번호를 로그로 출력하고 드랍 확률에 따라 패킷을 드랍할 수 있습니다. 드랍하지 않으면 파일에 패킷의 데이터를 씁니다.
```
        } else if (packet.type == 1) {
            char log_msg[128];
            sprintf(log_msg, "seqNum %d인 DATA 패킷 수신 완료", packet.seqNum);
            log_event(log_msg);

            if (rand() / (double)RAND_MAX < drop_probability) {
                log_event("DATA 패킷 드랍!");
                continue;
            }
```
### 다음 시퀀스 번호를 1 증가시킨 후 ACK 패킷을 생성하여 송신자에게 전송합니다.
```
            if (packet.seqNum == expected_seq_num) {
                fwrite(packet.data, 1, packet.length, file);
                expected_seq_num++;
            }
```
### Packet 구조체를 이용하여 ACK 패킷을 만듭니다. ACK 패킷의 type은 0으로 설정되고, flag는 2로 설정됩니다. seqNum은 수신된 데이터 패킷의 seqNum과 동일하게 설정됩니다. ackNum은 기대하는 다음 시퀀스 번호(expected_seq_num)로 설정됩니다.
### length는 ACK 패킷에는 필요 없으므로 0으로 설정됩니다.
```
            Packet ack;
            ack.type = 0;
            ack.flag = 2;
            ack.seqNum = packet.seqNum;
            ack.ackNum = expected_seq_num;
            ack.length = 0;
```
### sendto 함수를 사용하여 생성한 ACK 패킷을 송신자에게 전송합니다. sockfd는 소켓 파일 디스크립터, &ack는 전송할 데이터의 포인터, sizeof는 전송할 데이터의 크기, 0은 플래그 값으로 일반적으로 0으로 설정합니다.
```
            sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&sender_addr, addr_len);
            sprintf(log_msg, "ackNum %d번 ACK 패킷 전송 완료", ack.ackNum);
            log_event(log_msg);
        }
    }
```
### 수신된 파일을 닫고 네트워크 소켓을 종료합니다.
```
    fclose(file);
    close(sockfd);
    return 0;
}
```
### 사용법
./receiver <수신포트> <데이터손실확률>

<수신포트>: 데이터를 수신하는 포트 번호입니다.
<데이터손실확률>: 데이터를 드랍할 확률입니다.

### 프로그램 작동순서
1. 송신자 포트, 수신자 IP, 수신자 포트, 타임아웃 간격(ms), 파일 이름, ACK 드랍 확률을 설정합니다.
2. 소켓을 생성하고 송신자의 주소에 바인딩합니다.
3. Packet 구조체를 이용하여 SYN 패킷을 생성하고 송신자에게 전송합니다. 송신자로부터 SYN-ACK 패킷을 수신하고 로깅합니다.
4. SYN-ACK 패킷을 받은 후 ACK 패킷을 생성하여 송신자에게 전송합니다.
5. 지정된 파일을 열어 데이터를 읽어 Packet 구조체에 담아 송신자에게 전송합니다. 데이터 전송 시 각 데이터 패킷의 seqNum을 증가시키며 로깅합니다.
6. 데이터 패킷의 ACK를 기다리는 동안 타임아웃이 발생하면 해당 패킷을 재전송합니다.
7. 송신자로부터 ACK 패킷을 수신하면 해당 ACK의 ackNum을 기록하고 모든 데이터 패킷을 송신하면 송신 완료를 로깅하고 파일 전송을 종료합니다.
8. 모든 데이터를 송신한 후 FIN 패킷을 송신자에게 전송하고 송신자로부터 FIN-ACK 패킷을 수신하면 파일 전송이 완료된 것으로 로깅하고 소켓을 닫고 파일을 닫습니다.


#### 컴파일 화면
[https://github.com/torosa1/project2/assets/165176275/16523009-165b-4481-9bda-f60c05978188](https://github.com/torosa1/tcp-reno/assets/165176275/b81f9025-b810-4fbc-aafd-9b4e9f80eede)

#### 송신자 실행 파일
[https://github.com/torosa1/project2/assets/165176275/9146a915-dd51-45cc-9a01-c3128b51a81a](https://github.com/torosa1/tcp-reno/assets/165176275/6e975bee-778a-4169-9874-216b2f6bfe8f)

#### 수신자 실행 파일
[https://github.com/torosa1/project2/assets/165176275/c8456fea-f648-466a-b823-0a4c534b21e4](https://github.com/torosa1/tcp-reno/assets/165176275/6964f110-4e26-41cc-8c42-8b96af6b53ee)

#### 참고 사이트

https://jackpang.tistory.com/23

https://iijw.tistory.com/3

https://hyeo-noo.tistory.com/280
