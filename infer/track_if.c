//
// tracking process
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

typedef struct _pre_data{   // infer data from python
    int n;
    int uid;
    int cls;
    float score;
    float x, y;   // box center position; 
    float area;
    uint8_t Y;
    int8_t  Cb, Cr;
} pre_data_t;

typedef struct _track_data{ // Tracklet
    int uid, nlost; 
    float px, py;   // predicted x, y; 
    float dx, dy;   // velocity
    float area;
    uint8_t Y;
    int8_t  Cb, Cr;
} track_data_t;

static pre_data_t   pre[60];
static int nm[2], map[2][60];

int pre_data_set(pre_data_t infd, int ix)   // from python: set infer result
{
    pre[ix++] = infd;
    if(ix < 60) pre[ix].n = -1;
    return 0;
}

#if 0
static track_data_t Track[2][60];
static int ntracks[2] = {0,0};
static int pt[2][60];  // now [cls][60]
static int debug = 0;
static float metric[2][60][60];// [cls][][]

static void print_pre()
{
    printf("pre:");
    for(int i = 0; i < 60; i++){
        if(pre[i].n < 0) break;
        printf("%c %d %4.2f|", pre[i].cls?'C':'P',pre[i].uid,pre[i].x);
    }
    printf("\n");
}
static void print_track()
{
    printf("trk P %d:", ntracks[0]);
    for(int i = 0; i < ntracks[0]; i++){
        printf("%d(%d) %4.2f|", Track[0][i].uid,Track[0][i].nlost,Track[0][i].px);
    }
    printf("\n");
    printf("trk C %d:", ntracks[1]);
    for(int i = 0; i < ntracks[1]; i++){
        printf("%d(%d) %4.2f|", Track[1][i].uid,Track[1][i].nlost,Track[1][i].px);
    }
    printf("\n");
}

static float Klp = 0.1f;

static int update_track(int cls, int i, int map)
{
    if(map < 0){
    }else{
        track_data_t *trk = &Track[cls][map];
        trk->nlost = 0;
        trk->dx += ((pre[i].x - trk->px) - trk->dx) * Klp;
        trk->dy += ((pre[i].y - trk->py) - trk->dy) * Klp;
        trk->px = pre[i].x;// + trk->dx;
        trk->py = pre[i].y;// + trk->dy;
        trk->area += (pre[i].area - trk->area) * Klp;
        trk->Y = pre[i].Y;
        trk->Cb = pre[i].Cb;
        trk->Cr = pre[i].Cr;
    }
    return 0;
}

static void new_tracks(int cls, int unmap[])
{
    int i, n, m, p;
    //printf("new:");
    n = 0;
    for(i = 0; i < ntracks[cls]; i++){
        m = unmap[n++];
        //printf("%d ", m);
        if(m < 0) break;
        track_data_t *trk = &Track[cls][m];
        trk->nlost++;
        if(trk->nlost > 2){
            trk->uid = -1;
            trk->nlost = 0;
        }
    }

    n = 0;
    for(i = 0; i < 60; i++){
        m = unmap[n++];
        if(m < 0) break;
        track_data_t *trk = &Track[cls][m];
        if(trk->uid < 0){
            p = pt[cls][m];
            if(m >= ntracks[cls]) ntracks[cls]++;
            p = pt[cls][m];
            trk->uid = pre[p].uid;
            trk->nlost = 0;
            trk->px  = pre[p].x;
            trk->py  = pre[p].y;
            trk->dx  = trk->dy = 0.0;
            trk->area = pre[p].area;
            trk->Y   = pre[p].Y;
            trk->Cb  = pre[p].Cb;
            trk->Cr  = pre[p].Cr;
        }
    }
    if(Track[cls][ntracks[cls]-1].uid < 0) ntracks[cls]--;
}

static int matching(float mat[60][60], int n, int m, int map[60])  // n:now m:track
{
    #define LARGE 1e12f;
    int i, j, nm;
    float minv;
    // step1
    for(i = 0; i < n; i++){
        minv = LARGE;
        for(j = 0; j < m; j++){
            minv = mat[i][j] < minv ? mat[i][j] : minv;
        }
        for(j = 0; j < m; j++){
            if(minv < 0.2f) mat[i][j] -= minv;
        }
    }
    
    for(j = 0; j < m; j++){
        minv = LARGE;
        for(i = 0; i < n; i++){
            minv = mat[i][j] < minv ? mat[i][j] : minv;
        }
        for(i = 0; i < n; i++){
            if(minv < 0.2f) mat[i][j] -= minv;
        }
    }
    
    // step2
    for(i = 0; i < n; i++){
        map[i] = -1;
    }
    nm = 0;
    for(i = 0; i < n; i++){
        for(j = 0; j < m; j++){
            if(mat[i][j] == 0.0f){
                if(map[i] < 0) nm++;
                map[i] = j;   // [last][now]
            }
        }
    }
    for(i = 0; i < n; i++){
        for(j = i+1; j < n; j++){
            if(map[i] == map[j]){
                map[i] = -1;
            }
        }
    }

    return nm;
}

