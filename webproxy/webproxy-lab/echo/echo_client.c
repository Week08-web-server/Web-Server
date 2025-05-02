#include <stdio.h>
#include "csapp.h"

int open_clientfd(char *hostname, char*port);

/* ==========================================
 * 에코 클라이언트 / Echo Client
 * ========================================== */
/*
 * main - 서버에 연결해 사용자 입력을 전송하고, 응답을 받아 출력
 * 1. 인자를 통해 <host> <port> 정보를 입력받음
 * 2. 서버와 연결할 클라이언트 소켓 생성
 * 3. Robust I/O 초기화
 * 4. 사용자 입력을 서버에 보내고 응답을 출력하는 루프 수행
 */
int main(int argc, char **argv) {
    int clientfd;                       // 서버와 연결된 소켓 파일 디스크립터
    char *host, *port, buf[MAXLINE];    // 서버 주소, 포트 번호, 메시지 저장 버퍼
    rio_t rio;                          // Robust I/O를 위한 버퍼 구조체 (CSAPP 제공)

    // [1] 인자 개수 확인: 실행 시 반드시 <host> <port> 두 인자를 받아야 함
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);    // 인자 부족 시 프로그램 종료
    }

    // [2] 명령행 인자로부터 서버 주소와 포트 번호 가져오기
    host = argv[1];     // 예: "localhost" 또는 "127.0.0.1"
    port = argv[2];     // 예: "8080"

    // [3] 클라이언트 소켓을 열고 서버에 연결
    clientfd = open_clientfd(host, port);

    // [4] 안정적인 입출력을 위한 rio 버퍼 초기화
    Rio_readinitb(&rio, clientfd);

    // [5] 사용자 입력 → 서버 전송 → 서버 응답 → 출력
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); // 입력한 문자열을 서버로 전송
        Rio_readlineb(&rio, buf, MAXLINE);      // 서버로부터 한 줄 응답 수신
        Fputs(buf, stdout);                     // 응답을 화면에 출력
    }

    // [6] 통신 종료 후 소켓 닫기
    Close(clientfd);
    exit(0);
}

/* ==========================================
 * 클라이언트 소켓 연결 / Open Client Socket
 * ========================================== */
/*
 * open_clientfd - hostname과 port를 기반으로 서버에 TCP 연결을 시도
 * 1. getaddrinfo로 주소 정보 리스트를 얻음
 * 2. 각 주소에 대해 소켓 생성 후 connect 시도
 * 3. 연결 성공 시 해당 소켓 디스크립터 반환
 * 4. 모두 실패할 경우 -1 반환
 */
int open_clientfd(char *hostname, char*port) {
    int clientfd;                       // 최종적으로 반환할 클라이언트 측 소켓 파일 디스크립터
    struct addrinfo hints, *listp, *p;  // 주소 정보를 위한 구조체 및 결과 리스트 포인터들

    // [1] 주소 요청용 hints 구조체 초기화
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;    // TCP 스트림 소켓을 사용하겠다고 명시
    hints.ai_flags = AI_NUMERICSERV;    // 포트 번호가 숫자임을 명시 (예: "80"은 숫자이므로 DNS 조회 필요 없음)
    hints.ai_flags |= AI_ADDRCONFIG;    // 현재 시스템이 사용 중인 네트워크 주소 체계만 사용 → 실제 사용 가능한 주소만 받아 연결 실패 확률을 줄이고 성능을 높임
    
    // [2] 포트 번호에 해당하는 주소 리스트 요청 (host는 NULL → 로컬)
    // hostname과 port에 해당하는 주소 정보 리스트 가져오기 → listp는 여러 개의 주소 정보를 가리키는 연결 리스트의 시작점이 됨
    Getaddrinfo(hostname, port, &hints, &listp);

    // [3] 주소 리스트 순회하며 socket 생성 + connect 시도
    for (p = listp; p != NULL; p = p->ai_next) {

        // 현재 주소 정보에 맞는 소켓 생성 시도 (주소 체계, 소켓 타입, 프로토콜)
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;       // 소켓 생성 실패 시, 다음 주소로 넘어감
        }

        // 생성한 소켓으로 서버에 연결 시도
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) {
            break;          // 연결 선공 → 루프 탈출
        }

        Close(clientfd);    // 연결 실패한 경우 → 소켓을 닫고 다음 주소로 이동
    }

    Freeaddrinfo(listp);    // [4] 주소 정보 해제 (메모리 반환)

    if (p == NULL) {        // [5] 연결 성공 여부 확인
        return -1;          // 모든 주소에서 연결 실패
    }
    return clientfd;        // 연결 성공한 경우 → 해당 소켓 파일 디스크립터 반환
}