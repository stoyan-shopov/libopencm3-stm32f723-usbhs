This is a sample project that adds usb high speed support for the stm32f723
device in libopencm3.

To build this project, after cloning, run:
```
git submodule init
git submodule update
cd libopencm3
make
cd ../usbhstest/
make
```
If all is good, the compiled executable should be in file `usbhstest/usbhs.elf`

Here are some details on this project.

Note that usb high speed support for stm32f723 is already mostly present in
libopencm3. The stm32f723 device uses a so called 'DesignWare Core' usb IP 
from Synopsys, also used in other STM32 devices. The libopencm3 driver for
these `DesignWare Cores` (DWC) is in file `usb_dwc_common.c`. This particular
chip, however, employs an internal high speed usb phy controller, so it needs,
in the least, customized initialization. Also, while trying to make the stm32f723
work with libopencm3, I found some deficiencies, some of which I believe to be
genuine bugs in libopencm3 (I reported two of them on the libopencm3 github
page - issues #1241 and #1242). However, I am not competent enough on usb,
so it will take some time for me to prepare pull requests, so I prefer to place
the changes I made to libopencm3 into a custom branch.

The test project is just one source file - `usbhstest/cdcacm.c`. The main program
exposes a configurable number of cdcacm interfaces, and performs a simple loopback
on the selected interfaces.

The number of cdcacm interfaces is controlled by the
`SINGLE_CDCACM` preprocessor definition, in file `cdcacm.c`

If `SINGLE_CDCACM` is nonzero - two usb cdcacm interfaces are
exposed.

If `SINGLE_CDCACM` is zero - only one usb cdcacm interface is exposed.

The difference in usb throughput is significant between the two settings.
