#include <assert.h>
#include <stdio.h>
#include "motion.h"
#define P(x,y,z) ((Point){x,y,z,0})
int main(void) {
    Point a=P(0,0,0),b=P(100,0,0),p=P(10,2,0);
    RoadMotion m={0};
    motion_sample(&m,P(0,0,0),0);assert(!m.moving);
    motion_sample(&m,P(0,2,0),100);assert(m.moving&&m.dy>0&&m.dx==0);
    motion_sample(&m,P(2,2,0),200);assert(m.moving&&m.dx>0&&m.dy==0);
    motion_sample(&m,P(2,2,0),500);assert(!m.moving);
    motion_sample(&m,P(200,200,0),600);assert(!m.moving);
    puts("PASS motion follows turning, stops, teleports");
    Point q=edge_projection(p,a,b);
    assert(q.x==10 && q.y==0 && q.z==0);
    q=edge_projection(P(-10,2,0),a,b);assert(q.x==0);
    q=edge_projection(P(120,2,0),a,b);assert(q.x==100);
    q=edge_projection(p,a,a);assert(q.x==0 && q.y==0);
    puts("PASS segment projection and endpoint clamping");
    return 0;
}
