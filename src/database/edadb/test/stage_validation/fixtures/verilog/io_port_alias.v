module io_port_alias (in, out0, out1, out2);
  input in;
  output out0;
  output out1;
  output out2;
  wire in;
  wire out0;
  wire out1;
  wire out2;
  wire shared;

  assign shared = in;
  assign out0 = shared;
  assign out1 = shared;
  assign out2 = out1;
endmodule
