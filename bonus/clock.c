#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef long long ll;
#define NUMS 50000000

char *names[] = {"CLOCK_REALTIME", "CLOCK_MONOTONIC", "CLOCK_MONOTONIC_RAW"};
clockid_t  vals[] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW};

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

double bench(clockid_t id, char* name){
    struct timespec start, end, tmp;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i = 0; i < NUMS; i++){
        clock_gettime(id, &tmp);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double dur =  (double )((end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec)) /NUMS;
    printf("Average duration per call with %s: %f ns\n", name, dur);
    return dur;
}



int main(){
    double res[3][5];
    for(int v = 0; v < 3; v++)
        for(int i = 0; i < 5; i++)
            res[v][i] = bench(vals[v], names[v]);

    sort(res[0], 0, 4);
    sort(res[1], 0, 4);
    sort(res[2], 0, 4);
    for(int i = 0; i < 3; i++) {
        printf("Min duration with arg %s is %f ns\n", names[i], res[i][0]);
        printf("Max duration with arg %s is %f ns\n", names[i], res[i][4]);
        printf("Median duration with arg %s is %f ns\n", names[i], res[i][2]);
    }

}
