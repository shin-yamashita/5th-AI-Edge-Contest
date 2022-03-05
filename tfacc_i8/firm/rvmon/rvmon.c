//
// rvmon  monitor program
//
// 2021/

#include "stdio.h"
#include <stdlib.h>
#include <string.h>

#include "ulib.h"
#include "tfacc.h"
#include "readline.h"

#define limit(x, min, max)	((x)<(min)?(min):((x)>(max)?(max):(x)))

#undef f_clk
static float f_clk = 100e6f;

//CACHECTRL:flreq[3:0],clreq[3:0]

int d_cache_flush()
{
    int i = 0;
    *CACHECTRL = 0xf0; // flush d-cache
    while((*CACHECTRL & 0x80)){
        i++;
        if(uart_rx_ready()){
            getchar();
            printf("d_cache_flush() %2x %d\n", *CACHECTRL, i);
            break;
        }
    }
    return i;
}

int d_cache_clean()
{
    //	int i = 0;
    *CACHECTRL = 0xf; // clean d-cache
    //        *CACHECTRL = 0xf; // clean d-cache
    //	while((*CACHECTRL & 0xf)) i++;
    return 0;
}

u32 i_elapsed()
{
    static u32 lastfrc = 0;
    u32 elcnt, now;

    now = clock();	// frc : f_clk counter
    elcnt = (now - lastfrc);
    lastfrc = now;

    return elcnt;	// * (float)(1e3f / f_clk);	// elapsed(ms)
}

static volatile unsigned tick100, _clock;

//============
// IRQ Handler
//============
void set_param_and_kick();
void terminate_tfacc();

static void
timer_irqh (void)
{
    tick100 = (tick100 + 1) % 100;
    if (tick100 == 0)
    {
        _clock++;
        if ((_clock & 1) && ((_clock % 10) < 6))
            set_port (0);
        else
            reset_port (0);
    }
}

static void tfacc_irqh (void)
{
    if(*TFACCFLG & 0x100){  // cmpl from tfacc_core
    //    putchar('+');
        terminate_tfacc();
        *TFACCFLG = 0x000;
    }
    if(*RUN_REG & 0x1){ // kicked from PS
    //    putchar('-');
        set_param_and_kick();
        *RUN_REG = 0x0;
    }
}


unsigned int
str2u32 (char *s)
{
    return strtoul(s, NULL, 16);
}

unsigned char *
dump (unsigned char *pt, int wl)
{
    int i, j;
    u32 *wpt = (u32*)pt;

    for (i = 0; i < 256; i += 16)
    {
        printf ("%08x: ", (int) pt);
        if(wl){
            for (j = 0; j < 16/4; j++){
                printf ("%08x ", wpt[j]);
            }
        }else{
            for (j = 0; j < 16; j++){
                printf ("%02x ", pt[j]);
            }
            for (j = 0; j < 16; j++){
                char c = pt[j];
                putchar (c >= 0x20 && c < 0x7f ? c : '.');
                //                      putc(isgraph(c) ? c : '.');
            }
        }
        putchar ('\n');
        pt += 16;
        wpt += 16 / 4;
    }
    putchar ('\n');
    return pt;
}
void memtest (u32 *pt, int len){
    int i;
    for(i = 0; i < len; i++){
        *pt++ = i;
    }
}

static int ncyc = 0;
static u32 i_elapsed_list[128];
static int cntrun = 0, cntxrdy = 0;

