/* Player motion and segment projection; independent of game memory. */
#ifndef FASTGPS_MOTION_H
#define FASTGPS_MOTION_H
#include "tracker.h"

typedef struct { Point sample; float dx,dy; uint32_t time, last; int valid, moving; } RoadMotion;
static void motion_sample(RoadMotion *m,Point p,uint32_t now) {
    uint32_t dt=now-m->time;
    float dx=p.x-m->sample.x,dy=p.y-m->sample.y,dz=p.z-m->sample.z, d=norm2(dx,dy);
    if(!m->valid || dt>500 || d>1600 || absf(dz)>20) {
        m->sample=p;m->time=now;m->valid=1;m->moving=0;return;
    }
    if(dt<100)return;
    if(d<1.0f) { if(dt>=300)m->moving=0; return; }
    m->sample=p;m->time=now;
    m->moving=d>=16.0f*(float)dt*(float)dt/1000000.0f;
    m->dx=dx;m->dy=dy;m->last=now;
}
static Point edge_projection(Point p,Point a,Point b) {
    float x=b.x-a.x,y=b.y-a.y,l=norm2(x,y),t=l>0.001f?((p.x-a.x)*x+(p.y-a.y)*y)/l:0;
    Point q;
    if(t<0)t=0;if(t>1)t=1;
    q.x=a.x+t*x;q.y=a.y+t*y;q.z=a.z+t*(b.z-a.z);q.w=0;return q;
}
#endif
