#include "csapp.h"

// 입력: 정수 → 출력: 16진수
// ntohs() 사용: 네트워크 바이트 순서 → 호스트 바이트 순서
int main(int argc, char **argv)
{   // 전달된 인자의 개수, 전달된 인자들의 문자열 배열
    
    struct in_addr inaddr;      // IP 주소 저장용
    int rc;

    if (argc != 2) {            // 인자 확인
        fprintf(stderr, "usage: %s <network byte order>\n", argv[0]);
        exit(0);
    }

    // 점표기 IP 문자열 → 네트워크 바이트 순서로 변환
    rc = inet_pton(AF_INET, argv[1], &inaddr);
    if (rc == 0)
        app_error("inet_pton error: invalid network byte order");
    else if (rc < 0)
        unix_error("inet_pton error");

    // 네트워크 바이트 순서를 → 호스트 바이트 순서로 변환 후 출력
    printf("0x%x\n", ntohs(inaddr.s_addr));
    exit(0);
}