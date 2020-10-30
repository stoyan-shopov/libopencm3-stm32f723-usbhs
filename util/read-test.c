#include <stdio.h>

#include <libusb-1.0/libusb.h>

enum
{
	BMP_VID		= 0x0483,
	BMP_PID		= 0x5740,
	BMP_TRACE_INTERFACE_NUMBER	= 0,
	BMP_TRACE_IN_ENDPOINT		= 0x81,

	MAX_USB_PACKET_SIZE		= 512,

	BYTES_TO_READ_FOR_TEST		= 1000 * 1024 * 1024,

	DEBUG		= 0,
};

static void print_devs(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8]; 

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		printf("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

		r = libusb_get_port_numbers(dev, path, sizeof(path));
		if (r > 0) {
			printf(" path: %d", path[0]);
			for (j = 1; j < r; j++)
				printf(".%d", path[j]);
		}
		printf("\n");

		if (desc.idVendor == BMP_VID && desc.idProduct == BMP_PID) do
		{
			int e;
			libusb_device_handle * h;
			fprintf(stderr, "blackmagic probe found, attempting to open the trace interface...\n");
			if (e = libusb_open(dev, &h))
			{
				fprintf(stderr, "error opening device, error code %i\n", e);
				break;
			}
			if (libusb_claim_interface(h, BMP_TRACE_INTERFACE_NUMBER))
			{
				libusb_close(h);
				fprintf(stderr, "error claiming interface\n");
				break;
			}
			fprintf(stderr, "device opened successfully, attempting to read trace data...\n");
			unsigned char data[MAX_USB_PACKET_SIZE];
			int i;
			int total = 0;
			while (total < BYTES_TO_READ_FOR_TEST)
			{
			int transferred;
				e = libusb_bulk_transfer(h, BMP_TRACE_IN_ENDPOINT, data, sizeof data, & transferred, 0);
				if (e)
				{
					fprintf(stderr, "error reading trace data\n");
					break;
				}
				else
				{
					//fprintf(stderr, "trace data read successfully, received %i bytes: %s\n", transferred, (data[transferred] = 0, data));
					if (DEBUG)
					{
						fprintf(stderr, "trace data read successfully, received %i bytes\n", transferred);
						for (int i = 0; i < transferred; fprintf(stderr, "$%02x ", data[i ++]));
						fprintf(stderr, "\n");
					}
					total += transferred;
				}
			}
			libusb_close(h);
		}
		while(0);
	}
}

int main(void)
{
	libusb_device **devs;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		libusb_exit(NULL);
		return (int) cnt;
	}

	print_devs(devs);
	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
}
