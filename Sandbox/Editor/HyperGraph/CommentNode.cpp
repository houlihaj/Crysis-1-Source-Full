#include "stdafx.h"
#include "CommentNode.h"
#include "HyperNodePainter_Comment.h"

static CHyperNodePainter_Comment painter;

CCommentNode::CCommentNode()
{
	SetClass(GetClassType());
	m_pPainter = &painter;
	m_name = "This is a comment";
}

void CCommentNode::Init()
{
}

void CCommentNode::Done()
{
}

CHyperNode * CCommentNode::Clone()
{
	CCommentNode* pNode = new CCommentNode();
	pNode->CopyFrom(*this);
	return pNode;
}
