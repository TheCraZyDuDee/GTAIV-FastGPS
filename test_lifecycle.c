#include <assert.h>
#include <stdio.h>
#include "lifecycle.h"
#define P(x,y,z) ((Point){x,y,z,0})
int main(void) {
    Point forward[]={P(0,0,0),P(100,0,0),P(100,50,0)};
    Point reverse[]={P(100,0,0),P(0,0,0),P(0,50,0)};
    Point dest=P(100,50,0); RouteWatch w={0};
    assert(!route_conflict(P(30,1,0),forward,3,0,2,0));
    assert(route_conflict(P(30,1,0),reverse,3,0,2,0));
    assert(route_conflict(P(30,8,0),forward,3,0,0,2));
    puts("PASS forward, backwards, wrong turn");
    assert(!watch_retry(&w,P(0,0,0),dest,reverse,3,0,2,0,0));
    assert(!watch_retry(&w,P(30,0,0),dest,reverse,3,0,2,0,900));
    assert(watch_retry(&w,P(35,0,0),dest,reverse,3,0,2,0,1100));
    assert(!watch_retry(&w,P(40,0,0),dest,reverse,3,0,2,0,2200));
    assert(watch_retry(&w,P(65,0,0),dest,reverse,3,0,2,0,2300));
    puts("PASS sustained reverse retries are bounded by time AND distance");
    watch_reset(&w,P(0,0,0),dest,0);
    assert(!watch_retry(&w,P(40,0,0),dest,forward,3,0,2,0,2000));
    assert(!watch_retry(&w,P(70,0,0),P(50,50,0),reverse,3,0,2,0,4000));
    assert(!watch_retry(&w,P(1000,0,0),dest,reverse,3,0,2,0,6000));
    puts("PASS normal route, changed target and teleport reset");
    return 0;
}
