# RTS/CTS silicon-reference configuration

This software-reference package declares exact package AF pads. A PCB
net, connector or Nucleo virtual COM route is not inferred from it.
Products supply the reviewed four-wire cross-connection, peer voltage
and response bound, and electrical startup/stop policy.

The authored normal input selects `irq-fifo-rtscts-bytes` with one
statically owned byte interface. Software models and ARM linking do not
establish a physical RTS/CTS or noise/latency qualification. Physical
validation remains not executed until the actual station runs it.
