/* Road-edge matching for a forward start; independent of game memory. */
#ifndef FASTGPS_ROAD_START_H
#define FASTGPS_ROAD_START_H
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
/* An edge must actually contain the player and lead forward. The chosen node
   is the immediate endpoint, not a fabricated position or a farther junction. */
static float forward_edge_score(Point p,Point a,Point b,float dx,float dy) {
    float ex=b.x-a.x,ey=b.y-a.y,l=norm2(ex,ey),m=norm2(dx,dy);
    float ax=a.x-p.x,ay=a.y-p.y,bx=b.x-p.x,by=b.y-p.y;
    float dot=ex*dx+ey*dy,t,lat,z,align;
    if(!valid_point(p)||!valid_point(a)||!valid_point(b)||l<4||l>14400||m<0.01f)return -1;
    if(dot<=0 || dot*dot<0.75f*l*m)return -1;
    if(ax*dx+ay*dy>=0 || (ax*dx+ay*dy)*(ax*dx+ay*dy)<4*m)return -1;
    if(bx*dx+by*dy<=0 || (bx*dx+by*dy)*(bx*dx+by*dy)<9*m)return -1;
    t=-(ax*ex+ay*ey)/l;
    if(t<=0.02f||t>=0.98f)return -1;
    lat=norm2(ax+t*ex,ay+t*ey);z=a.z+t*(b.z-a.z)-p.z;
    if(lat>25 || absf(z)>4)return -1;
    align=dot*dot/(l*m);
    return lat+z*z+16*(1-align);
}
/* Do not select a route whose second point immediately doubles back past the
   actual vehicle. A native U-turn remains available in the unmodified search. */
static int doubles_back(Point p,Point a,Point b,float dx,float dy) {
    float ex=b.x-a.x,ey=b.y-a.y,dot=ex*dx+ey*dy;
    float m=norm2(dx,dy),l=norm2(ex,ey);
    return dot<0 && dot*dot>0.5f*m*l && (b.x-p.x)*dx+(b.y-p.y)*dy<0;
}
#endif
