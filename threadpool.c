#include "threadpool.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *Thread_run(threadpool_t *tp);

/**
 * Get a single job from the threadpool.
 */
threadpool_job_t *threadpool_get(threadpool_t *tp);

threadpool_t *threadpool_create(unsigned int num) {
  // create job queue
  threadpool_job_queue_t job_queue = {0};

  job_queue.job_queue_mutex = calloc(1, sizeof(pthread_mutex_t));
  assert(job_queue.job_queue_mutex != NULL);

  job_queue.not_empty = calloc(1, sizeof(pthread_cond_t));
  assert(job_queue.not_empty != NULL);

  job_queue.is_empty = calloc(1, sizeof(pthread_cond_t));
  assert(job_queue.is_empty != NULL);

  pthread_mutex_init(job_queue.job_queue_mutex, NULL);
  pthread_cond_init(job_queue.not_empty, NULL);
  pthread_cond_init(job_queue.is_empty, NULL);

  // create pool
  threadpool_t *pool = calloc(1, sizeof(threadpool_t));
  assert(pool != NULL);

  pool->job_queue = job_queue;
  *(unsigned int *)&(pool->total_threads) = num;

  pool->busy_threads_semaphore = calloc(1, sizeof(sem_t));
  assert(pool->busy_threads_semaphore != NULL);

  sem_init(pool->busy_threads_semaphore, 0, num);

  // create threads
  pthread_t *threads = calloc(num, sizeof(pthread_t));
  assert(threads != NULL);

  for (unsigned int i = 0; i < num; i++) {
    pthread_create(threads + i, NULL, (void *(*)(void *))(&Thread_run), pool);
  }
  pool->threads = threads;
  return pool;
}

void threadpool_destroy(threadpool_t *tp) {
  // prevent concurrent access to job queue and this function
  pthread_mutex_lock(tp->job_queue.job_queue_mutex);
  if (tp->job_queue.shutdown) {
    return;
  }
  tp->job_queue.shutdown = 1;
  pthread_cond_broadcast(tp->job_queue.not_empty);
  pthread_mutex_unlock(tp->job_queue.job_queue_mutex);

  // wait for all threads to gracefully terminate
  for (unsigned int i = 0; i < tp->total_threads; i++) {
    pthread_join(tp->threads[i], NULL);
  }

  // job queue deallocate
  pthread_cond_destroy(tp->job_queue.not_empty);
  pthread_cond_destroy(tp->job_queue.is_empty);
  pthread_mutex_destroy(tp->job_queue.job_queue_mutex);
  free(tp->job_queue.job_queue_mutex);
  free(tp->job_queue.not_empty);
  free(tp->job_queue.is_empty);

  free(tp->threads);
  sem_destroy(tp->busy_threads_semaphore);
  free(tp->busy_threads_semaphore);
  free(tp);
}

threadpool_job_t *threadpool_get(threadpool_t *tp) {
  pthread_mutex_lock(tp->job_queue.job_queue_mutex);
  while (tp->job_queue.size == 0 && !tp->job_queue.shutdown) {
    pthread_cond_wait(tp->job_queue.not_empty, tp->job_queue.job_queue_mutex);
  }

  threadpool_job_t *job;
  if (tp->job_queue.shutdown) {
    job = NULL;
  } else {
    job = tp->job_queue.head;
    // assert(job != NULL);
    tp->job_queue.head = job->next;
    tp->job_queue.size--;

    if (tp->job_queue.size == 0) {
      pthread_cond_signal(tp->job_queue.is_empty);
    }

    // add to the count of busy threads. must be done
    // atomically with incrementing job queue size
    sem_wait(tp->busy_threads_semaphore);
  }

  pthread_mutex_unlock(tp->job_queue.job_queue_mutex);
  return job;
}

void threadpool_submit(threadpool_t *tp, threadpool_job_t **jobs,
                       unsigned int batch_size) {
  if (batch_size < 1) {
    return;
  }
  // set up a new tail to link jobs
  threadpool_job_t *new_tail = calloc(1, sizeof(threadpool_job_t));
  new_tail->arg = jobs[0]->arg;
  new_tail->func = jobs[0]->func;

  // first element of the new jobs
  threadpool_job_t *new_head = new_tail;

  // form the new linked list
  for (unsigned int i = 1; i < batch_size; i++) {
    threadpool_job_t *job = calloc(1, sizeof(threadpool_job_t));
    job->arg = jobs[i]->arg;
    job->func = jobs[i]->func;
    new_tail->next = jobs[i];
    new_tail = jobs[i];
  }

  pthread_mutex_lock(tp->job_queue.job_queue_mutex);
  // append the new linked list
  if (tp->job_queue.head == NULL) {
    tp->job_queue.head = new_head;
  } else {
    tp->job_queue.tail->next = new_head;
  }
  // update tail
  tp->job_queue.tail = new_tail;
  // update size
  tp->job_queue.size += batch_size;

  if (batch_size == 1) {
    pthread_cond_signal(tp->job_queue.not_empty);
  } else {
    pthread_cond_broadcast(tp->job_queue.not_empty);
  }
  pthread_mutex_unlock(tp->job_queue.job_queue_mutex);
}

void *Thread_run(threadpool_t *tp) {
  while (1) {
    threadpool_job_t *job = threadpool_get(tp);
    if (!job) { // stopping condition
      break;
    }

    // execute job
    job->func(job->arg);
    free(job);

    sem_post(tp->busy_threads_semaphore);
  }
  return NULL;
}

void threadpool_wait(threadpool_t *tp) {
  // wait for jobs queue to be empty first
  pthread_mutex_lock(tp->job_queue.job_queue_mutex);
  while (tp->job_queue.size > 0) {
    pthread_cond_wait(tp->job_queue.is_empty, tp->job_queue.job_queue_mutex);
  }
  pthread_mutex_unlock(tp->job_queue.job_queue_mutex);

  // check that all threads are idle by acquiring semaphores once for each
  // thread and releasing the locks to make sure future jobs can be added
  for (unsigned int i = 0; i < tp->total_threads; i++) {
    sem_wait(tp->busy_threads_semaphore);
  }
  for (unsigned int i = 0; i < tp->total_threads; i++) {
    sem_post(tp->busy_threads_semaphore);
  }
}
