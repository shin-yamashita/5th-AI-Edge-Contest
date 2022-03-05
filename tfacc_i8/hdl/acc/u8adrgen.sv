
`timescale 1ns/1ns

// u8adrgen.sv
// Conv2d dwConv2d input/filter/bias/output address generator
//
// quantized op version
//
`include "logic_types.svh"

module u8adrgen #(parameter Np = 1)
  (
  input  logic clk,
  input  logic xrst,
//
  input  logic kick,	// start 1 frame sequence

// Convolution parameter
  input  logic pwe,	// param register write
  input  logic pre,	//                read
  input  u8_t  padr,	// param addr 0 to 22 , 23 to 23+Np-1
  input  u32_t pdata,	// param write data
  output u32_t prdata,	//       read data

// address
  output u24_t in_adr[Np], // input addr (byte)
  output logic valid[Np],  // in_adr valid
  input  logic in_rdy,
  output u24_t fil_adr,	// filter addr (byte)
  input  logic fil_rdy,
  output u24_t out_adr[Np],// output addr (byte)
  input  logic out_rdy,
  output u18_t bias_adr,	// bias addr (byte)
  input  logic bias_rdy,
  output logic oen[Np],	// output enable
  output logic chen[Np],	// para channel enable

// running flag
  output logic run,	// 1 : running 

// u8mac control
  output logic aen,	// acc en
  output logic acl,	// acc clear
  input  logic acvalid,	// acc data valid

// quantize parameters
  output s9_t  in_offs , // quantize params
  output s9_t  out_offs
 );


 u24_t in_adr_i[Np], in_adr_h[Np];
 u24_t fil_adr_i, fil_adr_h;
 logic valid_i[Np], valid_h[Np];
 logic rdy;
 logic aen_i, aen_h;

 always_comb begin
  for(int i = 0; i < Np; i++) begin
    in_adr[i] = rdy ? in_adr_i[i] : in_adr_h[i];
    valid[i]  = rdy ? valid_i[i]  : valid_h[i];
  end
 end
 assign fil_adr = rdy ? fil_adr_i : fil_adr_h;
 assign aen = rdy ? aen_i : aen_h;

 always@(posedge clk) begin
  if(!xrst) begin
    aen_h <= 1'b0;
  end else begin
    for(int i = 0; i < Np; i++) begin
      in_adr_h[i] <= (rdy ? in_adr_i[i] : in_adr_h[i]);
      valid_h[i]  <= (rdy ? valid_i[i] : valid_h[i]);
    end
    fil_adr_h <= rdy ? fil_adr_i : fil_adr_h;
    aen_h <= rdy ? aen_i : aen_h;
  end
 end

 u11_t inH, inW, inC;
 u3_t  filH, filW;
 u11_t filC;
 u11_t outH, outW, outC;
 u20_t outWH, pH;
 s16_t dim123;

 u3_t strH, strW, dilH, dilW, padH, padW;
 u8_t n_chen;
 bit  depthmul;
 bit  calc, outtc;
 u12_t ch1C, ch2C, ch3C, finc, dw_c;
 s24_t inCW;

 u24_t cntrun, cntxrdy;

 assign ch1C = depthmul ? inC : outC;
 assign ch2C = depthmul ? 'd1 : inC;
 assign ch3C = depthmul ? outC : inC;
 assign finc = depthmul ? filC: 'd1;
 
