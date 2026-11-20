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
    assert(road_tile(-3000)==0 && road_tile(-2250)==1 && road_tile(0)==4 && road_tile(3000)==7);
    assert(road_tile(-4000)==0 && road_tile(4000)==7);
    puts("PASS native 750-unit tile indexing and clamping");
    {
        EdgeChoice c;Point q=edge_projection(p,a,b);
        assert(q.x==10 && q.y==0 && q.z==0);
        edge_choice_init(&c,10);assert(!edge_choice_unique(&c));
        edge_choice_offer(&c,20,4,q,b);assert(edge_choice_unique(&c));
        edge_choice_offer(&c,20,4.1f,q,b);assert(edge_choice_unique(&c));
        edge_choice_offer(&c,30,5,q,b);assert(!edge_choice_unique(&c));
        edge_choice_offer(&c,30,1,q,b);assert(edge_choice_unique(&c)&&c.best==30);
        puts("PASS edge ranking deduplicates endpoints and rejects ambiguity");
        Point path[4]={P(100,0,0),P(100,50,0)};unsigned char flags[4]={0x83,5};int n=2;
        assert(segment_distance2(p,path[0],path[1],0)>6400);
        assert(prepend_road(path,flags,4,&n,q,b));
        assert(n==3&&path[0].x==10&&path[1].x==100&&path[2].y==50);
        assert(flags[0]==0x82&&flags[1]==0x83&&flags[2]==5);
        assert(segment_distance2(p,path[0],path[1],0)==4);
        puts("PASS forward road prefix removes artificial 90-unit route gap");
        assert(!prepend_road(path,flags,3,&n,q,b)&&n==3);
        assert(!prepend_road(path,flags,4,&n,q,b)&&n==3);
        puts("PASS full or mismatching route left untouched");
    }
    return 0;
}
