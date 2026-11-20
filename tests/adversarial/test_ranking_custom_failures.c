/* Fault-injection rollback for custom ranking insert and update. */
#include "ranking.h"
#include "alloc_internal.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(p) do {if(!(p)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#p);return EXIT_FAILURE;}}while(0)
typedef struct key { int64_t a,b; } test_key_t;
static int compare(const void *lhs,const void *rhs,void *context) {
    (void)context;
    const test_key_t *a=lhs,*b=rhs;
    if(a->a!=b->a) return a->a>b->a?-1:1;
    if(a->b!=b->b) return a->b>b->b?-1:1;
    return 0;
}
static int attempt(size_t at, int updating) {
    ranking_t *r=ranking_new_custom(sizeof(test_key_t),compare,NULL);
    CHECK(r!=NULL);
    const test_key_t before={2,INT64_MIN}, other={5,INT64_MAX};
    CHECK(ranking_upsert_key(r,"alpha",&before)==0);
    CHECK(ranking_upsert_key(r,"beta",&other)==0);
    const char *borrowed=NULL;
    CHECK(ranking_at_key(r,1U,&borrowed,NULL)==0 && strcmp(borrowed,"alpha")==0);
    const char *saved=borrowed;
    const test_key_t replacement={8,INT64_MAX};
    leaderboard_alloc_fail_after(at);
    int rc=ranking_upsert_key(r,updating?"alpha":"gamma",&replacement);
    leaderboard_alloc_reset();
    CHECK(rc==0 || rc==-ENOMEM);
    test_key_t observed={0};size_t position=SIZE_MAX;
    CHECK(ranking_get_key(r,"alpha",&observed,&position)==0);
    if(rc==-ENOMEM) {
        CHECK(position==1U && observed.a==before.a && observed.b==before.b);
        CHECK(ranking_size(r)==2U);
        CHECK(ranking_get_key(r,"gamma",NULL,NULL)==-ENOENT);
    } else {
        if(updating) {
            CHECK(position==0U && observed.a==replacement.a && observed.b==replacement.b);
            CHECK(ranking_at_key(r,0U,&borrowed,NULL)==0 && borrowed==saved);
        } else CHECK(ranking_size(r)==3U);
    }
    ranking_destroy(r);
    return rc;
}
int main(void) {
    for(int updating=0;updating<=1;++updating) {
        size_t failures=0U;int success=0;
        for(size_t at=0U;at<32U;++at) {
            const int result=attempt(at,updating);
            if(result==-ENOMEM) ++failures;
            else if(result==0) {success=1;break;}
            else return EXIT_FAILURE;
        }
        CHECK(success && failures>=2U);
    }
    return EXIT_SUCCESS;
}
