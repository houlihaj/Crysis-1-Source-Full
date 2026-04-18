#ifndef __HYPERNODEPAINTER_DEFAULT_H__
#define __HYPERNODEPAINTER_DEFAULT_H__

#pragma once

#include "IHyperNodePainter.h"

class CHyperNodePainter_Default : public IHyperNodePainter
{
public:
	virtual void Paint( CHyperNode * pNode, CDisplayList * pList );
private:
	void AddDownArrow( CDisplayList * pList,const Gdiplus::PointF &p,Gdiplus::Pen *pPen );
};

#endif
