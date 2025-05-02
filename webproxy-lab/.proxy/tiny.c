/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include "logger.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트의 정보를 담을 구조체

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 리스닝 소켓 만들기
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,                                    // 요청이 들어오면 connfd에 연결 소켓 번호 넘기기
                    &clientlen);                                                    // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 정보 가져오기
    printf("Accepted connection from (%s, %s)\n", hostname, port);                  // 클라이언트 정보 출력
    doit(connfd);                                                                   // line:netp:tiny:doit
    Close(connfd);                                                                  // line:netp:tiny:close
  }
}

void doit(int fd)
{ // 인자로 소켓 fd 받음
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio; // 입출력을 위한 변수??

  Rio_readinitb(&rio, fd);           // fd를 rio_t 구조체로 감싸고 초기화
  Rio_readlineb(&rio, buf, MAXLINE); // rio를 통해 buf에 read() 해옴
  printf("Request header:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);               // 요청 헤더에서 각각 메소드, uri, version 읽어옴
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) // 오직 GET/HEAD method만 지원함 !! 다른 method가 들어오면 501 응답 리턴
  {                                                            // strcasecmp는 두 문자열이 같으면 0을 반환함 !1
    clienterror(fd, method, "501", "Not implemeted", "Tiny는 해당 메소드를 지원하지 않음 !!");
    return;
  }
  read_requesthdrs(&rio); // 그냥 읽고 무시해버림

  is_static = parse_uri(uri, filename, cgiargs); // CGI 인자 스트링 분석하고,
                                                 // 요청이 어떠한 컨텐츠(정적 or 동적)을 요청하는지 플래그 세움
  if (stat(filename, &sbuf) < 0)                 // 이건 무슨 함수??
  {                                              // 리눅스/유닉스에서 해당 파일이름을 가진 파일이 있는지 검사하는 함수 !!
    clienterror(fd, filename, "404", "Not Found", "Tiny에서 해당 파일 찾을 수 없음 !!");
    return;
  }

  if (is_static)
  { // 정적 컨텐츠 요청일 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    { // 이 파일이 보통 파일이거나 읽기 권한을 가지고 있는지
      clienterror(fd, filename, "403", "Forbidden", "Tiny는 해당 파일을 읽을 수 없음 !!");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method); // 정적 컨텐츠 제공
  }
  else // 동적 컨텐츠 요청일 경우
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 이 파일이 보통 파일이고 실행 가능한가?
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny는 해당 CGI를 실행할 수 없음 !!");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 컨텐츠 제공 -> 내부적으로 fork(), execve() 작동
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) // 에러 메세지 출력
{
  char buf[MAXLINE], body[MAXBUF];

  // HTTP 응답 바디
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=ffffff>\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);  // 에러 코드
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // long msg 출력
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP 응답
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 에러 응답 헤더
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n"); // 컨텐츠 타입 명시
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); // 컨텐츠 길이 명시
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp) // 요청 헤더 읽고 무시하기??
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); // 첫 줄 읽기
  while (strcmp(buf, "\r\n"))
  {
    printf("%s", buf);               // 🔁 이 줄 먼저 출력해야 함!
    Rio_readlineb(rp, buf, MAXLINE); // 그다음 줄로 이동
  }
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) // cgi-bin 내에 있는 파일이 아닐 경우
  // strstr은 문자열 안에서 다른 부분 문자열을 찾음 !!
  // 따라서 uri에 cgi-bin이 있는지 없는지 검사하는것
  {
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/') // 만약 uri의 끝이 '/'로 끝난다면
      strcat(filename, "home.html"); // home.html을 제공 -> index.html과 같다
    return 1;
  }
  else
  {                        // 만약 uri 내에 cgi-bin이 있다면
    ptr = index(uri, '?'); // 인자의 위치 찾기 위해 '?'의 위치 찾음
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // '?'의 다음 위치의 문자열 (인자)를 cgiargs에 복사
      *ptr = '\0';              // WTF
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, "."); // .filename으로 만듬 -> 왜? 현재 디렉토리에서 찾아야 하니까
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  rio_t rio;

  // HTTP 응답 제작
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // HTTP 응답 전송
  printf("Response headers:\n");
  printf("%s", buf);

  if (strcasecmp(method, "GET") == 0) // GET method일 경우에만
  {
    srcfd = Open(filename, O_RDONLY, 0); // 인자로 받은 정적파일 이름으로 읽기 전용으로 읽어서 파일 포인터 만듬
    Rio_readinitb(&rio, srcfd);
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 위에서 연 파일을 메모리에 매핑하고, 해당 매핑 영역의 시작 주소 반환
    // Close(srcfd);                                               // 파일 포인터 닫기 -> 안하면 메모리 누수
    // Rio_writen(fd, srcp, filesize);                             // srcp 시작 주소부터 filesize 만큼을 클라이언트에게 전달
    // Munmap(srcp, filesize);                                     // 매핑된 메모리 해제
    srcp = (char *)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Free(srcp);
  }
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")) // 만약 파일네임에 .html이 포함될 경우
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")) // 파일네임에 .gif가 포함될 경우
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png")) // 파일네임에 .png가 포함될 경우
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg")) // 파일네임에 .jpg가 포함될 경우
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  // HTTP 응답 만들기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  printf("filename = %s\n", filename);
  printf("cgiargs = %s\n", cgiargs);
  if (Fork() == 0) // 자식 프로세스일 경우
  {
    setenv("QUERY_STRING", cgiargs, 1); // 환경변수 QUERY_STRING에 인자 저장
    Dup2(fd, STDOUT_FILENO);            // 표준출력을 넘겨받은 소켓 fd로 덮어쓰기
    log_message("자식 프로세스 진입");
    Execve(filename, emptylist, environ); // 넘겨받은 파일네임으로 자식 프로세스에서 프로그램 실행
  }
  Wait(NULL);
}