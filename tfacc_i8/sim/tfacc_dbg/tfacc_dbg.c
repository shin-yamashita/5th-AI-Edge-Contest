
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define TFACCFLG   0xffff0300
#define TFACCPARAM 0xffff0400
#define TB_CTRL    0xffff0800
#define TFACCCACHE 0xffff0180

#define BASEADR_OUT     0xffff0304
#define BASEADR_IN      0xffff0308
#define BASEADR_FILT    0xffff030c
#define BASEADR_BIAS    0xffff0310
#define BASEADR_QUANT   0xffff0314

#define OUTPUT  0x1000000
#define INPUT   0x2000000
#define FILTER  0x3000000
#define BIAS    0x4000000
#define QUANT   0x5000000

int Np = 32;
int stage = 0;

uint8_t *input = NULL, *filter = NULL, *output = NULL, *refout = NULL;
int32_t *bias = NULL, *quant = NULL;
int32_t tfaccparam[30];
size_t input_size, filter_size, bias_size, output_size;

void init_mem(uint8_t *mem, size_t size, uint8_t ini)
{
  int i;
  for(i = 0; i < size; i++) mem[i] = ini;
}

static int ill_w_max;

int compare_mem()
{
  int i, err = 0;
  uint8_t *ref = refout;
  uint8_t *out = output;
  size_t size = output_size;

  for(i = 0; i < size; i++){
    if(ref[i] != out[i]){
      err++;
      printf(" %8x : %2x %2x\n", i, ref[i], out[i]);
    }
  }
  printf("**** %d  compare  %d error.  %d over\n", stage, err, ill_w_max - (OUTPUT + output_size));
  return err;
}

static int debug = 0;
int input_acces_check(int adr, int data)
{
	uint8_t rdata = 0xff;
	int chk = 1;
	if(adr < input_size){
	    rdata = input[adr];
	    chk = data==rdata;
	    if(debug & 2)
	    	if(!chk) printf("in: %8x %2x %2x %c\n", adr, data, rdata, data==rdata?' ':'x');
	}else{
		if(debug & 2) printf("in: %8x %2x ill adr\n", adr, data);
	}
	return chk;
}
int ram_access_rd(int adr)
{
  uint8_t *pt = NULL, rdata = 0xff;
  if (adr >= INPUT && adr < INPUT+input_size)           pt = input+(adr-INPUT);
  else if(adr >= FILTER && adr < FILTER+filter_size)    pt = filter+(adr-FILTER);
  else if(adr >= BIAS  && adr < BIAS+bias_size)         pt = ((uint8_t*)bias)+(adr-BIAS);
  else if(adr >= QUANT && adr < QUANT+bias_size)        pt = ((uint8_t*)quant)+(adr-QUANT);
  if(pt){
      rdata = *pt;
      if(debug & 1) printf("ram r: %8x %2x\n", adr, rdata);
  }else{
      if(debug & 8) printf("ram ill r: %8x\n", adr);
  }
  return rdata;
}
int ram_access_wr(int adr, int wdata)
{
  uint8_t *pt = NULL;
  int ill = 0, out = 0;
  if(adr >= OUTPUT && adr < OUTPUT+output_size){
      pt = output+(adr-OUTPUT);
      out = 1;
  }
  if(pt){
      *pt = wdata;
      if(debug & 2){
          printf("ram w: %8x %2x", adr, wdata);//, refout[adr-OUTPUT],refout[adr-OUTPUT] == wdata ? ' ':'x');
          if(out){
              if(refout[adr-OUTPUT] == wdata) printf("\n");
              else printf(" %2x x\n", refout[adr-OUTPUT]);
          }else{
              printf("\n");
          }
      }
      if(out && refout[adr-OUTPUT] != wdata) ill = 2;
  }else{
      if(debug & 4) printf("ram ill w: %8x\n", adr);
      if(adr > ill_w_max)  ill_w_max = adr;
      ill = 1;
  }
  return ill;
}

int mem_rd(int s, int adr)	// s:0,1,2 in,fil,bias 8,8,32
{
  int rdata = 0;
  switch(s){
  case 0: if(adr <= input_size) rdata = input[adr]; else fprintf(stderr,"ill mem : in(%d)\n",adr); break;
  case 1: rdata = filter[adr]; break;
  case 2: rdata = bias[adr]; break;
  case 3: rdata = refout[adr]; break;
  }
  return rdata;
}

int mem_wr(int adr, int data)
{
  output[adr] = data;
  return output[adr] == refout[adr];
}

// dpi interface func wrapper
int _reg_wr(int addr, int data){
//    printf("=w %x %x\n", addr, data);
    return reg_wr(addr, data);
}
int _reg_rd(int addr, int *data){
//    printf("=r %x\n", addr);
    return reg_rd(addr, data);
}
int _nop(){
//    printf("=nop\n");
    return nop();
}

