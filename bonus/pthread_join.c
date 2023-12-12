#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>

#define NUMS 100000

typedef long long ll;

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

void* func(void *arg){
    return NULL;
}

double  create_join(){
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i = 0; i < NUMS; i++){
        pthread_t thread;
        pthread_create(&thread, NULL, func, NULL);
        pthread_join(thread, NULL);

    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double dur = (double ) ((end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec)) / NUMS;
    printf("Average time per create+join: %f ns\n", dur);
    return dur;
}

int main(){
    double timings[5];

    for(int i = 0; i < 5; i++){
        timings[i] = create_join();
    }

    sort(timings, 0, 4);

    printf("Min: %f ns\n", timings[0]);
    printf("Max: %f ns\n", timings[4]);
    printf("Median: %f ns\n", timings[2]);
}