#include "csapp.h"  

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;          // data를 R/W할 때 타입

    if (argc != 3){
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);     
        // buf는 읽어야 할 string, stdout는 file stream이 찍힐 장소를 나타내는 포인터.
    }
    Close(clientfd);
    exit(0);    
}

/*
두 개의 메인 argument를 받는다. server의 hostname과 port number
클라이언트 소켓을 만들고. 
rio_read는 rio_t data를 fd로 초기화한다. 
Fgets는 input을 읽고 buf에 저장한다. 
Rio_writen은 buffer의 content를 소켓을 이용해 서버로 보낸다. 


<rio_t>=====================================================
void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}

indicating that the buffer is empty, 
and the rio_bufptr to point to the start of the buffer.


#define RIO_BUFSIZE 8192

rio_t는 buffer를 나타낸다- I/O
bufptr: buffer의, 다음에 읽어야 할 next byte를 가리키는 포인터.
rio_buf: buffered data에 저장된 문자열
-The rio_t data type is typically used in network programming to simplify input/output operations and to provide robustness against incomplete data transfers.
-fd에서 읽은 데이터로 버퍼가 채워지고, buffer로부터 읽는다. 

typedef struct {
    int rio_fd;                /* Descriptor for this internal buf 
    int rio_cnt;               /* Unread bytes in internal buf 
    char *rio_bufptr;          /* Next unread byte in internal buf 
    char rio_buf[RIO_BUFSIZE];  /* Internal buffer 
} rio_t;
*/


