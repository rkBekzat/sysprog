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

struct my_context {
    char *filename;
    int* arr;
    int size;
    bool sorted;

    uint total_time;

	/** ADD HERE YOUR OWN MEMBERS, SUCH AS FILE NAME, WORK TIME, ... */
};

struct works{
    struct my_context *files;
    int sz;
};

static struct my_context *
my_context_new(const char *filename)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->filename = strdup(filename);
    ctx->size = 0;
	ctx->arr = NULL;
    ctx->total_time = 0;
    ctx->sorted = false;
    return ctx;
}

static struct  works * works_new(int size){
    struct works * result = malloc(sizeof (*result));
    result->files = (struct my_context *)malloc(sizeof (struct my_context)*size);
    result->sz = size;
    return result;
}

//static void
//my_context_delete(struct my_context *ctx)
//{
//	free(ctx->filename);
//    free(ctx);
//}


static void assign(int *arr, int *p1, int *cur, int *p2){
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

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static int
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */

//    struct coro *this = coro_this();
    struct works *ctx = context;
    for(int i = 0; i < ctx->sz; i++){
        if(ctx->files[i].sorted){
            continue;
        }
        char *name = ctx->files[i].filename;

        FILE *f = fopen(name, "r");
        int next;

        while(fscanf(f, "%d", &next) != EOF){
            ctx->files[i].size++;
            ctx->files[i].arr = (int *) realloc(ctx->files[i].arr, sizeof (int) * ctx->files[i].size);
            ctx->files[i].arr[ctx->files[i].size-1] = next;
        }

        sorting(ctx->files[i].arr, 0, ctx->files[i].size);
        ctx->files[i].sorted = true;
        coro_yield();
    }
	return 0;
}

struct my_context  combine(struct my_context * first, struct my_context *second) {
    struct my_context * answer = my_context_new("result.txt");
    answer->size = first->size + second->size;
    answer->arr = (int *) malloc(sizeof (int)*answer->size);
    int position = 0;
    int p1 = 0, p2 = 0;
    while(p1 < first->size || p2 < second->size){
        if(p1 == first->size){
            assign(answer->arr, &position, second->arr, &p2);
            continue;
        }
        if(p2 == second->size){
            assign(answer->arr, &position, first->arr, &p1);
            continue;
        }
        if(first->arr[p1] < second->arr[p2]){
            assign(answer->arr, &position, first->arr, &p1);
        } else {
            assign(answer->arr, &position, second->arr, &p2);
        }
    }
    return *answer;
}

uint microtimes() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint s = (uint) ts.tv_sec*100000+(uint)ts.tv_nsec/1000;
    return s;
}

int
main(int argc, char **argv)
{
    if(argc == 1) {
        exit(0);
    }
    if(argc < 3){
        printf("STARNGE\n");
        fprintf(stderr, "Usage: %s COROUTINES FILE...", argv[0]);
        exit(1);
    }
    int num_cor = 1;
    if(!sscanf(argv[1], "%d", &num_cor) || num_cor < 1){
        printf("VERY STARNGE\n");
        fprintf(stderr, "COROUTINES should be natural");
        exit(1);
    }
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();

    uint start = microtimes();


    int num_file = argc - 2;
    struct works * data = works_new(num_file);

    for(int i = 0; i < num_file; i++){
        data->files[i] = *my_context_new(argv[i+2]);
    }
	/* Start several coroutines. */
    for (int i = 0; i < num_cor; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
		coro_new(coroutine_func_f, data);
    }
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL) {
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		printf("Finished %d\n", coro_status(c));
		coro_delete(c);
	}
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */

    for(int i = 1; i < num_file; i++){

        data->files[i] = combine(&data->files[i], &data->files[i-1]);
    }
    FILE *result = fopen(data->files[num_file-1].filename, "w");
    for(int i = 0; i < data->files[num_file-1].size; i++){
        fprintf(result, "%d ", data->files[num_file-1].arr[i]);
    }
    free(data);
    fclose(result);

    uint end = microtimes();
    printf("Total execution time: %llu\n", (long long) (end-start));
	return 0;
}
