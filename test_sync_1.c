/* Global shared variable */
volatile long cnt = 0; /* Counter */

int main(int argc, char **argv)
{
    long niters;
    pthread_t tid1, tid2;

    niters = atoi(argv[1]);
    Pthread_create(&tid1, NULL,
        thread, &niters);
    Pthread_create(&tid2, NULL,
        thread, &niters);
    Pthread_join(tid1, NULL);
    Pthread_join(tid2, NULL);

    /* Check result */
    if (cnt != (2 * niters))
        printf("BOOM! cnt=%ld\n", cnt);
    else
        printf("OK cnt=%ld\n", cnt);
    exit(0);
}

/* Thread routine */                                                                                             
void *thread(void *vargp) 
{                                                                                                                
    long i, niters = 
               *((long *)vargp);                                                                           
                                                                                                                 
    for (i = 0; i < niters; i++)
        cnt++;                   
                                                                                                                 
    return NULL;                                                                                                 
} 
