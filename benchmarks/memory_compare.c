/*
 * LeaderboardSystem isolated one-backend heap-peak workload (3.1.2).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Massif profiles one backend per process; timing data from this executable
 * are NOT scientific performance evidence.
 */
#include "ranking.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_N = 100000 };
typedef struct score_key {
    uint32_t points;
    uint32_t fantasy;
    int64_t difference;
} score_key_t;
typedef struct data {
    char id[24];
    score_key_t key;
} data_t;
static void check(int ok, const char *message) {
    if (!ok) { fprintf(stderr,"memory_compare: %s\n",message); exit(EXIT_FAILURE); }
}
static uint32_t rng(uint32_t *s) {
    uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; return *s=x;
}
static int cmp_key(const void *a,const void *b,void *ignored) {
    const score_key_t *x=a,*y=b;
    (void)ignored;
    if(x->points!=y->points) return x->points>y->points?-1:1;
    if(x->fantasy!=y->fantasy) return x->fantasy>y->fantasy?-1:1;
    if(x->difference!=y->difference) return x->difference>y->difference?-1:1;
    return 0;
}
static int cmp_index(const data_t *d, uint32_t a, uint32_t b) {
    int result=cmp_key(&d[a].key,&d[b].key,NULL);
    return result ? result : strcmp(d[a].id,d[b].id);
}
static void sift(uint32_t *arr,size_t root,size_t end,const data_t *d) {
    while(root<end/2U){
        size_t child=root*2U+1U,late=root;
        if(cmp_index(d,arr[late],arr[child])<0) late=child;
        if(child+1U<end && cmp_index(d,arr[late],arr[child+1U])<0) late=child+1U;
        if(late==root) return;
        uint32_t tmp=arr[root];arr[root]=arr[late];arr[late]=tmp;
        root=late;
    }
}
static void heap_sort(uint32_t *v,size_t n,const data_t *d){
    for(size_t i=n/2U;i>0U;--i)sift(v,i-1U,n,d);
    for(size_t end=n;end>1U;--end){
        uint32_t t=v[0];v[0]=v[end-1U];v[end-1U]=t;
        sift(v,0U,end-1U,d);
    }
}
static size_t parse_count(const char *value) {
    char *end=NULL; errno=0; unsigned long n=strtoul(value,&end,10);
    check(!errno && end!=value && !*end && n>=2UL && n<=MAX_N,"count 2..100000");
    return (size_t)n;
}
static uint32_t parse_seed(const char *value){
    char *end=NULL;errno=0;unsigned long n=strtoul(value,&end,10);
    check(!errno && end!=value && !*end && n>0 && n<=UINT32_MAX,"seed 1..UINT32_MAX");
    return (uint32_t)n;
}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: %s <count> <heap|avl> <seed>\n",argv[0]);return 2;}
    const size_t n=parse_count(argv[1]);
    const int heap=strcmp(argv[2],"heap")==0;
    check(heap||strcmp(argv[2],"avl")==0,"backend must be heap or avl");
    uint32_t state=parse_seed(argv[3]);
    data_t *d=calloc(n,sizeof(*d));
    uint32_t *permutation=malloc(n*sizeof(*permutation));
    check(d && permutation,"input allocation failed");
    for(size_t i=0;i<n;++i){
        uint32_t r=rng(&state);
        int len=snprintf(d[i].id,sizeof(d[i].id),"team-%08" PRIx32,(uint32_t)i);
        check(len>0 && (size_t)len<sizeof(d[i].id),"id formatting failed");
        d[i].key.points=r%40U;
        d[i].key.fantasy=500U+(r>>8)%500U;
        d[i].key.difference=(int64_t)(int32_t)(r%51U)-25;
        permutation[i]=(uint32_t)i;
    }
    for(size_t i=n;i>1U;--i){
        size_t j=rng(&state)%i;
        uint32_t tmp=permutation[i-1U];permutation[i-1U]=permutation[j];permutation[j]=tmp;
    }
    uint64_t checksum=0;
    if(heap){
        uint32_t *order=malloc(n*sizeof(*order));
        size_t *positions=malloc(n*sizeof(*positions));
        check(order && positions,"heap ranking allocation failed");
        memcpy(order,permutation,n*sizeof(*order));
        heap_sort(order,n,d);
        for(size_t i=0;i<n;++i){
            if(i) check(cmp_index(d,order[i-1U],order[i])<0,"heap order");
            positions[order[i]]=i;
            checksum+=(uint64_t)(i+1U)*(uint64_t)(order[i]+1U);
        }
        for(size_t i=0;i<n;++i)check(positions[order[i]]==i,"heap index");
        free(positions);free(order);
    }else{
        ranking_t *rank=ranking_new_custom(sizeof(score_key_t),cmp_key,NULL);
        check(rank!=NULL,"AVL constructor failed");
        for(size_t i=0;i<n;++i){
            uint32_t j=permutation[i];
            check(ranking_upsert_key(rank,d[j].id,&d[j].key)==0,"AVL insert failed");
        }
        check(ranking_size(rank)==n,"AVL size mismatch");
        for(size_t i=0;i<n;++i){
            const char *id=NULL;
            score_key_t key;
            uint32_t index=0;
            check(ranking_at_key(rank,i,&id,&key)==0,"AVL rank selection failed");
            check(sscanf(id,"team-%" SCNx32,&index)==1,"AVL id invalid");
            /* Read back data index safely without relying on pointer ownership. */
            check(index<n && cmp_key(&key,&d[index].key,NULL)==0,"AVL wrong key");
            checksum+=(uint64_t)(i+1U)*(uint64_t)(index+1U);
        }
        ranking_destroy(rank);
    }
    printf("backend,count,seed,checksum\n%s,%zu,%s,%" PRIu64 "\n",argv[2],n,argv[3],checksum);
    free(permutation);free(d);
    return 0;
}
