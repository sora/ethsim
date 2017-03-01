module dut (
	input  clk156,
	input  rst,
	
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


/*
 * Bridge
 */

always @(posedge clk156) begin
	if (rst) begin
		phy0_tx_tvalid <= 0;
		phy0_tx_tdata  <= 0;
		phy0_tx_tkeep  <= 0;
		phy0_tx_tlast  <= 0;
		phy0_tx_tuser  <= 0;
		phy0_rx_tready <= 0;

		phy1_tx_tvalid <= 0;
		phy1_tx_tdata  <= 0;
		phy1_tx_tkeep  <= 0;
		phy1_tx_tlast  <= 0;
		phy1_tx_tuser  <= 0;
		phy1_rx_tready <= 0;

		phy2_tx_tvalid <= 0;
		phy2_tx_tdata  <= 0;
		phy2_tx_tkeep  <= 0;
		phy2_tx_tlast  <= 0;
		phy2_tx_tuser  <= 0;
		phy2_rx_tready <= 0;

		phy3_tx_tvalid <= 0;
		phy3_tx_tdata  <= 0;
		phy3_tx_tkeep  <= 0;
		phy3_tx_tlast  <= 0;
		phy3_tx_tuser  <= 0;
		phy3_rx_tready <= 0;
	end else begin
		phy0_tx_tvalid <= phy0_rx_tvalid;
		phy0_tx_tdata  <= phy0_rx_tdata;
		phy0_tx_tkeep  <= phy0_rx_tkeep;
		phy0_tx_tlast  <= phy0_rx_tlast;
		phy0_tx_tuser  <= phy0_rx_tuser;
		phy0_rx_tready <= phy0_tx_tready;

		phy1_tx_tvalid <= phy1_rx_tvalid;
		phy1_tx_tdata  <= phy1_rx_tdata;
		phy1_tx_tkeep  <= phy1_rx_tkeep;
		phy1_tx_tlast  <= phy1_rx_tlast;
		phy1_tx_tuser  <= phy1_rx_tuser;
		phy1_rx_tready <= phy1_tx_tready;

		phy2_tx_tvalid <= phy2_rx_tvalid;
		phy2_tx_tdata  <= phy2_rx_tdata;
		phy2_tx_tkeep  <= phy2_rx_tkeep;
		phy2_tx_tlast  <= phy2_rx_tlast;
		phy2_tx_tuser  <= phy2_rx_tuser;
		phy2_rx_tready <= phy2_tx_tready;

		phy3_tx_tvalid <= phy3_rx_tvalid;
		phy3_tx_tdata  <= phy3_rx_tdata;
		phy3_tx_tkeep  <= phy3_rx_tkeep;
		phy3_tx_tlast  <= phy3_rx_tlast;
		phy3_tx_tuser  <= phy3_rx_tuser;
		phy3_rx_tready <= phy3_tx_tready;
	end
end
endmodule 

