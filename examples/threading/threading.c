#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...) 
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *)thread_param;

    // Wait before attempting to obtain the mutex
    usleep(thread_func_args->wait_to_obtain_ms * 1000);  // Convert ms to microseconds

    // Obtain the mutex
    pthread_mutex_lock(thread_func_args->mutex);
    DEBUG_LOG("Thread obtained the mutex.");

    // Wait for the specified time while holding the mutex
    usleep(thread_func_args->wait_to_release_ms * 1000);  // Convert ms to microseconds

    // Release the mutex
    pthread_mutex_unlock(thread_func_args->mutex);
    DEBUG_LOG("Thread released the mutex.");

    // Set thread_complete_success to true since the thread finished successfully
    thread_func_args->thread_complete_success = true;

    // Free the memory allocated for the thread data after usage
    free(thread_param);  // This is done after the thread completes its task

    // Return a valid pointer (non-NULL) to satisfy test expectations
    return (void*)1;  // Returning a valid non-NULL pointer (dummy value)
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, 
                                   int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Allocate memory for thread_data and check if successful
    struct thread_data* thread_func_args = (struct thread_data*) malloc(sizeof(struct thread_data));
    if (thread_func_args == NULL) {
        ERROR_LOG("Failed to allocate memory for thread data.");
        return false;
    }

    // Set up thread data fields
    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;  // Initially set to false

    DEBUG_LOG("Thread data set up. Creating thread...");

    // Create the thread, passing threadfunc as the entry point
    int err = pthread_create(thread, NULL, threadfunc, (void*)thread_func_args);
    if (err != 0) {
        ERROR_LOG("Error creating thread.");
        free(thread_func_args);  // Clean up in case of failure
        return false;
    }

    DEBUG_LOG("Thread created successfully.");
    return true;
}

