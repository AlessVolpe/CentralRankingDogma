/*
 * Constrained edge recolorings of periodic one-factorizations (3.0.12).
 * Copyright (C) 2026 Alessandro Volpe
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "calendar_internal.h"
#include "alloc_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int same_pair(fixture_t a,fixture_t b) {
    return (a.home==b.home && a.away==b.away) ||
           (a.home==b.away && a.away==b.home);
}
static int same_matching(const fixture_t *a,const fixture_t *b,size_t count) {
    for(size_t i=0;i<count;++i) {
        int found=0;
        for(size_t j=0;j<count;++j) if(same_pair(a[i],b[j])) {found=1;break;}
        if(!found) return 0;
    }
    return 1;
}

/* Select k distinct positions from an equally frequent group. */
static int select_positions(const calendar_generator_t *g,mtwister_t *rng,
                            size_t requested,size_t positions[3]) {
    const size_t split=g->frequency_group_split;
    const size_t first_length=split,second_length=g->base_rounds-split;
    int first_ok=first_length>=requested,second_ok=second_length>=requested;
    if(!first_ok && !second_ok) return 0;
    size_t start=0U, count=first_length;
    if(!first_ok || (second_ok && calendar_random_bounded(rng,2U)==1U)) {
        start=split;count=second_length;
    }
    for(size_t i=0;i<requested;++i) {
        size_t pick;
        int repeat;
        do {
            pick=start+calendar_random_bounded(rng,(uint32_t)count);
            repeat=0;
            for(size_t j=0;j<i;++j) if(positions[j]==pick) {repeat=1;break;}
        } while(repeat);
        positions[i]=pick;
    }
    return 1;
}

/* In the union of two perfect matchings each vertex has exactly one edge
 * of each color. Hence its components are alternating even cycles. */
static int two_round_move(const calendar_generator_t *g,mtwister_t *rng,
                          fixture_t *base,calendar_diagnostics_t *stats,
                          team_index_t *partner_a,team_index_t *partner_b,
                          size_t *components,size_t *queue) {
    size_t positions[3];
    if(!select_positions(g,rng,2U,positions)) return 0;
    ++stats->two_round_attempts;
    const size_t n=g->n,k=g->fixtures_per_round;
    fixture_t *a=base+positions[0]*k,*b=base+positions[1]*k;
    for(size_t i=0;i<n;++i) {partner_a[i]=UINT32_MAX;partner_b[i]=UINT32_MAX;components[i]=SIZE_MAX;}
    for(size_t i=0;i<k;++i) {
        partner_a[a[i].home]=a[i].away;partner_a[a[i].away]=a[i].home;
        partner_b[b[i].home]=b[i].away;partner_b[b[i].away]=b[i].home;
    }
    size_t count=0U;
    for(size_t v=0;v<n;++v) if(components[v]==SIZE_MAX) {
        size_t head=0U,tail=0U;
        queue[tail++]=v;components[v]=count;
        while(head<tail) {
            size_t x=queue[head++];
            size_t nb[2]={partner_a[x],partner_b[x]};
            for(size_t i=0;i<2U;++i) if(components[nb[i]]==SIZE_MAX) {
                components[nb[i]]=count;queue[tail++]=nb[i];
            }
        }
        ++count;
    }
    if(count==0U) return 0;
    size_t chosen=calendar_random_bounded(rng,(uint32_t)count);
    /* Each component contributes the same number of edges to each round. */
    size_t left=0U,right=0U;
    for(size_t i=0;i<k;++i) if(components[a[i].home]==chosen) queue[left++]=i;
    /* Reuse partner_a as temporary right-side indices: n >= k. */
    for(size_t i=0;i<k;++i) if(components[b[i].home]==chosen) partner_a[right++]=(team_index_t)i;
    if(left!=right) return -EINVAL;
    for(size_t i=0;i<left;++i) {
        fixture_t tmp=a[queue[i]];
        a[queue[i]]=b[partner_a[i]];
        b[partner_a[i]]=tmp;
    }
    if(count>1U) ++stats->two_round_partial_moves;
    else ++stats->two_round_whole_swaps;
    return 0;
}

/* Coloring the 3-factor union is an exact-cover/3-edge-coloring problem.
 * Each vertex must use each of three colors exactly once. Iterative bounded
 * DFS avoids stack exhaustion on large league sizes. */
