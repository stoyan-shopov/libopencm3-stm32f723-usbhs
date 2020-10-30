This sample project is just one source file - `usbhstest/cdcacm.c`. The main program
exposes a configurable number of cdcacm interfaces, and performs a simple loopback
on the selected interfaces.

The number of cdcacm interfaces is controlled by the
`SINGLE_CDCACM` preprocessor definition, in file `cdcacm.c`

If `SINGLE_CDCACM` is nonzero - two usb cdcacm interfaces are
exposed.

If `SINGLE_CDCACM` is zero - only one usb cdcacm interface is exposed.

The difference in usb throughput is significant between the two settings.

A simple Qt program for performing usb CDCACM loopback tests (only tested on
windows) is in this repository:

https://github.com/stoyan-shopov/cdctest
