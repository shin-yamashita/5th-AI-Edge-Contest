//
// tfacc_core
//

`timescale 1ns/1ns
`include "logic_types.svh"

module tfacc_core 
    #(parameter Np = 1,
      parameter debug = 0)
  (
    input  logic clk,
    input  logic xrst,

    // sr_cpu bus
    input  u32_t adr,
    input  u4_t  we,     
    input  logic re,     
    output logic rdy,    
    input  u32_t dw,  
    output u32_t dr,
    output logic irq,
        
    // cache bus a output
    output logic wreq[Np],	// write request (512B burst)
    input  logic wack[Np],	// write ack/enable
    output u32_t wadr[Np],	// burst start address (byte, 0 offset)   
    output u64_t wdata[Np],	// fp x2
    output u8_t  wstb[Np],
    output u8_t  wlen[Np],	// write burst length - 1
    output u32_t wbase,

    // cache bus b input
    output logic rreq[Np],     // read request (1024B burst)
    input  logic rack[Np],     // read ack/enable
    output u24_t radr[Np],     //  burst start address (byte, 0 offset)  
    input  u64_t rdata[Np],     // fp16 x2
    output u32_t rbase,
//    output logic in_rdy,

    // read cache bus c filter uint8
    output u32_t c_base,
    output u32_t c_adr,   
    output logic c_re,     
    input  logic c_rdy,    
    input  u8_t  c_dr,

    // read cache bus d bias int32
    output u32_t d_base,
    output u32_t d_adr,    
    output logic d_re,     
    input  logic d_rdy,    
    input  s32_t d_dr,
    
    // read cache bus e quant uint32
    output u32_t e_base,
    output u32_t e_adr,    
    output logic e_re,     
    input  logic e_rdy,    
    input  u32_t e_dr,

    output u4_t  clreq, // cache clear request
    
    output logic [4:0] fp
  );

function logic and_unpack(input logic x[Np]);
    logic y;
    y = 1'b1;
    for(int i = 0; i < Np; i++) begin
      y = y & x[i];
    end
    return y;
endfunction

//--- u8adrgen / i8mac
logic kick;
logic pwe, pre;
u8_t  padr;
u32_t pdata, prdata;
u24_t in_adr[Np], out_adr[Np];
u24_t fil_adr;
u12_t bias_adr, quant_adr;
logic fil_rdy, out_rdy, bias_rdy, quant_rdy, run, aen, acl, acvalid, mac_rdy, out_cmpl;
logic aen_1d, acl_1d;
logic valid[Np], ivalid[Np], acv[Np];
s8_t  accd[Np];
s8_t  fil_d;
s32_t bias;
u32_t quant;
s9_t  in_offs , out_offs;// quantize params
s18_t out_mult;
u8_t  out_shift;

logic [4:0]  fpr;

//--- input_cache / output_cache ----
u32_t baseadr[5];    // output, input, filer, bias, quant

logic i_re[Np];     //
logic i_rdy[Np];    //
s8_t  in_d[Np];
u32_t i_adr[Np];    // input addr (byte)
logic civ;          // input cache invalidate

logic o_rdy[Np], o_cmpl[Np];	//
u32_t o_adr[Np];    // output addr (byte)
u8_t  o_dw[Np];     // uint8
logic oen[Np];      // output enable
logic chen[Np];     // para channel enable

u4_t flreq, clbsy;
logic [Np-1:0] flbsy;

assign rbase = baseadr[1];
assign wbase = baseadr[0];

assign c_base = baseadr[2];
assign c_adr = fil_adr;
assign c_re  = aen;
assign fil_rdy = c_rdy;
assign fil_d = c_dr;    // u8

