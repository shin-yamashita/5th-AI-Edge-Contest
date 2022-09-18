
# tfacc FPGA での debug 

/media/shin/Linux-Ext/github/5th-AI-Edge-Contest/tfacc_i8/sim/tfacc_dbg  

## 構成  

- 入力 file :  tvec/tdump-*-i8.in  
    `tdump-*-i8.in` contains  follows  

0. Conv2D parameters  
1. input  
2. filter  
3. bias  
4. quant  
5. output  
6. refout  

----

1. cma 領域に 1. ~ 6. の領域確保  
2. input, filter, bias, quant を file からロード  
3. kick, wait for run goes to 0  
4. flush output cache  
5. compare output and refout  

----

u96:arm と FPGA/PL とのインターフェース  
memory: cma 領域を用いる。
base:a0000000 range:10000  rv32 memory  

    #define ACCPARAMS       (0x00000100)  // 
    m_reg = reinterpret_cast<uint32_t*>(cma_mmap(0xa0000000, 0x10000));
    accparams = reinterpret_cast<uint32_t*>(&m_reg[ACCPARAMS/4]);

    set_param_and_kick() で 
    BASEADR_OUT[0]  = get_param(0); // accparams[0]
    BASEADR_IN[0]   = get_param(1); // accparams[1]
    BASEADR_FILT[0] = get_param(2);
    BASEADR_BIAS[0] = get_param(3);
    BASEADR_QUANT[0] = get_param(4);

tfacc_u8.cc  
m_reg  

```C
void kick_tfacc()
{
    m_reg[0x84/4] = 0x1;    // apb slvreg1[0]->1  irq to rv32  
}
```

arm から kTfaccRun = 1; -> irq to rv32 ->  set_param_and_kick()  

Conv2DquantPerChannel()  

    param set  
    set_param(kTfaccRun, 1);    // run tfacc
    kick_tfacc();

APB register (m_reg)  
slvreg0  0x80  [0] xreset for rv32  
slvreg1  0x84  [0] rv32 eirq  RUN_REG  

include/tfacc.h:#define RUN_REG         ((volatile u32 *)0x00000084)  



## 6th-ai-reference  追試  

8/31 ~ 9/1  train 13.5H で完了、結果は正しそう  




