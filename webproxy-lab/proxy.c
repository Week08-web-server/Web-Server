#include "csapp.h"
#include "tiny/logger.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void process(int fd);
void get_host_and_port(char *host_header, char *hostname, char *port);
void make_response(char *send, char *content);
void read_requesthdrs(rio_t *rp);
void parse_path_from_uri(const char *uri, char *path);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트의 정보를 담을 구조체

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return;
  }

  listenfd = Open_listenfd(argv[1]); // 리스닝 소켓 오픈
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 커넥트 소켓 오픈
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("현재 (%s, %s)에서 접속 중입니다\n", hostname, port);
    process(connfd);
    Close(connfd);
  }

  return 0;
}

void process(int fd) // 클라이언트 소켓을 통해 작업 수행
{
  int clntfd;
  char hostname[MAXLINE], port[MAXLINE], buf[MAXBUF], version[MAXLINE], method[MAXLINE], uri[MAXLINE];
  char send[MAXBUF], path[MAXBUF];
  struct addrinfo hints, *res;
  rio_t rio;
  // uri정보를 받아서 파싱한 다음 서버랑 프록시로 맺어주기
  // Host:port 정보가 필요함 (hostname, port)
  // 그 경로로 똑같은 정보를 요청해서 받은 응답을 클라이언트로 그대로 보내주기
  // 그럼 일단 서버랑 연결해줘야 하니까 일단 커넥트를 해야할듯?

  printf("process on\n");
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE); // 첫 줄 읽기 → method, uri, version
  sscanf(buf, "%s %s %s", method, uri, version);
  printf("%s\n", buf);
  parse_path_from_uri(uri, path);

  // 헤더 읽기 + host/port 추출
  read_requesthdrs_and_extract_host(&rio, hostname, port);

  char log[MAXLINE];

  sprintf(log, "Hostname : %s, port : %s\n", hostname, port);
  log_message(log);

  clntfd = Open_clientfd(hostname, port); // 호스트와 포트로 연결 요청
  // 연결 성공 후 메소드와 도메인, 버전을 그대로 다시 보내주기
  memset(send, 0, sizeof(send));                        // 초기화
  sprintf(send, "%s %s %s\r\n", method, path, version); // send 메세지 생성
  sprintf(send, "%sHost: %s\r\n", send, hostname);      // 호스트 헤더 명시
  sprintf(send, "%sConnection: close\r\n\r\n", send);   // 연결 끊어주기

  Rio_writen(clntfd, send, strlen(send)); // send 메세지 보내기
  ssize_t n;
  while ((n = read(clntfd, buf, MAXBUF)) > 0) // 응답을 스트림으로 읽어보면서
  {
    Rio_writen(fd, buf, n); // 받은 응답을 바로 한줄씩 브라우저(클라이언트)에게 보내기
  }
  printf("%s 에게 전송 완료\n", hostname);
  Close(clntfd);
}

void get_host_and_port(char *host_header, char *hostname, char *port)
{
  char buf[MAXBUF];
  strncpy(buf, host_header, MAXBUF - 1);
  buf[MAXBUF - 1] = '\0'; // 널 종료 보장

  if (strncmp(buf, "Host:", 5) != 0)
  {
    fprintf(stderr, "Not a Host header\n");
    exit(1);
  }

  char *hostport = buf + 5;
  while (*hostport == ' ')
    hostport++; // 공백 제거

  char *colon_ptr = strchr(hostport, ':');
  if (colon_ptr)
  {
    *colon_ptr = '\0'; // 포인터가 가리키는 문자를 null로 변경
    strcpy(hostname, hostport);
    strcpy(port, colon_ptr + 1);
  }
  else // 포트 번호가 구분되어있지 않으면
  {
    strcpy(hostname, hostport);
    strcpy(port, "80"); // 80 포트로 기본 설정
  }
}

void read_requesthdrs_and_extract_host(rio_t *rp, char *hostname, char *port)
{
  char buf[MAXLINE];

  while (Rio_readlineb(rp, buf, MAXLINE) > 0)
  {
    if (strcmp(buf, "\r\n") == 0)
      break; // 헤더 종료

    // Host 헤더 파싱
    if (strncasecmp(buf, "Host:", 5) == 0)
    {
      char *hostport = buf + 5;
      while (*hostport == ' ')
        hostport++; // 공백 제거

      char *colon_ptr = strchr(hostport, ':');
      if (colon_ptr)
      {
        *colon_ptr = '\0';
        strcpy(hostname, hostport);
        strcpy(port, colon_ptr + 1);
        port[strcspn(port, "\r\n")] = '\0'; // 개행 제거
      }
      else
      {
        strcpy(hostname, hostport);
        hostname[strcspn(hostname, "\r\n")] = '\0';
        strcpy(port, "80");
      }
    }
  }
}

void parse_path_from_uri(const char *uri, char *path)
{
  const char *start = strstr(uri, "://");

  if (start == NULL)
  {
    // "://" 없으면 잘못된 형식 → uri 자체를 경로로 처리
    if (uri[0] == '/')
    {
      strncpy(path, uri, MAXLINE - 1);
      path[MAXLINE - 1] = '\0';
    }
    else
    {
      // 예외 처리: 그냥 루트로
      strcpy(path, "/");
    }
    return;
  }

  start += 3; // "://" 건너뛰기
  // start: "localhost:8080/index.html" 상태

  // '/'를 찾아 path 시작 위치 지정
  const char *path_start = strchr(start, '/');

  if (path_start)
  {
    strncpy(path, path_start, MAXLINE - 1);
    path[MAXLINE - 1] = '\0';
  }
  else
  {
    strcpy(path, "/"); // 경로가 없으면 기본 "/"
  }
}
