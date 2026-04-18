#ifndef __COMMENTNODE_H__
#define __COMMENTNODE_H__

#pragma once

#include "HyperGraphNode.h"

class CCommentNode : public CHyperNode
{
public:
	static const char* GetClassType() {
		return "_comment";
	}
	CCommentNode();
	void Init();
	void Done();
	CHyperNode * Clone();
};

#endif
