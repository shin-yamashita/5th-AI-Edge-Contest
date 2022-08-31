//--
//-- rv_axi_port
//    axi access port for rv32_core bus
//-- 2022/01/30 

`timescale 1ns/1ns
`include "logic_types.svh"

module rv_axi_port #(parameter BASE = 16'h0010, LAST = 16'h7FFF) ( // 0x0010_0000 ~ 0x7fff_ffff

    input logic xrst,
//-- CPU bus
    input  logic cclk,    //	: in  	std_logic;
    input  u32_t adr,    //       adr     : in  	unsigned(31 downto 0);
    input  u4_t  we,     //      : in  	std_logic_vector(3 downto 0);
    input  logic re,     //      : in  	std_logic;
    input  logic rdyin,    //	: in	std_logic;
    output logic rdy,    //	: out	std_logic;
    input  u32_t dw,    //      : in  	unsigned(31 downto 0);
    output u32_t dr,    //      : out 	unsigned(31 downto 0);

//-- axi interface
    input  logic        aclk,           //
    input  logic        arst_n,         //

    output logic [39:0] awaddr,         // write port
    output logic [7:0]  awlen,          //
    output logic        awvalid,        //
    input  logic        awready,        // 

    output logic [31:0] wr_data,        //
    output logic        wvalid,         //
    output logic        wlast,          //
    input  logic        wready,         //

    output logic [39:0] araddr,         //
    output logic [7:0]  arlen,          //
    output logic        arvalid,        //
    input  logic        arready,        //

    input  logic [31:0] rd_data,        //
    input  logic        rvalid,         //
    input  logic        rlast,          //
    output logic        rready          //
    );

    
typedef enum u3_t {Idle, Ack, WrdyWait, Writecyc, Writecmd, Readcyc, Readcmd, Post} migstate_t;

migstate_t mst;

logic cs_d;    //	: std_logic;
logic req2, wrq2, ack2, re1; //	: std_logic;

//-- chip select
// assign cs_d = adr[31:16] >= BASE && adr[31:16] <= LAST;
 assign cs_d = 1'b0;    // disable
 assign req2 = cs_d && (re || (we != '0));
 assign wrq2 = cs_d && (we != '0);
// rdy <= '0' when cs_d = '1' and hit = '0' and (we /= 0 or re = '1')	else '1';

// assign rdy = !req2 || rvalid || wready || !xrst;
// assign dr = re1 ? rd_data : '0;
// assign wr_data = dw;
 assign rdy = 1'b1; //
 assign dr = '0;
 assign wr_data = '0;

//-- axi sequencer

// web <= (ren2e,ren2e,ren2e,ren2e);
 always_ff@(posedge aclk) begin
    if(!arst_n) begin
        mst <= Idle;
        ack2 <= '0;
        //ren2 <= '0;
        wvalid <= '0;
        rready <= '0;
        re1 <= '0;
    end else begin
//        re1 <= cs_d & re;
        case(mst)
        Idle: begin
            ack2 <= '0;
            //ren2 <= '0;
            wvalid <= '0;
            awvalid <= '0;
            arvalid <= '0;
            rready <= '0;
            if(req2) begin
                mst <= Ack;
           end
        end
        Ack: begin   // =>
            ack2 <= '1;
            if(wrq2) begin
                mst <= Writecmd;
                awvalid <= '1;
            end else begin
                mst <= Readcmd;
                arvalid <= '1;
            end
        end
        Writecmd: begin   // =>
            if(awready) begin
                mst <= Writecyc;
                awvalid <= '0;
                wvalid <= '1;
            end else begin
                awvalid <= '1;
            end
        end
        Writecyc: begin   // =>
            if(wvalid && wready) begin
                wvalid <= '0;
                mst <= Idle;
            end
        end
        Readcmd: begin   // =>
            if(arready) begin
                mst <= Readcyc;
                arvalid <= '0;
                rready <= '1;
            end else begin
                arvalid <= '1;
            end
        end
        Readcyc: begin // =>
            arvalid <= '0;
            if(rvalid) begin
                rready <= '0;
                mst <= Idle;
            end
        end
        Post: begin // =>
            wvalid <= '0;
            mst <= Idle;
            rready <= '0;
        end
        endcase
    end
 end
 
 assign wlast = 1'b1;

 assign  awlen = 8'b00000000; //	-- 16x32=64byte burst
 assign  arlen = 8'b00000000;    //	-- 16x32=64byte burst
 assign  awaddr = adr;
 assign  araddr = adr;

endmodule


