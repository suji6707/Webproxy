#include "csapp.h"

int open_listenfd(char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval=1; 

    /* Get a list of "potential" server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_socktype = SOCK_STREAM;  /* Accept a connection */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;  /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV; /* using a numeric port arg */
    
    Getaddrinfo(NULL, port, &hints, &listp);

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; // failed
        
        /* Eliminates "Address already in use" error from bind - 바로 반응할 수 있게 */
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                    (const void *)&optval , sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;           // Success
        Close(listenfd);     // bind failed, try next
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p)
        return -1;

    /* Make it a listening socket ready to accept connection requests 
    - LISTENQ는 pending connection 큐의 최대 길이. 커널이 소켓에 allow 할-
        if listen() returns a value less then 0, error occured, while attempting to set socket to the listening state.
    - listen()은 listening socket이 다가올 연결요청을 받을 준비시키는 것이고.
        클라이언트가 연결하려 하면, 서버의 listening socket이 클라이언트로부터 SYN 패킷을 받아
        3-way handshake가 시작된다. 
        이후 커넥션을 accept하기 위해 서버는 accept() function을 쓴다.
    */
    if (listen(listenfd, LISTENQ) < 0) {
        Close(listenfd);
        return -1;
    }    
    return listenfd;
}










/* 
- 서버는 listner socket이고, 클라이언트는 reaches out to the server.
- 클라이언트는 socket(), connect()가 성공할 때까지 walk this list(getaddr result)
- 서버는 socket(), bind()가 성공할 때까지 탐색.
> server 주소를 탐색하는 게 당연하다. 클라이언트는 하나고, 여러 IP 주소들 중 뭘 연결할 것이냐- 

- the server first needs to bind a unique address to the socket it will be using to listen for incoming connections.
The bind() function associates a socket with a specific IP address and port number combination.
> listen에서 서버가 가장 먼저 할 일은, 특정 IP주소/포트와 소켓을 결합하는 것이다(커넥션 형성)
- 서버가 소켓을 주소에 결합하고 나면, 연결요청으로부터 listening하기 시작. 
> listen()은 소켓을 passive mode- 클라이언트 요청을 기다리는- 으로 바꾼다.
- 따라서, 서버는 먼저 bind를 하고 listening 모드로 들어가야 한다. 
그래야만 뒤이어 올 연결 요청이 올바른 소켓/포트를 향한 것인지 알 수 있기 때문이다. 
> 연결 다음 리슨. 


- getaddrinfo는 도메인을 받으면 IP주소 리스트로 바꿔준다. 
    addrinfo는 연결리스트로 쌓이고, 후보자 리스트라 볼 수. 
- *listp는 addrinfo 연결리스트 헤드를 가리키고, *p는 반복문을 도는 커서다. 


- file descriptor라는 건 숫자로 던져진다. id같은 식별자라고 판단해야 하고. 
- listenfd는 socket()에서 new socket으로 만들어 준 것이다. 
- socket()는 file descripter(unsigned integer 형태)를 리턴하므로 
    음수가 나오면 실패임. 
- listenfd = socket()을 할당하므로, listenfd 자체가 하나의 식별자라 보면 될듯.



<중요>
p->ai_addr 의 -> 포인터 연산자는 구조체 멤버 접근 방식. *(p).ai_addr


*/