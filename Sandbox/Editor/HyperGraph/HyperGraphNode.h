////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraphNode.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: Implements IHyperNode interface.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __HyperGraphNode_h__
#define __HyperGraphNode_h__
#pragma once

#include "IHyperGraph.h"
#include "DisplayList.h"
#include <IFlowSystem.h>
#include <stack>

// forward declarations
class CHyperGraph;
class CHyperGraphView;
struct IHyperNodePainter;

enum EHyperNodeSubObjectId
{
	eSOID_InputTransparent = -1,

	eSOID_InputGripper = 1,
	eSOID_OutputGripper,
	eSOID_AttachedEntity,
	eSOID_Title,
	eSOID_Background,
	eSOID_Minimize,

	eSOID_FirstInputPort = 1000,
	eSOID_LastInputPort = 1999,
	eSOID_FirstOutputPort = 2000,
	eSOID_LastOutputPort = 2999,
	eSOID_ShiftFirstOutputPort = 3000,
	eSOID_ShiftLastOutputPort = 3999,
};

#define PORT_DEBUGGING_COLOR Gdiplus::Color(228, 202, 66)

//////////////////////////////////////////////////////////////////////////
// Description of hyper node port.
// Used for both input and output ports.
//////////////////////////////////////////////////////////////////////////
class CHyperNodePort
{
public:
	_smart_ptr<IVariable> pVar;

	// True if input/false if output.
	unsigned int bInput : 1;
	unsigned int bSelected : 1;
	unsigned int bVisible : 1;
	unsigned int bAllowMulti : 1;
	unsigned int nConnected;
	int nPortIndex;
	// Local rectangle in node space, where this port was drawn.
	Gdiplus::RectF rect;

	CHyperNodePort()
	{
		nPortIndex = 0;
		bSelected = false;
		bInput = false;
		bVisible = true;
		bAllowMulti = false;
		nConnected = 0;
	}
	CHyperNodePort( IVariable *_pVar,bool _bInput )
	{
		nPortIndex = 0;
		bInput = _bInput;
		pVar = _pVar;
		bSelected = false;
		bVisible = true;
		bAllowMulti = false;
		nConnected = 0;
	}
	CHyperNodePort( const CHyperNodePort &port )
	{
		bInput = port.bInput;
		bSelected = port.bSelected;
		bVisible = port.bVisible;
		nPortIndex = port.nPortIndex;
		nConnected = port.nConnected;
		bAllowMulti = port.bAllowMulti;
		rect = port.rect;
		pVar = port.pVar->Clone(true);
	}
	const char* GetName() { return pVar->GetName(); }
	const char* GetHumanName() { return pVar->GetHumanName(); }
};

enum EHyperNodeFlags
{
	EHYPER_NODE_ENTITY         = 0x0001,
	EHYPER_NODE_ENTITY_VALID   = 0x0002,
	EHYPER_NODE_GRAPH_ENTITY   = 0x0004,   // This node targets graph default entity.
	EHYPER_NODE_GRAPH_ENTITY2  = 0x0008,   // Second graph default entity.
	EHYPER_NODE_INSPECTED      = 0x0010,   // Node is being inspected [dynamic, visual-only hint]
	EHYPER_NODE_HIDE_UI        = 0x0100,   // Not visible in components tree.
	EHYPER_NODE_CUSTOM_COLOR1  = 0x0200,   // Custom Color1
};

//////////////////////////////////////////////////////////////////////////
// IHyperNode is an interface to the single hyper graph node.
//////////////////////////////////////////////////////////////////////////
class CHyperNode : public IHyperNode
{
public:
	typedef std::vector<CHyperNodePort> Ports;

	//////////////////////////////////////////////////////////////////////////
	// Node clone context.
	struct CloneContext
	{
	};
	//////////////////////////////////////////////////////////////////////////

	CHyperNode();
	virtual ~CHyperNode();

	//////////////////////////////////////////////////////////////////////////
	// IHyperNode
	//////////////////////////////////////////////////////////////////////////
	virtual IHyperGraph* GetGraph() const;
	virtual HyperNodeID GetId() const { return m_id; }
	virtual const char* GetName() const { return m_name; };
	virtual void SetName( const char *sName );
	//////////////////////////////////////////////////////////////////////////

