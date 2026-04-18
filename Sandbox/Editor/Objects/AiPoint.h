////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AIPoint.h
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: AIPoint object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AIPoint_h__
#define __AIPoint_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"

enum EAIPointType
{
	EAIPOINT_WAYPOINT = 0,	//!< AI Graph node, waypoint.
	EAIPOINT_HIDE,			//!< Good hiding point.
	EAIPOINT_ENTRYEXIT,			//!< Entry/exit point to indoors.
	EAIPOINT_EXITONLY,			//!< Exit point from indoors.
	EAIPOINT_HIDESECONDARY,			//!< secondary hiding point (use for advancing, etc.).
};

enum EAINavigationType
{
	EAINAVIGATION_HUMAN = 0,  //!< Normal human-type waypoint navigation
	EAINAVIGATION_3DSURFACE,  //!< General 3D-surface navigation
};

struct IVisArea;

/*!
 *	CAIPoint is an object that represent named AI waypoint in the world.
 *
 */
class CAIPoint : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CAIPoint)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	void Display( DisplayContext &disp );
	//! Get object color.
	COLORREF GetColor() const;

	virtual CString GetTypeDescription() const;

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	bool HitTest( HitContext &hc );

	void GetLocalBounds( BBox &box );

	void OnEvent( ObjectEvent event );

	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	//! Invalidates cached transformation matrix.
	virtual void InvalidateTM( int nWhyFlags );

	virtual void SetHelperScale( float scale ) { m_helperScale = scale; };
	virtual float GetHelperScale() { return m_helperScale; };
	//////////////////////////////////////////////////////////////////////////

 	//! Retreive number of link points.
 	int GetLinkCount() const { return m_links.size(); }
 	CAIPoint*	GetLink( int index );
 	void AddLink( CAIPoint* obj,bool bIsPassable,bool bNeighbour=false );
 	void RemoveLink( CAIPoint* obj,bool bNeighbour=false );
 	bool IsLinkSelected( int iLink ) const { m_links[iLink].selected; };
 	void SelectLink( int iLink,bool bSelect ) { m_links[iLink].selected = bSelect; };

	/// Removes all of our links
	void RemoveAllLinks();

	void SetAIType( EAIPointType type );
	EAIPointType GetAIType() const { return m_aiType; };

  void SetAINavigationType( EAINavigationType type );
	EAINavigationType GetAINavigationType() const { return m_aiNavigationType; };

	const struct GraphNode* GetGraphNode() const {return m_aiNode;}

	void MakeRemovable( bool bRemovable );
	bool GetRemovable( ) const {return m_bRemovable;}

	// Enable picking of AI points.
	void StartPick();
	void StartPickImpass();

	// Prompts a regeneration of all links in the same nav region as this one
	void RegenLinks();

	//////////////////////////////////////////////////////////////////////////
	void Validate( CErrorReport *report );

protected:
	//! Dtor must be protected.
	CAIPoint();
  ~CAIPoint();
	void DeleteThis() { delete this; };

	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );
 	void ResolveLink( CBaseObject *pOtherLink );
 	void ResolveImpassableLink( CBaseObject *pOtherLink );
  void ResolveInternalAILink( CBaseObject *pOtherLink, uint userData );

  // Update links in AI graph.
	void UpdateLinks();
	float GetRadius();

 	//! Ids of linked waypoints.
 	struct Link
 	{
 		CAIPoint *object;
 		GUID id;
 		bool selected; // True if link is currently selected.
		bool passable;
 		Link() { object = 0; id = GUID_NULL; selected = false; passable = true;}
 	};
 	std::vector<Link> m_links;
  // on load serialisation we get the list of links that are internal to AI but
  // can only actually update later on when the ai node has been created etc - so store
  // them in this list
  std::vector< std::pair<CAIPoint *, float> > m_internalAILinks;

	EAIPointType m_aiType;
  EAINavigationType m_aiNavigationType;
  bool m_bRemovable;

	//! True if this waypoint is indoors.
	bool m_bIndoors;
	IVisArea *m_pArea;

	//////////////////////////////////////////////////////////////////////////
	bool m_bLinksValid;
	bool m_bIgnoreUpdateLinks;

	// AI graph node.
	int m_nodeIndex;
	struct GraphNode *m_aiNode;
	bool m_bIndoorEntrance;

	static int m_rollupId;
	static class CAIPointPanel* m_panel;
	static float m_helperScale;
};

/*!
 * Class Description of AIPoint.	
 */
class CAIPointClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {07303078-B211-40b9-9621-9910A0271AB7}
		static const GUID guid = { 0x7303078, 0xb211, 0x40b9, { 0x96, 0x21, 0x99, 0x10, 0xa0, 0x27, 0x1a, 0xb7 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_AIPOINT; };
	const char* ClassName() { return "AIPoint"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIPoint); };
	int GameCreationOrder() { return 110; };
};

#endif // __AIPoint_h__