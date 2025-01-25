#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <stdbool.h>

typedef void (*thread_func_t)(void *arg);

typedef struct threadpool_job_t {
  thread_func_t func;            // thread function
  void *arg;                     // thread function arguments
  struct threadpool_job_t *next; // pointer to the next job in the queue
  unsigned long size;            // size of the job calculated ahead of time
} threadpool_job_t;

typedef struct {
  threadpool_job_t *head; // pointer to a list of jobs
  threadpool_job_t *tail; // maintain tail for insertions
  pthread_mutex_t *job_queue_mutex; // protects changes to the queue
  pthread_cond_t *not_empty;
  pthread_cond_t *is_empty;
  unsigned int size; // no. jobs in the queue
  bool shutdown; // indicates that the threadpool should not wait for jobs and
                 // shutdown.
} threadpool_job_queue_t;

typedef struct {
  threadpool_job_queue_t jobs; // queue of jobs waiting for a thread to run
  pthread_t *threads;          // pointer to the array of thread handles
  pthread_mutex_t *busy_threads_mutex;
  pthread_cond_t *threads_idle_cond;
  unsigned int busy_threads;
  unsigned int total_threads; // readonly after definition
} threadpool_t;

typedef struct {
  thread_func_t func;
  void *arg;
  unsigned long size;
} job_batch_t;

/**
 * Creates the threadpool object.
 */
threadpool_t *threadpool_create(unsigned int num);

/**
 * Destroys the threadpool and frees all related resources.
 * May NOT be called more than once.
 */
void threadpool_destroy(threadpool_t *tp);

/**
 * Submit a job into the threadpool.
 * Accepts a linked list of jobs and adds them to the queue.
 * Jobs are processed in FIFO/FCFS order. This means ordering is left
 * to the user of this library.
 */
void threadpool_submit(threadpool_t *tp, threadpool_job_t **batch,
                        unsigned int num);

/**
 * Gets a single job from the threadpool.
 */
threadpool_job_t *threadpool_get(threadpool_t *tp);


/**
 * It waits for all jobs currently in the pool to finish.
 * Can be used as a synchronization point for threads adding jobs.
 */
void threadpool_wait(threadpool_t *tp);

#endif
