//
// rv32_core
//   Instruction / Data dual port RAM
//

`timescale 1ns/1ns
`include "rv_types.svh"

module rv_mem #(parameter Nk = 32)(
    input  logic clk,
    input  logic xreset,
    input  logic rdy,
    //-- Insn Bus (read only)
    input  u32_t i_adr,
    output u32_t i_dr,
    input  logic i_re,
    //-- Data Bus
    input  u32_t d_adr,
    input  u32_t d_dw,
    output u32_t d_dr,
    input  u4_t  d_we,
    input  logic d_re,
    //-- Peripheral Bus
    input  u32_t p_adr,
    input  u32_t p_dw,
    output u32_t p_dr,
    input  logic p_we,
    input  logic p_re,
    output logic p_ack
    );

parameter Nb = $clog2(Nk) + 10; // Nk:32(kB) -> Nb:15(bit)

//-- Registers
//-- wire
u32_t addra, addrb; // : std_logic_vector(13 downto 0);
u32_t doa, dob;
u32_t dob2;
u32_t dib;
logic csa, csb;
logic csb2;
logic ena, enb;
u4_t  web;
logic d_re1;
logic i_re1;
logic den;

assign den = (d_we != 4'b0000) || d_re ? (csb & xreset) : 1'b0;    // when d_we /= 0 or d_re = '1'	else '0';	-- d-bus enable

assign addra = i_adr;
assign addrb = den ? d_adr : p_adr;

assign csa  = i_adr[31:Nb] == '0;
assign csb  = d_adr[31:Nb] == '0;
assign csb2 = p_adr[31:Nb] == '0;

assign ena = csa ? i_re & rdy : '0;
//  ena <= i_re and rdy 	when csa = '1'	else '0';
assign enb = (csb | csb2);    // & rdy;
// enb <= (csb or csb2) and rdy;
assign web = !rdy ? 4'b0000 : 
             (den ? d_we : 
             (p_we && csb2 ? 4'b1111 : 4'b0000));
//  web <= "0000"	when rdy = '0'	else
//	d_we	when den = '1'	else
//	"1111"	when p_we = '1' and csb2 = '1'	else 
//	"0000";
assign dib = den ? d_dw : p_dw;
//  dib  <= std_logic_vector(d_dw)	when den = '1' else
//	  std_logic_vector(p_dw);

assign i_dr = i_re1 ? doa : '0;
//  i_dr <= unsigned(doa) when i_re1 = '1'	else (others => '0');
assign d_dr = d_re1 ? dob : '0;
//  d_dr <= unsigned(dob) when d_re1 = '1'	else (others => '0');
assign p_dr = dob;
// p_dr <= unsigned(dob);
assign p_ack = (p_we || p_re) & !den;
//  p_ack <= (p_we or p_re) and not den;

  always_ff@(posedge clk) begin
    if(rdy) begin
      d_re1 <= csb & d_re;
      i_re1 <= csa & i_re;
    end
  end

  dpram #(.ADDR_WIDTH(Nb-2), .init_file_u("prog_u.mem"), .init_file_l("prog_l.mem")) u_dpram (
      .clk  (clk),
      .enaA (1'b1),      // read port
      .addrA(addra[Nb-1:1]), // half word address
      .doutA(doa),
      .enaB (enb),     // read write port
      .weB  (web),
      .addrB(addrb[Nb-1:2]),
      .dinB (dib),
      .doutB(dob)
      );

//u_dpram32kB : dpram32kB
//  PORT MAP (
//    clka  => clk,    ena   => ena,    wea   => wea,
//    addra => addra(12 downto 0),      dina  => dia,    douta => doa,
//    clkb  => clk,    enb   => enb,    web   => web,
//    addrb => addrb(12 downto 0),      dinb  => dib,    doutb => dob
//  );
endmodule

