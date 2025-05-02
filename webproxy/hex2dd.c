#include "csapp.h"

// 입력: 16진수 → 출력: 정수
// htos() 사용: 호스트 바이트 순서 → 네트워크 바이트 순서
int main(int argc, char **argv) 
{   // 전달된 인자의 개수, 전달된 인자들의 문자열 배열

    struct in_addr inaddr;      // 네트워크 바이트 순서 주소
    unit16_t addr;              // 호스트 바이트 순서 주소
    char buf[MAXBUF];           // 점표기 문자열 출력용 버퍼

    if (argc != 2) {            // 인자 확인
        // stdin 표준 입력, stdout 표준 출력, stderr 표준 에러 출력
        fprintf(stderr, "usage: %s <hex number>\n", argv[0]);
        exit(0);
    }

    sscanf(argv[1], "%x", &addr);   // 입력 받은 16진수 문자열을 숫자로 변환
    inaddr.s_addr = htons(addr);    // 네트워크 바이트 순서로 변환
    if (!inet_ntop(AF_INET, &inaddr, buf, MAXBUF))  // IP 문자열로 변환 
        unix_error("inet_ntop");

    printf("%s\n", buf);
    exit(0);                      
}