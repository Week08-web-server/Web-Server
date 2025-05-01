#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port); // 클라이언트 소켓 오픈
    Rio_readinitb(&rio, clientfd);        // IO를 위해 rio에 초기화를 하는 함수??

    while (Fgets(buf, MAXLINE, stdin) != NULL) // 표준입력으로 버퍼에 MAXLINE까지 받아오기
    {
        Rio_writen(clientfd, buf, strlen(buf)); // rio를 사용해서 클라이언트 소켓으로 write
        Rio_readlineb(&rio, buf, MAXLINE);      // 서버가 write 해준것을 버퍼에 read 해오기
        Fputs(buf, stdout);                     // 버퍼값을 표준 출력에 출력하기
    }
    Close(clientfd); // 클라이언트 소켓 닫기
    exit(0);
}