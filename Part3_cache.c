#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_MAGIC_NUMBER 9999
#define CACHE_OBJS_COUNT 10
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";


void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri,char *host,char *path,int *port);

/*cache function*/
void cache_init();
int cache_find(char *url);
int cache_eviction();
void cache_LRU(int index);
void cache_uri(char *uri,char *buf);
void readerPre(int i);
void readerAfter(int i);

/* 구조체 
- CACHE_OBJS_COUNT(MAX_CACHE_SIZE / MAX_OBJECT_SIZE), which is defined as 10. 
- cache_block is a struct that represents a single object in the cache
- 
*/
typedef struct {
    char cache_obj[MAX_OBJECT_SIZE];    // 캐시 데이터가 들어있는 문자열
    char cache_url[MAXLINE];            // 해당 캐시 데이터와 연관된 URL을 담고있는 문자열
    int LRU;
    int isEmpty;

    int readCnt;            /*count of readers*/
    sem_t wmutex;           /*protects accesses to cache 세마포어 타입. 1: 사용가능, 0: 사용 불가능*/
    sem_t rdcntmutex;       /*protects accesses to readcnt*/

} cache_block;

typedef struct {
    cache_block cacheobjs[CACHE_OBJS_COUNT];  /*ten cache blocks*/
    int cache_num;
} Cache;

Cache cache;
/* */


int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t  clientlen;
    char host[MAXLINE],port[MAXLINE];
    pthread_t tid;
    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    cache_init();

    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }
    Signal(SIGPIPE,SIG_IGN); // 현재 다른 여러 클라이언트들과도 연결되어있는 상태기 때문에 하나 종료됐다고 해서 다 꺼버리면 안되니 그런 시그널을 무시해라, 라는 함수

    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);

        /*print accepted message*/
        Getnameinfo((SA*)&clientaddr,clientlen,host,MAXLINE,port,MAXLINE,0);
        printf("Accepted connection from (%s %s).\n",host,port);

        /*concurrent request*/
        Pthread_create(&tid, NULL, thread, (void *)connfd);
    }
    return 0;
}

/*thread function*/
void *thread(void *vargp){
    int connfd = (int)vargp;
    Pthread_detach(pthread_self());

    doit(connfd);

    Close(connfd);
}


/*handle the client HTTP transaction*/
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

    // @ 굳이 url_store를 새로 선언해야 하나?
    char url_store[100];
    strcpy(url_store, uri);  /*store the original url */

    /* the uri is cached ? - 캐시되어 있으면 그걸 바로 클라이언트에게 */
    int cache_index;
    if((cache_index = cache_find(url_store)) != -1){   /*in cache then return the cache content*/
         /* '해당 index 캐시 object에 대한' read 권한을 얻고(if rdcnt == 1) writer lock을 건다 */
         readerPre(cache_index); 

         /* 캐시에 있으면, 엔드서버 데이터를 connfd에 써서 바로 보냄. 따로 rio_read 없음. (쓰는 행위 자체가 전달) */
         Rio_writen(connfd, cache.cacheobjs[cache_index].cache_obj, strlen(cache.cacheobjs[cache_index].cache_obj));

         /* rdcnt -1, last reader이면 writer lock도 푼다 */
         readerAfter(cache_index); // 캐시 뮤텍스를 닫음. 
         return;
    }

    /*parse the uri to get host,file path ,port*/
    parse_uri(uri, host, path, &port);
    char portStr[16];
    sprintf(portStr, "%d", port);
    clientfd = Open_clientfd(host, portStr);

    /* proxy -> server: clientfd에 헤더를 모두 담음 */
    do_request(clientfd, method, path, host);

    /* server -> proxy : clientfd에 담긴 내용을 connfd에 담아서 보냄*/
    do_response(connfd, clientfd);

    /* 
    uri를 캐시에 저장할 때 두 개의 파라미터
    1. cachebuf: 캐시할 응답을 임시로 저장
    2. url_store: uri
    3. sizebuf: 응답 사이즈
    - cachebuf에 한줄씩 읽으면서 넣는다
    - OBJECT_SIZE를 넘지 않는 만큼 connfd에 쓰고,
    - 넘는 부분은 cachebuf에 저장
    */
   char cachebuf[MAX_OBJECT_SIZE];
   int sizebuf = 0;
   size_t n;

   while((n = Rio_readlineb(clientfd, buf, MAXLINE)) != 0) {  // clientfd -> buf
    sizebuf += n;
    /* end server -> client 가기 전 캐싱하고 보낼 것 */
    if (sizebuf < MAX_OBJECT_SIZE) {
      strcat(cachebuf, buf);      // cachebuf에 buf를 이어붙임 
      Rio_writen(connfd, buf, n); // 일단 buf 내용을 client에 보내고, 
    }
   }

    Close(clientfd);

    /*store it*/
    if(sizebuf < MAX_OBJECT_SIZE){
        cache_uri(url_store, cachebuf);   // cachebuf를 uri와 함께 캐시에 저장
    }
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


