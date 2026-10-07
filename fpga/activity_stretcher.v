// Optional activity stretcher for the Shrike-fi FPGA LED.
// The radio path does not enter the fabric. A rising edge on `activity`
// holds `led` high for STRETCH clocks so a short MCU strobe is visible.
//
// Clock this from the board's FPGA clock if one is routed; otherwise
// treat the file as the interface contract and regenerate in Go Configure.
// 1120 LUTs is far more than this needs. Do not grow it into a parser.

module activity_stretcher #(
    parameter STRETCH = 24'd6_000_000  // ~200 ms at 30 MHz, tune to the real clock
) (
    input  wire clk,
    input  wire rst_n,
    input  wire activity,
    output reg  led
);
    reg activity_d;
    reg [23:0] hold;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            activity_d <= 1'b0;
            hold <= 24'd0;
            led <= 1'b0;
        end else begin
            activity_d <= activity;
            if (activity && !activity_d) begin
                hold <= STRETCH;
                led <= 1'b1;
            end else if (hold != 24'd0) begin
                hold <= hold - 24'd1;
                led <= 1'b1;
            end else begin
                led <= 1'b0;
            end
        end
    end
endmodule
