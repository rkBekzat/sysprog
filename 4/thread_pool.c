#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>

struct thread_task {
	thread_task_f function;
	void *arg;
    void *result;

    bool pushed;
    bool finished;
    bool joined;
    bool delete;

    pthread_cond_t  *task_cond;
    pthread_mutex_t  *task_mutex;
	/* PUT HERE OTHER MEMBERS */
};

struct thread_pool {
	pthread_t *threads;

    int max_threads;
    int cnt_threads;
    int run_threads;

    struct thread_task **task_queue;

    int cnt_task;

    pthread_mutex_t *mutex;
    pthread_cond_t  *cond;

    bool shutdown;
	/* PUT HERE OTHER MEMBERS */
};


void * tpool_worker(void *arg){
    struct thread_pool * pool = (struct thread_pool *) arg;
    while(true){
        pthread_mutex_lock(pool->mutex);
        while (!pool->cnt_task  && !pool->shutdown){
            pthread_cond_wait(pool->cond, pool->mutex);
        }
        if(pool->shutdown){
            pthread_mutex_unlock(pool->mutex);
            return NULL;
        }
        pool->cnt_task--;
        struct thread_task *task = pool->task_queue[pool->cnt_task];
        pool->run_threads++;
        thread_task_f function = NULL;
        void *_arg = NULL;
        void *result = NULL;
        pthread_mutex_lock(task->task_mutex);
        pthread_mutex_unlock(pool->mutex);
        function = task->function;
        _arg = task->arg;
        if(task->finished){
            pthread_mutex_unlock(task->task_mutex);
            continue;
        }
        pthread_mutex_unlock(task->task_mutex);
        result = function(_arg);
        pthread_mutex_lock(task->task_mutex);
        task->result = result;
        task->finished = true;
        if(task->delete){
            pthread_mutex_unlock(task->task_mutex);
            thread_task_delete(task);
            continue;
        }
        pthread_cond_signal(task->task_cond);
        pthread_mutex_unlock(task->task_mutex);
    }
    pthread_exit(NULL);
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if(max_thread_count > TPOOL_MAX_THREADS || max_thread_count < 0)
        return TPOOL_ERR_INVALID_ARGUMENT;

    struct thread_pool *new_pool = malloc(sizeof (struct thread_pool));
    if(!new_pool)
        return -1;
    new_pool->max_threads = max_thread_count;
    new_pool->cnt_threads = 0;
    new_pool->run_threads = 0;

    new_pool->mutex = malloc(sizeof (pthread_mutex_t));
    new_pool->cond = malloc(sizeof (pthread_cond_t));

    new_pool->task_queue = malloc(TPOOL_MAX_TASKS * sizeof (struct thread_task *));
    new_pool->cnt_task = 0;

    pthread_cond_init(new_pool->cond, NULL);
    pthread_mutex_init(new_pool->mutex, NULL);

    *pool = new_pool;
    return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	return pool->cnt_threads;
}

int
thread_pool_delete(struct thread_pool *pool)
{
    pthread_mutex_lock(pool->mutex);
    if(pool->cnt_task || pool->run_threads){
        pthread_mutex_unlock(pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }
    pool->shutdown = true;
    pthread_cond_broadcast(pool->cond);
    pthread_mutex_unlock(pool->mutex);

    for(int i = 0; i < pool->cnt_threads; i++)
        pthread_join(pool->threads[i], NULL);

    pthread_mutex_destroy(pool->mutex);
    free(pool->mutex);

    pthread_cond_destroy(pool->cond);
    free(pool->cond);

    free(pool->task_queue);
    free(pool->threads);
    free(pool);

	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    pthread_mutex_lock(pool->mutex);
    if(pool->cnt_task == TPOOL_MAX_TASKS){
        pthread_mutex_unlock(pool->mutex);
        return TPOOL_ERR_TOO_MANY_TASKS;
    }
    pthread_mutex_lock(task->task_mutex);
    pool->task_queue[pool->cnt_task] = task;
    pool->cnt_task++;
    task->pushed = true;
    task->joined = false;
    task->delete = false;
    pthread_mutex_unlock(task->task_mutex);
    if(pool->cnt_threads < pool->max_threads && pool->run_threads == pool->cnt_threads){}
        pthread_create(&pool->threads[pool->cnt_threads++], NULL, tpool_worker,  pool);

    pthread_mutex_unlock(pool->mutex);
    pthread_cond_signal(pool->cond);
    return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = malloc(sizeof (struct thread_task));
    (*task)->function = function;
    (*task)->arg = arg;

    (*task)->delete = false;
    (*task)->finished = false;
    (*task)->joined = false;
    (*task)->pushed = false;

    pthread_mutex_init((*task)->task_mutex, NULL);
    pthread_cond_init((*task)->task_cond, NULL);
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	return task->finished;
}

bool
thread_task_is_running(const struct thread_task *task)
{

    return task->pushed;
}

int
thread_task_join(struct thread_task *task, void **result)
{
    if(!task->pushed) return TPOOL_ERR_TASK_NOT_PUSHED;
    pthread_mutex_lock(task->task_mutex);
    while(task->finished){
        pthread_cond_wait(task->task_cond, task->task_mutex);
    }
    task->joined= true;
    pthread_mutex_unlock(task->task_mutex);
    *result = task->result;
    return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
    if(!task->pushed) return TPOOL_ERR_TASK_NOT_PUSHED;
    if(timeout <= 0) return TPOOL_ERR_TIMEOUT;

    struct timespec abs_timeout;
    clock_gettime(CLOCK_REALTIME, &abs_timeout);

    abs_timeout.tv_sec += (long) (timout);
    abs_timeout.tv_nsec += (long) (timeout * 1000000000) % 1000000000;

    pthread_mutex_lock(task->mutex);
    while(!task->finished){
        int err = pthread_cond_timedwait(&task->cond, &task->mutex, &abs_timeout);
        if(err == ETIMEDOUT){
            pthread_mutex_unlock(task->mutex);
            return TPOOL_ERR_TIMEOUT;
        } else if(err == 22){
            clock_gettime(CLOCK_REALTIME, &abs_timeout);
            abs_timeout.tv_sec += (long) (timout);
            abs_timeout.tv_nsec += (long) (timeout * 1000000000) % 1000000000;
        }
    }
    task->joined = true;
    pthread_mutex_unlock(task->mutex);
    *result = task->result;
    return 0;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
    if(task->pushed && !(task->joined || task->delete)){
        return TPOOL_ERR_TASK_IN_POOL;
    }
    pthread_cond_destroy(task->task_cond);
    free(task->task_cond);
    pthread_mutex_destroy(task->task_mutex);
    free(task->task_mutex);
    free(task);

    return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
    if(!task->pushed) return TPOOL_ERR_TASK_NOT_PUSHED;

    pthread_mutex_lock(task->mutex);
    task->delete = true;
    if(task->finished){
        pthread_mutex_unlock(task->mutex);
        thread_task_delete(task);
        return 0;
    }
    pthread_mutex_unlock(task->mutex);
    return 0;
}

#endif