/*----- param read disable ----
 // parameter register read
 always@(posedge clk) begin
  if(pre)
    case(padr)
     0: prdata <= inH;
     1: prdata <= inW;
     2: prdata <= inC;
     3: prdata <= filH;
     4: prdata <= filW;
     5: prdata <= filC;
     6: prdata <= outH;
     7: prdata <= outW;
     8: prdata <= outC;
     9: prdata <= pH;
    10: prdata <= {strH,dilH,padH};
    11: prdata <= {strW,dilW,padW};
    12: prdata <= depthmul;
    13: prdata <= actmin;
    14: prdata <= actmax;
    15: prdata <= in_offs;
    16: prdata <= fil_offs;
    17: prdata <= out_offs;
    18: prdata <= out_mult;
    19: prdata <= out_shift;
    default: prdata <= 'd0;
    endcase
  else
    prdata <= 'd0;
 end
---------*/
 always@(posedge clk) begin
  if(kick) begin
    cntrun <= 'd0;
    cntxrdy <= 'd0;
  end else begin
    if(run) cntrun <= cntrun + 1;
    if(run && !rdy) cntxrdy <= cntxrdy + 1;
  end
  if(pre)
    case(padr)
     0: prdata <= cntrun;
     1: prdata <= cntxrdy;
    default: prdata <= 'd0;
    endcase
  else
    prdata <= 'd0;
 end

 enum {Idle, AccClear, Acc, AccFlush, AccTerm} state;

 // address counter
 s12_t in_x[Np],  in_y[Np];
 s12_t in_xo[Np], in_yo[Np];
 u11_t  out_x[Np], out_y[Np];
 u11_t out_c, in_c;
 u3_t  fil_x,  fil_y;
 s28_t in_a[Np];
 u20_t out_a;

 assign dw_c = depthmul ? out_c : in_c;

 always@(posedge clk) begin
   for(int i = 0; i < Np; i++) begin
     in_xo[i] <= signed'((out_x[i] * strW) - padW);
     in_yo[i] <= signed'((out_y[i] * strH) - padH);
     in_a[i]  <= inCW * in_yo[i] + signed'(ch3C) * in_xo[i];
   end
 end

 always_comb begin
  for(int i = 0; i < Np; i++) begin
   in_x[i] <= in_xo[i] + fil_x;
   in_y[i] <= in_yo[i] + fil_y;
  end
 end


// logic rdy;
 assign rdy = in_rdy && fil_rdy && bias_rdy;
 logic [2:0] ncalc;

/*---- Conv3quant ---*/
 always@(posedge clk) begin
  if(!xrst) begin
    state <= Idle;
    chen   <= '{Np{'b0}};
  end
// params
  if(pwe) begin
    case(padr)
     0: inH <= pdata;
     1: inW <= pdata;
     2: inC <= pdata;
     3: filH <= pdata;
     4: filW <= pdata;
     5: filC <= pdata;
     6: outH <= pdata;
     7: outW <= pdata;
     8: outC <= pdata;
     9: pH <= pdata;	// (outWH + (Np-1)) / Np  srmon.c
    10: {strH,dilH,padH} <= pdata;
    11: {strW,dilW,padW} <= pdata;
    12: depthmul <= pdata;
//    13: actmin <= pdata;
//    14: actmax <= pdata;
    15: in_offs <= pdata;
//    16: fil_offs <= pdata;
    17: out_offs <= pdata;
//    18: out_mult <= pdata;
//    19: out_shift <= pdata;
    20: outWH <= pdata;		// outW * outH
    21: dim123 <= pdata;	// filH * filW * filC 
    22: n_chen <= pdata;	// (outWH+pH-1)/pH
    default: ;
    endcase
  end

// output address counter
  if(state == AccFlush && calc) begin
    if(out_c == 0) begin
      for(int i = 0; i < Np; i++) begin
        if(oen[i]) begin
          if(out_x[i] == outW-1) begin
            out_x[i] <= 'd0;
            out_y[i] <= out_y[i] + 1;
          end else begin
            out_x[i] <= out_x[i] + 1;
          end
        end
      end
    end
  end else begin
    if(pwe) begin
        for(int i = 0; i < Np; i++) begin
          if(padr == i + 24) begin
            out_y[i] <= pdata[26:16];
            out_x[i] <= pdata[10:0];
          end
        end
     end
  end

  if(ncalc > 0 && rdy) begin // Prepare filter address
    if(depthmul) fil_adr_i <= out_c;
    else         fil_adr_i <= dim123 * out_c;

    inCW <= ch3C * inW;
  end

  if(state == Idle) begin
    if(kick) begin
      state  <= AccClear;
      calc   <= 1'b1;
    end else begin
      calc   <= 1'b0;
    end
    run    <= 1'b0;
    out_a  <= 0;
    out_c  <= 0;
    fil_y  <= 0;
    fil_x  <= 0;
    in_c   <= 0;
    valid_i  <= '{Np{'b0}};
    oen    <= '{Np{'b0}};
