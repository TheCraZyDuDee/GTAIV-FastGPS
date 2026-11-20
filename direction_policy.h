/* Local, direction-aware routing policy. No game-memory access. */
#ifndef FASTGPS_DIRECTION_POLICY_H
#define FASTGPS_DIRECTION_POLICY_H
#include "road_start.h"
typedef struct { Point pos; float hx,hy; } Direction;
static int direction_init(Direction *d,Point p,float dx,float dy) {
    float m=norm_approx(dx,dy);
    if(!valid_point(p)||!(m>=0.5f&&m<100))return 0;
    d->pos=p;d->hx=dx/m;d->hy=dy/m;return 1;
}
static float along(const Direction *d,Point p) {
    return (p.x-d->pos.x)*d->hx+(p.y-d->pos.y)*d->hy;
}
static float across(const Direction *d,Point p) {
    return (p.y-d->pos.y)*d->hx-(p.x-d->pos.x)*d->hy;
}
/* Filter only edges travelling strongly against the car's heading within
   its local corridor. Used identically for reverse relaxation AND forward
   extraction, because the native search computes costs from the destination. */
static int forbidden_edge(const Direction *d,Point from,Point to) {
    float x=to.x-from.x,y=to.y-from.y,l=norm2(x,y);
    float dot=x*d->hx+y*d->hy,h2=norm2(d->hx,d->hy);
    float a=along(d,from),b=along(d,to),c=across(d,from),e=across(d,to);
    if(l<0.25f||dot>=0||dot*dot<0.64f*l*h2)return 0;
    if(a< -45||b>100)return 0; /* backwards interval must overlap corridor */
    if(absf(c)>20||absf(e)>20||absf((c+e)*0.5f)>12)return 0;
    if(absf(from.z-d->pos.z)>8||absf(to.z-d->pos.z)>8)return 0;
    return 1;
}
/* Measure progress 25 route units ahead from projection onto the first
   meaningful segment; reject an unconnected beginning instead of guessing. */
static int start_progress(const Direction *d,const Point *route,int n,float *progress,int *initial_reverse) {
    int i=0,j;float remaining=25;
    Point p=d->pos,a,b,q;
    if(n<2||n>200||!valid_point(route[0]))return 0;
    if(norm2(p.x-route[0].x,p.y-route[0].y)>625||absf(p.z-route[0].z)>8)return 0;
    while(i<n-1) {
        if(!valid_point(route[i+1]))return 0;
        if(norm2(route[i+1].x-route[i].x,route[i+1].y-route[i].y)>=0.25f)break;
        ++i;
    }
    if(i>=n-1)return 0;
    a=route[i];b=route[i+1];
    if(segment_distance2(p,a,b,1)>225)return 0;
    q=edge_projection(p,a,b);
    { float x=b.x-a.x,y=b.y-a.y,dot=x*d->hx+y*d->hy;
      *initial_reverse=dot<0 && dot*dot>=0.36f*norm2(x,y)*norm2(d->hx,d->hy); }
    for(j=i+1;j<n;j++) {
        float x,y,len,t;
        b=route[j];if(!valid_point(b))return 0;
        x=b.x-q.x;y=b.y-q.y;len=norm_approx(x,y);
        if(len>=remaining&&len>0) {
            t=remaining/len;q.x+=x*t;q.y+=y*t;*progress=along(d,q);return 1;
        }
        remaining-=len;q=b;
    }
    *progress=along(d,q);return 1;
}
static int backward_route(const Direction *d,const Point *route,int n) {
    float progress;int reverse;
    return start_progress(d,route,n,&progress,&reverse)&&(reverse||progress< -6);
}
static float route_length(const Point *route,int n) {
    int i;float length=0;
    for(i=0;i<n;i++) {
        if(!valid_point(route[i]))return -1;
        if(i)length+=norm_approx(route[i].x-route[i-1].x,route[i].y-route[i-1].y)+0.25f*absf(route[i].z-route[i-1].z);
    }
    return length;
}
enum { ALT_INVALID=0,ALT_ACCEPT=1,ALT_REVERSE=-1,ALT_DEST=-2,ALT_DETOUR=-3 };
static int alternative_verdict(const Direction *d,const Point *baseline,int bn,const Point *candidate,int cn) {
    float progress,l0,l1,allowance;int reverse;
    if(bn<2||bn>200||cn<2||cn>200)return ALT_INVALID;
    if(!start_progress(d,candidate,cn,&progress,&reverse))return ALT_INVALID;
    if(reverse||progress<4)return ALT_REVERSE;
    if(norm2(candidate[cn-1].x-baseline[bn-1].x,candidate[cn-1].y-baseline[bn-1].y)>0.0625f||
       absf(candidate[cn-1].z-baseline[bn-1].z)>0.25f)return ALT_DEST;
    l0=route_length(baseline,bn);l1=route_length(candidate,cn);
    if(l0<0||l1<0)return ALT_INVALID;
    allowance=l0*0.35f;if(allowance<250)allowance=250;if(allowance>600)allowance=600;
    return l1<=l0+allowance?ALT_ACCEPT:ALT_DETOUR;
}
static int accept_alternative(const Direction *d,const Point *baseline,int bn,const Point *candidate,int cn) {
    return alternative_verdict(d,baseline,bn,candidate,cn)==ALT_ACCEPT;
}
/* Retry a rejected alternative only after meaningful new movement/heading.
   Native distance-based invalidation remains independent of this gate. */
typedef struct { Direction at; Point target; int valid; } RejectedDirection;
static int retry_allowed(const RejectedDirection *r,const Direction *d,Point target) {
    float moved,dot;
    if(!r->valid||norm2(target.x-r->target.x,target.y-r->target.y)>1||absf(target.z-r->target.z)>1)return 1;
    moved=norm2(d->pos.x-r->at.pos.x,d->pos.y-r->at.pos.y);
    if(moved>=625||absf(d->pos.z-r->at.pos.z)>8)return 1;
    dot=d->hx*r->at.hx+d->hy*r->at.hy;
    return moved>=16&&(dot<=0||dot*dot<0.49f*norm2(d->hx,d->hy)*norm2(r->at.hx,r->at.hy));
}
#endif
