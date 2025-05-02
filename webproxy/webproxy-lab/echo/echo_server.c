#include <stdio.h>
#include "csapp.h"

int open_listenfd(char *port);
void echo(int connfd);

/* ==========================================
 * 에코 서버 메인 루프 / Echo Server Main Loop
 * ========================================== */
/*
 * main - 지정한 포트로 클라이언트 연결을 수락하고, 에코 처리 수행
 * 1. 명령행 인자로 포트 번호 입력 받음
 * 2. 리스닝 소켓 생성
 * 3. 반복적으로 클라이언트 연결 수락 → 정보 출력 → echo 처리 → 연결 종료
 */
int main(int argc, char **argv) {
    int listenfd, connfd;   // listenfd: 클라이언트 연결 요청을 수신하는 리스닝 소켓
                            // connfd: 연결이 수락된 후 실제 데이터 통신에 사용되는 소켓
    socklen_t clientlen;    // 클라이언트 주소 구조체의 크기 (accept 시 필요)
    struct sockaddr_storage clientaddr;     // 클라이언트 주소 정보 저장용 (IPv4, IPv6 모두 대응)
    char client_hostname[MAXLINE], client_port[MAXLINE];    // 연결된 클라이언트의 호스트명, 포트 번호 저장

    // [1] 포트 번호 인자 확인: 포트 번호 하나만 입력받아야 함
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);    // 인자가 부족하면 종료
    }

    // [2] 리스닝 소켓 열기: 주어진 포트로 수신용 서버 소켓 생성
    listenfd = open_listenfd(argv[1]);

    // [3] 클라이언트 연결 수락 및 처리 루프
    while (1) {
        clientlen = sizeof(struct sockaddr_storage); // 주소 구조체 크기 설정 초기화
        // [3-1] 클라이언트 연결 수락 → 새로운 통신용 소켓 생성
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // [3-2] 클라이언트 주소 정보 출력 → 클라이언트의 주소 정보를 사람이 읽을 수 있는 문자열로 변환
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, 
                    MAXLINE, client_port, MAXLINE, 0);
        // 연결된 클라이언트 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);   // [3-3] 에코 처리 함수 호출: 클라이언트와 데이터 송수신 처리 
        Close(connfd);  // [3-4] 연결 종료
    }
    exit(0);    // 정상 종료 (사실상 도달하지 않으며 형식적으로 존재하는 코드임)
}

/* ==========================================
 * 서버 리스닝 소켓 열기 / Open Listening Socket
 * ========================================== */
/*
 * open_listenfd - 주어진 포트 번호로 서버용 리스닝 소켓을 생성
 * 1. getaddrinfo로 바인딩 가능한 주소 리스트를 얻음
 * 2. 각 주소에 대해 socket 생성 → bind 시도
 * 3. bind에 성공하면 listen 상태로 설정
 * 4. 실패 시 -1 반환
 */
int open_listenfd(char *port) {
    struct addrinfo hints, *listp, *p;  // 주소 정보 구조체 및 결과 리스트 포인터
    int listenfd, optval = 1;           // 리스닝 소켓 fd, 소켓 옵션 값

    // [1] 주소 요청용 hints 구조체 초기화
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;    // TCP 스트림 소켓을 사용하겠다고 명시
    hints.ai_flags = AI_NUMERICSERV;    // 포트 번호가 숫자임을 명시 (예: "80"은 숫자이므로 DNS 조회 필요 없음)
    hints.ai_flags |= AI_ADDRCONFIG;    // 현재 시스템이 사용 중인 네트워크 주소 체계만 사용 → 실제 사용 가능한 주소만 받아 연결 실패 확률을 줄이고 성능을 높임
    
    // [2] 포트 번호에 해당하는 주소 리스트 요청 (host는 NULL → 로컬)
    Getaddrinfo(NULL, port, &hints, &listp);

    // [3] 주소 리스트 순회하며 socket 생성 + bind 시도
    for (p = listp; p != NULL; p = p->ai_next) {

        // 소켓 생성 시도
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;       // 소켓 생성 실패 시, 다음 주소로 넘어감
        }

        // 포트 재사용 가능 옵션 설정 (TIME_WAIT 상태에서도 재바인딩 가능)
        // 서버 재시작 시, "Address already in use" 방지
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

        // 바인딩 시도 (서버 소켓에 IP/포트 연결)
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;          // 바인딩 성공 시 루프 탈출
        }
        
        Close(listenfd);    // 바인딩 실패 시 소켓 닫고 다음 주소로
    }

    Freeaddrinfo(listp);    // [4] 주소 정보 메모리 해제

    if (p == NULL) {        // [5] 연결 가능한 주소가 없었다면 실패 반환
        return -1;
    }

    // [6] 리스닝 소켓 상태로 설정 (= 클라이언트의 연결 요청을 대기 상태로 설정)
    if (listen(listenfd, LISTENQ) < 0) {
        Close(listenfd);    // 실패 시 소켓 닫고 -1 반환
        return -1;
    }

    // [7] 성공 시, 클라이언트 연결을 받을 수 있는 리스닝 소켓 디스크립터 반환
    return listenfd;
}

/* ==========================================
 * 에코 처리 함수 / Echo Handler
 * ========================================== */
/*
 * echo - 클라이언트로부터 받은 데이터를 그대로 다시 전송
 * 1. Robust I/O 초기화
 * 2. 한 줄씩 읽어와 출력 후, 동일한 내용을 클라이언트에 다시 전송
 * 3. 입력이 끝나면 루프 종료
 */
void echo(int connfd) {
    size_t n;               // 읽어온 바이트 수 
    char buf[MAXLINE];      // 입력/출력 버퍼
    rio_t rio;              // Robust I/O 버퍼 구조체

    // [1] Robust I/O 구조체 초기화 (connfd와 연결)
    Rio_readinitb(&rio, connfd);

    // [2] 클라이언트로부터 한 줄씩 읽어 반복 처리
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", (int)n);   // 수신 로그 출력
        Rio_writen(connfd, buf, n);                     // 수신 내용을 그대로 다시 전송
    }
}