int run_conv(int st)
{
  int i, rd, n, pdat, dwen;
  char str[81], id[20];
//  FILE *fp = fopen("stage.in", "r");
//  fscanf(fp, "%d", &stage);
//  fclose(fp);
  stage = st;
  printf(" run_conv: stage = %d\n", stage);
  sprintf(str, "tvec/tdump-%d-i8.in", stage);
  FILE *dfp = fopen(str, "r");
  int nprd;

  nop();nop();
  reg_rd(0xffff031c, &nprd);
  printf("stage: %d  Np from sv : %d, Np:%d\n", stage, nprd, Np);
  if(nprd) Np = nprd;

  if(dfp){
    while(fgets(str, 80, dfp)){
      sscanf(str, "%d: %s %d\n", &n, id, &pdat);
      printf("%d: %s %d\n", n, id, pdat);
      if(n == -1) dwen = pdat;
      if(n >= 0 && n < 30) tfaccparam[n] = pdat;
      if(n == 19) break;
    }
    fgets(str, 80, dfp);
    sscanf(str, "%s %zd\n", id, &input_size);
    printf("%s: %d\n", id, input_size);
    fgets(str, 80, dfp);
    sscanf(str, "%s %zd\n", id, &filter_size);
    printf("%s: %d\n", id, filter_size);
    fgets(str, 80, dfp);
    sscanf(str, "%s %zd\n", id, &bias_size);
    printf("%s: %d\n", id, bias_size);
    fgets(str, 80, dfp);
    sscanf(str, "%s %zd\n", id, &output_size);
    printf("%s: %d\n", id, output_size);
    input  = (uint8_t*)realloc(input, input_size);
    filter = (uint8_t*)realloc(filter, filter_size);
    bias   = (int32_t*)realloc(bias, bias_size);
    quant  = (int32_t*)realloc(quant, bias_size);
    output = (uint8_t*)realloc(output, output_size);
    refout = (uint8_t*)realloc(refout, output_size);
    fread(input, 1, input_size, dfp);
    fread(filter, 1, filter_size, dfp);
    fread(bias, 1, bias_size, dfp);
    fread(quant, 1, bias_size, dfp);
    fread(refout, 1, output_size, dfp);
    fclose(dfp);
  }else{
    perror("");
  }
  init_mem(output, output_size, 0xf0);

  ill_w_max = OUTPUT + output_size;

  for(i = 0; i < 10; i++) {
    nop();
  }

  int filH = tfaccparam[3];
  int filW = tfaccparam[4];
  int filC = tfaccparam[5];
  int outH = tfaccparam[6];
  int outW = tfaccparam[7];
  int outWH = outH * outW;
  int pH = (outWH + (Np-1)) / Np;	//tfaccparam[9];
  tfaccparam[9] = pH;

// depthmul
  if(!dwen) tfaccparam[12] = 0;
  for(i = 0; i <= 19; i++){
    _reg_wr(TFACCPARAM + i*4, tfaccparam[i]);
  }

  _reg_wr(TFACCPARAM + 20*4, outWH);	// outHW
  _reg_wr(TFACCPARAM + 21*4, filH * filW * filC);	// dim123
  _reg_wr(TFACCPARAM + 22*4, (outWH+pH-1)/pH);	// N chen

// out_x, out_y initial value set
  for(i = 0; i < Np; i++){
    int out_y = i*pH / outW;
    int out_x = i*pH % outW;
    _reg_wr(TFACCPARAM + (i+24)*4, (out_y<<16)|out_x);
  }

  // base addr
  _reg_wr(BASEADR_OUT,  OUTPUT);
  _reg_wr(BASEADR_IN ,  INPUT);
  _reg_wr(BASEADR_FILT, FILTER);
  _reg_wr(BASEADR_BIAS, BIAS);
  _reg_wr(BASEADR_QUANT, QUANT);

  nop();
  nop();
//  _reg_wr(TFACCCACHE, 0x0f000000);	// read cache clean request
  _reg_wr(TFACCCACHE, 0x0f);   // read cache clean request
  _reg_wr(TFACCFLG, 1);	// kick
  _reg_wr(TFACCFLG, 0);	// kick
  nop();
  nop();
  nop();

  int run;
  do{
	  nop();
	  reg_rd(TFACCFLG, &run);
  }while(run & 0x1);
  printf("== run complete\n");

  int flrdy;
  do{
	  reg_rd(TFACCCACHE, &flrdy);
//	  printf("%2x\n",flrdy);
//  }while(flrdy != 0x40000000);  // wait out cache all complete
  }while(flrdy != 0x40);  // wait out cache all complete
  printf("== out cache complete and flush request ... ");
//  _reg_wr(TFACCCACHE, 0xf0000000);	// out cache flush request
  _reg_wr(TFACCCACHE, 0xf0);   // out cache flush request
  _reg_wr(TFACCCACHE, 0x00);
  nop();
  do{
	  reg_rd(TFACCCACHE, &flrdy);
//  }while(flrdy != 0x40000000);	// wait out cache flush complete
  }while(flrdy != 0x40);  // wait out cache flush complete
  printf("flush complete.\n");
  for(i = 0; i < 90; i++) nop();

  _reg_wr(TB_CTRL, 2);	// eval
  for(i = 0; i < 10; i++) nop();
}
#include <time.h>

//static int stage_begin = 0, stage_end = 0;

int main(int argc, char *argv[])
{
	int i;  //, stage, endst;
	time_t tstart = time(NULL);
	time_t now;
  int stage = 0;

 	printf("run stage %d\n", stage);
  run_conv(stage);
	now = time(NULL);
	printf("elapsed:%ld  %s", now-tstart, ctime(&now));

}