static int three_round_search(const calendar_generator_t *g, mtwister_t *rng,
                              fixture_t *base,size_t slots[3],
                              calendar_diagnostics_t *stats,
                              fixture_t *edges,fixture_t *result,
                              uint8_t *used,uint8_t *choice,
                              uint8_t *assigned,uint8_t *starts,
                              size_t *order) {
    const size_t n=g->n,k=g->fixtures_per_round,e=3U*k;
    for(size_t a=0;a<3U;++a)
        memcpy(edges+a*k,base+slots[a]*k,k*sizeof(*edges));
    for(size_t a=0;a<e;++a) order[a]=a;
    for(size_t a=e;a>1U;--a) {
        size_t j=calendar_random_bounded(rng,(uint32_t)a);
        size_t tmp=order[a-1U];order[a-1U]=order[j];order[j]=tmp;
    }
    memset(used,0,n);
    memset(choice,0,e);
    for(size_t a=0;a<e;++a) starts[a]=(uint8_t)calendar_random_bounded(rng,3U);
    size_t depth=0U,nodes=0U;
    while(1) {
        if(depth==e) {
            size_t counts[3]={0U,0U,0U};
            for(size_t i=0;i<e;++i) {
                uint8_t color=assigned[i];
                result[color*k+counts[color]++]=edges[order[i]];
            }
            /* Reject all three-round permutations of original groups. */
            int trivial=1;
            for(size_t i=0;i<3U;++i) {
                int found=0;
                for(size_t j=0;j<3U;++j)
                    if(same_matching(result+i*k,base+slots[j]*k,k)) {found=1;break;}
                if(!found) {trivial=0;break;}
            }
            if(!trivial) {
                for(size_t i=0;i<3U;++i)
                    memcpy(base+slots[i]*k,result+i*k,k*sizeof(*result));
                stats->three_round_nodes+=nodes;
                return 1;
            }
            /* Continue searching after trivial factorizations. */
        }
        if(depth==e || choice[depth]==3U) {
            if(depth==0U) {stats->three_round_nodes+=nodes;return 0;}
            if(depth<e) choice[depth]=0U;
            --depth;
            fixture_t m=edges[order[depth]];
            uint8_t bit=(uint8_t)(1U<<assigned[depth]);
            used[m.home]=(uint8_t)(used[m.home]&~bit);
            used[m.away]=(uint8_t)(used[m.away]&~bit);
            continue;
        }
        if(nodes==g->options.three_round_node_limit) {
            stats->three_round_nodes+=nodes;
            ++stats->three_round_search_exhaustions;
            return 0;
        }
        uint8_t color=(uint8_t)((starts[depth]+choice[depth]++)%3U);
        ++nodes;
        fixture_t m=edges[order[depth]];
        uint8_t bit=(uint8_t)(1U<<color);
        if((used[m.home]&bit)!=0U || (used[m.away]&bit)!=0U) continue;
        used[m.home]|=bit;used[m.away]|=bit;
        assigned[depth]=color;
        ++depth;
        if(depth<e) choice[depth]=0U;
    }
}

int calendar_recombine_base(const calendar_generator_t *g,
                            mtwister_t *rng, fixture_t *base,
                            size_t moves,calendar_diagnostics_t *stats) {
    if(moves==0U) return 0;
    const size_t n=g->n,k=g->fixtures_per_round;
    /* Allocate all move workspace through the injectable allocation layer,
     * before any attempt; generator/PRNG state commit is caller-controlled. */
    team_index_t *a=leaderboard_alloc_malloc(n*sizeof(*a));
    team_index_t *b=leaderboard_alloc_malloc(n*sizeof(*b));
    size_t *components=leaderboard_alloc_malloc(n*sizeof(*components));
    size_t *queue=leaderboard_alloc_malloc(n*sizeof(*queue));
    if(a==NULL || b==NULL || components==NULL || queue==NULL) {
        free(a);free(b);free(components);free(queue);return -ENOMEM;
    }
    if(k>SIZE_MAX/3U) {
        free(a);free(b);free(components);free(queue);return -EOVERFLOW;
    }
    size_t triple_size=3U*k;
    fixture_t *edges=NULL,*result=NULL;
    uint8_t *used=NULL,*choice=NULL,*assigned=NULL,*starts=NULL;
    size_t *order=NULL;
    int triple_possible = g->options.enable_three_round &&
        (g->frequency_group_split>=3U || g->base_rounds-g->frequency_group_split>=3U);
    if(triple_possible) {
        if(triple_size>UINT32_MAX ||
           triple_size>SIZE_MAX/sizeof(fixture_t) ||
           triple_size>SIZE_MAX/sizeof(size_t)) {
            free(a);free(b);free(components);free(queue);return -EOVERFLOW;
        }
        edges=leaderboard_alloc_malloc(triple_size*sizeof(*edges));
        result=leaderboard_alloc_malloc(triple_size*sizeof(*result));
        used=leaderboard_alloc_malloc(n*sizeof(*used));
        choice=leaderboard_alloc_malloc(triple_size*sizeof(*choice));
        assigned=leaderboard_alloc_malloc(triple_size*sizeof(*assigned));
        starts=leaderboard_alloc_malloc(triple_size*sizeof(*starts));
        order=leaderboard_alloc_malloc(triple_size*sizeof(*order));
        if(edges==NULL || result==NULL || used==NULL || choice==NULL ||
           assigned==NULL || starts==NULL || order==NULL) {
            free(a);free(b);free(components);free(queue);
            free(edges);free(result);free(used);free(choice);
            free(assigned);free(starts);free(order);return -ENOMEM;
        }
    }
    int status=0;
    for(size_t move=0;move<moves;++move) {
        /* A three-round move is selected in one out of four attempts; the
         * remaining moves are fast alternating-cycle recolorings. */
        int triple=triple_possible && calendar_random_bounded(rng,4U)==0U;
        if(triple) {
            size_t slots[3];
            ++stats->three_round_attempts;
            if(!select_positions(g,rng,3U,slots)) continue;
            for(size_t retry=0;retry<g->options.three_round_attempts;++retry) {
                if(three_round_search(g,rng,base,slots,stats,edges,result,used,
                                      choice,assigned,starts,order)) {
                    ++stats->three_round_moves;break;
                }
            }
        } else {
            status=two_round_move(g,rng,base,stats,a,b,components,queue);
            if(status!=0) break;
        }
    }
    free(a);free(b);free(components);free(queue);
    free(edges);free(result);free(used);free(choice);
    free(assigned);free(starts);free(order);
    return status;
}
