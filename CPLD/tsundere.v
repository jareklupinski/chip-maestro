// Copyright (C) 1991-2010 Altera Corporation
// Your use of Altera Corporation's design tools, logic functions 
// and other software and tools, and its AMPP partner logic 
// functions, and any output files from any of the foregoing 
// (including device programming or simulation files), and any 
// associated documentation or information are expressly subject 
// to the terms and conditions of the Altera Program License 
// Subscription Agreement, Altera MegaCore Function License 
// Agreement, or other applicable license agreement, including, 
// without limitation, that your use is for the sole purpose of 
// programming logic devices manufactured by Altera and sold by 
// Altera or its authorized distributors.  Please refer to the 
// applicable agreement for further details.

// PROGRAM		"Quartus II"
// VERSION		"Version 9.1 Build 350 03/24/2010 Service Pack 2 SJ Web Edition"
// CREATED		"Fri Nov 19 02:36:31 2010"

module tsundere(
	latch,
	SCLK,
	SERIALIN,
	/CE,
	A,
	Interrupt,
	D
);


input	latch;
input	SCLK;
input	SERIALIN;
input	/CE;
input	[2:0] A;
output	Interrupt;
output	[7:0] D;

wire	[7:0] add;
wire	[2:0] address;
wire	[7:0] data;
wire	[7:0] SYNTHESIZED_WIRE_0;
wire	[7:0] SYNTHESIZED_WIRE_1;
wire	[7:0] SYNTHESIZED_WIRE_2;
wire	[7:0] SYNTHESIZED_WIRE_3;
wire	[7:0] SYNTHESIZED_WIRE_4;
wire	[7:0] SYNTHESIZED_WIRE_5;
wire	SYNTHESIZED_WIRE_6;
wire	SYNTHESIZED_WIRE_7;
wire	SYNTHESIZED_WIRE_14;
wire	SYNTHESIZED_WIRE_9;
wire	SYNTHESIZED_WIRE_10;
wire	SYNTHESIZED_WIRE_12;
wire	[7:0] SYNTHESIZED_WIRE_13;

assign	SYNTHESIZED_WIRE_7 = 1;
assign	SYNTHESIZED_WIRE_14 = 0;
assign	SYNTHESIZED_WIRE_9 = 1;




lpm_mux0	b2v_inst(
	.data0x(SYNTHESIZED_WIRE_0),
	.data1x(data),
	.data2x(SYNTHESIZED_WIRE_1),
	.data3x(add),
	.data4x(SYNTHESIZED_WIRE_2),
	.data5x(SYNTHESIZED_WIRE_3),
	.data6x(SYNTHESIZED_WIRE_4),
	.data7x(SYNTHESIZED_WIRE_5),
	.sel(address),
	.result(SYNTHESIZED_WIRE_13));


A0rom	b2v_inst1(
	.result(SYNTHESIZED_WIRE_0));

assign	SYNTHESIZED_WIRE_6 =  ~address[2];

assign	Interrupt = address[0] & address[1] & SYNTHESIZED_WIRE_6;


\8Crom 	b2v_inst2(
	.result(SYNTHESIZED_WIRE_1));

assign	SYNTHESIZED_WIRE_12 =  ~/CE;


\74595 	b2v_inst29(
	.RCLK(latch),
	.SRCLRN(SYNTHESIZED_WIRE_7),
	.SRCLK(SCLK),
	.SER(SERIALIN),
	.GN(SYNTHESIZED_WIRE_14),
	.QHN(SYNTHESIZED_WIRE_10),
	.QH(add[7]),
	.QG(add[6]),
	.QF(add[5]),
	.QE(add[4]),
	.QC(add[2]),
	.QD(add[3]),
	.QA(add[0]),
	.QB(add[1]));


\40rom 	b2v_inst3(
	.result(SYNTHESIZED_WIRE_2));


\74595 	b2v_inst30(
	.RCLK(latch),
	.SRCLRN(SYNTHESIZED_WIRE_9),
	.SRCLK(SCLK),
	.SER(SYNTHESIZED_WIRE_10),
	.GN(SYNTHESIZED_WIRE_14),
	
	.QH(data[7]),
	.QG(data[6]),
	.QF(data[5]),
	.QE(data[4]),
	.QC(data[2]),
	.QD(data[3]),
	.QA(data[0]),
	.QB(data[1]));





\00rom 	b2v_inst4(
	.result(SYNTHESIZED_WIRE_3));


\00rom 	b2v_inst5(
	.result(SYNTHESIZED_WIRE_4));


\80rom 	b2v_inst6(
	.result(SYNTHESIZED_WIRE_5));


lpm_bustri0	b2v_inst9(
	.enabledt(SYNTHESIZED_WIRE_12),
	.data(SYNTHESIZED_WIRE_13),
	.tridata(D)
	);

assign	address = A;

endmodule
