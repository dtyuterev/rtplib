/* Loosely based on Evgeniy Khramtsov's original approach - erlrtp */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <gsm/gsm.h>
#include "erl_driver.h"

typedef struct {
	ErlDrvPort port;
	gsm state;
} codec_data;

enum {
	CMD_ENCODE = 1,
	CMD_DECODE = 2
};

#define FRAME_SIZE 160
#define GSM_SIZE 33

static ErlDrvData codec_drv_start(ErlDrvPort port, char *buff)
{
	codec_data* d = (codec_data*)driver_alloc(sizeof(codec_data));
	d->port = port;
	d->state = gsm_create();
	set_port_control_flags(port, PORT_CONTROL_FLAG_BINARY);
	return (ErlDrvData)d;
}

static void codec_drv_stop(ErlDrvData handle)
{
	codec_data *d = (codec_data *) handle;
	gsm_destroy(d->state);
	driver_free((char*)handle);
}

static int codec_drv_control(
		ErlDrvData handle,
		unsigned int command,
		char *buf, int len,
		char **rbuf, int rlen)
{
	codec_data* d = (codec_data*)handle;
	gsm_signal sample[FRAME_SIZE];

	int i;
	int ret = 0;
	ErlDrvBinary *out;
	*rbuf = NULL;

	switch(command) {
		case CMD_ENCODE:
			if (len != FRAME_SIZE * 2)
				break;
			for (i = 0; i < FRAME_SIZE; i++) {
				sample[i] = (((gsm_signal) buf[i * 2]) & 0xf8) | (((gsm_signal) buf[i * 2 + 1]) << 8);
			}
			out = driver_alloc_binary(GSM_SIZE);
			gsm_encode(d->state, sample, (gsm_byte *) out->orig_bytes);
			*rbuf = (char *)out;
			ret = GSM_SIZE;
			break;
		 case CMD_DECODE:
			if (len != GSM_SIZE)
				break;
			gsm_decode(d->state, (gsm_byte *) buf, sample);
			out = driver_alloc_binary(FRAME_SIZE * 2);
			for (i = 0; i < FRAME_SIZE; i++){
				out->orig_bytes[i * 2] = (char) (sample[i] & 0xff);
				out->orig_bytes[i * 2 + 1] = (char) (sample[i] >> 8);
			}
			*rbuf = (char *)out;
			ret = FRAME_SIZE * 2;
			break;
		 default:
			break;
	}
	return ret;
}

ErlDrvEntry codec_driver_entry = {
	NULL,			/* F_PTR init, N/A */
	codec_drv_start,	/* L_PTR start, called when port is opened */
	codec_drv_stop,		/* F_PTR stop, called when port is closed */
	NULL,			/* F_PTR output, called when erlang has sent */
	NULL,			/* F_PTR ready_input, called when input descriptor ready */
	NULL,			/* F_PTR ready_output, called when output descriptor ready */
	"gsm_codec_drv",	/* char *driver_name, the argument to open_port */
	NULL,			/* F_PTR finish, called when unloaded */
	NULL,			/* handle */
	codec_drv_control,	/* F_PTR control, port_command callback */
	NULL,			/* F_PTR timeout, reserved */
	NULL			/* F_PTR outputv, reserved */
};

DRIVER_INIT(codec_drv) /* must match name in driver_entry */
{
	return &codec_driver_entry;
}
