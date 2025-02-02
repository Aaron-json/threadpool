#include "../threadpool.h"
#include "stdatomic.h"
#include <stdio.h>

typedef struct {
  atomic_int acount;
  int count;
} thread_arg_t;

void increment(void *arg) {
  thread_arg_t *counters = arg;
  counters->acount++;
  counters->count++;
}

int main(int argc, char *argv[]) {
  thread_arg_t counters = {};

  const int THREAD_COUNT = 50;
  const int JOB_COUNT = 1000;
  threadpool_t *tp = threadpool_create(THREAD_COUNT);

  threadpool_job_t job = {.arg = &counters, .func = &increment};
  threadpool_job_t *jobs = {&job};

  for (int i = 0; i < JOB_COUNT; i++) {
    threadpool_submit(tp, &jobs, 1);
  }

  threadpool_wait(tp);
  threadpool_destroy(tp);
  printf("atomic count: %d\n", counters.acount);
  printf("count: %d\n", counters.count);
}
