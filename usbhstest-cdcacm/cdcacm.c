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

/* Define this to nonzero, to have only one cdcacm interaface (usb serial port) active. */
#define SINGLE_CDCACM		1

enum
{
	/* WARNING: at this time, data IN endpoint sizes *must* equal the corresponding data OUT endpoint sizes. */
	CDCACM_INTERFACE_0_DATA_IN_ENDPOINT			= 0x81,
	CDCACM_INTERFACE_0_DATA_IN_ENDPOINT_SIZE		= 512,
	CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT			= 0x01,
	CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT_SIZE		= CDCACM_INTERFACE_0_DATA_IN_ENDPOINT_SIZE,
	CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT		= 0x82,
	CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT_SIZE	= 16,

	CDCACM_INTERFACE_1_DATA_IN_ENDPOINT			= 0x83,
	CDCACM_INTERFACE_1_DATA_IN_ENDPOINT_SIZE		= 512,
	CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT			= 0x03,
	CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT_SIZE		= CDCACM_INTERFACE_1_DATA_IN_ENDPOINT_SIZE,
	CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT		= 0x84,
	CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT_SIZE	= 16,

	MAX_USB_PACKET_SIZE					= 512,
	CDCACM_INTERFACE_COUNT					= 2,
};

static struct
{
	int		out_epnum;
	int		in_epnum;
	uint8_t		buf[MAX_USB_PACKET_SIZE];
	int		max_packet_size;
	int		len;
}
incoming_usb_data[CDCACM_INTERFACE_COUNT] =
{
	[0] =
	{
		.out_epnum		= CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT,
		.in_epnum		= CDCACM_INTERFACE_0_DATA_IN_ENDPOINT,
		.max_packet_size	= CDCACM_INTERFACE_0_DATA_IN_ENDPOINT_SIZE,
		.len			= 0,
	},
	[1] =
	{
		.out_epnum		= CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT,
		.in_epnum		= CDCACM_INTERFACE_1_DATA_IN_ENDPOINT,
		.max_packet_size	= CDCACM_INTERFACE_1_DATA_IN_ENDPOINT_SIZE,
		.len			= 0,
	},
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
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};


static const struct usb_iface_assoc_descriptor cdcacm_0_assoc = {
	.bLength = USB_DT_INTERFACE_ASSOCIATION_SIZE,
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface = 0,
	.bInterfaceCount = 2,
	.bFunctionClass = USB_CLASS_CDC,
	.bFunctionSubClass = USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol = USB_CDC_PROTOCOL_AT,
	.iFunction = 0,
};

static const struct usb_iface_assoc_descriptor cdcacm_1_assoc = {
	.bLength = USB_DT_INTERFACE_ASSOCIATION_SIZE,
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
	.bFirstInterface = 2,
	.bInterfaceCount = 2,
	.bFunctionClass = USB_CLASS_CDC,
	.bFunctionSubClass = USB_CDC_SUBCLASS_ACM,
	.bFunctionProtocol = USB_CDC_PROTOCOL_AT,
	.iFunction = 0,
};


/*
 * This notification endpoint isn't implemented. According to CDC spec it's
 * optional, but its absence causes a NULL pointer dereference in the
 * Linux cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp_cdcacm_0[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT_SIZE,
	.bInterval = 255,
} };

static const struct usb_endpoint_descriptor comm_endp_cdcacm_1[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT_SIZE,
	.bInterval = 255,
} };

static const struct usb_endpoint_descriptor data_endp_cdcacm_0[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_0_DATA_IN_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = CDCACM_INTERFACE_0_DATA_IN_ENDPOINT_SIZE,
	.bInterval = 1,
} };

static const struct usb_endpoint_descriptor data_endp_cdcacm_1[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = CDCACM_INTERFACE_1_DATA_IN_ENDPOINT,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = CDCACM_INTERFACE_1_DATA_IN_ENDPOINT_SIZE,
	.bInterval = 1,
} };

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 }
};

static const struct usb_interface_descriptor comm_iface_cdcacm_0[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp_cdcacm_0,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors)
} };

static const struct usb_interface_descriptor comm_iface_cdcacm_1[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp_cdcacm_1,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors)
} };

static const struct usb_interface_descriptor data_iface_cdcacm_0[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp_cdcacm_0,
} };

static const struct usb_interface_descriptor data_iface_cdcacm_1[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 3,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp_cdcacm_1,
} };

static const struct usb_interface ifaces[] = {
	{
		.num_altsetting = 1,
		.iface_assoc = &cdcacm_0_assoc,
		.altsetting = comm_iface_cdcacm_0,
	},
	{
		.num_altsetting = 1,
		.altsetting = data_iface_cdcacm_0,
	},
	{
		.num_altsetting = 1,
		.iface_assoc = &cdcacm_1_assoc,
		.altsetting = comm_iface_cdcacm_1,
	},
	{
		.num_altsetting = 1,
		.altsetting = data_iface_cdcacm_1,
	},
};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
#if SINGLE_CDCACM
	.bNumInterfaces = 2,
#else
	.bNumInterfaces = 4,
#endif
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

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev,
	struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
	void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	static uint8_t xbuf[7];
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
		case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
								 /*
								  * This Linux cdc_acm driver requires this to be implemented
								  * even though it's optional in the CDC spec, and we don't
								  * advertise it in the ACM functional descriptor.
								  */
								 return USBD_REQ_HANDLED;
							 }
		case 0x21:
							 /* get line coding */
							 if (* len != 7)
								 return USBD_REQ_NOTSUPP;
							 memcpy(*buf, xbuf, 7);
							 return USBD_REQ_HANDLED;
		case 0x20:
							 memcpy(xbuf, *buf, 7);

							 return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
