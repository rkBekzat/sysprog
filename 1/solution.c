#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libcoro.h"

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

    uint total_time;

	/** ADD HERE YOUR OWN MEMBERS, SUCH AS FILE NAME, WORK TIME, ... */
};

static struct my_context *
my_context_new(const char *filename)
{
	struct my_context *ctx = malloc(sizeof(*ctx));
	ctx->filename = strdup(filename);
    ctx->size = 0;
	ctx->arr = NULL;
    ctx->total_time = 0;
    return ctx;
}

static void
my_context_delete(struct my_context *ctx)
{
	free(ctx->filename);
    free(ctx);
}

/**
 * A function, called from inside of coroutines recursively. Just to demonstrate
 * the example. You can split your code into multiple functions, that usually
 * helps to keep the individual code blocks simple.
 */
//static void
//other_function(const char *name, int depth)
//{
//	printf("%s: entered function, depth = %d\n", name, depth);
//	coro_yield();
//	if (depth < 3)
//		other_function(name, depth + 1);
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
    struct my_context *ctx = context;
    char *name = ctx->filename;

    FILE *f = fopen(name, "r");
    int next;

    while(fscanf(f, "%d", &next) != EOF){
        ctx->size++;
        ctx->arr = (int *) realloc(ctx->arr, sizeof (int) * ctx->size);
        ctx->arr[ctx->size-1] = next;
    }

    sorting(ctx->arr, 0, ctx->size);

//    for(int i = 0; i < ctx->size; i++){
//        if(i > 1 && ctx->arr[i] < ctx->arr[i-1]){
//            printf("\n-------------------------------------------WTF--------------------------------------/n");
//        }
//        printf("%d ", ctx->arr[i]);
//    }
//	printf("Started coroutine %s\n", name);
//	printf("%s: switch count %lld\n", name, coro_switch_count(this));
//	printf("%s: yield\n", name);
//	coro_yield();
//
//	printf("%s: switch count %lld\n", name, coro_switch_count(this));
//	printf("%s: yield\n", name);
//	coro_yield();
//
//	printf("%s: switch count %lld\n", name, coro_switch_count(this));
//	other_function(name, 1);
//	printf("%s: switch count after other function %lld\n", name,
//	       coro_switch_count(this));

	my_context_delete(ctx);
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

int
main(int argc, char **argv)
{
    if(argc == 1) {
        exit(0);
    }
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();

    int num_file = argc - 1;
    struct my_context *all_files = (struct my_context *)malloc(sizeof (struct my_context)*num_file);

    for(int i = 1; i < argc; i++){
        all_files[i-1] = *my_context_new(argv[i]);
    }
	/* Start several coroutines. */
    for (int i = 0; i < num_file; ++i) {
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
//		coro_new(coroutine_func_f, my_context_new(argv[i]));
        coroutine_func_f(&all_files[i]);
    }
	/* Wait for all the coroutines to end. */
//	struct coro *c;
//	while ((c = coro_sched_wait()) != NULL) {
//		/*
//		 * Each 'wait' returns a finished coroutine with which you can
//		 * do anything you want. Like check its exit status, for
//		 * example. Don't forget to free the coroutine afterwards.
//		 */
//		printf("Finished %d\n", coro_status(c));
//		coro_delete(c);
//	}
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */

    for(int i = 1; i < num_file; i++){
        all_files[i] = combine(&all_files[i], &all_files[i-1]);
    }
    FILE *result = fopen(all_files[num_file-1].filename, "w");
    for(int i = 0; i < all_files[num_file-1].size; i++){
        fprintf(result, "%d ", all_files[num_file-1].arr[i]);
    }
    free(all_files);
    fclose(result);
	return 0;
}
