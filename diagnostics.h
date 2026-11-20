/* Local bounded diagnostic log. No CRT or external telemetry. */
#ifndef FASTGPS_DIAGNOSTICS_H
#define FASTGPS_DIAGNOSTICS_H
IMPORT HANDLE __stdcall CreateFileW(const wchar_t *,U32,U32,void *,U32,U32,HANDLE);
IMPORT int __stdcall WriteFile(HANDLE,const void *,U32,U32 *,void *);
static HANDLE log_file;
static U32 log_lines;
static void log_open(void) {
    const wchar_t *path;
    if(!settings.logging)return;
    path=sibling_file(L"GTAIV-FastGPS.log");
    if(path)log_file=CreateFileW(path,0x40000000,3,0,2,0x80,0);
    if(log_file==(HANDLE)-1)log_file=0;
    if(!log_file)OutputDebugStringA("[FastGPS v0.8] Cannot open log beside ASI.\n");
}
static char *log_text(char *p,const char *s) {while(*s)*p++=*s++;return p;}
static char *log_uint(char *p,U32 n) {
    char b[10];unsigned i=0;do {b[i++]=(char)('0'+n%10);n/=10;}while(n);
    while(i)*p++=b[--i];return p;
}
static char *log_int(char *p,int n) {if(n<0){*p++='-';return log_uint(p,0U-(U32)n);}return log_uint(p,(U32)n);}
static char *log_point(char *p,Point v) {
    if(!valid_point(v))return log_text(p,"invalid");
    p=log_int(p,(int)(v.x*10));*p++=',';p=log_int(p,(int)(v.y*10));*p++=',';return log_int(p,(int)(v.z*10));
}
static void log_send(char *b,char *p) {
    U32 written;*p++='\r';*p++='\n';*p=0;
    if(log_file&&log_lines<20000) {++log_lines;WriteFile(log_file,b,(U32)(p-b),&written,0);}
}
static void log_event(const char *event) {char b[256];char *p=log_text(b,event);log_send(b,p);}
static void log_nodes(U32 original,U32 candidate) {
    char b[128],*p=log_text(b,"start_node original=");p=log_uint(p,original);
    p=log_text(p," selected=");p=log_uint(p,candidate);log_send(b,p);
}
static U32 route_hash(const Point *route,int n) {
    U32 h=2166136261UL;int i,j;
    for(i=0;i<n;i++)for(j=0;j<12;j++)h=(h^((const U8 *)&route[i])[j])*16777619UL;
    return h;
}
static void log_route(const char *event,U8 *self,Point pos) {
    char b[512],*p=b;int n=*(int *)(self+COUNT);const Point *route=(const Point *)self;
    p=log_text(p,event);p=log_text(p," t=");p=log_uint(p,*(U32 *)(game+TIMER));
    p=log_text(p," slot=");p=log_uint(p,*(U32 *)(self+0xd70));
    p=log_text(p," state=");p=log_int(p,*(int *)(self+STATE));
    p=log_text(p," n=");p=log_int(p,n);
    p=log_text(p," player10=");p=log_point(p,pos);
    p=log_text(p," target10=");p=log_point(p,*(Point *)(self+TARGET));
    if(n>0&&n<=200){p=log_text(p," hash=");p=log_uint(p,route_hash(route,n));p=log_text(p," first10=");p=log_point(p,route[0]);}
    if(n>1&&n<=200){p=log_text(p," second10=");p=log_point(p,route[1]);}
    log_send(b,p);
}
static void log_region(const char *event,U8 *graph,U32 slot) {
    char b[256],*p=b;unsigned i;
    if(slot>2)return;
    p=log_text(p,event);p=log_text(p," t=");p=log_uint(p,*(U32 *)(game+TIMER));
    p=log_text(p," slot=");p=log_uint(p,slot);p=log_text(p," bounds10=");
    for(i=0;i<4;i++){float f=*(float *)(graph+0x1a90+12*i+4*slot);if(i)*p++=',';
        if(absf(f)<100000)p=log_int(p,(int)(f*10));else p=log_text(p,"invalid");}
    log_send(b,p);
}
#endif
