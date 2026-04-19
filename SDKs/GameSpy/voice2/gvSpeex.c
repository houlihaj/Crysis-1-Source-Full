#include "gvSpeex.h"
#include <speex.h>

// this is hardcoded for narrowband (8khz) speex
#define GVI_SAMPLES_PER_FRAME  160

static GVBool gviSpeexInitialized;
static void * gviSpeexEncoderState;
static SpeexBits gviSpeexBits;
static int gviSpeexEncodedFrameSize;

GVBool gviSpeexInitialize(int quality)
{
	int rate;
	int bitsPerFrame;
	int samplesPerSecond;

	// we shouldn't already be initialized
	if(gviSpeexInitialized)
		return GVFalse;

	// create a new encoder state
	gviSpeexEncoderState = speex_encoder_init(&speex_nb_mode);
	if(!gviSpeexEncoderState)
		return GVFalse;

	// set the quality
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_SET_QUALITY, &quality);

	// set the sampling rate
	samplesPerSecond = GV_SAMPLES_PER_SECOND;
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_SET_SAMPLING_RATE, &samplesPerSecond);

	// initialize the bits struct
	speex_bits_init(&gviSpeexBits);

	// get the bitrate
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_GET_BITRATE, &rate);

	// convert to bits per frame
	bitsPerFrame = (rate / (GV_SAMPLES_PER_SECOND / GVI_SAMPLES_PER_FRAME));

	// convert to bytes per frame and store
	gviSpeexEncodedFrameSize = (bitsPerFrame / 8);

	// we're now initialized
	gviSpeexInitialized = GVTrue;

	return GVTrue;
}

void gviSpeexCleanup(void)
{
	// make sure there is something to cleanup
	if(!gviSpeexInitialized)
		return;

	// destroy the encoder state
	speex_encoder_destroy(gviSpeexEncoderState);
	gviSpeexEncoderState = NULL;

	// destroy the bits struct
	speex_bits_destroy(&gviSpeexBits);

	// no longer initialized
	gviSpeexInitialized = GVFalse;
}

int gviSpeexGetSamplesPerFrame(void)
{
	return GVI_SAMPLES_PER_FRAME;
}

int gviSpeexGetEncodedFrameSize(void)
{
	return gviSpeexEncodedFrameSize;
}

GVBool gviSpeexNewDecoder(GVDecoderData * data)
{
	void * decoder;
	int perceptualEnhancement = 1;

	// create a new decoder state
	decoder = speex_decoder_init(&speex_nb_mode);
	if(!decoder)
		return GVFalse;

	// turn on the perceptual enhancement
	speex_decoder_ctl(decoder, SPEEX_SET_ENH, &perceptualEnhancement);

	*data = decoder;
	return GVTrue;
}

void gviSpeexFreeDecoder(GVDecoderData data)
{
	// destory the decoder state
	speex_decoder_destroy((void *)data);
}

void gviSpeexEncode(GVByte * out, const GVSample * in)
{
	float input[GVI_SAMPLES_PER_FRAME];
	int bytesWritten;
	int i;

	// convert the input to floats for encoding
	for(i = 0 ; i < GVI_SAMPLES_PER_FRAME ; i++)
		input[i] = in[i];

	// flush the bits
	speex_bits_reset(&gviSpeexBits);

	// encode the frame
	speex_encode(gviSpeexEncoderState, input, &gviSpeexBits);

	// write the bits to the output
	bytesWritten = speex_bits_write(&gviSpeexBits, (char *)out, gviSpeexEncodedFrameSize);
	assert(bytesWritten == gviSpeexEncodedFrameSize);
}

void gviSpeexDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data)
{
	float output[GVI_SAMPLES_PER_FRAME];
	int rcode;
	int i;

	// read the data into the bits
	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	// decode it
	rcode = speex_decode((void *)data, &gviSpeexBits, output);
	assert(rcode == 0);

	// convert the output from floats
	for(i = 0 ; i < GVI_SAMPLES_PER_FRAME ; i++)
		out[i] += (GVSample)output[i];
}

void gviSpeexDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data)
{
	float output[GVI_SAMPLES_PER_FRAME];
	int rcode;
	int i;

	// read the data into the bits
	speex_bits_read_from(&gviSpeexBits, (char *)in, gviSpeexEncodedFrameSize);

	// decode it
	rcode = speex_decode((void *)data, &gviSpeexBits, output);
	assert(rcode == 0);

	// convert the output from floats
	for(i = 0 ; i < GVI_SAMPLES_PER_FRAME ; i++)
		out[i] = (GVSample)output[i];
}

void gviSpeexResetEncoder(void)
{
	// reset the encoder's state
	speex_encoder_ctl(gviSpeexEncoderState, SPEEX_RESET_STATE, NULL);
}
