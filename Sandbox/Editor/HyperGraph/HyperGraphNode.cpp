////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraphNode.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "HyperGraphNode.h"
#include "HyperGraph.h"
#include "HyperNodePainter_Default.h"

#include <XTToolkitPro.h>

#define NODE_BACKGROUND_COLOR  Gdiplus::Color(210,210,210)
#define NODE_BACKGROUND_SELECTED_COLOR  Gdiplus::Color(255,255,245)
#define NODE_OUTLINE_COLOR     Gdiplus::Color(190,190,190)

//#define TITLE_TEXT_COLOR       Gdiplus::Color(255,255,255)
//#define TITLE_TEXT_SELECTED_COLOR Gdiplus::Color(255,255,0)
#define TITLE_TEXT_COLOR       Gdiplus::Color(0,0,0)
#define TITLE_TEXT_SELECTED_COLOR Gdiplus::Color(0,0,0)

#define PORT_BACKGROUND_COLOR  Gdiplus::Color(200,200,200)
#define PORT_BACKGROUND_SELECTED_COLOR  Gdiplus::Color(200,200,255)
#define PORT_TEXT_COLOR        Gdiplus::Color(80,80,80)
#define PORT_OUTLINE_COLOR     Gdiplus::Color(190,190,190)

#define GRIPPER_COLOR          Gdiplus::Color(140,140,180)
#define GRIPPER_OUTLINE_COLOR  Gdiplus::Color(180,180,180)

#define BITMAP_BACKGROUND_COLOR  Gdiplus::Color(180,180,180)

// Sizing.
#define TITLE_X_OFFSET 4
#define TITLE_Y_OFFSET 0
#define TITLE_HEIGHT 16
#define GRIPPER_HEIGHT 10
#define PORTS_OUTER_MARGIN 12
#define PORTS_INNER_MARGIN 12
#define PORTS_Y_SPACING 14

static COLORREF PortTypeToColor[] =
{
	0x0000FF00,
	0x00FF0000,
	0x000000FF,
	0x00FFFFFF,
	0x00FF00FF,
	0x00FFFF00,
	0x0000FFFF,
	0x007F00FF,
	0x007FFF7F,
	0x00FF7F00,
	0x0000FF7F,
	0x007F7F7F,
	0x00000000
};

static const WCHAR * CvtStr( CString& str )
{
	static const size_t MAX_BUF = 1024;
	static WCHAR buffer[MAX_BUF];
	assert( str.GetLength() < MAX_BUF );
	int nLen = MultiByteToWideChar( CP_ACP, 0, str.GetBuffer(), -1, NULL, NULL );
	assert( nLen < MAX_BUF );
	nLen = MIN( nLen, MAX_BUF-1 );
	MultiByteToWideChar( CP_ACP, 0, str.GetBuffer(), -1, buffer, nLen );
	return buffer;
}

//////////////////////////////////////////////////////////////////////////
// Undo object for CHyperNode.
class CUndoHyperNode : public IUndoObject
{
public:
	CUndoHyperNode( CHyperNode *node )
	{
		// Stores the current state of this object.
		assert( node != 0 );
		m_node = node;
		m_redo = 0;
		m_undo = CreateXmlNode("Undo");
		m_node->Serialize( m_undo,false );
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return "HyperNodeUndo"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_redo = CreateXmlNode("Redo");
			m_node->Serialize( m_redo,false );
		}
		// Undo object state.
		m_node->Invalidate(true); // Invalidate previous position too.
		m_node->Serialize( m_undo,true );
		m_node->Invalidate(true);
	}
	virtual void Redo()
	{
		m_node->Invalidate(true); // Invalidate previous position too.
		m_node->Serialize( m_redo,true );
		m_node->Invalidate(true);
	}

private:
	_smart_ptr<CHyperNode> m_node;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

static CHyperNodePainter_Default painterDefault;

//////////////////////////////////////////////////////////////////////////
CHyperNode::CHyperNode()
{
	m_pBitmap = NULL;
	m_pGraph = NULL;
	m_bSelected = 0;
	m_bSizeValid = 0;
	m_nFlags = 0;
	m_id = 0;

	m_rect = Gdiplus::RectF( 0,0,0,0 );

	m_pPainter = &painterDefault;
}

//////////////////////////////////////////////////////////////////////////
CHyperNode::~CHyperNode()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::CopyFrom( const CHyperNode &src )
{
	m_id = src.m_id;
	m_name = src.m_name;
	m_classname = src.m_classname;
	m_rect = src.m_rect;
	m_pBitmap = src.m_pBitmap;
	m_nFlags = src.m_nFlags;

	m_inputs = src.m_inputs;
	m_outputs = src.m_outputs;
}

