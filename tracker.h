/* FastGPS v0.4: platform-independent motion-aware route tracking. */
#ifndef FASTGPS_TRACKER_H
#define FASTGPS_TRACKER_H
#include <stdint.h>

typedef struct { float x,y,z,w; } Point;
typedef struct {
    Point sample, destination, last_force_pos;
    uint32_t time, last_force;
    float evidence, force_dx, force_dy;
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
/* Keep retry history across a native recalculation, but never let a stale
   route permanently suppress off-route detection. */
static int tracker_update(Tracker *s,Point p,Point destination,const Point *route,
                          int count,int use_z,uint32_t now) {
    Point ahead; float dx,dy,dz,m2,m,scale,near=1e20f,future=1e20f;
    float aligned_near=1e20f, aligned_future=1e20f, reverse_near=1e20f;
    int i,n,aligned,reverse,reason;
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
        if(m2>1600 || absf(dz)>20)s->locked=0;
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
            if(dot>0 && d<aligned_near) { aligned_near=d; aligned_future=f; }
            else if(dot<0 && d<reverse_near)reverse_near=d;
        }
    }
    /* A nearby but unrelated leg must not mask the closest route. Also
       require forward projection to stay close: passing the END of a leg
       is not evidence of following it, even if its tangent points forward. */
    aligned=aligned_near<=near+16.0f && aligned_future<=aligned_near+36.0f;
    reverse=reverse_near<=near+16.0f;
    reason=(!aligned && reverse && near<=64)?REVERSE:
           (near>=9 && !aligned && (future>144 && future>near+36))?DEPARTURE:KEEP;
    /* A nearby route loop can keep the global future distance small while
       the current local leg is already behind. Require a meaningful gap
       to an aligned leg, and evidence of moving away from the closest leg. */
    if(!reason && !aligned && near>=16 && aligned_near>near+16 && future>near+16)
        reason=DEPARTURE;
    if(!reason) {
        s->bad=0; s->evidence=0;
        if(aligned && near<=64 && aligned_future<=144) {
            if(s->good<2)s->good++;
            if(s->good>=2 && now-s->last_force>=350)s->locked=0;
        } else s->good=0;
        return KEEP;
    }
    s->good=0;
    if(s->bad<3)s->bad++;
    s->evidence+=m;
    if(s->bad>=2 && s->evidence>=4.0f) {
        if(s->locked) {
            uint32_t age=now-s->last_force;
            float moved=norm2(p.x-s->last_force_pos.x,p.y-s->last_force_pos.y);
            float dot=dx*s->force_dx+dy*s->force_dy;
            float old2=norm2(s->force_dx,s->force_dy);
            int turned=dot<=0 || dot*dot<0.49f*m2*old2;
            /* No repeated requests for a straight, legitimate U-turn.
               A new heading can override the lock; persistent departure
               retries require both elapsed time and new displacement. */
            if(!(age>=350 && moved>=16 && turned) &&
               !(reason==DEPARTURE && age>=650 && moved>=36))return KEEP;
        }
        s->locked=1; s->last_force=now; s->last_force_pos=p;
        s->force_dx=dx; s->force_dy=dy;
        s->bad=0; s->evidence=0;
        return reason;
    }
    return KEEP;
}
#endif
