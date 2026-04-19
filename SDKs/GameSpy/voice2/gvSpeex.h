/*
GameSpy Voice2 SDK
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 2004 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com
http://gamespy.net
*/

#ifndef _GV_SPEEX_H_
#define _GV_SPEEX_H_

#include "gvMain.h"

/*
quality: samplesPerFrame encodedFrameSize bitsPerSecond
0:             160               5             2000
1:             160               9             3600
2:             160              14             5600
3:             160              20             8000
4:             160              20             8000
5:             160              27            10800
6:             160              27            10800
7:             160              37            14800
8:             160              37            14800
9:             160              45            18000
10:            160              61            24400
*/

GVBool gviSpeexInitialize(int quality);
void gviSpeexCleanup(void);

int gviSpeexGetSamplesPerFrame(void);
int gviSpeexGetEncodedFrameSize(void);

GVBool gviSpeexNewDecoder(GVDecoderData * data);
void gviSpeexFreeDecoder(GVDecoderData data);

void gviSpeexEncode(GVByte * out, const GVSample * in);
void gviSpeexDecodeAdd(GVSample * out, const GVByte * in, GVDecoderData data);
void gviSpeexDecodeSet(GVSample * out, const GVByte * in, GVDecoderData data);

void gviSpeexResetEncoder(void);

#endif
