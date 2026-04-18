#ifndef __HYPERNODEPAINTER_COMMENT_H__
#define __HYPERNODEPAINTER_COMMENT_H__

#pragma once

#include "IHyperNodePainter.h"

class CHyperNodePainter_Comment : public IHyperNodePainter
{
public:
	virtual void Paint( CHyperNode * pNode, CDisplayList * pList );
};

#endif
