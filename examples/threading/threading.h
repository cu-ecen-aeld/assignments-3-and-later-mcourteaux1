#ifndef THREADING_H
#define THREADING_H

#include <pthread.h>
#include <stdbool.h>

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct thread_data {
    pthread_mutex_t *mutex;   // Mutex to be used by the thread
    int wait_to_obtain_ms;     // Time to wait before obtaining the mutex (in ms)
    int wait_to_release_ms;    // Time to hold the mutex before releasing it (in ms)
    bool thread_complete_success; // Status flag to indicate if thread completed successfully
};

/**
 * Start a thread which sleeps @param wait_to_obtain_ms number of milliseconds, 
 * then obtains the mutex in @param mutex, then holds for @param wait_to_release_ms milliseconds, 
 * then releases.
 * The start_thread_obtaining_mutex function should only start the thread and should not block
 * for the thread to complete.
 */
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, 
                                   int wait_to_obtain_ms, int wait_to_release_ms);

#endif /* THREADING_H */

