#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcoro.h"
#include <time.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

typedef unsigned int uint;

struct my_context {
    char *filename;
    int* arr;
    size_t size;
    bool sorted;

};

struct works{
    struct my_context **files;
    int coro_id;
    int sz;
    uint lat;
};

static struct my_context *
my_context_new(const char *filename)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->filename = strdup(filename);
    ctx->size = 0;
	ctx->arr = NULL;
    ctx->sorted = false;


    FILE *f = fopen(filename, "r");

    for(int next; fscanf(f, "%d", &next) != EOF; )
        ctx->size++;
    ctx->arr = (int *) malloc(sizeof (int) * ctx->size);

    fseek(f, SEEK_SET, 0);
    for(int _i = 0, next; fscanf(f, "%d", &next) != EOF; _i ++)
        ctx->arr[_i] = next;
    fclose(f);

    return ctx;
}

static struct  works * works_new(int size, int lat){
    struct works * result = malloc(sizeof (*result));
    result->files = (struct my_context **)malloc(sizeof (struct my_context *)*size);
    result->sz = size;
    result->lat = (uint)lat;
    return result;
}

static void
my_context_delete(struct my_context *ctx) {
    free(ctx->arr);
    free(ctx->filename);
    free(ctx);
}

static void
works_delete(struct works *ctx)
{
    for(int i = 0; i < ctx->sz; i++)
        my_context_delete(ctx->files[i]);
    free(ctx->files);
    free(ctx);
}


static void assign(int *arr, int *p1, const int *cur, int *p2){
    arr[*p1] = cur[*p2];
    (*p1)++;
    (*p2)++;
}

static void merge(int *arr, int l, int r) {
    int mid = (l+r)/2;
    int pointer1 = l;
    int pointer2 = mid+1, position = 0;
    int *answer = (int *) malloc((r-l+1) * sizeof (int));
    while(pointer1 <= mid || pointer2 <= r) {
        if(pointer1 > mid){
            assign(answer, &position, arr, &pointer2);
            continue;
        }
        if(pointer2 > r){
            assign(answer, &position, arr, &pointer1);
            continue;
        }
        if(arr[pointer1] < arr[pointer2]){
            assign(answer, &position, arr, &pointer1);
        } else {
            assign(answer, &position, arr, &pointer2);
        }
    }
    for(int i = l; i <= r; i++){
        arr[i] = answer[i-l];
    }
    free(answer);
}

static void
sorting(int *arr, int l, int r) {
    if(r==l){
        return ;
    }
    int mid = (l+r) / 2;
    sorting(arr, l, mid);
    sorting(arr, mid+1, r);

    merge(arr, l, r);
}


uint microtimes() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint s = (uint) ts.tv_sec*100000+(uint)ts.tv_nsec/1000;
    return s;
}


static int
coroutine_func_f(void *context)
{
    struct works *ctx = context;
    uint last_update = microtimes();

    struct coro *this = coro_this();
    printf("Coro %d started\n", ctx->coro_id);
    uint result_time = 0;
    for(int i = 0; i < ctx->sz; i++){
        if(ctx->files[i] == NULL || ctx->files[i]->sorted){
            continue;
        }
        printf("%d: switch count %lld\n", ctx->coro_id, coro_switch_count(this));
        sorting(ctx->files[i]->arr, 0, ctx->files[i]->size);
        ctx->files[i]->sorted = true;
        if(microtimes()-last_update >= ctx->lat) {
            printf("%d: yield\n", ctx->coro_id);
            result_time += microtimes()-last_update;
            coro_yield();
            last_update = microtimes();
        }
    }
    result_time += microtimes()-last_update;
    printf("Coroutine %d works %u nanosecond\n", ctx->coro_id, result_time);
	return 0;
}

static void combine(struct my_context * first, struct my_context *second) {
    size_t size = first->size + second->size;
    int * arr = (int *) malloc(sizeof (int)*size);
    int position = 0;
    int p1 = 0, p2 = 0;
    while(p1 < first->size || p2 < second->size){
        if(p1 == first->size){
            assign(arr, &position, second->arr, &p2);
            continue;
        }
        if(p2 == second->size){
            assign(arr, &position, first->arr, &p1);
            continue;
        }
        if(first->arr[p1] < second->arr[p2]){
            assign(arr, &position, first->arr, &p1);
        } else {
            assign(arr, &position, second->arr, &p2);
        }
    }
    free(first->arr);
    first->size = size;
    first->arr = arr;
}

int
main(int argc, char **argv)
{
    if(argc == 1) {
        exit(0);
    }
    if(argc < 4){
        fprintf(stderr, "Usage: %s LATENCY COROUTINES FILE...", argv[0]);
        exit(1);
    }

    int latency;
    if(!sscanf(argv[1], "%d", &latency) || latency < 1){
        fprintf(stderr, "LATENCY should be natural");
        exit(1);
    }

    int num_cor;
    if(!sscanf(argv[2], "%d", &num_cor) || num_cor < 1){
        fprintf(stderr, "COROUTINES should be natural");
        exit(1);
    }

    coro_sched_init();
    uint start = microtimes();

    int num_file = argc - 3;
    struct works * data = works_new(num_file, latency);

    for(int i = 0; i < num_file; i++){
        data->files[i] = my_context_new(argv[i+3]);
    }

    for (int i = 0; i < num_cor; ++i) {
        data[i].coro_id = i;
		coro_new(coroutine_func_f, data);
    }
    struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
    for(int i = 1; i < num_file; i++){
        combine(data->files[i], data->files[i-1]);
    }

    FILE *result = fopen(data->files[num_file-1]->filename, "w");
    for(int i = 0; i < data->files[num_file-1]->size; i++){
        fprintf(result, "%d ", data->files[num_file-1]->arr[i]);
    }

    works_delete(data);
    fclose(result);

    uint end = microtimes();
    printf("Total execution time: %llu\n", (long long) (end-start));
	return 0;
}
