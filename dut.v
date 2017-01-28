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

logic [3:0] count;
always @(posedge clk156) begin
	if (rst) begin
		count <= 0;
		m_axis_tx_tvalid <= s_axis_rx_tvalid & 0;
		m_axis_tx_tdata <= s_axis_rx_tdata & 0;
		m_axis_tx_tkeep <= s_axis_rx_tkeep & 0;
		m_axis_tx_tlast <= s_axis_rx_tlast & 0;
		m_axis_tx_tuser <= s_axis_rx_tuser & 0;
		s_axis_rx_tready <= m_axis_tx_tready & 0;
	end else begin
		m_axis_tx_tvalid <= 1'b1;
		m_axis_tx_tdata <= 64'h00_11_22_33_44_55_66_77;
		m_axis_tx_tkeep <= 8'hff;
		m_axis_tx_tuser <= 1'b0;

		count <= count + 1;
		if (count == 4) begin
			m_axis_tx_tlast <= 1;
			count <= 0;
		end else begin
			m_axis_tx_tlast <= 0;
		end
	end
end
endmodule 

