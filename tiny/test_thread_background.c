// It's just happened to it
#include <pthread.h>

typedef void *(func)(void*);

int pthread_create(pthread_t *tid, pthread_attr_t *attr, func *f, void *arg);

/* main thread가 호출하면, peer thread가 다 종료될 때까지 기다렸다가 main thread와 process를 종료, 리턴한다 
- Linux exit()가 해당 프로세스와 관련 모든 쓰레드를 종료시키는 것과 다름! */
void pthread_exit(void *thread_return);

/* 다른 peer가 현재 thread를 tid (ID)를 가지고 종료할 수 있다 */
int pthread_cancel(pthread_t tid);

/* Reaping termainated threads 
- Linux wait()과 달리, pthread_join은 특정 쓰레드가 종료되기까지 기다릴 수 있다.
*/
int pthread_join(pthread_t tid, void **thread_return);
// 성공하면 0 리턴

/* Detached thread
- joinable thread와 달리, 다른 쓰레드가 reap or kill할 수 없다. 
- 종료시 메모리 자원이 자동으로 회수된다.
- 디폴트 옵션은, 쓰레드는 created joinable. 
    따라서 메모리 누수를 피하려면 명시적으로 reap해줘야 하고,
    또는 detach 함수를 써야 한다.  

- 실제로, 고성능 웹서버는 detach가 효율적이다.
    매 커넥션마다 seperate thread가 생성되는데, 각 peer thread가 종료될 때까지 기다린다는 건 ㄴ.
- 각 peer thread는 요청을 처리하기 전 detach itself를 해서 종료후 메모리가 회수될 수 있도록 한다. 
*/
int pthread_detach(pthread_t tid);