int i;
	/* Locate endpoint data. */
	for (i = 0; i < CDCACM_INTERFACE_COUNT; i ++)
		if (incoming_usb_data[i].out_epnum == ep)
			break;
	if (i == CDCACM_INTERFACE_COUNT)
		/* Endpoint not found. */
		return;

	incoming_usb_data[i].len = usbd_ep_read_packet(usbd_dev, ep, incoming_usb_data[i].buf, sizeof incoming_usb_data[i].buf);
	if (incoming_usb_data[i].len)
		usbd_ep_nak_set(usbd_dev, ep, 1);
	avaiable_incoming_endpoints |= 1 << i;
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT, USB_ENDPOINT_ATTR_BULK, CDCACM_INTERFACE_0_DATA_OUT_ENDPOINT_SIZE, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_0_DATA_IN_ENDPOINT, USB_ENDPOINT_ATTR_BULK, CDCACM_INTERFACE_0_DATA_IN_ENDPOINT_SIZE, NULL);
	usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT, CDCACM_INTERFACE_0_NOTIFICATION_IN_ENDPOINT_SIZE, NULL);

	if (!SINGLE_CDCACM)
	{
		usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT, USB_ENDPOINT_ATTR_BULK, CDCACM_INTERFACE_1_DATA_OUT_ENDPOINT_SIZE, cdcacm_data_rx_cb);
		usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_1_DATA_IN_ENDPOINT, USB_ENDPOINT_ATTR_BULK, CDCACM_INTERFACE_1_DATA_IN_ENDPOINT_SIZE, NULL);
		usbd_ep_setup(usbd_dev, CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT, CDCACM_INTERFACE_1_NOTIFICATION_IN_ENDPOINT_SIZE, NULL);
	}

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}

usbd_device *usbd_dev;
volatile uint32_t busy_count;
int main(void)
{
	/* if this does not get incremented, it is possible that some usb interrupt flag is not handled,
	 * and the usb interrupt handler gets continuously invoked. */
	volatile int i;
	rcc_periph_clock_enable(RCC_APB2ENR_SYSCFGEN);
	rcc_clock_setup_hse(rcc_3v3 + RCC_CLOCK_3V3_216MHZ, 25);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOH);

	usbd_dev = usbd_init(&stm32f723_usb_driver, &dev, &config,
		usb_strings, 3,
			usbd_control_buffer, sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);

	nvic_enable_irq(NVIC_OTG_HS_IRQ);

	while (1) {
cm_disable_interrupts();
		if (avaiable_incoming_endpoints)
		{
cm_enable_interrupts();
			uint8_t buf[MAX_USB_PACKET_SIZE];
			int len;
			for (i = 0; i < CDCACM_INTERFACE_COUNT; i ++)
				if (avaiable_incoming_endpoints & (1 << i))
				{
					len = incoming_usb_data[i].len;
					memcpy(buf, incoming_usb_data[i].buf, len);
cm_disable_interrupts();
					avaiable_incoming_endpoints ^= 1 << i;
					usbd_ep_nak_set(usbd_dev, incoming_usb_data[i].out_epnum, 0);
cm_enable_interrupts();
					if (len)
						while (usbd_ep_write_packet(usbd_dev, incoming_usb_data[i].in_epnum, buf, len) == 0xffff)
							busy_count ++;
					if (len == incoming_usb_data[i].max_packet_size)
					{
						while (usbd_ep_write_packet(usbd_dev, incoming_usb_data[i].in_epnum, 0, 0) == 0xffff)
							busy_count ++;
					}
				}
			continue;
		}
		// The 'wfi' instruction interferes with debugging, so keep it disabled for the time being.
		//__asm__("wfi");
cm_enable_interrupts();
	}
}

void otg_hs_isr()
{
	usbd_poll(usbd_dev);
}