assign d_base = baseadr[3];
assign d_adr = {bias_adr,2'b00};
assign d_re  = run;
assign bias_rdy = d_rdy;
//assign bias  = d_dr; // s32

assign quant_adr = bias_adr;
assign e_base = baseadr[4];
assign e_adr = {quant_adr,2'b00};
assign e_re  = run;
assign quant_rdy = e_rdy;
//assign quant  = e_dr; // u32
assign out_mult = quant[31:15];
assign out_shift = quant[7:0];
logic fb_rdy;

assign mac_rdy = in_rdy & fb_rdy;

assign acvalid  = acv[0];
assign out_rdy  = and_unpack(o_rdy);
assign out_cmpl = and_unpack(o_cmpl);
assign in_rdy   = and_unpack(i_rdy);

always_ff@(posedge clk) begin
  bias  <= d_dr; // s32
  quant <= e_dr; // u32

  for(int i = 0; i < Np; i++) begin
    ivalid[i] <= valid[i];
  end
  aen_1d <= aen;
//  aen_1d <= mac_rdy ? aen : aen_1d;
  acl_1d <= acl;
//  mac_rdy <= in_rdy & fil_rdy & bias_rdy;
  fb_rdy <= fil_rdy & bias_rdy & quant_rdy;
end

generate
  for(genvar i = 0; i < Np; i++) begin

    assign i_re[i]  = chen[i] & aen & valid[i];
    assign i_adr[i] = in_adr[i];
    assign o_adr[i] = out_adr[i];
    assign o_dw[i]  = accd[i];

    input_cache u_input_cache (
      .clk  (clk),        //  input  logic   clk,
      .xrst (xrst & chen[i]),// input logic  xrst,

      .civ  (civ),        //  input  logic  cache invalidate

    // i8mac
      .re   (i_re[i]),    //  input  logic  input data read enable
      .adr  (i_adr[i]),   //  input  u32_t  address (byte, 0 offset)
      .dr   (in_d[i]),    //  output s8_t   input data (int8)
      .rdy  (i_rdy[i]),   //  output logic  1:ready
    //  .rdyin(fil_rdy & bias_rdy),

    // memory
      .rreq (rreq[i]),    //  output logic  read request (1024B burst)
      .rack (rack[i]),    //  input  logic  read ack/enable
      .radr (radr[i]),    //  output u32_t  burst start address (byte, 0 offset)  
      .rdata(rdata[i])    //  input  u64_t  uint8 x 8
    );
    output_cache u_output_cache
      (
      .clk  (clk),          // input  logic
      .xrst (xrst & chen[i]),// input  logic

      .civ  (civ),          // input  logic cache invalidate
      .flreq(flreq[0]),     // input  logic flush request
      .flbsy(flbsy[i]),     // out logic       busy

    // i8mac
      .adr  (o_adr[i]),     // in  u32_t output addr (byte, 0 offset)
      .we   (acvalid & oen[i]),// in  logic write enable
      .dw   (o_dw[i]),      // in  s8_t  int8 output data
      .rdy  (o_rdy[i]),     // out logic ready
      .cmpl (o_cmpl[i]),    // out logic complete

    // memory
      .wreq (wreq[i]),      // out logic  burst write request
      .wack (wack[i]),      // in  logic  write ack, enable
      .wadr (wadr[i]),      // out u32_t  burst addr (byte, 0 offset)
      .wdata(wdata[i]),     // out u64_t  u8 x8
      .wstb (wstb[i]),      // out u8_t   write strobe
      .wlen (wlen[i])       // out u8_t   write burst length - 1
    );

    i8mac u_i8mac (
     .clk       (clk),             //in                clk     , //
     .xreset    (xrst),            //in                xreset  , //
     .aen       (aen_1d),          //in                aen     , // acc enable
     .acl       (acl_1d),          //in                acl     , // acc clear
     .rdy       (mac_rdy),         //in                rdy     , // memory read data ready
     .ivalid    (ivalid[i]),       //in                ivalid  , // input data valid
     .in_d      (in_d[i]),         //in  signed [7:0]  in_d    , // s8 input
     .fil_d     (fil_d),           //in  signed [7:0]  fil_d   , // s8 filter
     .bias      (bias),            //in  signed [31:0] bias    , // s32 bias
     .in_offs   (in_offs),         //in  signed [8:0]  in_offs , // quantize params
     .out_offs  (out_offs),        //in  signed [8:0]  out_offs,
     .out_mult  (out_mult),        //in  signed [17:0] out_mult,
     .out_shift (out_shift),       //in  unsigned [7:0]  out_shift,
     .accd      (accd[i]),         //out signed [7:0]  accd    , // s8 out
     .acvalid   (acv[i])           //out               acvalid   // accd data valid
    );
  end

endgenerate


// sr_cpu interface  
// adrgen parameter regs
//  ffff0300   kick, runflag
//  ffff0304 - ffff0310  baseadr[4]
//  ffff0400 - ffff047f  accparams[18]
logic cs_prm, cs_flg, cs_cache;
u32_t flgdr;
u4_t monisel;
logic flbsyflg;
logic inten, cmpl;    // run complete interrupt, enable,flag
u2_t d_run;
assign irq = inten & cmpl;

assign cs_cache = (adr & 32'hfffffffc) == 32'hffff0180 ? 1'b1 : 1'b0; // ffff0180-ffff0183
assign cs_flg = (adr[31:8] == 24'hffff03);  // ffff0300 - ffff03ff
assign cs_prm = ({adr[31:9],1'b0} == 24'hffff04);  // ffff0400 - ffff05ff 128word
// flreq3,2,1,0 clreq3,2,1,0
//assign flreq = (cs_cache && we[3]) ? dw[31:28] : 4'b0000;
//assign clreq = (cs_cache && we[3]) ? dw[27:24] : 4'b0000;
assign flreq = (cs_cache && we[0]) ? dw[7:4] : 4'b0000;
assign clreq = (cs_cache && we[0]) ? dw[3:0] : 4'b0000;
assign civ = clreq[0];

assign pwe   = cs_prm & (we != 4'b0000);
assign pre   = cs_prm & re;
assign kick  = cs_flg & we[0] & dw[0] & (adr[7:2] == 0);
assign padr  = adr[9:2];
assign pdata = dw;
assign dr    = prdata | flgdr;
assign rdy   = 1'b1;

assign flbsyflg = flbsy != 0;

always@(posedge clk) begin
  if(!xrst) begin
      monisel <= 4'h0;
  end else begin
    if(cs_flg && (we != 0)) begin
        case(adr[7:2])
        6'd1: baseadr[0] <= dw;      // ffff0304 out
        6'd2: baseadr[1] <= dw;      // ffff0308 in
        6'd3: baseadr[2] <= dw;      // ffff030c filt
        6'd4: baseadr[3] <= dw;      // ffff0310 bias
        6'd5: baseadr[4] <= dw;      // ffff0314 quant
        
        6'd8: monisel <= dw[3:0];    // ffff0320 monisel
        6'd9: fpr     <= dw[4:0];    // ffff0324 fp test
        endcase
    end
    d_run <= {d_run[0], run};
    if(!xrst) begin
        monisel <= 4'h0;
        inten <= 1'b0;   
    end else if(cs_flg && we[1] && (adr[7:2] == 0)) begin
        inten <= dw[9];
        cmpl <= dw[8];  
    end else if (d_run == 'd2) begin    // run trailing edge
        cmpl <= inten;
    end
  end
  
  //-- read
  if(cs_flg & re) begin
    case(adr[7:2])
    6'd0: flgdr <= {22'h0, inten, cmpl, 7'h0, run};// ffff0300 run flag
    
    6'd1: flgdr <= baseadr[0];      // ffff0304 out
    6'd2: flgdr <= baseadr[1];      // ffff0308 in
    6'd3: flgdr <= baseadr[2];      // ffff030c filt
    6'd4: flgdr <= baseadr[3];      // ffff0310 bias
    6'd5: flgdr <= baseadr[4];      // ffff0314 quant
    
    6'd7: flgdr <= Np;      // ffff031c Np
    6'd8: flgdr <= monisel; // ffff0320
    6'd9: flgdr <= fpr;     // ffff0324
    default: flgdr <= 32'h00000000;
    endcase
  end else if(cs_cache & re) begin
//    flgdr <= {flbsyflg, out_cmpl, 30'h0000000};
    flgdr <= {flbsyflg, out_cmpl, 6'h0};
 end else flgdr <= 32'h00000000;
end


u8adrgen #(.Np(Np)) u_u8adrgen
  (
  .clk      (clk),          //  input  logic clk,
  .xrst     (xrst),         //  input  logic xrst,
//
  .kick     (kick),         //  input  logic kick,     // start 1 frame sequence

// Convolution parameter
  .pwe      (pwe),          //  input  logic pwe,      // param register write
  .pre      (pre),          //  input  logic pre,      //                read
  .padr     (padr),         //  input  u8_t  padr,     // param addr 0 to 19
  .pdata    (pdata),        //  input  u32_t pdata,    // param write data
  .prdata   (prdata),       //  output u32_t prdata,   //       read data
// address
  .in_adr   (in_adr),       //  output u24_t in_adr[Np], // input addr (byte)
  .valid    (valid),        //  output logic valid[Np],  // in_adr valid
  .in_rdy   (in_rdy),       //  input  logic in_rdy,
  .fil_adr  (fil_adr),      //  output u24_t fil_adr,  // filter addr (byte)
  .fil_rdy  (fb_rdy),	//fil_rdy),      //  input  logic fil_rdy,
  .out_adr  (out_adr),      //  output u24_t out_adr[Np],// output addr (byte)
  .out_rdy  (out_rdy),      //  input  logic out_rdy,
  .bias_adr (bias_adr),     //  output u12_t bias_adr, // bias addr (byte)
  .bias_rdy (1'b1),	//bias_rdy),     //  input  logic bias_rdy,
  .oen      (oen),          //  output logic oen[Np],  // output enable
  .chen     (chen),         //  output logic chen[Np], // para channel enable

// running flag
  .run      (run),          //  output logic run,      // 1 : running 

// i8mac control
  .aen      (aen),          //  output logic aen,      // acc en
  .acl      (acl),          //  output logic acl,      // acc clear
  .acvalid  (acvalid),      //  input  logic acvalid,  // acc data valid

// quantize parameters
  .in_offs  (in_offs),      //  output s9_t  in_offs , // quantize params
  .out_offs (out_offs)     //  output s9_t  out_offs,
 );

assign fp = 0;

/*---
always@(*) begin
  case(monisel)
  0: fp = {run    , aen,    acl,     acvalid, valid[0]};
  1: fp = {out_rdy, in_rdy, fil_rdy, bias_rdy,mac_rdy};
  2: fp = {accd[0][31], rrd1[0][31], rrd2[31], bias[31], mac_rdy};
  3: fp = {awen,     out_adr[0][0], wdata[0][31], acvalid,  accd[0][31]};
  4: fp = fpr;
  default: fp = 0;
  endcase
end
---*/
/*---
ila_0 u_ila (
	.clk(clk), // input wire clk

	.probe0(in_d[0]), 	// input wire [7:0]  probe0  
	.probe1(fil_d), 	// input wire [7:0]  probe1 
	.probe2(bias), 		// input wire [31:0]  probe2 
	.probe3(accd[0]), 	// input wire [7:0]  probe3 
	.probe4(
          {run,aen_1d,acl_1d,acvalid,mac_rdy,ivalid[0],in_rdy,fil_rdy}) // input wire [7:0]  probe4
         // 7     6     5      4       3         2       1        0
);
---*/

endmodule


