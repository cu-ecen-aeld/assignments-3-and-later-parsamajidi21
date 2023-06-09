#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data*) thread_param;

    thread_func_args->thread_complete_success = false;

    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    int lock = pthread_mutex_lock(thread_func_args->mutex_data);

    if(lock != 0){
        perror("pthread_mutex_lock");
        thread_func_args->thread_complete_success = false;
        pthread_exit(NULL);
    }
    else{
        thread_func_args->thread_complete_success = true;
    }
    usleep(thread_func_args->wait_to_release_ms * 1000);

    int unlock = pthread_mutex_unlock(thread_func_args->mutex_data);
    if(unlock != 0){
        perror("pthread_mutex_unlock");
        pthread_exit(NULL);
        thread_func_args->thread_complete_success = false;
    }
    else{
        thread_func_args->thread_complete_success = true;
    }
    //thread_func_args->thread_id =  pthread_self();
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    //****************************************Allocate memory to thread_data *********************************//
    struct thread_data* thread_dy = (struct thread_data*) malloc(sizeof(struct thread_data));
    //****************************************Start the thread*********************************************//   
     
    thread_dy->mutex_data = mutex;
    thread_dy->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_dy->wait_to_release_ms = wait_to_release_ms;

    int s = pthread_create(&thread_dy->thread_id, NULL, threadfunc, (void*)thread_dy);
    if(s != 0){
        perror("pthread_create");
        pthread_exit(NULL);
        thread_dy->thread_complete_success = false;
    }
    else{
        *thread = thread_dy->thread_id;
        return true;
    }

    return false;
}

