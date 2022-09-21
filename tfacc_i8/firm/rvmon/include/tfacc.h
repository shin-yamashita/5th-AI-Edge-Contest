//
// 2020/03/23 tfacc.h
// tfacc_core driver
//

#include "types.h"

//      tfacc_core 

#define CACHECTRL       ((volatile u8*)0xffff0180)
#define TFACCCACHE      ((volatile u32 *)0xffff0180)

#define TFACCFLG        ((volatile u32 *)0xffff0300)	// b0:(w)kick (r)run b8:(wr)cmpl b9:(wr)inten

#define BASEADR_OUT     ((volatile u32 *)0xffff0304)
#define BASEADR_IN      ((volatile u32 *)0xffff0308)
#define BASEADR_FILT    ((volatile u32 *)0xffff030c)
#define BASEADR_BIAS    ((volatile u32 *)0xffff0310)
#define BASEADR_QUANT   ((volatile u32 *)0xffff0314)

#define TFACC_NP        ((volatile u32 *)0xffff031c)
#define TFACCMON        ((volatile u32 *)0xffff0320)    // monisel
#define TFACCFPR        ((volatile u32 *)0xffff0324)    // fpr 

#define TFACCPARAM      ((volatile u32 *)0xffff0400)    // accparams[64]

#define RESET_REG       ((volatile u32 *)0x00000080)
#define RUN_REG         ((volatile u32 *)0x00000084)

#define TRACK_BUF       ((volatile u32 *)0x000000c0)    // Tracking data buffer addr
#define TRACK_ST        ((volatile u32 *)0x000000c4)    // Tracking process status
#define TRACK_FRM       ((volatile u32 *)0x000000c8)    // frame no.
#define TRACK_BUFVM     ((volatile u64 *)0x000000d0)
#define TRACK_MAP       ((volatile u32 *)0x000000e0)    // pointer to map data

#define FREQ_REG        ((volatile u32 *)0x000000f0)
#define ACCPARAMS       ((volatile u32 *)0x00000100)

//u32 *accparams = (u32*)0x100;

#define set_param(n, p)     (ACCPARAMS[n] = (p))
#define get_param(n)        (ACCPARAMS[n])
#define set_accparam(n, p)  set_param(8+(n), (p))
#define get_accparam(n)     get_param(8+(n))

typedef enum {
    kTfaccNstage = 5,
    kTfaccDWen,
    kTfaccRun,
} TfaccCtrl;

void run_tracking();

