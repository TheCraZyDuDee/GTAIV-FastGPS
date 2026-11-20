#include <assert.h>
#include <stdio.h>
#include "tracker.h"
#define P(x,y,z) ((Point){x,y,z,0})
static int step(Tracker *s,Point p,const Point *route,int count,unsigned t) {
    return tracker_update(s,p,route[count-1],route,count,0,t);
}
int main(void) {
    Point straight[]={P(-50,0,0),P(0,0,0),P(50,0,0),P(100,0,0)};
    Point corner[]={P(-50,0,0),P(0,0,0),P(0,50,0),P(0,100,0)};
    Tracker s={0}; int r=0;
    assert(step(&s,P(0,0,0),straight,4,0)==KEEP);
    assert(step(&s,P(0,2,0),straight,4,100)==KEEP);
    assert(step(&s,P(0,4,0),straight,4,200)==KEEP);
    assert(step(&s,P(0,6,0),straight,4,300)==DEPARTURE);
    assert(step(&s,P(0,8,0),straight,4,400)==KEEP);
    puts("PASS wrong turn: early reroute at six units, then locked");
    s=(Tracker){0};
    step(&s,P(10,0,0),straight,4,0);
    assert(step(&s,P(8,0,0),straight,4,100)==KEEP);
    assert(step(&s,P(6,0,0),straight,4,200)==REVERSE);
    tracker_pause(&s);
    assert(step(&s,P(4,0,0),straight,4,300)==KEEP);
    assert(step(&s,P(2,0,0),straight,4,400)==KEEP);
    assert(step(&s,P(0,0,0),straight,4,500)==KEEP);
    puts("PASS reverse progress: reroute at four units; no repeat after route refresh");
    tracker_pause(&s);
    step(&s,P(0,0,0),straight,4,1500);
    assert(step(&s,P(2,0,0),straight,4,1600)==KEEP);
    assert(step(&s,P(4,0,0),straight,4,1700)==KEEP);
    assert(!s.locked);
    assert(step(&s,P(2,0,0),straight,4,1800)==KEEP);
    assert(step(&s,P(0,0,0),straight,4,1900)==REVERSE);
    puts("PASS direction lock rearms after stable forward movement and cooldown");
    s=(Tracker){0};
    for(unsigned i=0;i<10;i++)assert(step(&s,P(-10+(int)i*2,0,0),straight,4,i*100)==KEEP);
    puts("PASS straight driving");
    s=(Tracker){0};
    Point turn[]={P(-6,0,0),P(-4,0,0),P(-2,0,0),P(-1,1,0),P(0,3,0),P(0,5,0),P(0,7,0)};
    for(unsigned i=0;i<sizeof turn/sizeof *turn;i++)assert(step(&s,turn[i],corner,4,i*100)==KEEP);
    puts("PASS correct turn");
    s=(Tracker){0};
    for(unsigned i=0;i<10;i++)assert(step(&s,P((int)i*2,(int)i<4?(int)i:4,0),straight,4,i*100)==KEEP);
    puts("PASS lane change");
    s=(Tracker){0};
    for(unsigned i=0;i<20;i++)assert(step(&s,P(5,5,0),straight,4,i*100)==KEEP);
    puts("PASS stationary");
    s=(Tracker){0};step(&s,P(0,0,0),straight,4,0);
    assert(step(&s,P(500,500,0),straight,4,100)==KEEP);
    assert(step(&s,P(500,502,0),straight,4,1000)==KEEP);
    puts("PASS teleport and long frame gap reset");
    s=(Tracker){0};Point end[]={P(0,0,0),P(20,0,0)};
    for(unsigned i=0;i<4;i++)assert(step(&s,P(14-(int)i,0,0),end,2,i*100)==KEEP);
    puts("PASS near arrival");
    s=(Tracker){0};
    step(&s,P(0,0,0),straight,4,0xfffffff0U);
    assert(step(&s,P(0,2,0),straight,4,84)==KEEP);
    assert(step(&s,P(0,4,0),straight,4,184)==KEEP);
    assert(step(&s,P(0,6,0),straight,4,284)==DEPARTURE);
    puts("PASS unsigned game timer wrap");
    s=(Tracker){0};
    Point bridge[]={P(-100,0,0),P(100,0,0),P(100,100,0),P(100,200,0)};
    assert(tracker_update(&s,P(0,0,30),bridge[3],bridge,4,1,0)==KEEP);
    assert(tracker_update(&s,P(0,2,30),bridge[3],bridge,4,1,100)==KEEP);
    r=tracker_update(&s,P(0,4,30),bridge[3],bridge,4,1,200);
    assert(r==DEPARTURE); /* Native distance rule normally handles this first. */
    assert(segment_distance2(P(0,0,30),bridge[0],bridge[1],1)==900);
    assert(segment_distance2(P(0,0,30),bridge[0],bridge[1],0)==0);
    puts("PASS 3D segment distance use");
    return 0;
}
