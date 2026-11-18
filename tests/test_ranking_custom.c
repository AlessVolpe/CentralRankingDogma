/* Composite-key ranking and model-based differential tests, milestone 3.1.2. */
#include "ranking.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}}while(0)

typedef struct custom_key {
    uint32_t first;
    int64_t second;
} custom_key_t;
static int key_cmp(const void *lhs, const void *rhs, void *ctx) {
    const custom_key_t *a=lhs, *b=rhs;
    const int reversed=ctx != NULL ? *(const int *)ctx : 0;
    if(a->first!=b->first) return a->first>b->first ? (reversed?1:-1) : (reversed?-1:1);
    if(a->second!=b->second) return a->second>b->second ? (reversed?1:-1) : (reversed?-1:1);
    return 0;
}
enum { COUNT=41, STEPS=1500 };
static char ids[COUNT][16];
static custom_key_t values[COUNT];
static unsigned char present[COUNT];
static uint32_t prng(uint32_t *state) {
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;return *state=x;
}
static int model_order(const void *a, const void *b) {
    const unsigned i=*(const unsigned *)a,j=*(const unsigned *)b;
    const int cmp=key_cmp(&values[i],&values[j],NULL);
    return cmp != 0 ? cmp : strcmp(ids[i],ids[j]);
}
static int verify(ranking_t *r) {
    unsigned order[COUNT];size_t count=0U;
    for(unsigned i=0U;i<COUNT;++i) if(present[i]) order[count++]=i;
    qsort(order,count,sizeof(order[0]),model_order);
    CHECK(ranking_size(r)==count);
    CHECK(ranking_at_key(r,count,NULL,NULL)==-ENOENT);
    for(size_t pos=0U;pos<count;++pos) {
        const char *id=NULL;custom_key_t copy={0};size_t got=SIZE_MAX;
        CHECK(ranking_at_key(r,pos,&id,&copy)==0);
        CHECK(strcmp(id,ids[order[pos]])==0);
        CHECK(copy.first==values[order[pos]].first && copy.second==values[order[pos]].second);
        copy=(custom_key_t){0};
        CHECK(ranking_get_key(r,id,&copy,&got)==0 && got==pos);
        CHECK(copy.first==values[order[pos]].first && copy.second==values[order[pos]].second);
    }
    return EXIT_SUCCESS;
}
int main(void) {
    CHECK(ranking_new_custom(0U,key_cmp,NULL)==NULL);
    CHECK(ranking_new_custom(sizeof(custom_key_t),NULL,NULL)==NULL);
    ranking_t *scalar=ranking_new();CHECK(scalar!=NULL);
    custom_key_t value={UINT32_MAX,INT64_MIN};
    CHECK(ranking_upsert_key(scalar,"a",&value)==-EINVAL);
    CHECK(ranking_get_key(scalar,"a",NULL,NULL)==-EINVAL);
    CHECK(ranking_at_key(scalar,0U,NULL,NULL)==-EINVAL);
    ranking_destroy(scalar);
    ranking_t *r=ranking_new_custom(sizeof(custom_key_t),key_cmp,NULL);
    CHECK(r!=NULL);
    CHECK(ranking_upsert(r,"a",2)==-EINVAL);
    CHECK(ranking_get(r,"a",NULL,NULL)==-EINVAL);
    CHECK(ranking_at(r,0U,NULL,NULL)==-EINVAL);
    CHECK(ranking_upsert_key(r,"",&value)==-EINVAL);
    CHECK(ranking_upsert_key(r,"a",NULL)==-EINVAL);
    CHECK(ranking_get_key(r,"",NULL,NULL)==-EINVAL);
    CHECK(ranking_upsert_key(r,"zzz",&value)==0);
    CHECK(ranking_upsert_key(r,"aaa",&value)==0);
    const char *id=NULL; CHECK(ranking_at_key(r,0U,&id,NULL)==0 && strcmp(id,"aaa")==0);
    CHECK(ranking_remove(r,"zzz")==0 && ranking_remove(r,"aaa")==0);
    CHECK(ranking_size(r)==0U);
    for(unsigned i=0U;i<COUNT;++i) (void)snprintf(ids[i],sizeof(ids[i]),"id-%03u",i);
    uint32_t seed=0x4e672135U;
    for(unsigned step=0U;step<STEPS;++step) {
        const uint32_t rnum=prng(&seed);const unsigned i=rnum%COUNT;
        if((rnum>>12)%6U==0U) {
            CHECK(ranking_remove(r,ids[i])==(present[i]?0:-ENOENT));
            present[i]=0U;
        } else {
            value.first=(rnum>>20)%7U;
            value.second=(rnum&15U)==0U?INT64_MIN:
                         (rnum&15U)==1U?INT64_MAX:(int64_t)(int)(prng(&seed)%11U)-5;
            CHECK(ranking_upsert_key(r,ids[i],&value)==0);
            values[i]=value;present[i]=1U;
        }
        if(step%11U==0U) CHECK(verify(r)==EXIT_SUCCESS);
    }
    CHECK(verify(r)==EXIT_SUCCESS);
    /* Comparator-equivalent updates are no-ops on positions but must copy
     * the entire new key (including representation differences). */
    for(unsigned i=0U;i<COUNT;++i) if(present[i]) {
        custom_key_t got={0};
        CHECK(ranking_get_key(r,ids[i],&got,NULL)==0);
        CHECK(got.first==values[i].first && got.second==values[i].second);
    }
    ranking_destroy(r);
    puts("custom ranking differential: passed");
    return EXIT_SUCCESS;
}
