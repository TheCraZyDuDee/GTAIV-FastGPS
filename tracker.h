/* FastGPS v0.3: platform-independent motion-aware route tracking. */
#ifndef FASTGPS_TRACKER_H
#define FASTGPS_TRACKER_H
#include <stdint.h>

typedef struct { float x,y,z,w; } Point;
typedef struct {
    Point sample, destination;
    uint32_t time, last_force;
    float evidence;
    unsigned bad, good, valid, has_target, locked;
} Tracker;

enum { KEEP=0, DEPARTURE=1, REVERSE=2 };
static float absf(float x) { return x<0?-x:x; }
static float norm2(float x,float y) { return x*x+y*y; }
static float norm_approx(float x,float y) {
    x=absf(x); y=absf(y);
    return x>y?x+0.375f*y:y+0.375f*x;
}
static int valid_point(Point p) {
    return absf(p.x)<100000.0f && absf(p.y)<100000.0f && absf(p.z)<100000.0f;
}
static float segment_distance2(Point p,Point a,Point b,int use_z) {
    float x=b.x-a.x,y=b.y-a.y,z=use_z?b.z-a.z:0;
    float u=p.x-a.x,v=p.y-a.y,w=use_z?p.z-a.z:0;
    float l=x*x+y*y+z*z, t=l>0.0001f?(u*x+v*y+w*z)/l:0;
    if(t<0)t=0; if(t>1)t=1;
    u-=x*t; v-=y*t; w-=z*t;
    return u*u+v*v+w*w;
}
static void tracker_pause(Tracker *s) {
    s->valid=0; s->bad=0; s->good=0; s->evidence=0;
}
/* Native state transitions call pause; the direction lock survives a reroute
   until actual movement agrees with the new route or the target changes. */
static int tracker_update(Tracker *s,Point p,Point destination,const Point *route,
                          int count,int use_z,uint32_t now) {
    Point ahead; float dx,dy,dz,m2,m,scale,near=1e20f,future=1e20f;
    int i,n,aligned=0,reverse=0,reason;
    uint32_t dt;
    if(count<2 || count>200 || !valid_point(p) || !valid_point(destination)) {
        tracker_pause(s); return KEEP;
    }
    if(!s->has_target || norm2(destination.x-s->destination.x,destination.y-s->destination.y)>100 ||
       absf(destination.z-s->destination.z)>10) {
        tracker_pause(s); s->locked=0; s->has_target=1; s->destination=destination;
    }
    if(!valid_point(route[count-1]) || (count<=3 &&
       norm2(p.x-route[count-1].x,p.y-route[count-1].y)<100 &&
       (!use_z || absf(p.z-route[count-1].z)<5))) {
        tracker_pause(s); return KEEP;
    }
    if(!s->valid) { s->sample=p; s->time=now; s->valid=1; return KEEP; }
    dt=now-s->time;
    if(dt<100)return KEEP;
    dx=p.x-s->sample.x; dy=p.y-s->sample.y; dz=p.z-s->sample.z;
    m2=norm2(dx,dy);
    if(dt>500 || m2>1600 || absf(dz)>20) {
        tracker_pause(s); s->sample=p; s->time=now; s->valid=1; return KEEP;
    }
    if(m2<1.44f || m2<16.0f*(float)dt*(float)dt/1000000.0f) {
        if(dt>=300) tracker_pause(s);
        return KEEP;
    }
    s->sample=p; s->time=now;
    m=norm_approx(dx,dy); scale=10.0f/m;
    ahead=p; ahead.x+=dx*scale; ahead.y+=dy*scale;
    if(use_z) { float rise=dz*scale; if(rise>5)rise=5; if(rise< -5)rise=-5; ahead.z+=rise; }
    n=count-1; if(n>11)n=11;
    for(i=0;i<n;i++) {
        Point a=route[i], b=route[i+1]; float d,f,x,y,l,dot;
        if(!valid_point(a)||!valid_point(b)) { tracker_pause(s); return KEEP; }
        d=segment_distance2(p,a,b,use_z); f=segment_distance2(ahead,a,b,use_z);
        if(d<near)near=d; if(f<future)future=f;
        x=b.x-a.x; y=b.y-a.y; l=norm2(x,y); dot=dx*x+dy*y;
        if(d<=100 && l>0.25f && dot*dot>0.25f*m2*l) {
            if(dot>0)aligned=1; else reverse=1;
        }
    }
    /* Backward progress on the route, or movement predicted to leave the
       corridor. Require actual lateral deviation for the latter. */
    reason=(!aligned && reverse && near<=64)?REVERSE:
           (near>=9 && future>144 && future>near+36 && (!aligned || near>64))?DEPARTURE:KEEP;
    if(!reason) {
        s->bad=0; s->evidence=0;
        if(aligned && near<=64 && future<=144) {
            if(s->good<2)s->good++;
            if(s->good>=2 && now-s->last_force>=1200)s->locked=0;
        } else s->good=0;
        return KEEP;
    }
    s->good=0;
    if(s->locked)return KEEP;
    if(s->bad<3)s->bad++;
    s->evidence+=m;
    if(s->bad>=2 && s->evidence>=4.0f) {
        s->locked=1; s->last_force=now; s->bad=0; s->evidence=0;
        return reason;
    }
    return KEEP;
}
#endif