/*Connect to the end server*/
inline int connect_endServer(char *host,int port,char *http_header){
    char portStr[100];
    sprintf(portStr,"%d",port);
    return Open_clientfd(host,portStr);
}

/*parse the uri to get host,file path ,port*/
void parse_uri(char *uri,char *host,char *path,int *port)
{
    *port = 80;
    char* pos = strstr(uri,"//");

    pos = pos!=NULL? pos+2:uri;

    char*pos2 = strstr(pos,":");
    if(pos2!=NULL)
    {
        *pos2 = '\0';
        sscanf(pos,"%s",host);
        sscanf(pos2+1,"%d%s",port,path);
    }
    else
    {
        pos2 = strstr(pos,"/");
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",host);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",host);
        }
    }
    return;
}
/**************************************
 * Cache Function
 **************************************/

/* obj x 10 = cache size
* 배열 10칸 정도. 각 1블럭을 캐시 구조체로. 10개로 구성. 
- cacheobj

Sem_init
- 초기화 할 세마포어 포인터, 0: 스레드끼리 세마포어 공유(세마포어를 mutex로 쓰려면)
- wmutex: 캐시 접근 보호해주는 mutex
- rdcntmutex: read cnt 접근 보호

*/
void cache_init(){
    cache.cache_num = 0;
    int i;
    for(i=0;i<CACHE_OBJS_COUNT;i++){
        cache.cacheobjs[i].LRU = 0;
        cache.cacheobjs[i].isEmpty = 1;
        Sem_init(&cache.cacheobjs[i].wmutex,0,1);
        Sem_init(&cache.cacheobjs[i].rdcntmutex,0,1);
        cache.cacheobjs[i].readCnt = 0;
    }
}

/*
* reader first
-  
*/
void readerPre(int i){
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt++;
    if(cache.cacheobjs[i].readCnt == 1) P(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);
}

void readerAfter(int i){
    P(&cache.cacheobjs[i].rdcntmutex);
    cache.cacheobjs[i].readCnt--;
    if(cache.cacheobjs[i].readCnt==0) V(&cache.cacheobjs[i].wmutex);
    V(&cache.cacheobjs[i].rdcntmutex);

}

void writePre(int i){
    P(&cache.cacheobjs[i].wmutex);
}

void writeAfter(int i){
    V(&cache.cacheobjs[i].wmutex);
}

/*find url is in the cache or not */
int cache_find(char *url){
    int i;
    for(i=0;i<CACHE_OBJS_COUNT;i++){
        readerPre(i);
        if((cache.cacheobjs[i].isEmpty==0) && (strcmp(url,cache.cacheobjs[i].cache_url)==0)) break;
        readerAfter(i);
    }
    if(i>=CACHE_OBJS_COUNT) return -1; /*can not find url in the cache*/
    return i;
}

/*find the empty cacheObj or which cacheObj should be evictioned*/
int cache_eviction(){
    int min = LRU_MAGIC_NUMBER;
    int minindex = 0;
    int i;
    for(i=0; i<CACHE_OBJS_COUNT; i++)
    {
        readerPre(i);
        if(cache.cacheobjs[i].isEmpty == 1){/*choose if cache block empty */
            minindex = i;
            readerAfter(i);
            break;
        }
        /* 처음에 9999, 1에 i가 들어오면 min은 1로 업데이트됨. */
        if(cache.cacheobjs[i].LRU< min){    /*if not empty choose the min LRU*/
            minindex = i;
            min = cache.cacheobjs[i].LRU;
            readerAfter(i);
            continue;
        }
        readerAfter(i);
    }

    return minindex;
}

/* update the LRU number except the new cache one */
void cache_LRU(int index){
    int i;
    for(i=0; i<index; i++)    {

        writePre(i);

        if(cache.cacheobjs[i].isEmpty==0 && i!=index){
            cache.cacheobjs[i].LRU--;       // decrements the LRU value of each cache object that has been accessed before the newly cached object
        }                                   // newly cached object is higher than all other cache objects, indicating that it has been accessed recently 
        writeAfter(i);
    }

    i++;

    for(i; i<CACHE_OBJS_COUNT; i++)    {
        writePre(i);
        if(cache.cacheobjs[i].isEmpty==0 && i!=index){
            cache.cacheobjs[i].LRU--;
        }
        writeAfter(i);
    }
}

/*cache the uri and content in cache*/
void cache_uri(char *uri,char *buf){

    int i = cache_eviction();

    writePre(i);/*writer P*/

    strcpy(cache.cacheobjs[i].cache_obj, buf);   // 선택한 i번째 캐시 object에 response를 저장
    strcpy(cache.cacheobjs[i].cache_url, uri);  // uri 인자가 값으로 저장
    cache.cacheobjs[i].isEmpty = 0;           // 할당되었다는 걸 표시하기 위해 0으로 
    cache.cacheobjs[i].LRU = LRU_MAGIC_NUMBER; // track the least recently used cache object.
    cache_LRU(i);

    writeAfter(i);/*writer V*/
}

