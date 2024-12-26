
# define _GNU_SOURCE

# include <stdio.h>
# include <stdlib.h>
# include <sched.h>
# include <unistd.h>
# include <pthread.h>
# include <string.h>
# include <time.h>


# define NORMAL 0
# define FIFO 1
# define NANO 1000000000

extern char *optarg;

pthread_barrier_t barrier;  // Declare a barrier variable

double busy_time;

typedef struct {
    pthread_t thread_id;
    int thread_num;
} thread_info_t;

void *thread_func(void *arg) {
    /* 1. Wait until all threads are ready */
    pthread_barrier_wait(&barrier);
    /* 2. Do the task */ 
    thread_info_t *p = (thread_info_t *)arg;
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", p->thread_num);
        struct timespec t1,t2;
        double time_used;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
        while(1) {   /* Busy for <time_wait> seconds */
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t2);
            time_used = (t2.tv_sec + (double)t2.tv_nsec/NANO) - (t1.tv_sec + (double)t1.tv_nsec/NANO);
            if(time_used >= busy_time) {
                break;
            }
        }
    }
    /* 3. Exit the function  */
    return NULL;
}


int main(int argc, char *argv[]) {
    /* 1. Parse program arguments */
    int opt;
    int num_threads = 0;
    char *sched_strings = NULL;
    char *prio_strings = NULL;
    while((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
        switch(opt) {
            case 'n': 
                num_threads = atoi(optarg);
                break;
            case 't':
                busy_time = atof(optarg);
                break;
            case 's':
                sched_strings = calloc(strlen(optarg), sizeof(char));
                strncpy(sched_strings, optarg, strlen(optarg));
                break;
            case 'p':
                prio_strings = calloc(strlen(optarg), sizeof(char));
                strncpy(prio_strings, optarg, strlen(optarg));
                break;
        }
    }

    // Record the policy             
    int policy[num_threads];
    char *token = strtok(sched_strings, ",");
    for(int i = 0; i < num_threads; i++) {
        if(strcmp(token, "NORMAL") == 0) {
            policy[i] = NORMAL;
        } else {
            policy[i] = FIFO;
        }
        token = strtok(NULL, ",");
    }

    // Record the priority
    struct sched_param param[num_threads];
    token = strtok(prio_strings, ",");
    for(int i = 0; i < num_threads; i++) {
        param[i].sched_priority = atoi(token);
        token = strtok(NULL, ",");
    }

    /* 2. Create <num_threads> worker threads */
    thread_info_t thread_info[num_threads];

    /* 3. Set CPU affinity */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset); 

    /* 4. Set the attributes to each thread */
    pthread_attr_t attr[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_attr_init(&attr[i]);
        pthread_attr_setschedpolicy(&attr[i], policy[i]);
        pthread_attr_setschedparam(&attr[i], &param[i]);
        pthread_attr_setinheritsched(&attr[i], PTHREAD_EXPLICIT_SCHED);
    }
    /* 5. Start all threads at once */

    // Initialize barrier
    if (pthread_barrier_init(&barrier, NULL, num_threads+1) != 0) {
        perror("pthread_barrier_init");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_threads; i++) {
        thread_info[i].thread_num = i;
        pthread_create(&thread_info[i].thread_id, &attr[i], thread_func, &thread_info[i]);
    }
    pthread_barrier_wait(&barrier);

    /* 6. Wait for all threads to finish  */ 
    for (int i = 0; i < num_threads; i++) {
        pthread_join(thread_info[i].thread_id, NULL);
    }
    pthread_barrier_destroy(&barrier);

    return 0;
}

// sudo ./sched_demo -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30