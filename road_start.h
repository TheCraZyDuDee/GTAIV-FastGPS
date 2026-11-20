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
static int road_tile(float coord) {
    int tile=(int)((coord+3000.0f)*(1.0f/750.0f));
    return tile<0?0:tile>7?7:tile;
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
/* A route chosen at the forward endpoint must retain the remaining portion
   of the actual matched edge, otherwise native tracking sees an artificial gap. */
typedef struct {
    uint32_t best, second;
    float best_score,second_score;
    Point prefix,end;
} EdgeChoice;
static void edge_choice_init(EdgeChoice *c,uint32_t original) {
    c->best=original;c->second=0xffffffffU;c->best_score=c->second_score=1e10f;
}
static Point edge_projection(Point p,Point a,Point b) {
    float x=b.x-a.x,y=b.y-a.y,l=norm2(x,y),t=l>0.001f?((p.x-a.x)*x+(p.y-a.y)*y)/l:0;
    Point q;
    if(t<0)t=0;if(t>1)t=1;
    q.x=a.x+t*x;q.y=a.y+t*y;q.z=a.z+t*(b.z-a.z);q.w=0;return q;
}
static void edge_choice_offer(EdgeChoice *c,uint32_t id,float score,Point prefix,Point end) {
    if(id==c->best) {
        if(score<c->best_score){c->best_score=score;c->prefix=prefix;c->end=end;}
        return;
    }
    if(score<c->best_score) {
        c->second=c->best;c->second_score=c->best_score;
        c->best=id;c->best_score=score;c->prefix=prefix;c->end=end;
    } else if(score<c->second_score) {
        c->second=id;c->second_score=score;
    }
}
static int edge_choice_unique(const EdgeChoice *c) {
    return c->best_score<1e9f && c->second_score-c->best_score>=2.0f;
}
static int prepend_road(Point *route,unsigned char *flags,int max,int *count,Point prefix,Point end) {
    int i,n=*count;
    if(max<2||max>200||n<2||n>=max||!valid_point(prefix)||!valid_point(end)||!valid_point(route[0])||
       norm2(route[0].x-end.x,route[0].y-end.y)>0.01f||absf(route[0].z-end.z)>0.01f||
       norm2(prefix.x-end.x,prefix.y-end.y)<1.0f)return 0;
    for(i=n;i>0;i--){route[i]=route[i-1];flags[i]=flags[i-1];}
    route[0]=prefix;flags[0]=(unsigned char)(2|(flags[1]&0x80));*count=n+1;
    return 1;
}
/* Do not select a route whose second point immediately doubles back past the
   actual vehicle. A native U-turn remains available in the unmodified search. */
static int doubles_back(Point p,Point a,Point b,float dx,float dy) {
    float ex=b.x-a.x,ey=b.y-a.y,dot=ex*dx+ey*dy;
    float m=norm2(dx,dy),l=norm2(ex,ey);
    return dot<0 && dot*dot>0.5f*m*l && (b.x-p.x)*dx+(b.y-p.y)*dy<0;
}
#endif
