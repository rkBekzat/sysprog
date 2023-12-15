#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef long long ll;

#define NUMS 10000000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;



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

void * inc(void *arg){
    (void)arg;
    while (counter < NUMS){
        pthread_mutex_lock(&mutex);
        counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

double bench(int n, pthread_t *threads){
    struct  timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for(int i = 0; i < n; i++)
        pthread_create(&threads[i], NULL, inc, NULL);

    for(int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &end);
    ll dur = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    double  per_lock = (double)dur / (n);
    printf("Threads quantity: %d, Time per lock: %.6f ns\n", n, per_lock);
    return per_lock;
}

int main(){
    int n;
    printf("Write the number of threads:");
    scanf("%d", &n);
    pthread_t threads[n];
    double res[20];
    for(int i = 0; i < 20; i++){
        counter = 0;
        res[i] = bench(n, threads);
    }

    sort(res, 0, 19);

    printf("Min: %f ns\n", res[0]);
    printf("Max: %f ns\n", res[19]);
    printf("Median: %f ns\n", res[10]);
}