	virtual const char* GetClassName() const { return m_classname; };
	virtual const char* GetDescription() { return "";	}
	virtual void Serialize( XmlNodeRef &node,bool bLoading,CObjectArchive* ar=0 );

	virtual IHyperNode* GetParent() { return 0; }
	virtual IHyperGraphEnumerator* GetChildrenEnumerator() { return 0; }

	virtual Gdiplus::Color GetCategoryColor() { return Gdiplus::Color(160,160,165); }
	virtual void DebugPortActivation(TFlowPortId port, const char *value) {}
	virtual bool IsPortActivationModified(const CHyperNodePort *port = NULL) { return false; }
	virtual void ClearDebugPortActivation() {}
	virtual CString GetDebugPortValue(const CHyperNodePort &pp) { return GetPortName(pp); }

	void SetPainter( IHyperNodePainter * pPainter ) 
	{ 
		Invalidate(true);
		m_pPainter = pPainter; 
	}

	void SetFlag( uint32 nFlag,bool bSet ) { if (bSet) m_nFlags |= nFlag; else m_nFlags &= ~nFlag; }
	bool CheckFlag( uint32 nFlag ) const { return ((m_nFlags&nFlag) == nFlag); }

	// Changes node class.
	void SetGraph( CHyperGraph *pGraph ) { m_pGraph = pGraph; }
	void SetClass( const CString &classname ) 
	{ 
		m_classname = classname;
	}
	void SetBitmap( CBitmap *pBitmap ) { m_pBitmap = pBitmap;	}

	Gdiplus::PointF GetPos() const { return Gdiplus::PointF( m_rect.X, m_rect.Y ); }
	void SetPos( Gdiplus::PointF pos ) 
	{ 
		m_rect = Gdiplus::RectF( pos.X, pos.Y, m_rect.Width, m_rect.Height );
		Invalidate(false);
	}

	// Initializes HyperGraph node for specific graph.
	virtual void Init() = 0;
	// Deinitializes HyperGraph node.
	virtual void Done() = 0;
	// Clone this hyper graph node.
	virtual CHyperNode* Clone() = 0;
	virtual void CopyFrom( const CHyperNode &src );

	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx ) {};

	//////////////////////////////////////////////////////////////////////////
	Ports* GetInputs() { return &m_inputs; };
	Ports* GetOutputs() { return &m_outputs; };
	//////////////////////////////////////////////////////////////////////////

	// Finds a port by name.
	CHyperNodePort* FindPort( const char *portname,bool bInput );
	
	// Adds a new port to the hyper graph node.
	// Only used by hypergraph manager.
	void AddPort( const CHyperNodePort &port );
	void ClearPorts();

	// Invalidate is called to notify UI that some parameters of graph node have been changed.
	void Invalidate(bool redraw);

	//////////////////////////////////////////////////////////////////////////
	// Drawing interface.
	//////////////////////////////////////////////////////////////////////////
	void Draw( CHyperGraphView *pView,Gdiplus::Graphics& gr, bool render );

	const Gdiplus::RectF& GetRect() const 
	{ 
		return m_rect; 
	}
	void SetRect( const Gdiplus::RectF& rc );

	// Calculates node's screen size.
	Gdiplus::SizeF CalculateSize( Gdiplus::Graphics &dc );

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetSelected( bool bSelected );
	bool IsSelected() const { return m_bSelected; };

	void SelectInputPort( int nPort,bool bSelected );
	void SelectOutputPort( int nPort,bool bSelected );

	void SetModified( bool bModified=true );

	// Called when any input ports values changed.
	virtual void OnInputsChanged() {};

	// Called when edge is unlinked from this node.
	virtual void Unlinked( bool bInput ) {};

	//////////////////////////////////////////////////////////////////////////
	// Context Menu processing.
	//////////////////////////////////////////////////////////////////////////
	void PopulateContextMenu( CMenu &menu );
	void OnContextMenuCommand( int nCmd );

	//////////////////////////////////////////////////////////////////////////
	virtual CVarBlock* GetInputsVarBlock();

	virtual CString GetTitle() const;
	virtual CString GetEntityTitle() const { return ""; };
	virtual CString GetPortName( const CHyperNodePort &port );

	virtual CString VarToValue( IVariable *var );


	bool GetAttachRect( int i, Gdiplus::RectF * pRect )
	{
		return m_dispList.GetAttachRect( i, pRect );
	}

	int GetObjectAt( Gdiplus::Graphics * pGr, Gdiplus::PointF point )
	{
		Gdiplus::Graphics * pOld = m_dispList.GetGraphics();
		m_dispList.SetGraphics(pGr);
		int obj = m_dispList.GetObjectAt( point - GetPos() );
		m_dispList.SetGraphics(pOld);
		return obj;
	}

	CHyperNodePort * GetPortAtPoint( Gdiplus::Graphics * pGr, Gdiplus::PointF point )
	{
		int obj = GetObjectAt( pGr, point );
		if (obj >= eSOID_FirstInputPort && obj <= eSOID_LastInputPort)
			return &m_inputs.at(obj - eSOID_FirstInputPort);
		else if (obj >= eSOID_FirstOutputPort && obj <= eSOID_LastOutputPort)
			return &m_outputs.at(obj - eSOID_FirstOutputPort);
		return 0;
	}
	void UnselectAllPorts();

	void RecordUndo();
	virtual IUndoObject* CreateUndo();

	IHyperGraphEnumerator* GetRelatedNodesEnumerator();

