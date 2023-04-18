// csapp.c 내용
#include "csapp.h"

int open_clientfd(char *hostname, char *port)
{
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;      // 초기화되지 않은 변수 선언

    /* Get a list of "potential" server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));     // 초기화

    // 내가 찾고자 하는 서버(service) IP:port 값을 넣음
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections */

    // getaddrinfo는 addrinfo 구조체 연결리스트를 가리키는 포인터를 **result로 반환 (성공하면 0)
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));           // fprintf: 성공적으로 write한 문자의 개수 리턴. 바로 write하지 않고 버퍼에 쌓음.
        return -2;
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {        // ai_next는 다음 IP주소 리스트
        /* Create a socket descriptor - 성공하면 변수에 할당됨 */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;  // Socket faild

        /* Connect to server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
            break; /* Success */
        if (close(clientfd) < 0) { /* Connect failed, try another */  
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p)
        return -1;
    else
        return clientfd;
}
