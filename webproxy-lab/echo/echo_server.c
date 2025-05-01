#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // 모든 형태의 소켓 주소를 저장하기에 충분히 큰 구조체?
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2)
    {
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]); // 1번 인자로 받은 포트로 리스닝 소켓 열기
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                                     // SA == sockaddr, 클라이언트의 정보를 clinetaddr에 저장함
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // 해당 연결 소켓의 호스트와 포트 가져오기
        printf("Connected to (%s, %s)\n", client_hostname, client_port);                              // 호스트와 포트 출력
        echo(connfd);                                                                                 // read한 문자열을 그대로 write 해주기
        Close(connfd);                                                                                // 클라이언트 소켓 닫기
    }

    exit(0);
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);                         // 초기화 작업
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) // read 해오기
    {
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n); // write 해주기
    }
}