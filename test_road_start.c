#include <assert.h>
#include <stdio.h>
#include "road_start.h"
#define P(x,y,z) ((Point){x,y,z,0})
int main(void) {
    Point a=P(0,0,0),b=P(100,0,0),p=P(10,2,0);
    assert(forward_edge_score(p,a,b,2,0)>=0);
    puts("PASS long road: forward endpoint qualifies before midpoint");
    assert(forward_edge_score(p,a,b,-2,0)<0);
    assert(forward_edge_score(p,a,b,0,2)<0);
    puts("PASS reverse and perpendicular roads rejected");
    assert(forward_edge_score(P(10,8,0),a,b,2,0)<0);
    assert(forward_edge_score(P(10,2,8),a,b,2,0)<0);
    puts("PASS parallel road and wrong elevation rejected");
    assert(forward_edge_score(P(-3,0,0),a,b,2,0)<0);
    assert(forward_edge_score(P(102,0,0),a,b,2,0)<0);
    assert(forward_edge_score(P(98,0,0),a,b,2,0)<0);
    puts("PASS behind, overshot, and imminent endpoints rejected");
    assert(forward_edge_score(P(10,0,0),a,P(200,0,0),2,0)<0);
    assert(forward_edge_score(P(10,0,0),a,a,2,0)<0);
    puts("PASS oversized and degenerate edges rejected");
    assert(doubles_back(p,b,a,2,0));
    assert(!doubles_back(p,b,P(100,100,0),2,0));
    assert(!doubles_back(p,b,P(70,0,0),2,0));
    puts("PASS immediate doubling back rejected; ordinary next turn allowed");
    RoadMotion m={0};
    motion_sample(&m,P(0,0,0),0);assert(!m.moving);
    motion_sample(&m,P(0,2,0),100);assert(m.moving&&m.dy>0&&m.dx==0);
    motion_sample(&m,P(2,2,0),200);assert(m.moving&&m.dx>0&&m.dy==0);
    motion_sample(&m,P(2,2,0),500);assert(!m.moving);
    motion_sample(&m,P(200,200,0),600);assert(!m.moving);
    puts("PASS motion follows turning, stops, teleports");
    return 0;
}
