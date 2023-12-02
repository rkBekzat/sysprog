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


typedef unsigned long long ull;

struct my_context {
    char *filename;
    int* arr;
    int size;
    bool sorted;

};

struct works{
    struct my_context **files;
    int sz;
    ull lat;
};

struct coro_data{
    struct works *data;
    int coro_id;

    struct timespec start;
    ull total_time;
};

static struct coro_data * coro_data_new(struct works * data, int id) {
    struct coro_data *ctx = malloc(sizeof (*ctx));
    ctx->coro_id = id;
    ctx->data = data;
    return ctx;
}

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
    result->lat = (ull)lat;
    return result;
}

static void coro_data_delete(struct  coro_data *ctx){
    free(ctx);
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

ull to_ms(struct timespec tm){
    return (ull) tm.tv_sec*1000000+(ull)tm.tv_nsec/1000;
}

void to_yield(struct coro_data *ctx){
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    if(to_ms(current) - to_ms(ctx->start) > ctx->data->lat) {
//        printf("%d: switch count %lld\n", ctx->coro_id, coro_switch_count(coro_this()));
//        printf("%d: yield\n", ctx->coro_id);
        ctx->total_time += (to_ms(current) - to_ms(ctx->start));
        coro_yield();
        clock_gettime(CLOCK_MONOTONIC, &ctx->start);
    }
}


static void
sorting(struct coro_data *ctx, int *arr, int l, int r) {
    if(r==l){
        return ;
    }
    int mid = (l+r) / 2;
    sorting(ctx, arr, l, mid);
    to_yield(ctx);
    sorting(ctx, arr, mid+1, r);
    to_yield(ctx);
    merge(arr, l, r);
}

static int
coroutine_func_f(void *context)
{
    struct coro_data *coro_ctx = context;
    struct works *ctx = coro_ctx->data;
    struct coro *this = coro_this();
    printf("Coro %d started\n", coro_ctx->coro_id);

    coro_ctx->total_time = (ull)0;
    clock_gettime(CLOCK_MONOTONIC, &coro_ctx->start);

    for(int i = 0; i < ctx->sz; i++){
        if(ctx->files[i] == NULL || ctx->files[i]->sorted){
            continue;
        }
        ctx->files[i]->sorted = true;

        printf("%d: yield\n", coro_ctx->coro_id);
        coro_yield();
        clock_gettime(CLOCK_MONOTONIC, &coro_ctx->start);

        sorting(coro_ctx, ctx->files[i]->arr, 0, ctx->files[i]->size);

        to_yield(coro_ctx);
    }
    struct timespec cur;
    clock_gettime(CLOCK_MONOTONIC, &cur);
    coro_ctx->total_time += to_ms(cur) - to_ms(coro_ctx->start);
    printf("%d: switch count %lld\n", coro_ctx->coro_id, coro_switch_count(this));
    printf("Coroutine %d works %llu microsecond\n", coro_ctx->coro_id, coro_ctx->total_time);
    coro_data_delete(coro_ctx);
    return 0;
}

static void combine(struct my_context * first, struct my_context *second) {
    int size = first->size + second->size;
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
    if(!sscanf(argv[1], "%d", &latency) || latency < 0){
        fprintf(stderr, "LATENCY should be natural");
        exit(1);
    }

    int num_cor;
    if(!sscanf(argv[2], "%d", &num_cor) || num_cor < 1){
        fprintf(stderr, "COROUTINES should be natural");
        exit(1);
    }

    coro_sched_init();
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int num_file = argc - 3;
    struct works * data = works_new(num_file, latency);

    for(int i = 0; i < num_file; i++){
        data->files[i] = my_context_new(argv[i+3]);
    }

    for (int i = 0; i < num_cor; ++i) {
        coro_new(coroutine_func_f, coro_data_new(data, i));
    }
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL) {
        printf("Finished %d\n", coro_status(c));
        coro_delete(c);
    }
    for(int i = 1; i < num_file; i++){
        combine(data->files[i], data->files[i-1]);
    }

    FILE *result = fopen("result.txt", "w");
    for(int  i = 0; i < data->files[num_file-1]->size; i++){
        fprintf(result, "%d ", data->files[num_file-1]->arr[i]);
    }

    works_delete(data);
    fclose(result);

    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("Total execution time: %llu microseconds\n",  to_ms(end)-to_ms(start));
    return 0;
}