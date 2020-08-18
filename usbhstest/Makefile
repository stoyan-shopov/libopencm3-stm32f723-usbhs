PROJECT = usbhs
BUILD_DIR = bin

ARCH_FLAGS = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
CFLAGS = -DUSE_HAL_DRIVER -DSTM32F723xx
INCLUDES = -IInc -IDrivers/STM32F7xx_HAL_Driver/Inc -IDrivers/STM32F7xx_HAL_Driver/Inc/Legacy -IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc -IMiddlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -IDrivers/CMSIS/Device/ST/STM32F7xx/Include -IDrivers/CMSIS/Include -IDrivers/CMSIS/Include

CFILES += cdcacm.c

DEVICE=stm32f723iek6

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
