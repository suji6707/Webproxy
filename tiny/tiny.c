/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* main */
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}
/* end of main */


void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);  // rp를 읽어서 buf에 저장
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {    // 같으면 0 -> 끝날때까지
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return; // The request headers have been read and printed to the console.
}


int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // static contents
  if (!strstr(uri, "cgi-bin")) {    
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);   
    if (uri[strlen(uri) -1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  // dynamic contents
  else {      
    ptr = index(uri, '?');      // ?이 있으면 ptr이 가리키도록 함.
    if (ptr) {
      strcpy(cgiargs, ptr+1);   // ? 뒤에 인자들을 붙여주고, ?은 " " 처리.
      *ptr = '\0';
    }
    else {
      strcpy(cgiargs, "");      // cgiargs를 empty string으로.
    }
    strcpy(filename, ".");
    strcat(filename, uri);

    return 0;
  } 
}



/* doit 
- 핵심은 connfd를 읽고 쓴다는 점. 
*/
void doit(int fd)   // connfd를 인자로 받음
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];   // parse_uri
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);    // fd로 버퍼 초기화
  Rio_readlineb(&rio, buf, MAXLINE);  // rio는 읽어올 곳(fd), buf는 저장될 곳.
  printf("Request headers:\n");
  printf("%s", buf);   // 
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 각 문자열에 저장.
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented",
              "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, method, "404", "Not found",
              "Tiny couldn't find this file");
    return;
  }
  
  // serve static content
  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
    clienterror(fd, method, "403", "Forbidden",
              "Tiny couldn't read the file");
    return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }

  // serve dynamic content 
  // static은 읽어오는 거고, dynamic은 실행하는 것
  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 실행 가능한지
    clienterror(fd, method, "403", "Forbidden",
              "Tiny couldn't run the CGI program");
    return;
  }
  serve_dynamic(fd, filename, cgiargs);
  }
}



/* 원리는 똑같음. 파일을 scrp에 저장해서 fd로 쓰는 것. 
결국은 connfd에 내용이 담겨서 소통함.
*/
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);     // argument filename으로 타입을 아는거고, filetype은 처음에 buffer로 두고 여기에 result값을 넣는듯.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");   // 앞의 buf는 결과 누적, 뒤의 buf는 해당 값을 넣어서 출력
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf); 
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);    // 원격 저장소
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 가상메모리로 가져와서 작업할거임.
  Close(srcfd);
  Rio_written(fd, scrp, filesize);    // connected file descriptor에 시킬 것. scrp로 시작하는 가상메모리에서 filesize만큼 쓰게 함.
  Munmap(srcp, filesize);
}

/* HTTP response generated by the server */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { /* Child */
        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); 
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */          
        Execve(filename, emptylist, environ); /* Run CGI program */ 
    }
    Wait(NULL); /* Parent waits for and reaps child */
}

/*
- 두 개의 char array 'buf', 'body' 사용. (buf는 header, 바로 프린트, body는 쌓아뒀다가 한번에)
- sprinf로 HTTP 응답 메시지를 'body' array에 넣음.
- Each sprintf statement appends its formatted string to the end of the previous contents of the body array, allowing multiple strings to be combined into a single string.
- sprintf(body, "ㅇㅇ"); 는 body에 덮어써버리는 거고,
  sprintf(body, "ㅇㅇ", body);는 뒤 인자에 덧붙이는 것. builds upon the previous contents of the body array
*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];  // 에러메시지, 응답

  // build HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);  // 404: Not Found
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server></em>\r\n", body);

  // print HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));   // resulting string is stored in 'buf'
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // Rio_writen으로 buf, body를 소켓 fd를 통해 클라이언트에 보냄
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}


void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, '.gif'))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
	  strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
	  strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}



/* rio_wirten

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
  size_t nleft = n;
  ssize_t nwritten;
  char *bufp = usrbuf;

  while (nleft > 0){
    if ((nwritten = write(fd, bufp, nleft)) <= 0){
      if (errno == EINTR)
      nwritten = 0;
      else
      return -1;
    }
    nleft -= nwritten;
    bufp += nwritten;
  }
  return n;
}
*/
