`default_nettype none
`timescale 1ns / 1ns

module testbench #(
	parameter max_recvpkt = 10,
	parameter nPreamble = 8,
	parameter nIFG = 12
)(
	input wire clk156,
	input wire cold_reset, 

	/* PHY0 */
	input  wire        phy0_tx_tready,
	output wire        phy0_tx_tvalid,
	output wire [63:0] phy0_tx_tdata,
	output wire [ 7:0] phy0_tx_tkeep,
	output wire        phy0_tx_tlast,
	output wire        phy0_tx_tuser,
	output wire        phy0_rx_tready,
	input  wire        phy0_rx_tvalid,
	input  wire [63:0] phy0_rx_tdata,
	input  wire [ 7:0] phy0_rx_tkeep,
	input  wire        phy0_rx_tlast,
	input  wire        phy0_rx_tuser,

	/* PHY1 */
	input  wire        phy1_tx_tready,
	output wire        phy1_tx_tvalid,
	output wire [63:0] phy1_tx_tdata,
	output wire [ 7:0] phy1_tx_tkeep,
	output wire        phy1_tx_tlast,
	output wire        phy1_tx_tuser,
	output wire        phy1_rx_tready,
	input  wire        phy1_rx_tvalid,
	input  wire [63:0] phy1_rx_tdata,
	input  wire [ 7:0] phy1_rx_tkeep,
	input  wire        phy1_rx_tlast,
	input  wire        phy1_rx_tuser,

	/* PHY2 */
	input  wire        phy2_tx_tready,
	output wire        phy2_tx_tvalid,
	output wire [63:0] phy2_tx_tdata,
	output wire [ 7:0] phy2_tx_tkeep,
	output wire        phy2_tx_tlast,
	output wire        phy2_tx_tuser,
	output wire        phy2_rx_tready,
	input  wire        phy2_rx_tvalid,
	input  wire [63:0] phy2_rx_tdata,
	input  wire [ 7:0] phy2_rx_tkeep,
	input  wire        phy2_rx_tlast,
	input  wire        phy2_rx_tuser,

	/* PHY3 */
	input  wire        phy3_tx_tready,
	output wire        phy3_tx_tvalid,
	output wire [63:0] phy3_tx_tdata,
	output wire [ 7:0] phy3_tx_tkeep,
	output wire        phy3_tx_tlast,
	output wire        phy3_tx_tuser,
	output wire        phy3_rx_tready,
	input  wire        phy3_rx_tvalid,
	input  wire [63:0] phy3_rx_tdata,
	input  wire [ 7:0] phy3_rx_tkeep,
	input  wire        phy3_rx_tlast,
	input  wire        phy3_rx_tuser
);

dut dut0 (
	.clk156 (clk156),
	.rst (cold_reset),
	.*
);

endmodule

`default_nettype wire

