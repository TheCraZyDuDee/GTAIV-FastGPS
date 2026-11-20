#include <assert.h>
#include <stdio.h>
#include "direction_policy.h"
#define P(x,y,z) ((Point){x,y,z,0})
/* A small directed graph, evaluated using destination-side relaxation and
   forward extraction with exactly the same local restriction in both passes.
   It models the topology responsible for the repeated backward guidance. */
static int graph_route(const Direction *d,const Point *nodes,const unsigned short edges[6][6],
                       int restrict_reverse,Point *route) {
    int dist[6],done[6]={0},i,j,k,best,n=0;
    for(i=0;i<6;i++)dist[i]=100000;
    dist[5]=0;
    for(k=0;k<6;k++) {
        best=-1;for(i=0;i<6;i++)if(!done[i]&&(best<0||dist[i]<dist[best]))best=i;
        if(best<0||dist[best]==100000)break;
        done[best]=1;
        for(j=0;j<6;j++)if(edges[j][best]&&(!restrict_reverse||!forbidden_edge(d,nodes[j],nodes[best]))&&
            dist[best]+edges[j][best]<dist[j])dist[j]=dist[best]+edges[j][best];
    }
    if(dist[0]==100000)return 0;
    i=0;route[n++]=nodes[0];
    while(i!=5&&n<6) {
        for(j=0;j<6;j++)if(edges[i][j]&&(!restrict_reverse||!forbidden_edge(d,nodes[i],nodes[j]))&&
            dist[i]==dist[j]+edges[i][j])break;
        if(j==6)return 0;
        i=j;route[n++]=nodes[i];
    }
    return i==5?n:0;
}
int main(void) {
    Direction d;assert(direction_init(&d,P(0,0,0),2,0));
    assert(forbidden_edge(&d,P(90,0,0),P(70,0,0)));
    assert(forbidden_edge(&d,P(2,5,0),P(-5,5,0)));
    assert(!forbidden_edge(&d,P(70,0,0),P(90,0,0)));
    assert(!forbidden_edge(&d,P(20,0,0),P(20,20,0)));
    assert(!forbidden_edge(&d,P(20,25,0),P(0,25,0)));
    assert(!forbidden_edge(&d,P(20,0,15),P(0,0,15)));
    assert(!forbidden_edge(&d,P(300,0,0),P(200,0,0)));
    assert(!forbidden_edge(&d,P(-50,0,0),P(-80,0,0)));
    puts("PASS local reverse restriction, forward and perpendicular turns, distant/parallel/elevated roads");
    Point base[]={P(0,0,0),P(-100,0,0),P(-100,100,0),P(80,100,0)};
    Point forward[]={P(0,0,0),P(80,0,0),P(80,100,0)};
    assert(backward_route(&d,base,4));assert(!backward_route(&d,forward,3));
    assert(accept_alternative(&d,base,4,forward,3));
    puts("PASS old route behind car versus forward alternative");
    Point short_back[]={P(0,0,0),P(-5,0,0),P(30,0,0),P(80,100,0)};
    assert(backward_route(&d,short_back,4));
    assert(!accept_alternative(&d,base,4,short_back,4));
    Point return_back[]={P(0,0,0),P(5,0,0),P(-30,0,0),P(80,100,0)};
    assert(!accept_alternative(&d,base,4,return_back,4));
    puts("PASS no immediate doubling back");
    Point disconnected[]={P(100,0,0),P(150,0,0),P(80,100,0)};
    Point different_dest[]={P(0,0,0),P(80,0,0),P(85,100,0)};
    Point detour[]={P(0,0,0),P(100,0,0),P(100,1000,0),P(80,100,0)};
    assert(!accept_alternative(&d,base,4,disconnected,3));
    assert(!accept_alternative(&d,base,4,different_dest,3));
    assert(!accept_alternative(&d,base,4,detour,4));
    assert(!accept_alternative(&d,base,4,forward,0));
    puts("PASS reject disconnected, incomplete, excessive and failed paths");
    Point near_behind[]={P(-4,0,0),P(30,0,0),P(80,100,0)};
    assert(!backward_route(&d,near_behind,3));assert(accept_alternative(&d,base,4,near_behind,3));
    puts("PASS a node behind the player is fine if the route itself leads forward");
    RejectedDirection r={d,P(80,100,0),1};
    direction_init(&d,P(10,0,0),2,0);assert(!retry_allowed(&r,&d,r.target));
    direction_init(&d,P(10,0,0),0,2);assert(retry_allowed(&r,&d,r.target));
    direction_init(&d,P(25,0,0),2,0);assert(retry_allowed(&r,&d,r.target));
    direction_init(&d,P(1,0,0),2,0);assert(retry_allowed(&r,&d,P(80,110,0)));
    puts("PASS failed route retry gate rearms with distance, heading or new target");
    direction_init(&d,P(0,0,0),2,0);
    Point nodes[]={P(0,0,0),P(-30,0,0),P(30,0,0),P(60,0,0),P(60,60,0),P(-30,60,0)};
    unsigned short edges[6][6]={{0}};
    Point original[6],alternative[6];int bn,cn;
    edges[0][1]=30;edges[1][5]=60;edges[0][2]=30;edges[2][3]=30;edges[3][4]=60;edges[4][5]=90;
    bn=graph_route(&d,nodes,edges,0,original);cn=graph_route(&d,nodes,edges,1,alternative);
    assert(bn==3&&cn==5&&backward_route(&d,original,bn)&&accept_alternative(&d,original,bn,alternative,cn));
    puts("PASS reverse shortest path replaced by a legal forward detour in directed graph");
    edges[0][2]=0;cn=graph_route(&d,nodes,edges,1,alternative);
    assert(cn==0&&alternative_verdict(&d,original,bn,alternative,cn)==ALT_INVALID);
    puts("PASS no legal alternate preserves native U-turn candidate");
    /* Regression from log shape: first node changes with the car, the second
       remains behind it for repeated rebuilding. No real game coordinates. */
    for(int j=0;j<11;j++) {
        Point car=P(10*j,0,0),path[]={P(10*j-1,1,0),P(-70,1,0),P(-70,100,0)};
        assert(direction_init(&d,car,2,0)&&backward_route(&d,path,3));
    }
    puts("PASS repeated moving start with fixed backwards junction is identified every time");
    return 0;
}
