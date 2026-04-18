/*=============================================================================
	RLWaitProgress.h : 
	Copyright 2004 Crytek Studios. All Rights Reserved.

	Revision history:
	* Created by Nick Kasyan
=============================================================================*/

#ifndef __RLWAITPROGRESS_H__
#define __RLWAITPROGRESS_H__

#include "WaitProgress.h"

class CRLWaitProgress
{
	public:
		CRLWaitProgress()
		{
			m_pWait = new CWaitProgress("Starting the calculations...");
			m_nHiBound = 100;
			m_nLowBound = 0;
		}

		~CRLWaitProgress()
		{
			SAFE_DELETE(m_pWait);
			m_pWait = NULL;
		}

		void Caption(LPCTSTR lpszText)
		{
			m_pWait->SetText( lpszText );
		}

		void SetProgress(f32 fProgress)
		{
			assert(fProgress >= 0.0f && fProgress <= 1.0f); 

			int32 nProgress = m_nLowBound + fProgress * (m_nHiBound-m_nLowBound);
			nProgress = nProgress == 0 ? 1 : nProgress;
			m_pWait->Step( nProgress );
		}

		void ProgressSetBounds(int32 low, int32 hi)
		{
			assert (low <=hi);
			m_nHiBound = hi;
			m_nLowBound = low;
		}

	private:
		CWaitProgress* m_pWait;

		int32 m_nHiBound, m_nLowBound;
};

#endif