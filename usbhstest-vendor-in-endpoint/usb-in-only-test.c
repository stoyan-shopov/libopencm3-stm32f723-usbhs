/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/cortex.h>

#include <string.h>

enum
{
	TRACE_ENDPOINT		= 0x81,
	TRACE_ENDPOINT_SIZE	= 512,
	MAX_USB_PACKET_SIZE	= 512,
};

static unsigned avaiable_incoming_endpoints;


static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0xef,
	.bDeviceSubClass = 2,
	.bDeviceProtocol = 1,
	/* The size of the control endpoint for usb high speed devices *must* be 64, as dictated by the usb standard. */
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5750,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor trace_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = TRACE_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = TRACE_ENDPOINT_SIZE,
	.bInterval = 0,
}};

const struct usb_interface_descriptor trace_iface = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = 0xFF,
	.bInterfaceSubClass = 0xFF,
	.bInterfaceProtocol = 0xFF,
	.iInterface = 1,

	.endpoint = trace_endp,
};

static const struct usb_iface_assoc_descriptor trace_assoc = {
	.bLength = USB_DT_INTERFACE_ASSOCIATION_SIZE,
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface = 0,
	.bInterfaceCount = 1,
	.bFunctionClass = 0xFF,
	.bFunctionSubClass = 0xFF,
	.bFunctionProtocol = 0xFF,
	.iFunction = 1,
};


static const struct usb_interface ifaces[] = {
	{
		.num_altsetting = 1,
		.iface_assoc = &trace_assoc,
		.altsetting = &trace_iface,		
	},
};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char * usb_strings[] = {
	"Black Sphere Technologies",
	"CDC-ACM Demo",
	"DEMO",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[256];
volatile bool configured = false;
static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, TRACE_ENDPOINT, USB_ENDPOINT_ATTR_BULK, TRACE_ENDPOINT_SIZE, NULL);
	configured = true;
}

usbd_device *usbd_dev;
uint8_t tracebuf[MAX_USB_PACKET_SIZE];
int main(void)
{
	int c = 0;
	rcc_periph_clock_enable(RCC_APB2ENR_SYSCFGEN);
	rcc_clock_setup_hse(rcc_3v3 + RCC_CLOCK_3V3_216MHZ, 25);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOH);

	usbd_dev = usbd_init(&stm32f723_usb_driver, &dev, &config,
		usb_strings, 3,
			usbd_control_buffer, sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);

	nvic_enable_irq(NVIC_OTG_HS_IRQ);

	while (!configured);
	while (1)
	{
		memset(tracebuf, c ++ + '0', TRACE_ENDPOINT_SIZE);
		if (c == 10)
			c = 0;
		while (usbd_ep_write_packet(usbd_dev, TRACE_ENDPOINT, tracebuf, TRACE_ENDPOINT_SIZE) == 0xffff)
			;
	}
}

void otg_hs_isr()
{
	usbd_poll(usbd_dev);
}

