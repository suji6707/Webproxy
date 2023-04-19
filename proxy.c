#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";


void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void do_request(char *clientfd, char *method, char *path, char *host);
void do_response(int connfd, int clientfd);
void sigchld_handler(int sig);

int main(int argc,char **argv)
{
    int listenfd, *connfdp;
    socklen_t  clientlen;
    char hostname[MAXLINE],port[MAXLINE];
    pthread_t tid;
    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    Signal(SIGCHLD, sigchld_handler);

    listenfd = Open_listenfd(argv[1]);
    printf("%d\n", listenfd);

    while(1){
        clientlen = sizeof(clientaddr);

        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);  /* Accept는 연결요청이 listenfd에 도착하면 addr에 클라이언트 주소를 채우고, SA* 구조체로 type casting */
        printf("%p %d\n", connfdp, *connfdp);

        /* SA(소켓주소) -> hostname, port */
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        /* print accepted message */
        printf("Accepted connection from (%s %s).\n",hostname,port);

        Pthread_create(&tid, NULL, thread, (void *)connfdp);
        /*sequential handle the client transaction*/
    }
    return 0;
}


void *thread(void *vargp){
    int connfd = *((int *)vargp);       
    Pthread_detach(pthread_self());     // main <-> peer thread 분리
    Free(vargp);                        // main에서 받은 메모리공간 해제
    
    doit(connfd);
    Close(connfd);
}


void sigchld_handler(int sig)
{ 
    while (waitpid(-1, 0, WNOHANG) > 0)
        ;
    return;
}

  /* 
  Before Parsing (Client로부터 받은 Request Line)
  => GET http://www.google.com:80/index.html HTTP/1.1
  Result Parsing
  => host = www.google.com
  => port = 8080
  => path = /index.html
  After Parsing (Server로 보낼 Request Line)
  => GET /index.html HTTP/1.0 
  */ 
 
/* thread 루틴. handle the client HTTP transaction*/
void doit(int connfd)
{
    int clientfd;
    int port;
    char buf[MAXLINE],
         host[MAXLINE],
         method[MAXLINE],
         uri[MAXLINE],
         version[MAXLINE],
         path[MAXLINE];
    rio_t rio;      //  rio_readlineb를 위해 rio_t 타입(구조체)의 읽기 버퍼를 선언

    /* client -> proxy : connfd로부터 request line 읽기 */
    Rio_readinitb(&rio, connfd); // connfd -> rio 
    Rio_readlineb(&rio, buf, MAXLINE);  // rio(==proxy의 connfd)에 있는 한 줄(응답 라인)을 모두 buf로 옮긴다.
    // client -> proxy
    printf("Request headers to proxy:\n");
    sscanf(buf, "%s %s %s", method, uri, version);

    /* Parse uri */
    parse_uri(uri, host, path, &port);  // uri 외에는 전부 파싱해서 받아올 인자들(uri = input, 나머지 = output of parse_uri function)
    char portStr[16];
    sprintf(portStr, "%d", port);
    clientfd = Open_clientfd(host, portStr);

    do_request(clientfd, method, path, host); 
    
    do_response(connfd, clientfd);

    Close(clientfd);
}


/* proxy -> server : clientfd에 buf의 모든 내용을 담았음 */
void do_request(char *clientfd, char *method, char *path, char *host)
{
    char buf[MAXLINE];

    printf("Request headers to server: \n");
    printf("%s %s %s\n", method, path, new_version);  // GET path HTTP 1.0

    /* Read request headers (서버에 보낼 내용을 buf에 저장) */
    sprintf(buf, "GET %s %s\r\n", path, new_version); // GET /index.html HTTP/1.0
    sprintf(buf, "%sHost: %s\r\n", buf, host);        // Host: www.google.com
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%sConnections: close\r\n", buf);
    sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);

    /* 실제로 보내는 부분. buf -> clientfd로 strlen(buf) byte 전송 */
    Rio_writen(clientfd, buf, (size_t)strlen(buf));
}

/* server -> proxy : clientfd에 담긴 내용을 connfd에 담아 보냄*/
void do_response(int connfd, int clientfd)
{
    char buf[MAX_CACHE_SIZE];
    ssize_t n;
    rio_t rio;

    /* clientfd -> rio -> buf -> connfd */
    Rio_readinitb(&rio, clientfd);
    n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);  
    Rio_writen(connfd, buf, n);
}


/* 파싱: 자료형 중요. port만 int임! (format %d) */
void parse_uri(char* uri, char* hostname, char* path, int* port)
{
  /* default webserver host, port */
  strcpy(hostname, "localhost");
  *port = 8080;     // 반드시 엔드서버 tiny.c는 8080으로 열어야.
//   strcpy(port, "8080"); 

  /* http:// 이후의 host:port/path parsing */
  char *pos = strstr(uri, "//");
  pos = pos != NULL ? pos + 2 : uri;

  /* host: 이후의 port/path parsing*/
  char *pos2 = strstr(pos, ":");

  /* port 번호를 포함하여 요청했다면  */
  if (pos2 != NULL)
  {
    *pos2 = '\0';
    sscanf(pos2 + 1, "%d%s", port, path); // 숫자는 port에 이후 문자열은 path에 저장
  }
  else /* port 번호가 없이 요청 왔다면 */
  {
    pos2 = strstr(pos, "/");
    if (pos2 != NULL) // path를 통해 특정 자원에 대한 요청이 있을 경우
    {
      sscanf(pos2, "%s", path); // pos2 위치의 문자열을 path에 저장함
    }
  }
  return;
}