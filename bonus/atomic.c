#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

typedef long long ll;

#define NUMS 100000000

atomic_int atomic_var;


static void assign(double *arr, int *p1, const double *cur, int *p2){
    arr[*p1] = cur[*p2];
    (*p1)++;
    (*p2)++;
}
void combine(double *arr, int l, int r){
    int mid = (l+r)/2;
    int pointer1 = l;
    int pointer2 = mid+1, position = 0;
    double *answer = (double *) malloc((r-l+1) * sizeof (double));
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

void sort(double *a, int l, int r){
    if(l==r) return ;
    int mid = (l + r) / 2;
    sort(a, l, mid);
    sort(a, mid+1, r);

    combine(a, l, r);
}


void* inc(void * arg){
    int memory_ord = *((int*)arg);
    while(atomic_var < NUMS){
        if(memory_ord) atomic_fetch_add_explicit(&atomic_var, 1, memory_order_seq_cst);
        else atomic_fetch_add_explicit(&atomic_var, 1, memory_order_relaxed);
    }
    return NULL;
}

ll bench(int n, pthread_t * threads, int memory_order){
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i = 0; i < n; i++)
        pthread_create(&threads[i], NULL, inc, &memory_order);

    for(int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    ll dur = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    printf("Memory Order: %s, Threads: %d, Time taken: %lld ns\n",
           (memory_order == 1) ? "Relaxed" : "Sequentially Consistent", n, dur);
    return dur;
}

int main(){

    int n;
    printf("Enter the number of threads:");
    scanf("%d", &n);
    pthread_t threads[n];
    double res[2][15];

    for(int i = 0; i < 2; i++){
        for(int r = 0; r < 15; r++){
            atomic_var = 0;
            res[i][r]  = bench(n, threads, i);
        }
    }

    sort(res[0], 0, 14);
    sort(res[1], 0, 14);

    printf("With Sequentially Consistent memory:\n");
    printf("Min: %f ns\n", res[1][0]);
    printf("Max: %f ns\n", res[1][14]);
    printf("Median: %f ns\n", res[1][7]);


    printf("With Relaxed memory:\n");
    printf("Min: %f ns\n", res[0][0]);
    printf("Max: %f ns\n", res[0][4]);
    printf("Median: %f ns\n", res[0][2]);
}