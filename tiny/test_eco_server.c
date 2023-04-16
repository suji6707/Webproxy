#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; 
    char client_hostname[MAXLINE], client_port[MAXLINE];

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&cilentaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        close(connfd);
    }
    exit(0);
}


void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("Server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
}

/* 핵심
- Open_listenfd로 bind할 수 있는 서버 소켓주소를 찾으면, 그걸 fd로 두고. 
- Accept로 connfd가 생성. 
- echo()는 인자로 받은 connfd를 rio_t 구조체로 두고, 그걸 읽어서 그대로 써준다. 

*/

/* 에코 서버 */
// socket으로 listenfd 생성. 맞는 것 찾으면 bind, listen
// accept로 connfd 생성. 
// connfd에 있는 내용을 읽어와서 씀. 그걸 클라가 받아서 읽고 씀. 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAXLINE 1024

int main(int argc, char *argv[]) {
    int listenfd, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t cliaddr_len;
    char buf[MAXLINE];
    int n;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8080);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind error");
        exit(1);
    }

    if (listen(listenfd, 10) < 0) {
        perror("listen error");
        exit(1);
    }

    printf("Listening on port 8080...\n");

    while (1) {
        cliaddr_len = sizeof(cliaddr);
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len)) < 0) {
            perror("accept error");
            exit(1);
        }

        printf("Connection from %s, port %d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        while ((n = read(connfd, buf, MAXLINE)) > 0) {
            if (write(connfd, buf, n) < 0) {
                perror("write error");
                exit(1);
            }
        }

        if (n < 0) {
            perror("read error");
            exit(1);
        }

        close(connfd);
    }

    return 0;
}