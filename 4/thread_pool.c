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
    pthread_mutex_unlock(task->task_mutex);
    if(pool->cnt_threads < pool->max_threads && pool->run_threads == pool->cnt_threads){}
//        pthread_create(pool->threads[pool->cnt_threads++], NULL,  );
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
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return false;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return false;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