//////////////////////////////////////////////////////////////////////////
IHyperGraph* CHyperNode::GetGraph() const
{
	return m_pGraph;
};

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetName( const char *sName )
{
	m_name = sName;
	Invalidate(true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Invalidate( bool redraw )
{
	if (redraw)
		m_dispList.Clear();
	if (m_pGraph)
		m_pGraph->InvalidateNode( this );
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetRect( const Gdiplus::RectF& rc )
{
	if (rc.Equals(m_rect))
		return;

	RecordUndo();
	m_rect = rc;
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetSelected( bool bSelected )
{
	if (bSelected != m_bSelected)
	{
		RecordUndo();
		m_bSelected = bSelected;
		if (!bSelected)
		{
			// Clear selection of all ports.
			int i;
			for (i = 0; i < m_inputs.size(); i++)
				m_inputs[i].bSelected = false;
			for (i = 0; i < m_outputs.size(); i++)
				m_outputs[i].bSelected = false;
		}

		Invalidate(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Draw( CHyperGraphView *pView,Gdiplus::Graphics &gr, bool render )
{
	Gdiplus::Graphics * pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	if (m_dispList.Empty())
		Redraw(gr);
	if (render)
	{
		Gdiplus::Matrix mat;
		gr.GetTransform(&mat);
		Gdiplus::PointF pos = GetPos();
		gr.TranslateTransform( pos.X, pos.Y );
		m_dispList.Draw();
		gr.SetTransform(&mat);
	}
	m_dispList.SetGraphics(pOld);
}

void CHyperNode::Redraw( Gdiplus::Graphics & gr )
{
	m_dispList.Empty();
	Gdiplus::Graphics * pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	m_pPainter->Paint(this, &m_dispList);

	Gdiplus::RectF temp = m_rect;
	m_rect = m_dispList.GetBounds();
	m_rect.X = temp.X;
	m_rect.Y = temp.Y;

	m_dispList.SetGraphics(pOld);
}

Gdiplus::SizeF CHyperNode::CalculateSize( Gdiplus::Graphics &gr )
{
	Gdiplus::Graphics * pOld = m_dispList.GetGraphics();
	m_dispList.SetGraphics(&gr);
	if (m_dispList.Empty())
		Redraw(gr);
	m_dispList.SetGraphics(pOld);

	Gdiplus::SizeF size;
	m_rect.GetSize(&size);
	return size;
}

//////////////////////////////////////////////////////////////////////////
CString CHyperNode::GetTitle() const
{
	if (m_name.IsEmpty())
	{
		CString title = m_classname;
#if 0 // AlexL: 10/03/2006 show full group:class 
		int p = title.Find(':');
		if (p >= 0)
			title = title.Mid(p+1);
#endif
		return title;
	}
	return m_name + " (" + m_classname + ")";
};

CString CHyperNode::VarToValue( IVariable *var )
{
	CString value =	var->GetDisplayValue();

	// smart object helpers are stored in format class:name
	// but it's enough to display only the name
	if ( var->GetDataType() == IVariable::DT_SOHELPER || var->GetDataType() == IVariable::DT_SONAVHELPER )
		value.Delete( 0, value.Find(':') + 1 );

	return value;
}

//////////////////////////////////////////////////////////////////////////
CString CHyperNode::GetPortName( const CHyperNodePort &port )
{
	if (port.bInput)
	{
		CString text = port.pVar->GetHumanName();
		if (port.pVar->GetType() != IVariable::UNKNOWN && !port.nConnected)
		{
			text = text + "=" + VarToValue(port.pVar);
		}
		return text;
	}
	return port.pVar->GetHumanName();
}

//////////////////////////////////////////////////////////////////////////
CHyperNodePort* CHyperNode::FindPort( const char *portname,bool bInput )
{
	if (bInput)
	{
		for (int i = 0; i < m_inputs.size(); i++)
		{
			if (stricmp(m_inputs[i].GetName(),portname) == 0)
				return &m_inputs[i];
		}
	}
	else
	{
		for (int i = 0; i < m_outputs.size(); i++)
		{
			if (stricmp(m_outputs[i].GetName(),portname) == 0)
				return &m_outputs[i];
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::Serialize( XmlNodeRef &node,bool bLoading, CObjectArchive* ar )
{
	if (bLoading)
	{
		unsigned int nInputHideMask = 0;
		unsigned int nOutputHideMask = 0;
		int flags = 0;
		// Saving.
		Vec3 pos(0,0,0);
		node->getAttr( "Name",m_name );
		node->getAttr( "Class",m_classname );
		node->getAttr( "pos",pos );
		node->getAttr( "flags",flags );
		node->getAttr( "InHideMask",nInputHideMask );
		node->getAttr( "OutHideMask",nOutputHideMask );

		if (flags&1)
			m_bSelected = true;
		else
			m_bSelected = false;

		m_rect.X = pos.x;
		m_rect.Y = pos.y;
		m_rect.Width = m_rect.Height = 0;
		m_bSizeValid = false;

		{
			XmlNodeRef portsNode = node->findChild( "Inputs" );
			if (portsNode)
			{
				for (int i = 0; i < m_inputs.size(); i++)
				{
					IVariable *pVar = m_inputs[i].pVar;
					if (pVar->GetType() != IVariable::UNKNOWN)
						pVar->Serialize( portsNode,true );

					if ((nInputHideMask & (1<<i)) && m_inputs[i].nConnected == 0)
						m_inputs[i].bVisible = false;
					else
						m_inputs[i].bVisible = true;
				}
/*
				for (int i = 0; i < portsNode->getChildCount(); i++)
				{
					XmlNodeRef portNode = portsNode->getChild(i);
					CHyperNodePort *pPort = FindPort( portNode->getTag(),true );
					if (!pPort)
						continue;
					pPort->pVar->Set( portNode->getContent() );
				}
				*/
			}
		}
		// restore outputs visibility.
		{
			for (int i = 0; i < m_outputs.size(); i++)
			{
				if ((nOutputHideMask & (1<<i)) && m_outputs[i].nConnected == 0)
					m_outputs[i].bVisible = false;
				else
					m_outputs[i].bVisible = true;
			}
		}
	}
	else
	{
		// Saving.
		if (!m_name.IsEmpty())
			node->setAttr( "Name",m_name );
		node->setAttr( "Class",m_classname );

		Vec3 pos(m_rect.X,m_rect.Y,0);
		node->setAttr( "pos",pos );

		int flags = 0;
		if (m_bSelected)
			flags |= 1;
		node->setAttr( "flags",flags );

		unsigned int nInputHideMask = 0;
		unsigned int nOutputHideMask = 0;
		if (!m_inputs.empty())
		{
			XmlNodeRef portsNode = node->newChild( "Inputs" );
			for (int i = 0; i < m_inputs.size(); i++)
			{
				if (!m_inputs[i].bVisible)
					nInputHideMask |= (1 << i);
				IVariable *pVar = m_inputs[i].pVar;

				if (pVar->GetType() != IVariable::UNKNOWN)
					pVar->Serialize( portsNode,false );
			}
		}
		{
			// Calc output vis mask.
			for (int i = 0; i < m_outputs.size(); i++)
			{
				if (!m_outputs[i].bVisible)
					nOutputHideMask |= (1 << i);
			}
		}
		if (nInputHideMask != 0)
			node->setAttr( "InHideMask",nInputHideMask );
		if (nOutputHideMask != 0)
			node->setAttr( "OutHideMask",nOutputHideMask );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SetModified( bool bModified )
{
	m_pGraph->SetModified(bModified);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::ClearPorts()
{
	m_inputs.clear();
	m_outputs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::AddPort( const CHyperNodePort &port )
{
	if (port.bInput)
	{
		CHyperNodePort temp = port;
		temp.nPortIndex = m_inputs.size();
		m_inputs.push_back(temp);
	}
	else
	{
		CHyperNodePort temp = port;
		temp.nPortIndex = m_outputs.size();
		m_outputs.push_back(temp);
	}
	Invalidate(true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::PopulateContextMenu( CMenu &menu )
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::OnContextMenuCommand( int nCmd )
{
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CHyperNode::GetInputsVarBlock()
{
	CVarBlock *pVarBlock = new CVarBlock;
	for (int i = 0; i < m_inputs.size(); i++)
	{
		IVariable *pVar = m_inputs[i].pVar;
		if (pVar->GetType() != IVariable::UNKNOWN)
			pVarBlock->AddVariable( pVar );
	}
	return pVarBlock;
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::RecordUndo()
{
	if (CUndo::IsRecording())
	{
		IUndoObject* pUndo = CreateUndo();
		assert (pUndo != 0);
		CUndo::Record(pUndo);
	}
}

//////////////////////////////////////////////////////////////////////////
IUndoObject* CHyperNode::CreateUndo()
{
	return new CUndoHyperNode(this);
}

//////////////////////////////////////////////////////////////////////////
IHyperGraphEnumerator* CHyperNode::GetRelatedNodesEnumerator()
{
	// Traverse the parent hierarchy upwards to find the ultimate ancestor.
	CHyperNode* pNode = this;
	while (pNode->GetParent())
		pNode = static_cast<CHyperNode*>(pNode->GetParent());

	// Create an enumerator that returns all the descendants of this node.
	return new CHyperNodeDescendentEnumerator(pNode);
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SelectInputPort( int nPort,bool bSelected )
{
	if (nPort >= 0 && nPort < m_inputs.size())
	{
		if (m_inputs[nPort].bSelected != bSelected)
		{
			m_inputs[nPort].bSelected = bSelected;
			Invalidate(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::SelectOutputPort( int nPort,bool bSelected )
{
	if (nPort >= 0 && nPort < m_outputs.size())
	{
		if (m_outputs[nPort].bSelected != bSelected)
		{
			m_outputs[nPort].bSelected = bSelected;
			Invalidate(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperNode::UnselectAllPorts()
{
	bool bAnySelected = false;
	int i = 0;
	for (i = 0; i < m_inputs.size(); i++)
	{
		if (m_inputs[i].bSelected)
			bAnySelected = true;
		m_inputs[i].bSelected = false;
	}

	for (i = 0; i < m_outputs.size(); i++)
	{
		if (m_outputs[i].bSelected)
			bAnySelected = true;
		m_outputs[i].bSelected = false;
	}

	if (bAnySelected)
		Invalidate(true);
}