void set_param_and_kick()
{
    int i;

    int Np = *TFACC_NP;
    i_elapsed();

    ncyc = get_param(kTfaccNstage);
    if(ncyc == 0) cntrun = cntxrdy = 0;
//        printf("%d\n", ncyc);

    BASEADR_OUT[0]  = get_param(0);
    BASEADR_IN[0]   = get_param(1);
    BASEADR_FILT[0] = get_param(2);
    BASEADR_BIAS[0] = get_param(3);
    BASEADR_QUANT[0] = get_param(4);

    int dwen = get_param(kTfaccDWen);

    int filH = get_accparam(3);
    int filW = get_accparam(4);
    int filC = get_accparam(5);
    int outH = get_accparam(6);
    int outW = get_accparam(7);
    int outWH = outH * outW;
    int pH = (outWH + (Np-1)) / Np;
    set_accparam(9, pH);

  // depthmul
    if(!dwen) set_accparam(12, 0);
    for(i = 0; i <= 19; i++){
      TFACCPARAM[i] = get_accparam(i);
      //printf("%d: %d\n", i, get_accparam(i));
      //TFACCPARAM[i]);
    }
    TFACCPARAM[20] = outWH;
    TFACCPARAM[21] = filH * filW * filC;    // dim123
    TFACCPARAM[22] = (outWH+pH-1)/pH;   // Nchen

  // out_x, out_y initial value set
    for(i = 0; i < Np; i++){
      int out_y = i*pH / outW;
      int out_x = i*pH % outW;
      TFACCPARAM[i+24] = (out_y<<16)|out_x;
    }
    *CACHECTRL = 0x0f;  // read cache invalidate (clean) request

//    enable_irq();
    *TFACCFLG = 0x201;  // kick PL accelarator, inten
//        printf("%d kick ... ", ncyc);
}

void terminate_tfacc()
{
    int flrdy;
    do{
        flrdy = *CACHECTRL;
    }while(flrdy != 0x40);  // wait out cache all complete
//        printf(" out cache complete and flush request ... :%02x\n", *CACHECTRL);
    *CACHECTRL = 0xf0;      // out cache flush request
    *CACHECTRL = 0x00;
    do{
        flrdy = *CACHECTRL;
    }while(flrdy != 0x40);  // wait out cache flush complete
//        printf("flush complete. :%02x\n", *CACHECTRL);
    cntrun  += TFACCPARAM[0];
    cntxrdy += TFACCPARAM[1];

//printf(" %d: %d %d %d\n", ncyc, cntrun, cntxrdy, i_elapsed());

    i_elapsed_list[ncyc] = i_elapsed();
//        printf(" %d: %d\n", ncyc, i_elapsed_list[ncyc]);
    ncyc = ncyc < 127 ? ncyc + 1 : 127;
//    set_param(kTfaccNstage, ncyc);
    set_param(kTfaccRun, 0);    // handshake with PS
}

