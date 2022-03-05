//
// tracking process
//

#include "stdio.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ulib.h"
#include "tfacc.h"


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
static pre_data_t   Dh[60];
static pre_data_t   Dl[60];

static track_data_t Track[2][60];
static int ntracks[2] = {0,0};
static int pt[2][60];  // now [cls][60]
static int debug = 0;

static float Klp = 0.1f;
static int Nlost = 4;
static float Dth = 0.75f;

static void print_pre()
{
    printf("pre:");
    for(int i = 0; i < 60; i++){
        if(pre[i].n < 0) break;
        printf("%c %d %4.2f|", pre[i].cls?'C':'P',pre[i].uid,fu(pre[i].x));
    }
    printf("\n");
    printf("Dh: ");
    for(int i = 0; i < 60; i++){
        if(Dh[i].n < 0) break;
        printf("%c %d %4.2f|", Dh[i].cls?'C':'P',Dh[i].uid,fu(Dh[i].x));
    }
    printf("\n");
}
static void print_track(char *s)
{
    printf("%strk P %d:", s, ntracks[0]);
    for(int i = 0; i < ntracks[0]; i++){
        printf("%d(%d) %4.2f|", Track[0][i].uid,Track[0][i].nlost,fu(Track[0][i].px));
    }
    printf("\n");
    printf("%strk C %d:", s, ntracks[1]);
    for(int i = 0; i < ntracks[1]; i++){
        printf("%d(%d) %4.2f|", Track[1][i].uid,Track[1][i].nlost,fu(Track[1][i].px));
    }
    printf("\n");
}

static int update_track(int cls, pre_data_t *Det, int map)
{
    if(map < 0){
    }else{
        track_data_t *trk = &Track[cls][map];
        if(trk->uid < 0) trk->uid = Det->uid;
        trk->nlost = 0;
        trk->dx += ((Det->x - trk->px) - trk->dx) * Klp;
        trk->dy += ((Det->y - trk->py) - trk->dy) * Klp;
        trk->px = Det->x;// + trk->dx;
        trk->py = Det->y;// + trk->dy;
        trk->area += (Det->area - trk->area) * Klp;
        trk->Y = Det->Y;
        trk->Cb = Det->Cb;
        trk->Cr = Det->Cr;
    }
    return 0;
}

static void new_tracks(int cls, int unmap[])    // unmap: unmatched Dh no.
{
    int i, n, m;
    //printf("new:");
    n = 0;
    for(i = 0; i < ntracks[cls]; i++){
        track_data_t *trk = &Track[cls][i];
        trk->nlost++;
        if(trk->nlost > Nlost){
            trk->uid = -1;
            trk->nlost = 0;
        }
    }

    n = 0;
    for(i = 0; i < 60; i++){
        track_data_t *trk = &Track[cls][i];
        if(ntracks[cls] <= i){
            ntracks[cls] = i+1;
            trk->uid = -1;
            trk->nlost = 0;
        }
        if(trk->uid < 0){
            m = unmap[n++];
            if(m < 0) break;
            trk->uid = Dh[m].uid;
            trk->nlost = 0;
            trk->px  = Dh[m].x;
            trk->py  = Dh[m].y;
            trk->dx  = trk->dy = 0.0;
            trk->area = Dh[m].area;
            trk->Y   = Dh[m].Y;
            trk->Cb  = Dh[m].Cb;
            trk->Cr  = Dh[m].Cr;
        }
    }
    if(Track[cls][ntracks[cls]-1].uid < 0) ntracks[cls]--;  // ???
}

int pre_data_set(pre_data_t pred, int ix)
{
    pre[ix++] = pred;
    if(ix < 60) pre[ix].n = -1;
    return 0;
}

static int matching(int cls, float mat[60][60], int n, int m, int map[60])  // n:now m:track
{
//    #define LARGE 1e12f;
    int i, j, nm, amin[60];
    float minv;
    for(j = 0; j < m; j++){
        amin[j] = 0;
        minv = mat[0][j];
        for(i = 1; i < n; i++){
            if(mat[i][j] < minv){
                amin[j] = i;    // argmin
                minv = mat[i][j];
            }
        }
    }
    
    for(j = 0; j < m; j++){
        for(i = 0; i < m; i++){
            if(i != j && amin[j] == amin[i]){ // batting
                if(mat[amin[j]][j] < mat[amin[i]][i]){
                    amin[i] = -1;
                }
            }
        }
    }
    
    // step2
    for(i = 0; i <= n; i++){
        map[i] = -1;
    }
    nm = 0;
    for(i = 0; i < m; i++){
        if(amin[i] >= 0){
            float mth = fabs(Track[cls][i].dx) * 4 + 0.05f;
            if(mat[amin[i]][i] < mth)  // 
                map[amin[i]] = i;   // track no.
            else
                map[amin[i]] = -1;
        } 
        else nm++;
    }

    return nm;
}

static float metric[2][60][60];// [cls][][]
static int nm[2], map[2][60];
static int pre_map[2][60];

