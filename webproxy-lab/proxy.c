#include "csapp.h"
#include "tiny/logger.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_THREADS 10
#define QUEUE_SIZE 16

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct cache_content
{
  char uri[MAXLINE];          // 비교할 uri
  char *content;              // 실제 웹 오브젝트
  int size;                   // 웹 오브젝트 사이즈
  time_t last_used;           // 마지막으로 참조한 날
  struct cache_content *next; // 다음 엔트리
  sem_t lock;                 // 각 엔트리마다 세마포어
} CACHE;

typedef struct
{
  int buf[QUEUE_SIZE];
  int front;
  int rear;
  int count;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
} conn_queue_t;

sem_t thread_lock, cache_size_lock;
CACHE *cache_head;
int CUR_CACHE_SIZE = 0;
conn_queue_t queue;

void process(int fd);
void read_requesthdrs(rio_t *rp);
void parse_path_from_uri(const char *uri, char *path);
void *thread(void *vargp);
void cache_init();
CACHE *cache_find(char *uri);
void cache_store(char *uri, char *data, int size);
CACHE *cache_evict();
void queue_init(conn_queue_t *q);
void queue_push(conn_queue_t *q, int connfd);
int queue_pop(conn_queue_t *q);
void *worker(void *arg);

int main(int argc, char **argv)
{
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트의 정보를 담을 구조체

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return;
  }

  listenfd = Open_listenfd(argv[1]);      // 리스닝 소켓 오픈
  sem_init(&thread_lock, 0, MAX_THREADS); // 스레드 세마포어 초기화
  sem_init(&cache_size_lock, 0, 1);       // 캐시 사이즈 락 초기화
  cache_init();                           // 캐시 초기화
  queue_init(&queue);

  // 스레드 풀 생성
  pthread_t tid[MAX_THREADS];
  for (int i = 0; i < MAX_THREADS; ++i)
    pthread_create(&tid[i], NULL, worker, NULL);

  while (1)
  {
    clientlen = sizeof(clientaddr);
    int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

    getnameinfo((struct sockaddr *)&clientaddr, clientlen,
                hostname, MAXLINE, port, MAXLINE, 0);
    printf("현재 (%s, %s)에서 접속 중입니다\n", hostname, port);

    queue_push(&queue, connfd);
  }

  sem_destroy(&thread_lock); // 세마포어 삭제 -> 딱히 필요없지만 습관을 위해

  return 0;
}

void process(int fd)
{
  int clntfd = -1; // 연결용 fd, 캐시 히트 시 -1 유지
  int total_size = 0;

  char hostname[MAXLINE], port[MAXLINE];
  char buf[MAXBUF], version[MAXLINE], method[MAXLINE], uri[MAXLINE];
  char send[MAXBUF], path[MAXBUF];
  char *response = malloc(MAX_OBJECT_SIZE); // 동적 할당으로 스택 오버플로 방지
  rio_t rio;
  CACHE *cache;

  printf("process on\n");

  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
  {
    free(response);
    return; // 연결 종료 시 조기 반환
  }

  sscanf(buf, "%s %s %s", method, uri, version);
  printf("%s\n", buf);
  parse_path_from_uri(uri, path);

  read_requesthdrs_and_extract_host(&rio, hostname, port);
  // log_message("Hostname: %s, Port: %s\n", hostname, port);

  cache = cache_find(uri);
  if (!cache)
  {
    clntfd = Open_clientfd(hostname, port);
    if (clntfd < 0)
    {
      fprintf(stderr, "Connection to server failed: %s:%s\n", hostname, port);
      free(response);
      return;
    }

    snprintf(send, MAXBUF,
             "%s %s %s\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "%s\r\n",
             method, path, version, hostname, user_agent_hdr);

    Rio_writen(clntfd, send, strlen(send));

    ssize_t n;
    while ((n = read(clntfd, buf, MAXBUF)) > 0)
    {
      Rio_writen(fd, buf, n);
      if (total_size + n <= MAX_OBJECT_SIZE)
        memcpy(response + total_size, buf, n);
      total_size += n;
    }

    if (total_size <= MAX_OBJECT_SIZE)
    {
      cache_store(uri, response, total_size);
    }
  }
  else
  {
    P(&cache->lock);
    Rio_writen(fd, cache->content, cache->size);
    V(&cache->lock);
  }

  printf("%s 에게 전송 완료\n", hostname);
  if (clntfd >= 0)
    Close(clntfd);
  free(response);
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
      while (*hostport == ' ') // 현재 공백을 가리키고 있으면
        hostport++;            // 앞으로 이동하면서  공백 제거

      char *colon_ptr = strchr(hostport, ':'); // ':'의 위치 찾기
      if (colon_ptr)                           // ':' 이 있으면
      {
        *colon_ptr = '\0';                  // 해당 자리를 null로 만들기
        strcpy(hostname, hostport);         // strcpy는 null 까지 복사하므로 위 코드에서 null을 만들어주는것 !!
        strcpy(port, colon_ptr + 1);        // 그 다음 위치부터 끝까지 복사
        port[strcspn(port, "\r\n")] = '\0'; // 개행 제거
      }
      else // ':'가 없을 경우 -> 80포트 요청
      {
        strcpy(hostname, hostport);                 // 문자열 전부가 호스트네임
        hostname[strcspn(hostname, "\r\n")] = '\0'; // 개행 제거
        strcpy(port, "80");                         // 포트는 80포트
      }
    }
  }
}

