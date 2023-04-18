#include "csapp.h"

void echo(int connfd);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
}

/* Thread routine*/
void *thread(void *vargp)
{
    int connfd = *((int *)vargp);       // 주소가 아니라 값을 할당했음
    Pthread_detach(pthread_self());     // 자체 식별자 부여 후 detach로 작업처리
    Free(vargp);                        // 기존 메모리 공간은 해제
    echo(connfd);                       // 할당받아놓은 값으로 echo 처리
    Close(connfd);                      // 완료 후 닫음(메모리 누수 피해야)
    return NULL;
}