protected:
	void Redraw( Gdiplus::Graphics& gr );

protected:
	friend class CHyperGraph;
	friend class CHyperGraphManager;

	CHyperGraph *m_pGraph;
	HyperNodeID m_id;

	CString m_name;
	CString m_classname;

	Gdiplus::RectF m_rect;

	uint32 m_nFlags;

	unsigned int m_bSizeValid : 1;
	unsigned int m_bSelected : 1;

	// Input/Output ports.
	Ports m_inputs;
	Ports m_outputs;

	// Icon.
	CBitmap *m_pBitmap;

	IHyperNodePainter * m_pPainter;
	CDisplayList m_dispList;
};

template <typename It> class CHyperGraphIteratorEnumerator : public IHyperGraphEnumerator
{
public:
	typedef It Iterator;

	CHyperGraphIteratorEnumerator( Iterator begin, Iterator end )
		: m_begin(begin),
			m_end(end)
	{
	}
	virtual void Release() { delete this; };
	virtual IHyperNode* GetFirst()
	{
		if (m_begin != m_end)
			return *m_begin++;
		return 0;
	}
	virtual IHyperNode* GetNext()
	{
		if (m_begin != m_end)
			return *m_begin++;
		return 0;
	}

private:
	Iterator m_begin;
	Iterator m_end;
};


//////////////////////////////////////////////////////////////////////////
class CHyperNodeDescendentEnumerator : public IHyperGraphEnumerator
{
	CHyperNode *m_pNextNode;

	std::stack<IHyperGraphEnumerator*> m_stack;

public:
	CHyperNodeDescendentEnumerator( CHyperNode *pParent )
		: m_pNextNode(pParent)
	{
	}
	virtual void Release() { delete this; };
	virtual IHyperNode* GetFirst()
	{
		return Iterate();
	}
	virtual IHyperNode* GetNext()
	{
		return Iterate();
	}

private:
	IHyperNode* Iterate()
	{
		CHyperNode* pNode = m_pNextNode;
		if (!pNode)
			return 0;

		// Check whether the current node has any children.
		IHyperGraphEnumerator* children = m_pNextNode->GetChildrenEnumerator();
		CHyperNode* pChild = 0;
		if (children)
			pChild = static_cast<CHyperNode*>(children->GetFirst());
		if (!pChild && children)
			children->Release();

		// The current node has children.
		if (pChild)
		{
			m_pNextNode = pChild;
			m_stack.push(children);
		}
		else
		{
			// The current node has no children - go to the next node.
			CHyperNode* pNextNode = 0;
			while (!pNextNode && !m_stack.empty())
			{
				pNextNode = static_cast<CHyperNode*>(m_stack.top()->GetNext());
				if (!pNextNode)
				{
					m_stack.top()->Release();
					m_stack.pop();
				}
			}
			m_pNextNode = pNextNode;
		}

		return pNode;
	}
};

#endif // __HyperGraphNode_h__
