# Lightweight Threadpool in C

## Overview
A flexible, efficient thread pool implementation in C using POSIX threads, designed for concurrent task execution.

## Features
- Dynamic thread pool creation
- Batch job submission
- Graceful shutdown
- Wait for job completion
- Configurable thread count

## Usage

### Creating a Threadpool
```c
threadpool_t *pool = threadpool_create(num_threads);
```

### Submitting Jobs
```c
threadpool_job_t *job = malloc(sizeof(threadpool_job_t));
job->func = your_task_function;
job->arg = task_arguments;

threadpool_submit(pool, &job, 1);  // Submit single job
// Or submit multiple jobs in a batch
threadpool_job_t *jobs[batch_size];
// ... prepare jobs ...
threadpool_submit(pool, jobs, batch_size);
```

### Waiting for Completion
```c
threadpool_wait(pool);  // Block until all jobs are done
```

### Cleanup
```c
threadpool_destroy(pool);  // Terminate threads and free resources
```

## Thread Safety
- Uses mutex locks for thread-safe job queue management
- Handles concurrent job submission and thread synchronization

## Requirements
- POSIX threads (pthread) support
- C99 or later standard

## Limitations
- Fixed thread pool size after creation
- No dynamic thread scaling