static int c_tracking(int frame)
{
    int i, j, n, cls;
    int n0[2];  // n0[cls] now
    if(frame == 0){
        ntracks[0] = ntracks[1] = 0;
        for(i = 0; i < 60; i++){
            if(pre[i].n < 0) break;
            cls = pre[i].cls;
            track_data_t *trk = &Track[cls][ntracks[cls]];
            ntracks[cls]++;
            trk->uid = pre[i].uid;    //printf("map(%x):\n", premap);

            trk->Y   = pre[i].Y;
            trk->Cb  = pre[i].Cb;
            trk->Cr  = pre[i].Cr;           
        }
        return 0;
    }

    n0[0] = n0[1] = 0;
    for(i = 0; i < 60; i++){ // frame now
        n = pre[i].n;
        cls = pre[i].cls & 0x1;
        pt[cls][n0[cls]] = n;
        if(n < 0) break;
        n0[cls]++;
    }
    float xx, yy, aa, cc, dx, dy, ra, dc;
    for(cls = 0; cls < 2; cls++){
        for(i = 0; i < n0[cls]; i++){   // now
            n = pt[cls][i];
            xx = pre[n].x;
            yy = pre[n].y;
            aa = pre[n].area;
            cc = pre[n].Y;
            for(j = 0; j < ntracks[cls]; j++){   // last (tracklet) 
                dx = fabs(xx - Track[cls][j].px);   // |x diff pos| < 0.1
                dy = fabs(yy - Track[cls][j].py);   // |y diff pos| < 0.05
                ra = Track[cls][j].area / aa;       // 0.5 < area ratio < 2.0 
                dc = abs(cc - Track[cls][j].Y) - 20; // |diff color| < 20
                dc = dc > 0.0f ? dc * 6e-3f : 0.0f; 
                metric[cls][i][j] = dx + dy + (dx > 0.08f || dy > 0.03f) + (ra < 0.5f || ra > 2.0f) + dc;
            }
        }
    }

    for(cls = 0; cls < 2; cls++){
        int unmap[60];
        for(i = 0; i < ntracks[cls]; i++){
            unmap[i] = -1;
        }
        if(debug){
            printf("%s:\n", cls ? "Car" : "Pede");
            for(i = 0; i < n0[cls]; i++){   // now
                printf("%2d:", pt[cls][i]);
                for(j = 0; j < ntracks[cls]; j++){   // last
                printf(" %4.2f", metric[cls][i][j]);
                }
                printf("\n");
            }
        }
        nm[cls] = matching(metric[cls], n0[cls], ntracks[cls], map[cls]);
        //printf("%d match : ", nm[cls]);
        j = 0;
        for(i = 0; i < n0[cls]; i++){
            n = map[cls][i];    // track no
            //printf("%d ", n);
            if(n < 0)
                unmap[j++] = i;
            else
                update_track(cls, pt[cls][i], n);
        }
        unmap[j] = -1;
        map[cls][i] = -1;
        //printf(" | ");
        nm[cls] = n0[cls];
        new_tracks(cls, unmap);
        //printf("\n");
    }
    if(debug){
        print_pre();
        print_track();
    }

    return 0;
}
#endif

#include <libxlnk_cma.h>
static volatile uint32_t* m_reg = NULL;	// rv32_cpu memory area

#define TRACK_BUF       (volatile uint32_t*)(&m_reg[0xc0/4])    // Tracking data buffer pointer phy addr
#define TRACK_ST        (volatile uint32_t*)(&m_reg[0xc4/4])    // Tracking process status
#define TRACK_FRM       (volatile uint32_t*)(&m_reg[0xc8/4])    // frame no.
#define TRACK_BUFVM     (volatile uint64_t*)(&m_reg[0xd0/4])    // Tracking data buffer pointer vm addr
#define TRACK_MAP       (volatile uint32_t*)(&m_reg[0xe0/4])    // premap addr
#define DUMMY_WR        (volatile uint32_t*)(&m_reg[0xe4/4])    // dummy

static int premap = 0;
static char* track_bufvm = NULL;
static uint32_t track_buf = 0;

static int get_mreg(int adr, int ofs)
{
    return m_reg[adr/4+ofs];
}

int tracking(int frame) // from python: pre data to rv32 and kick c_tracking()
{
    if(!m_reg){
        m_reg = (uint32_t*)(cma_mmap(0xa0000000, 0x10000)); // for tfacc_cpu_v1_0 APB access
        if(!m_reg){
            perror("cma_mmap()\n");
        }
    }
    if(!track_bufvm){
        track_bufvm = (char*)(*TRACK_BUFVM);    // track_buf vm address set by PL_if_config()
        printf("track_bufvm: %p\n", track_bufvm);
    }
    if(!track_buf){
        track_buf = *TRACK_BUF;     // track_buf phy address set by PL_if_config()
        printf("track_buf: %x\n", track_buf);
    }

    *TRACK_FRM = frame;
    //printf("memcpy(%p, %p, %ld)\n", track_bufvm, pre, sizeof(pre));
    memcpy((void*)(track_bufvm), pre, sizeof(pre));    //  copy pre to track_buf 
    cma_flush_cache((void*)(track_bufvm), track_buf, sizeof(pre));

    *TRACK_ST = 1;  // kick c_tracking() (rv32)
    while(*TRACK_ST){
        usleep(1);   // wait for c_tracking() done
    }
    
    if(!premap){
        premap = *TRACK_MAP;    // tracking result
        printf("track_map: %x\n", premap);
    }
    for(int cls = 0; cls < 2; cls++){
        nm[cls] = get_mreg(premap, cls*60);
        //printf("%s %d: ", cls ? "Car":"Ped", nm[cls]);
        for(int i = 0; i < nm[cls]; i++){
            map[cls][i] = get_mreg(premap, cls*60+i+1);
            //printf(" %d", map[cls][i]);
        } 
        //printf("\n");  
    }
    return 0;
    // copy pre to cma_area
    // kick C tracking and wait 
//    return c_tracking(frame);
}

int pre_map_get(int *m, int cls)    // from python:  get tracking results
{
    int i;
    for(i = 0; i < nm[cls]; i++){
        m[i] = map[cls][i];
    }
    m[i] = -1;
    return nm[cls];
}

