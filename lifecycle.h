/* Independent bounded retry policy for a stale or backwards route. */
#ifndef FASTGPS_LIFECYCLE_H
#define FASTGPS_LIFECYCLE_H
#include "tracker.h"
typedef struct { Point origin, target; uint32_t since; unsigned valid; } RouteWatch;
/* Inspect the first eleven segments, but don't let remote loops make a
   strictly backward local leg count as forward progress. */
static int route_conflict(Point p,const Point *route,int n,int use_z,float dx,float dy) {
    float m=norm2(dx,dy),near=1e20f,forward=1e20f,back=1e20f;
    float forward_future=1e20f, future=1e20f;
    Point q=p; int i;
    if(n<2||n>200||!valid_point(p)||m<0.01f)return 0;
    q.x+=dx*10/norm_approx(dx,dy);q.y+=dy*10/norm_approx(dx,dy);
    if(n>12)n=12;
    for(i=0;i<n-1;i++) {
        Point a=route[i],b=route[i+1];float d,f,x,y,l,dot;
        if(!valid_point(a)||!valid_point(b))return 0;
        d=segment_distance2(p,a,b,use_z);f=segment_distance2(q,a,b,use_z);
        if(d<near)near=d;if(f<future)future=f;
        x=b.x-a.x;y=b.y-a.y;l=norm2(x,y);dot=dx*x+dy*y;
        if(l<0.25f||dot*dot<0.36f*m*l)continue;
        if(dot>0 && d<forward) {forward=d;forward_future=f;}
        if(dot<0 && d<back)back=d;
    }
    if(forward<=near+9 && forward_future<=forward+36)return 0;
    if(back<=near+9 && near<=144)return 1;
    return near>=16 && future>near+16;
}
static void watch_reset(RouteWatch *w,Point p,Point target,uint32_t now) {
    w->origin=p;w->target=target;w->since=now;w->valid=1;
}
static int watch_retry(RouteWatch *w,Point p,Point target,const Point *route,int n,
                       int use_z,float dx,float dy,uint32_t now) {
    float moved;
    if(!valid_point(p)||!valid_point(target)||n<2||n>200) {w->valid=0;return 0;}
    if(!w->valid||norm2(target.x-w->target.x,target.y-w->target.y)>100||
       absf(target.z-w->target.z)>10) {watch_reset(w,p,target,now);return 0;}
    moved=norm2(p.x-w->origin.x,p.y-w->origin.y);
    if(moved>10000) {watch_reset(w,p,target,now);return 0;}
    if(now-w->since<1000||moved<625)return 0;
    if(!route_conflict(p,route,n,use_z,dx,dy)) {watch_reset(w,p,target,now);return 0;}
    watch_reset(w,p,target,now);return 1;
}
#endif
