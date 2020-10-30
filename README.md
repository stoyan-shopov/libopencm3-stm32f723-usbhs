This is a set of sample projects that use the usb high speed support for the stm32f723
device in libopencm3.

To build the projects, after cloning, run:
```
git submodule init
git submodule update
cd libopencm3
make
```
After that, the individual projects are built by entering each project directory, and running `make`.

Here are some notes.

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

You can find notes about each project in each project's `README.md` file.
