#ifndef __IHYPERNODEPAINTER_H__
#define __IHYPERNODEPAINTER_H__

#pragma once

#include "HyperGraphNode.h"
#include "DisplayList.h"
#include <GdiPlus.h>

struct IHyperNodePainter
{
	virtual void Paint( CHyperNode * pNode, CDisplayList * pList ) = 0;
};

#endif
