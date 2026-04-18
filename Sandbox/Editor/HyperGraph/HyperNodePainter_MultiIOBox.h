#ifndef __HYPERNODEPAINTER_MULTIIOBOX_H__
#define __HYPERNODEPAINTER_MULTIIOBOX_H__

#pragma once

#include "IHyperNodePainter.h"

class CHyperNodePainter_MultiIOBox : public IHyperNodePainter
{
public:
	virtual void Paint( CHyperNode * pNode, CDisplayList * pList );
};

class CHyperNodePainter_Image : public IHyperNodePainter
{
public:
	virtual void Paint( CHyperNode * pNode, CDisplayList * pList );
};

#endif