static int c_tracking(int frame)
{
    int i, j, n, cls;
    int ih, il;
    int n0[2];  // n0[cls] now
    ih = il = 0;
    for(i = 0; i < 60; i++){    // 
        if(pre[i].n < 0){
            Dh[ih].n = Dl[il].n = -1;
            break;
        }
        if(pre[i].score > Dth){
            Dh[ih++] = pre[i];
        }else{
            Dl[il++] = pre[i];
        }
    }
    if(frame == 0){
        ntracks[0] = ntracks[1] = 0;
        for(i = 0; i < 60; i++){
            Track[0][i].uid = -1;
            Track[1][i].uid = -1;
            if(Dh[i].n < 0) break;
            cls = Dh[i].cls;
            track_data_t *trk = &Track[cls][ntracks[cls]];
            ntracks[cls]++;
            trk->uid = Dh[i].uid;
            trk->nlost = 0;
            trk->px  = Dh[i].x;
            trk->py  = Dh[i].y;
            trk->dx  = trk->dy = 0.0;
            trk->area = Dh[i].area;
            trk->Y   = Dh[i].Y;
            trk->Cb  = Dh[i].Cb;
            trk->Cr  = Dh[i].Cr;           
        }
        return 0;
    }

    n0[0] = n0[1] = 0;
    for(i = 0; i < 60; i++){ // frame now
        n = Dh[i].n;
        cls = Dh[i].cls & 0x1;
        pt[cls][n0[cls]] = i;
        if(n < 0) break;
        n0[cls]++;
    }
    float xx, yy, aa, cc, dx, dy, ra, dc;
    for(cls = 0; cls < 2; cls++){
        for(i = 0; i < n0[cls]; i++){   // now
            n = pt[cls][i];
            xx = Dh[n].x;
            yy = Dh[n].y;
            aa = Dh[n].area;
            cc = Dh[n].Y;
            for(j = 0; j < ntracks[cls]; j++){   // last (tracklet) 
                track_data_t *trk = &Track[cls][j];
                dx = fabs(xx - (trk->px+trk->dx));   // |x diff pos| < 0.1
                dy = fabs(yy - (trk->py+trk->dy));   // |y diff pos| < 0.05
                ra = trk->area / aa;       // 0.5 < area ratio < 2.0 
                dc = abs(cc - trk->Y) - 20; // |diff color| < 20
                dc = dc > 0.0f ? dc * 6e-3f : 0.0f; 
                metric[cls][i][j] = dx + dy + (dx > 0.1f || dy > 0.05f) + (ra < 0.5f || ra > 2.0f) + dc;
            }
        }
    }

    for(cls = 0; cls < 2; cls++){
        int unmap[60];
        for(i = 0; i < ntracks[cls]; i++){
            unmap[i] = -1;
        }
        if(debug){
            printf("%s: now:%d ntrk:%d\n", cls ? "Car" : "Pede", n0[cls], ntracks[cls]);
            for(i = 0; i < n0[cls]; i++){   // now
                printf("%2d:", pt[cls][i]);
                for(j = 0; j < ntracks[cls]; j++){   // last
                    printf(" %4.2f", fu(metric[cls][i][j]));
                }
                printf("\n");
            }
        }
        matching(cls, metric[cls], n0[cls], ntracks[cls], map[cls]);
        //printf("%d match : ", nm[cls]);
        for(i = 0; i < n0[cls]; i++){
            int mid = map[cls][i];
            pre_map[cls][i] = mid < 0 ? mid : Track[cls][mid].uid;
        }
        //print_track("p ");
        j = 0;
        for(i = 0; i < n0[cls]; i++){
            n = map[cls][i];    // track no
            //printf("%d(%d) ", n,pt[cls][i]);
            if(n < 0)
                unmap[j++] = pt[cls][i]; // unmatched Dh no 
            else
                update_track(cls, &Dh[pt[cls][i]], n);  // update track n with matched Dh
        }
        unmap[j] = -1;
        map[cls][i] = -1;
        //printf(" | ");
        nm[cls] = n0[cls];
        if(debug){
            printf("\n newtracks:%c unmap:", cls ? 'C':'P');
            for(i = 0; i < 60; i++){
                if(unmap[i] < 0) break;
                printf("%d,", unmap[i]);
            }
            printf("\n");
        }
        new_tracks(cls, unmap);
        //print_track("n ");
        //printf("\n");
    }
    if(debug){
        print_pre();
        print_track("");
    }

    return 0;
}

int tracking(int frame)
{
    // copy pre to cma_area
    // kick C tracking and wait 
    return c_tracking(frame);
}

int pre_map_get(int *m, int cls)
{
    int i;  //, mid;
    for(i = 0; i < nm[cls]; i++){
        m[i] = pre_map[cls][i];
    }
    m[i] = -1;
    return nm[cls];
}

static int premap[2*60];
// rv32
void run_tracking()
{
    int frame;  // frame no.
    u32 *dpt = (u32*)pre;
    u32 *spt;
    //int m[60];

    while(1){
        if(uart_rx_ready()) return;
        if(*TRACK_ST) {
           *TRACK_MAP = (u32)premap;
           frame = *TRACK_FRM;
           spt = (u32*)*TRACK_BUF;
           //printf("run_track:%d %d pre:%x <- %x\n", frame, sizeof(pre), dpt, spt);
           for(int i = 0; i < sizeof(pre)/4; i++){
               dpt[i] = spt[i];
           }
           //for(int i = 0; i < 60; i++){
           //    printf(" %d(%c)",pre[i].uid, pre[i].cls?'C':'P');
           //}
           //printf("\n");
           //memcpy(pre, (void*)*TRACK_BUF, sizeof(pre));
           c_tracking(frame);

           premap[0] = pre_map_get(&premap[1], 0);
           premap[60] = pre_map_get(&premap[61], 1);

           if(debug){
               //printf("premap: %x\n", premap);
               printf("Ped uid:");
               for(int i = 0; i < nm[0]; i++){
                   printf(" %d", premap[i+1]);
               }
               printf("\n");
               printf("Car uid:");
               for(int i = 0; i < nm[1]; i++){
                   printf(" %d", premap[i+61]);
               }
               printf("\n");
           }

           *TRACK_ST = 0;
        }
    }
}

