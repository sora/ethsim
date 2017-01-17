module dut (
	input  clk156,
	input  rst,
	
	/* AXIS RX */
	output wire        s_axis_rx_tready,
	input  wire        s_axis_rx_tvalid,
	input  wire [63:0] s_axis_rx_tdata,
	input  wire [ 7:0] s_axis_rx_tkeep,
	input  wire        s_axis_rx_tlast,
	input  wire        s_axis_rx_tuser,
	
	/* AXIS TX */
	input  wire        m_axis_tx_tready,
	output wire        m_axis_tx_tvalid,
	output wire [63:0] m_axis_tx_tdata,
	output wire [ 7:0] m_axis_tx_tkeep,
	output wire        m_axis_tx_tlast,
	output wire        m_axis_tx_tuser
);


/*
 * Bridge
 */

always @(posedge clk156) begin
	if (rst) begin
		m_axis_tx_tvalid <= 0;
		m_axis_tx_tdata <= 0;
		m_axis_tx_tkeep <= 0;
		m_axis_tx_tlast <= 0;
		m_axis_tx_tuser <= 1'b0;
		s_axis_rx_tready <= 0;
	end else begin
		m_axis_tx_tvalid <= s_axis_rx_tvalid;
		m_axis_tx_tdata <= s_axis_rx_tdata;
		m_axis_tx_tkeep <= s_axis_rx_tkeep;
		m_axis_tx_tlast <= s_axis_rx_tlast;
		m_axis_tx_tuser <= 1'b0 & s_axis_rx_tuser;
		s_axis_rx_tready <= m_axis_tx_tready;
	end
end
endmodule 