//    chen   <= '{Np{'b0}};
    aen_i    <= 1'b0;
    acl    <= 1'b0;
    outtc  <= 0;
    ncalc  <= 'd0;
    in_adr_i  <= '{Np{'d0}};
    fil_adr_i <= 'd0;
    bias_adr <= 'd0;
  end else if(state == AccClear) begin
    valid_i  <= '{Np{'b0}};
//    oen    <= '{Np{'b1}};
    aen_i    <= 1'b0;
    acl    <= 1'b1;
    calc   <= 1'b0;
    ncalc  <= ncalc + 1;
    if(ncalc > 1) state  <= Acc;
    for(int i = 0; i < Np; i++) begin
      chen[i] <= i < n_chen;
      oen[i] <= i < n_chen;
      case(ncalc)
      0: out_adr[i] <= out_y[i] * outW + out_x[i];
      1: out_adr[i] <= out_adr[i] * outC;
      endcase
    end
  end else if(state == Acc) begin
    aen_i    <= 1'b1;
   	acl    <= 1'b0;
    outtc  <= 1'b0;
    ncalc  <= 'd0;
    run    <= 1'b1;

    if(rdy) begin
      if(in_c < ch2C-1) begin
        in_c <= in_c + 1;
      end else begin
        in_c <= 0;
        if(fil_x < filW-1) begin
          fil_x <= fil_x + 1;
        end else begin
          fil_x <= 0;
          if(fil_y < filH-1) begin
            fil_y <= fil_y + 1;
          end else begin
            fil_y <= 0;
            state <= AccFlush;
            calc <= 1'b1;
            if(out_c < ch1C-1) begin
              out_c <= out_c + 1;
            end else begin
              out_c <= 0;
              if(out_a < pH-1) begin
                out_a <= out_a + 1;
              end else begin
                out_a <= 0;
                state <= AccTerm;
              end

            end // out_c
          end // fil_y
        end // fil_x
      end // in_c

      if(aen_i) fil_adr_i <= fil_adr_i + finc;

      bias_adr <= out_c;
      for(int i = 0; i < Np; i++) begin
//        in_adr_i[i]  <= inCW * signed'(in_yo[i] + fil_y) + ch3C * signed'(in_xo[i] + fil_x) + dw_c;
        in_adr_i[i]  <= in_a[i] + signed'((inCW * fil_y) + (ch3C * fil_x) + dw_c);

        if(outtc) begin
          out_adr[i] <= out_adr[i] + oen[i];
          oen[i]     <= out_y[i] < outH;
        end
        valid_i[i]   <= ((in_x[i] >= 0) && (in_x[i] < inW) && (in_y[i] >= 0) && (in_y[i] < inH));
      end // for
    end

  end else if(state == AccFlush) begin
    ncalc  <= ncalc < 7 ? ncalc + 1 : 'd7;
    if(rdy) begin
      aen_i <= 1'b0;
      calc <= 1'b0;
    end
    if(acvalid && out_rdy) begin // output write
      acl <= 1'b1;
      state <= Acc;
      outtc <= 1'b1;
    end
  end else if(state == AccTerm) begin
  	if(rdy) begin
      aen_i <= 1'b0;
      calc <= 1'b0;
  	end
    if(acvalid && out_rdy) begin // output write last 1 data
      acl <= 1'b1;
      run <= 1'b0;
      state <= Idle;
    end
  end
 end

endmodule

