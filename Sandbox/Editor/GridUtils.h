////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   GridUtils.h
//  Version:     v1.00
//  Created:     12/2/2007 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __GRIDUTILS_H__
#define __GRIDUTILS_H__

namespace GridUtils
{
	template <typename F> inline void IterateGrid(F& f, const float minPixelsPerTick, float zoomX, float originX, float fps, int left, int right)
	{
		float pixelsPerSecond = zoomX;
		float pixelsPerFrame = pixelsPerSecond / fps;
		float framesPerTick = ceil(minPixelsPerTick / pixelsPerFrame);
		float scale = 1;
		bool foundScale = false;
		int numIters = 0;
		for (float OOM = 1; !foundScale; OOM *= 10)
		{
			float scales[] = {1, 2, 5};
			for (int scaleIndex = 0; !foundScale && scaleIndex < sizeof(scales) / sizeof(scales[0]); ++scaleIndex)
			{
				scale = scales[scaleIndex] * OOM;
				if (framesPerTick <= scale + 0.1f)
				{
					framesPerTick = scale;
					foundScale = true;
					if (numIters++ > 1000) break;
				}
			}
			if (numIters++ > 1000) break;
		}
		float pixelsPerTick = pixelsPerFrame * framesPerTick;
		float timeAtLeft = -left / zoomX + originX;
		float frameAtLeft = ceil(timeAtLeft * fps / framesPerTick) * framesPerTick;
		float firstTick = floor((frameAtLeft / fps - originX) * zoomX + 0.5f) + left;
		int frame = int(frameAtLeft);
		int lockupPreventionCounter = 10000;
		for (float tickX = firstTick; tickX < right && lockupPreventionCounter >= 0; tickX += pixelsPerTick, --lockupPreventionCounter)
		{
			f(frame, tickX);

			frame += int(framesPerTick);
		}
	}
}

#endif //__GRIDUTILS_H__
