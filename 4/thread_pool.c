#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

struct thread_task {
	thread_task_f function;
	void *arg;
    void *result;

    bool pushed;
    bool finished;
    bool joined;
    bool delete;

    pthread_cond_t  task_cond;
    pthread_mutex_t  task_mutex;

	/* PUT HERE OTHER MEMBERS */
};

struct tasks_queue {
    struct thread_task *task;
    struct tasks_queue *next;
    struct tasks_queue *last;
    int cnt;
};

void create_queue_node(struct tasks_queue ** tq) {
    (*tq) = malloc(sizeof (struct tasks_queue));
    (*tq)->next = NULL;
    (*tq)->last = NULL;
    (*tq)->cnt = 1;
}

void queue_push(struct tasks_queue ** tq, struct thread_task *ts){
    if(*tq == NULL){
        create_queue_node(&(*tq));
        (*tq)->task = ts;
    } else {
        if((*tq)->next == NULL){
            create_queue_node(&(*tq)->next);
            (*tq)->next->task = ts;
            (*tq)->last = (*tq)->next;
        } else {
            struct tasks_queue * tmp = NULL;
            create_queue_node(&tmp);
            tmp->task = ts;
            (*tq)->last->next = tmp;
            (*tq)->last = tmp;
        }
        (*tq)->cnt++;
    }
}

void pop_queue(struct tasks_queue ** tq){
    if(tq == NULL){
        return ;
    }
    if((*tq)->next != NULL){
        (*tq)->next->last = (*tq)->last;
        (*tq)->next->cnt = (*tq)->cnt;
        (*tq) = (*tq)->next;
        (*tq)->cnt--;
    } else {
        (*tq) = NULL;
    }
}

struct thread_pool {
	pthread_t *threads;

    int max_threads;
    int run_threads;
    int cnt_threads;

    struct tasks_queue *queue;

    pthread_mutex_t mutex;
    pthread_cond_t  cond;

    bool shutdown;
	/* PUT HERE OTHER MEMBERS */
};

void * tpool_worker(void *arg){
    struct thread_pool * pool = (struct thread_pool *) arg;

    thread_task_f function = NULL;
    void *arguments = NULL;
    void *result = NULL;
    while(true){
        pthread_mutex_lock(&pool->mutex);
        while (pool->queue == NULL && !pool->shutdown){
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        if(pool->queue == NULL) {
            continue;
        }
        struct thread_task *task = pool->queue->task;
        pop_queue(&pool->queue);
        pool->run_threads++;
        pthread_mutex_unlock(&pool->mutex);

        if(task == NULL) {
            continue;
        }

        pthread_mutex_lock(&task->task_mutex);
        function = task->function;
        arguments = task->arg;
        pthread_mutex_unlock(&task->task_mutex);

        result = function(arguments);

        pthread_mutex_lock(&task->task_mutex);
        task->result = result;
        task->finished = true;

        pthread_cond_signal(&task->task_cond);
        pthread_mutex_unlock(&task->task_mutex);
        pthread_mutex_lock(&pool->mutex);
        pool->run_threads--;
        pthread_cond_signal(&pool->cond);
        pthread_mutex_unlock(&pool->mutex);
    }
    pthread_exit(NULL);
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if(max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
        return TPOOL_ERR_INVALID_ARGUMENT;

    struct thread_pool *new_pool = malloc(sizeof (struct thread_pool));
    if(!new_pool)
        return -1;
    new_pool->threads = malloc(max_thread_count * sizeof (pthread_t));
    new_pool->max_threads = max_thread_count;
    new_pool->cnt_threads = 0;
    new_pool->shutdown = 0;

    new_pool->queue = NULL;

    pthread_cond_init(&new_pool->cond, NULL);
    pthread_mutex_init(&new_pool->mutex, NULL);

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
    pthread_mutex_lock(&pool->mutex);
    if(pool->run_threads || pool->queue != NULL){
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);

    for(int i = 0; i < pool->cnt_threads; i++)
        pthread_join(pool->threads[i], NULL);

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);

    free(pool->threads);
    free(pool);
	return 0;
}


int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    pthread_mutex_lock(&pool->mutex);
    if(pool->queue != NULL && pool->queue->cnt == TPOOL_MAX_TASKS){
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_TOO_MANY_TASKS;
    }
    queue_push(&pool->queue, task);

    task->pushed = true;
    task->joined = false;
    task->delete = false;
    task->finished = false;

    if(pool->cnt_threads < pool->max_threads && pool->cnt_threads == pool->run_threads ) {
        pthread_create(&pool->threads[pool->cnt_threads++], NULL, tpool_worker, pool);
    }
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
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

    pthread_mutex_init(&(*task)->task_mutex, NULL);

    pthread_cond_init(&(*task)->task_cond, NULL);
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
    pthread_mutex_lock(&task->task_mutex);
    while(task != NULL && !task->finished){
        pthread_cond_wait(&task->task_cond, &task->task_mutex);
    }
    task->joined= true;
    pthread_mutex_unlock(&task->task_mutex);
    *result = task->result;
    return 0;
}

#ifdef NEED_TIMED_JOIN

bool bigger(struct timespec *a, struct timespec *b){
    if(a->tv_sec == b->tv_sec){
        return a->tv_nsec > b->tv_nsec;
    } else {
        return a->tv_sec > b->tv_sec;
    }
}

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
    if(timeout <= 0) return TPOOL_ERR_TIMEOUT;
    if(task->pushed){

        struct timespec timeout_time;
        clock_gettime(CLOCK_MONOTONIC, &timeout_time);
        timeout_time.tv_sec  += (long) (timeout);
        timeout_time.tv_nsec += (long)(timeout*1000000000)%1000000000;

        pthread_mutex_lock(&task->task_mutex);
        while (!task->finished){
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if(bigger(&now, &timeout_time)){
                pthread_mutex_unlock(&task->task_mutex);
                return TPOOL_ERR_TIMEOUT;
            }
            pthread_cond_timedwait(&task->task_cond, &task->task_mutex, &timeout_time);
        }
        *result = task->result;
        task->joined = true;
        pthread_mutex_unlock(&task->task_mutex);
        return 0;
    }
	return TPOOL_ERR_TASK_NOT_PUSHED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
    if(task->pushed && !(task->joined || task->delete)){
        return TPOOL_ERR_TASK_IN_POOL;
    }

    pthread_cond_destroy(&task->task_cond);
    pthread_mutex_destroy(&task->task_mutex);
    free(task);
    return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
    if(task->pushed){
        pthread_mutex_lock(&task->task_mutex);
        task->delete = true;
        pthread_mutex_unlock(&task->task_mutex);
        if(task->finished)
            thread_task_delete(task);
        return 0;
    }
    return TPOOL_ERR_TASK_NOT_PUSHED;
}

#endif