void parse_path_from_uri(const char *uri, char *path)
{
  const char *start = strstr(uri, "://"); // "://" 가 처음 나타나는 위치 찾기 (http://)

  if (start == NULL) // 만약 없으면
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

  if (path_start) // '/' 가 있으면
  {
    strncpy(path, path_start, MAXLINE - 1);
    path[MAXLINE - 1] = '\0'; // 끝 문자 삽입
  }
  else
  {
    strcpy(path, "/"); // 경로가 없으면 기본 "/"
  }
}

// void *thread(void *vargp)
// {
//   int connfd = *(int *)vargp;
//   free(vargp); // 메모리 해제

//   // 세마포어 wait: 동시 처리 수 감소
//   P(&thread_lock);

//   process(connfd); // 프로세스 처리
//   close(connfd);   // 처리 후 닫기

//   // 처리 완료 후 자원 반환
//   V(&thread_lock);

//   return NULL;
// }

void cache_init()
{
  cache_head = NULL;
  CACHE *cur_cache = NULL;
  for (int i = 0; i < 10; i++) // 캐시 엔트리 10개 만들어두기
  {
    CACHE *new_cache = Malloc(sizeof(CACHE)); // 새로운 엔트리 초기화 작업
    sem_init(&new_cache->lock, 0, 1);         // 엔트리 세마포어 초기화
    new_cache->content = NULL;
    new_cache->last_used = NULL;
    new_cache->next = NULL,
    new_cache->size = -1; // 사이즈가 0보다 작으면 빈 캐시 !!
    new_cache->uri[0] = '\0';
    if (!cache_head) // 헤드 설정
    {
      cache_head = new_cache; // 헤드 설정
      cur_cache = new_cache;
    }
    else // 연결리스트 설정
    {
      cur_cache->next = new_cache;
      cur_cache = new_cache;
    }
  }
}

