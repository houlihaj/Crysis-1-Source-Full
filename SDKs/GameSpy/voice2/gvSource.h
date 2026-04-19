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

#ifndef _GV_SOURCE_H_
#define _GV_SOURCE_H_

#include "gvMain.h"

typedef struct GVISource * GVISourceList;

GVISourceList gviNewSourceList(void);
void gviFreeSourceList(GVISourceList sourceList);
void gviClearSourceList(GVISourceList sourceList);

GVBool gviIsSourceTalking(GVISourceList sourceList, GVSource source);
int gviListTalkingSources(GVISourceList sourceList, GVSource sources[], int maxSources);

void gviAddPacketToSourceList(GVISourceList sourceList,
							  const GVByte * packet, int len, GVSource source, GVFrameStamp frameStamp,
							  GVFrameStamp currentPlayClock);

GVBool gviWriteSourcesToBuffer(GVISourceList sourceList, GVFrameStamp startTime,
                               GVSample * sampleBuffer, int numFrames);

#endif
