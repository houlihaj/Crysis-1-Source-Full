//---------------------------------------------------------------------------
// Copyright 2006 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTENGINEVIEW_H__
#define __ANIMEVENTENGINEVIEW_H__

#include "IAnimEventView.h"

class CModelViewportCE;

class AnimEventEngineView : public IAnimEventView
{
public:
	AnimEventEngineView(CModelViewportCE* pModelViewportCE);
	virtual ~AnimEventEngineView();

	virtual void ReferenceDoc(IAnimEventDoc* pDoc);
	virtual void Refresh(IAnimEventDocChangeDescription* pDescription);

private:
	IAnimEventDoc* pDoc;
	CModelViewportCE* pModelViewportCE;
};

#endif //__ANIMEVENTENGINEVIEW_H__