void cache_store(char *uri, char *data, int size)
{
  P(&cache_size_lock);
  if (MAX_CACHE_SIZE - CUR_CACHE_SIZE >= size) // 캐시가 저장될 공간이 충분하면
  {
    CACHE *cur;
    for (cur = cache_head; cur != NULL; cur = cur->next)
    {
      P(&cur->lock);     // 락을 걸면서 찾기
      if (cur->size < 0) // 빈 엔트리를 찾았다면
        break;
      V(&cur->lock); // 락 해제, 만약 위 break 문제에서 탈출했다면 아직 락 걸려있음
    }
    if (!cur) // 캐시 리스트에 빈 공간이 없으면
    {
      CACHE *new_cache = Malloc(sizeof(CACHE)); // 새로운 캐시 엔트리 만들기
      sem_init(&new_cache->lock, 0, 1);
      P(&new_cache->lock);
      new_cache->content = Malloc(size);
      memcpy(new_cache->content, data, size);
      new_cache->last_used = time(NULL);
      new_cache->next = cache_head;
      new_cache->size = size;
      memcpy(new_cache->uri, uri, strlen(uri) + 1);
      cache_head = new_cache;
      V(&new_cache->lock);
    }
    else // 캐시 리스트의 빈 공간을 찾았을 경우
    {
      cur->content = Malloc(size);            // 컨텐츠에 동적 할당
      memcpy(cur->content, data, size);       // 동적 할당된 공간에 데이터 복사
      cur->last_used = time(NULL);            // 현재 시간 할당
      cur->size = size;                       // 캐시 엔트리의 사이즈 저장
      memcpy(cur->uri, uri, strlen(uri) + 1); // 캐시 엔트리의 uri 저장
      V(&cur->lock);                          // 빈 엔트리의 락 반환
    }
  }
  else // 캐시에 저장 공간이 없으면
  {
    CACHE *victim = cache_evict();       // 희생자 엔트리를 찾고
    CUR_CACHE_SIZE -= victim->size;      // 현재 총 캐시 사이즈를 희생자 엔트리만큼 줄이기
    free(victim->content);               // 희생자 엔트리의 컨텐츠 해제
    victim->content = Malloc(size);      // 희생자 엔트리에 덮어씌우기 위해 다시 할당
    memcpy(victim->content, data, size); // 엔트리에 데이터 덮어쓰기

    victim->size = size;                       // 사이즈 재할당
    victim->last_used = time(NULL);            // 마지막 사용 시간 다시 할당
    victim->uri[0] = '\0';                     // uri도 초기화
    memcpy(victim->uri, uri, strlen(uri) + 1); // uri 덮어쓰기
    V(&victim->lock);                          // 희생자 캐시의 락 해제
  }
  CUR_CACHE_SIZE += size; // 현재 총 캐시 사이즈를 데이터 사이즈만큼 추가
  V(&cache_size_lock);    // 캐시 사이즈 락 해제
}

CACHE *cache_evict()
{
  CACHE *victim = NULL;
  time_t oldest = time(NULL); // 현재 시간(최신)

  for (CACHE *cur = cache_head; cur != NULL; cur = cur->next)
  {
    P(&cur->lock);               // 일단 현재 엔트리에 락을 걸고
    if (cur->last_used < oldest) // 더 오래 전에 사용했었으면
    {
      if (victim != NULL)      // 이전에 찾아두었던 희생자의 락을
        V(&victim->lock);      // 넘기기 전에 풀기
      victim = cur;            // 희생자 엔트리를 현재로
      oldest = cur->last_used; // 비교 시간을 현재 엔트리 것으로
    }
    else // 현재 엔트리가 더 최신이면
    {
      V(&cur->lock); // 아무것도 안하고 락만 풀기 -> 다른 스레드도 써야하니까
    }
  }

  return victim; // 희생자 엔트리 반환
}

CACHE *cache_find(char *uri)
{
  CACHE *cur;

  for (cur = cache_head; cur != NULL; cur = cur->next)
  {
    P(&cur->lock); // 일단 락 걸고
    if (strcmp(uri, cur->uri) == 0)
    {
      V(&cur->lock);               // 락 해제한 후에
      cur->last_used = time(NULL); // 이 엔트리는 지금 사용했다고 갱신
      return cur;                  // 포인터 넘겨주기
    }
    V(&cur->lock); // 다음 엔트리 순회 전에 락 풀기
  }

  return cur;
}

void queue_init(conn_queue_t *q)
{
  q->front = q->rear = q->count = 0;
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->not_empty, NULL);
  pthread_cond_init(&q->not_full, NULL);
}

void queue_push(conn_queue_t *q, int connfd)
{
  pthread_mutex_lock(&q->lock);
  while (q->count == QUEUE_SIZE)
    pthread_cond_wait(&q->not_full, &q->lock);

  q->buf[q->rear] = connfd;
  q->rear = (q->rear + 1) % QUEUE_SIZE;
  q->count++;

  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
}

int queue_pop(conn_queue_t *q)
{
  pthread_mutex_lock(&q->lock);
  while (q->count == 0)
    pthread_cond_wait(&q->not_empty, &q->lock);

  int connfd = q->buf[q->front];
  q->front = (q->front + 1) % QUEUE_SIZE;
  q->count--;

  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->lock);
  return connfd;
}

void *worker(void *arg)
{
  while (1)
  {
    int connfd = queue_pop(&queue);
    process(connfd);
    close(connfd);
  }
  return NULL;
}