#if 0
int tfacc_run(int trig)
{
    int ncyc = 1;
    int i, c = 0;
    int cntrun = 0, cntxrdy = 0;
    float cnt2ms = (float)(1e3f/(f_clk));
    float el2ms = (float)(1e3f/(f_clk));

    int Np = *TFACC_NP;
    printf(" Np = %d\n", Np);
    for(i = 0; i < 128; i++) i_elapsed_list[i] = 0;

    i_elapsed();

    do{
        while(get_param(kTfaccRun) == 0){   // handshake with PS Invoke()
            ncyc = get_param(kTfaccNstage);
            ncyc = ncyc < 128 ? ncyc : 127;
            //	wait();
            if(uart_rx_ready()){
                c = getchar();
                if(c == 'q') return 0;
                if(c == 'e'){
                    u32 ute = 0;
                    for(i = 0; i <= ncyc; i++) ute += i_elapsed_list[i];
                    for(i = 0; i <= ncyc; i++)
                        printf("%2d: %5.2f %% %4.2f ms\n",
                                i, fu((float)i_elapsed_list[i]*100.0f/(float)ute), fu(i_elapsed_list[i]*el2ms));
                    printf("           %4.2f ms  run:%4.2f ms xrdy:%4.2f ms\n", fu(ute*el2ms), fu(cntrun*cnt2ms), fu(cntxrdy*cnt2ms));
                }
            }
        }
        if(ncyc < 1){	// init counter
           cntrun = cntxrdy = 0;
           i_elapsed();
        }
//        printf("%d\n", ncyc);

        BASEADR_OUT[0]  = get_param(0);
        BASEADR_IN[0]   = get_param(1);
        BASEADR_FILT[0] = get_param(2);
        BASEADR_BIAS[0] = get_param(3);
        BASEADR_QUANT[0] = get_param(4);

        int dwen = get_param(kTfaccDWen);

        int filH = get_accparam(3);
        int filW = get_accparam(4);
        int filC = get_accparam(5);
        int outH = get_accparam(6);
        int outW = get_accparam(7);
        int outWH = outH * outW;
        int pH = (outWH + (Np-1)) / Np;
        set_accparam(9, pH);

      // depthmul
        if(!dwen) set_accparam(12, 0);
        for(i = 0; i <= 19; i++){
          TFACCPARAM[i] = get_accparam(i);
          //printf("%d: %d\n", i, get_accparam(i));
          //TFACCPARAM[i]);
        }
        TFACCPARAM[20] = outWH;
        TFACCPARAM[21] = filH * filW * filC;    // dim123
        TFACCPARAM[22] = (outWH+pH-1)/pH;   // Nchen

      // out_x, out_y initial value set
        for(i = 0; i < Np; i++){
          int out_y = i*pH / outW;
          int out_x = i*pH % outW;
          TFACCPARAM[i+24] = (out_y<<16)|out_x;
        }
        *CACHECTRL = 0x0f;  // read cache invalidate (clean) request

        enable_irq();
        *TFACCFLG = 0x201;  // kick PL accelarator, inten
//        *TFACCFLG = 0;
//        printf("%d kick ... ", ncyc);

        int run;
        do{
          run = (*TFACCFLG) & 0x1;  // run bit
//           printf("+:%d\n", run);
        }while(!run);
        do{
          run = (*TFACCFLG) & 0x1;
//          printf("-:%d\r", run);
        }while(run);
//        printf(" done ... :%02x \n", *CACHECTRL);

        int flrdy;
        do{
            flrdy = *CACHECTRL;
        }while(flrdy != 0x40);  // wait out cache all complete
//        printf(" out cache complete and flush request ... :%02x\n", *CACHECTRL);
        *CACHECTRL = 0xf0;      // out cache flush request
        *CACHECTRL = 0x00;
        do{
            flrdy = *CACHECTRL;
        }while(flrdy != 0x40);  // wait out cache flush complete
//        printf("flush complete. :%02x\n", *CACHECTRL);
        cntrun  += TFACCPARAM[0];
        cntxrdy += TFACCPARAM[1];

//printf(" %d: %d %d %d\n", ncyc, cntrun, cntxrdy, i_elapsed());

        i_elapsed_list[ncyc] = i_elapsed();
//        printf(" %d: %d\n", ncyc, i_elapsed_list[ncyc]);

        set_param(kTfaccRun, 0);    // handshake with PS

    }while(trig && c != 'q');
    return 0;
}
#endif

