-- Copyright (C) 1991-2010 Altera Corporation
-- Your use of Altera Corporation's design tools, logic functions 
-- and other software and tools, and its AMPP partner logic 
-- functions, and any output files from any of the foregoing 
-- (including device programming or simulation files), and any 
-- associated documentation or information are expressly subject 
-- to the terms and conditions of the Altera Program License 
-- Subscription Agreement, Altera MegaCore Function License 
-- Agreement, or other applicable license agreement, including, 
-- without limitation, that your use is for the sole purpose of 
-- programming logic devices manufactured by Altera and sold by 
-- Altera or its authorized distributors.  Please refer to the 
-- applicable agreement for further details.

library ieee;
use ieee.std_logic_1164.all;
library altera;
use altera.altera_syn_attributes.all;

entity Tsundere is
	port
	(
		/CE : in std_logic;
		A : in std_logic_vector(2 downto 0);
		D : out std_logic_vector(7 downto 0);
		Interrupt : out std_logic;
		latch : in std_logic;
		SCLK : in std_logic;
		SERIALIN : in std_logic;
		TCK : in std_logic;
		TDI : in std_logic;
		TDO : out std_logic;
		TMS : in std_logic
	);

end Tsundere;

architecture ppl_type of Tsundere is

begin

end;
