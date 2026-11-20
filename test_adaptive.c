#include <assert.h>
#include <stdio.h>
#include "adaptive.h"
#define P(x,y,z) ((Point){x,y,z,0})
int main(void) {
    Settings s=defaults;Preference p={0};Direction d;
    assert(s.logging==0);s.logging=42;normalize_settings(&s);assert(s.logging==0);
    assert(detour_budget(&s,2000,0)==1000);
    assert(detour_budget(&s,2000,3)==2000);
    assert(detour_budget(&s,2000,99)==2000);
    assert(detour_budget(&s,100,0)==250);
    s.adaptive=0;assert(detour_budget(&s,2000,3)==1000);
    s.percent=0;assert(detour_budget(&s,2000,0)==0);
    s.percent=999;s.max_extra=99999;s.adaptive_percent=0;s.adaptive_extra=0;
    normalize_settings(&s);assert(s.percent==50&&s.max_extra==1000&&s.adaptive_percent==50&&s.adaptive_extra==1000);
    puts("PASS bounded settings, adaptive cap, disable and zero budget");
    s=defaults;preference_sync(&p,P(100,100,0));
    preference_result(&p,ALT_DETOUR);preference_result(&p,ALT_DETOUR);
    assert(p.detour_rejections==2&&!p.forward);
    preference_result(&p,ALT_REVERSE);assert(p.detour_rejections==2);
    preference_result(&p,ALT_DETOUR);preference_result(&p,ALT_DETOUR);assert(p.detour_rejections==3);
    preference_result(&p,ALT_ACCEPT);assert(p.forward&&!p.detour_rejections);
    preference_sync(&p,P(200,100,0));assert(!p.forward&&!p.detour_rejections);
    puts("PASS adaptive history and target reset");
    assert(direction_init(&d,P(0,0,0),2,0));
    Point base[]={P(0,0,0),P(-100,0,0),P(-100,100,0),P(80,100,0)};
    Point alt[]={P(0,0,0),P(300,0,0),P(300,100,0),P(80,100,0)};
    assert(alternative_verdict_budget(&d,base,4,alt,4,100)==ALT_DETOUR);
    assert(alternative_verdict_budget(&d,base,4,alt,4,detour_budget(&s,380,3))==ALT_ACCEPT);
    alt[3]=P(81,100,0);assert(alternative_verdict_budget(&d,base,4,alt,4,5000)==ALT_DEST);
    puts("PASS formerly rejected forward detour accepted with larger budget; endpoint guard unchanged");
    Point road[]={P(-50,0,0),P(100,0,0),P(100,100,0)};
    direction_init(&d,P(20,5,0),2,0);assert(strong_forward_match(&d,road,3));
    direction_init(&d,P(20,12,0),2,0);assert(strong_forward_match(&d,road,3));
    direction_init(&d,P(20,5,0),0,2);assert(!strong_forward_match(&d,road,3));
    direction_init(&d,P(20,5,0),-2,0);assert(!strong_forward_match(&d,road,3));
    direction_init(&d,P(20,5,20),2,0);assert(!strong_forward_match(&d,road,3));
    direction_init(&d,P(103,0,0),2,0);assert(!strong_forward_match(&d,road,3));
    direction_init(&d,P(20,15,0),2,0);assert(!strong_forward_match(&d,road,3));
    puts("PASS retain forward route but not wrong turn, reverse, other elevation, overshoot or far road");
    return 0;
}
