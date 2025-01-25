#include "threadpool.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

threadpool_job_t *job_get_next(threadpool_job_t *job) { return job->next; }

void *Thread_run(threadpool_t *tp);

threadpool_t *threadpool_create(unsigned int num) {
  // create job queue
  threadpool_job_queue_t jobs = {0};

  jobs.job_queue_mutex = calloc(1, sizeof(pthread_mutex_t));
  assert(jobs.job_queue_mutex != NULL);

  jobs.not_empty = calloc(1, sizeof(pthread_cond_t));
  assert(jobs.not_empty != NULL);

  jobs.is_empty = calloc(1, sizeof(pthread_cond_t));
  assert(jobs.is_empty != NULL);

  pthread_mutex_init(jobs.job_queue_mutex, NULL);
  pthread_cond_init(jobs.not_empty, NULL);
  pthread_cond_init(jobs.is_empty, NULL);

  // create pool
  threadpool_t *pool = calloc(1, sizeof(threadpool_t));
  assert(pool != NULL);

  pool->jobs = jobs;
  pool->total_threads = num;

  pool->busy_threads_mutex = calloc(1, sizeof(pthread_mutex_t));
  assert(pool->busy_threads_mutex != NULL);

  pool->threads_idle_cond = calloc(1, sizeof(pthread_cond_t));
  assert(pool->threads_idle_cond != NULL);

  pthread_mutex_init(pool->busy_threads_mutex, NULL);
  pthread_cond_init(pool->threads_idle_cond, NULL);

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
  pthread_mutex_lock(tp->jobs.job_queue_mutex);
  if (tp->jobs.shutdown) { // can only call destroy once
    return;
  }
  tp->jobs.shutdown = 1;
  pthread_cond_broadcast(tp->jobs.not_empty);
  pthread_mutex_unlock(tp->jobs.job_queue_mutex);

  // wait for all threads to gracefully terminate
  for (unsigned int i = 0; i < tp->total_threads; i++) {
    pthread_join(tp->threads[i], NULL);
  }

  // job queue deallocate
  pthread_cond_destroy(tp->jobs.not_empty);
  pthread_cond_destroy(tp->jobs.is_empty);
  pthread_mutex_destroy(tp->jobs.job_queue_mutex);
  free(tp->jobs.job_queue_mutex);
  free(tp->jobs.not_empty);
  free(tp->jobs.is_empty);

  free(tp->threads);
  pthread_mutex_destroy(tp->busy_threads_mutex);
  pthread_cond_destroy(tp->threads_idle_cond);
  free(tp->busy_threads_mutex);
  free(tp->threads_idle_cond);
  free(tp);
}

threadpool_job_t *threadpool_get(threadpool_t *tp) {
  pthread_mutex_lock(tp->jobs.job_queue_mutex);
  while (tp->jobs.size == 0) {
    pthread_cond_wait(tp->jobs.not_empty, tp->jobs.job_queue_mutex);
    if (tp->jobs.shutdown) {
      break;
    }
  }

  threadpool_job_t *job;
  if (tp->jobs.shutdown) {
    job = NULL;
  } else {
    job = tp->jobs.head;
    // assert(job != NULL);
    tp->jobs.head = job->next;
    tp->jobs.size--;

    if (tp->jobs.size == 0) {
      pthread_cond_signal(tp->jobs.is_empty);
    }

    // add to the count of busy threads. must be done
    // atomically with incrementing job queue size
    pthread_mutex_lock(tp->busy_threads_mutex);
    tp->busy_threads++;
    pthread_mutex_unlock(tp->busy_threads_mutex);
  }

  pthread_mutex_unlock(tp->jobs.job_queue_mutex);
  return job;
}

void threadpool_submit(threadpool_t *tp, threadpool_job_t **jobs,
                       unsigned int batch_size) {
  if (batch_size < 1) {
    return;
  }
  // convert the array to a linked list
  if (batch_size > 1) {
    for (unsigned int i = 1; i < batch_size; i++) {
      jobs[i - 1]->next = jobs[i];
    }
  }

  pthread_mutex_lock(tp->jobs.job_queue_mutex);
  if (tp->jobs.head == NULL) {
    tp->jobs.head = *jobs;
  }
  tp->jobs.tail = jobs[batch_size - 1];
  tp->jobs.size += batch_size;

  // get number of threads that are not busy
  pthread_mutex_lock(tp->busy_threads_mutex);
  unsigned int available_threads = tp->total_threads - tp->busy_threads;
  pthread_mutex_unlock(tp->busy_threads_mutex);

  // try to wake up exactly the number of threads needed.
  // Does not have to be accurate just an estimate since
  // the number of available threads could change concurrently.
  if (batch_size >= available_threads) {
    pthread_cond_broadcast(tp->jobs.not_empty);
  } else {
    for (unsigned int i = 0; i < batch_size; i++) {
      pthread_cond_signal(tp->jobs.not_empty);
    }
  }
  pthread_mutex_unlock(tp->jobs.job_queue_mutex);
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

    // decrement busy thread count
    pthread_mutex_lock(tp->busy_threads_mutex);
    tp->busy_threads--;
    if (tp->busy_threads == 0) {
      pthread_cond_signal(tp->threads_idle_cond);
    }
    pthread_mutex_unlock(tp->busy_threads_mutex);
  }
  return NULL;
}

void threadpool_wait(threadpool_t *tp) {
  // wait for jobs queue to be empty first
  pthread_mutex_lock(tp->jobs.job_queue_mutex);
  while (tp->jobs.size > 0) {
    pthread_cond_wait(tp->jobs.is_empty, tp->jobs.job_queue_mutex);
  }
  pthread_mutex_unlock(tp->jobs.job_queue_mutex);

  // check that all processors are idle
  pthread_mutex_lock(tp->busy_threads_mutex);
  while (tp->busy_threads > 0) {
    pthread_cond_wait(tp->threads_idle_cond, tp->busy_threads_mutex);
  }

  pthread_mutex_unlock(tp->busy_threads_mutex);
}
