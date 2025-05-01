/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
void logging();

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  logging();
  printf("adder 진입\n");

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p + 1);
    n1 = atoi(strchr(arg1, '=') + 1);
    n2 = atoi(strchr(arg2, '=') + 1);
  }
  printf("env까지 가져옴\n");

  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);
  sprintf(content + strlen(content), "Welcome to add.com: ");
  sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>",
          n1, n2, n1 + n2);
  sprintf(content + strlen(content), "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Content-type: text/html\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */

void logging()
{
  FILE *logFile = fopen("log.txt", "a"); // "a" 모드: append (뒤에 이어쓰기)
  if (logFile == NULL)
  {
    perror("log.txt 열기 실패");
    return;
  }

  // 현재 시간 가져오기
  time_t now = time(NULL);
  char *timestamp = ctime(&now);           // 문자열 포맷: "Wed May  1 21:32:18 2025\n"
  timestamp[strcspn(timestamp, "\n")] = 0; // 개행 문자 제거

  // 로그 메시지 작성
  fprintf(logFile, "[%s] INFO: adder 프로그램이 시작되었습니다.\n", timestamp);
  fprintf(logFile, "[%s] DEBUG: 디버그 시작\n", timestamp);

  fclose(logFile);
}