int main (void)
{
    char *str, *tok;
    int i;
    unsigned char *pt = 0;
    unsigned *wpt = 0;
//    u32 *f_clock = (u32*)0xf0;
    int baud = 115200;

    if(*FREQ_REG == 0) *FREQ_REG = 100000000;
    f_clk = (float)(*FREQ_REG);

    float cnt2ms = (float)(1e3f/(f_clk));
    float el2ms = (float)(1e3f/(f_clk));

    uart_set_baud((int)(f_clk / baud + 0.5f));
    init_timer ((int) (1e-3f * f_clk));
    add_timer_irqh_sys (timer_irqh);

    set_port (1);

    int Np = *TFACC_NP;
    printf ("rvmon loaded. %4.1f MHz\nNp = %d\n", fu(f_clk * 1e-6f), Np);

    reset_port (1);
    add_getchar_hook(run_tracking);

    while (1)
    {
        add_timer_irqh_sys (timer_irqh);
        add_user_irqh(tfacc_irqh);
        str = readline("rvmon$ ");
        putchar('\n');
        tok = strtok (str, " \n");
        if (!strcmp ("d", tok)){
            tok = strtok (NULL, " \n");
            if (tok)
                pt = (unsigned char*)str2u32 (tok);
            pt = dump (pt, 0);
        }
        else if (!strcmp ("dw", tok)){
            tok = strtok (NULL, " \n");
            if (tok)
                pt = (unsigned char*)str2u32 (tok);
            disable_irq();
            pt = dump (pt, 1);
            enable_irq();
        }
        else if (!strcmp ("mw", tok)){
            tok = strtok (NULL, " \n");
            if (tok)
                pt = (unsigned char*)str2u32 (tok);
            tok = strtok (NULL, " \n");
            int len = 16;
            if (tok)
                len = (int)str2u32 (tok);
            memtest ((u32*)pt, len);
            printf(" write inc pattern to memory @ %x %d wd\n", pt, len);
        }
        else if (!strcmp ("f", tok)){	// cache flush
            i = d_cache_flush();
            printf("flush %d\n", i);
        }
        else if (!strcmp ("c", tok)){	// cache clean
            i = d_cache_clean();
            printf("clean %d\n", i);
        }
        else if (!strcmp ("b", tok)){	// cache base
            printf("out:  %8x\n", *BASEADR_OUT);
            printf("in:   %8x\n", *BASEADR_IN);
            printf("filt: %8x\n", *BASEADR_FILT);
            printf("bias: %8x\n", *BASEADR_BIAS);
            printf("quant: %8x\n", *BASEADR_QUANT);
       }
        else if (!strcmp ("m", tok)){	// monisel
            tok = strtok (NULL, " \n");
            if(tok) *TFACCMON = str2u32(tok);
            tok = strtok (NULL, " \n");
            if(tok) TFACCMON[1] = str2u32(tok);
            printf("moni : %d\n", *TFACCMON);
        }
        else if (!strcmp ("p", tok)){
            wpt--;
            printf("%8x : %08x\n", (u32)wpt, *wpt);
        }
        else if (!strcmp ("w", tok)){
            tok = strtok (NULL, " \n");
            if (tok){
                if(*tok == '@'){	// set address
                    wpt = (unsigned*)str2u32 (&tok[1]);
                    tok = strtok (NULL, " \n");
                    if(tok){
                        *wpt = str2u32(tok);
                    }
                }else{
                    *wpt = str2u32(tok);
                }
            }
            printf("%8x : %08x\n", (u32)wpt, *wpt);
            wpt++;
        }
        else if (!strcmp ("r", tok)){
            cntrun = cntxrdy = 0;
        }
        else if (!strcmp ("e", tok)){
            u32 ute = 0;
            for(i = 0; i <= ncyc; i++) ute += i_elapsed_list[i];
            for(i = 0; i <= ncyc; i++)
                printf("%2d: %5.2f %% %4.2f ms\n",
                        i, fu((float)i_elapsed_list[i]*100.0f/(float)ute), fu(i_elapsed_list[i]*el2ms));
            printf("           %4.2f ms  run:%4.2f ms xrdy:%4.2f ms\n", fu(ute*el2ms), fu(cntrun*cnt2ms), fu(cntxrdy*cnt2ms));
            cntrun = cntxrdy = 0;
        }
        else if (!strcmp ("h", tok))
        {
            puts (
                    "    d/dw  {addr}\n"
                    "    w  {@addr} {data}\n"
                    "    p   addr--\n"
                    "    b  print base addr\n"
                    "    f  cache flush\n"
                    "    c  cache clean\n"
                    "    e  print elapsed time\n"
                    "    r  reset elapsed\n"
                //    "    r  tfacc run\n"
            );
        }
//        remove_timer_irqh ();
//        remove_user_irqh ();
    }
}

