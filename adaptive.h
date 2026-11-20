/* v1.0.0: configurable detour budget and conservative route stability. */
#ifndef FASTGPS_ADAPTIVE_H
#define FASTGPS_ADAPTIVE_H
#include "direction_policy.h"

typedef struct {
    unsigned prefer_forward, adaptive, percent, max_extra, adaptive_percent,
             adaptive_extra, preserve, logging;
} Settings;
static const Settings defaults={1,1,50,1000,100,2000,1,0};
static unsigned bounded(unsigned v,unsigned lo,unsigned hi,unsigned fallback) {
    return v>=lo && v<=hi?v:fallback;
}
static void normalize_settings(Settings *s) {
    s->prefer_forward=bounded(s->prefer_forward,0,1,1);
    s->adaptive=bounded(s->adaptive,0,1,1);
    s->percent=bounded(s->percent,0,200,50);
    s->max_extra=bounded(s->max_extra,0,5000,1000);
    s->adaptive_percent=bounded(s->adaptive_percent,0,200,100);
    s->adaptive_extra=bounded(s->adaptive_extra,0,5000,2000);
    if(s->adaptive_percent<s->percent)s->adaptive_percent=s->percent;
    if(s->adaptive_extra<s->max_extra)s->adaptive_extra=s->max_extra;
    s->preserve=bounded(s->preserve,0,1,1);
    s->logging=bounded(s->logging,0,1,0);
}
static float detour_budget(const Settings *s,float length,unsigned rejects) {
    unsigned level=s->adaptive?(rejects>3?3:rejects):0;
    float pct=(float)s->percent+(float)(s->adaptive_percent-s->percent)*level/3;
    float limit=(float)s->max_extra+(float)(s->adaptive_extra-s->max_extra)*level/3;
    float extra=length*pct/100;
    if(extra<250 && pct>0)extra=250;
    return extra>limit?limit:extra;
}
static int same_target(Point a,Point b) {
    return valid_point(a)&&valid_point(b)&&norm2(a.x-b.x,a.y-b.y)<=1&&absf(a.z-b.z)<=1;
}
typedef struct { Point target; unsigned valid, detour_rejections, forward; } Preference;
static void preference_sync(Preference *p,Point target) {
    if(!p->valid||!same_target(p->target,target)) {
        p->target=target;p->valid=1;p->detour_rejections=0;p->forward=0;
    }
}
static void preference_result(Preference *p,int verdict) {
    p->forward=verdict==ALT_ACCEPT;
    if(verdict==ALT_ACCEPT)p->detour_rejections=0;
    else if(verdict==ALT_DETOUR&&p->detour_rejections<3)++p->detour_rejections;
}
/* Only suppress a MOD-generated departure when the accepted route still
   matches the local road and actual motion. Never suppress native state
   changes, reverse detection, or a new destination. */
static int strong_forward_match(const Direction *d,const Point *route,int n) {
    Point ahead=d->pos;float near=1e20f, aligned=1e20f, projected=1e20f;
    int i,end=n-1;if(n<2||n>200)return 0;if(end>3)end=3;
    ahead.x+=10*d->hx;ahead.y+=10*d->hy;
    for(i=0;i<end;i++) {
        Point a=route[i],b=route[i+1];float x=b.x-a.x,y=b.y-a.y,l=norm2(x,y);
        float dot=x*d->hx+y*d->hy,dist,future;
        if(!valid_point(a)||!valid_point(b))return 0;
        dist=segment_distance2(d->pos,a,b,1);
        if(dist<near)near=dist;
        if(l<0.25f||dot<=0||dot*dot<0.75f*l*norm2(d->hx,d->hy))continue;
        future=segment_distance2(ahead,a,b,1);
        if(dist<aligned){aligned=dist;projected=future;}
    }
    return aligned<=156.25f && aligned<=near+9 && projected<=156.25f && projected<=aligned+9;
}
#endif
