////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   BaseObject.h
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Definition of basic Editor object.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BaseObject_h__
#define __BaseObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "IObjectManager.h"
#include "HitContext.h"
#include "ClassDesc.h"
#include "DisplayContext.h"
#include "ObjectLoader.h"

//////////////////////////////////////////////////////////////////////////
// forward declarations.
class CGroup;
class CUndoBaseObject;
class CObjectManager;
class CObjectLayer;
class CGizmo;
class CObjectArchive;
class CMaterial;
class CEdGeometry;
struct SSubObjSelectionModifyContext;
class ISubObjectSelectionReferenceFrameCalculator;

//////////////////////////////////////////////////////////////////////////
/*!
This class used for object references remapping dusring cloning operation.
*/
class CObjectCloneContext
{
public:
	//! Add cloned object.
	SANDBOX_API void AddClone( CBaseObject *pFromObject,CBaseObject *pToObject );

	//! Find cloned object for givven object.
	SANDBOX_API CBaseObject* FindClone( CBaseObject *pFromObject );

	// Find id of the cloned object.
	GUID ResolveClonedID( REFGUID guid );

private:
	typedef std::map<CBaseObject*,CBaseObject*> ObjectsMap;
	ObjectsMap m_objectsMap;
};

//////////////////////////////////////////////////////////////////////////
enum ObjectFlags
{
	OBJFLAG_SELECTED	= 0x0001,	//!< Object is selected. (Do not set this flag explicitly).
	OBJFLAG_HIDDEN		= 0x0002,	//!< Object is hidden.
	OBJFLAG_FROZEN		= 0x0004,	//!< Object is frozen (Visible but cannot be selected)
	OBJFLAG_FLATTEN		= 0x0008,	//!< Flatten area arround object.
	OBJFLAG_SHARED		= 0x0010,	//!< This object is shared between missions.

	OBJFLAG_PREFAB		= 0x0020,	//!< This object is part of prefab object.
	OBJFLAG_KEEP_HEIGHT	= 0x0040,	//!< This object should try to preserve height when snapping to flat objects.

	// object is in editing mode.
	OBJFLAG_EDITING		= 0x01000,
	OBJFLAG_ATTACHING	= 0x02000,	//!< Object in attaching to group mode.
	OBJFLAG_DELETED		= 0x04000,	//!< This object is deleted.
	OBJFLAG_HIGHLIGHT	= 0x08000,	//!< Object is highlighted (When mouse over).
	OBJFLAG_INVISIBLE		= 0x10000,	//!< This object is invisible.
	OBJFLAG_SUBOBJ_EDITING = 0x20000,	//!< This object is in the sub object editing mode.

	OBJFLAG_SHOW_ICONONTOP = 0x100000,	//!< Icon will be drawn on top of the object.

	OBJFLAG_PERSISTMASK = OBJFLAG_HIDDEN|OBJFLAG_FROZEN|OBJFLAG_FLATTEN,
};

//////////////////////////////////////////////////////////////////////////
//! This flags passed to CBaseObject::BeginEditParams method.
enum ObjectEditFlags
{
	OBJECT_CREATE	= 0x001,
	OBJECT_EDIT		= 0x002,
};

//////////////////////////////////////////////////////////////////////////
//! Return values from CBaseObject::MouseCreateCallback method.
enum MouseCreateResult
{
	MOUSECREATE_CONTINUE = 0,	//!< Continue placing this object.
	MOUSECREATE_ABORT,				//!< Abort creation of this object.
	MOUSECREATE_OK,						//!< Accept this object.
};

//////////////////////////////////////////////////////////////////////////
// Interface to the object create with the mouse callback.
//////////////////////////////////////////////////////////////////////////
struct IMouseCreateCallback
{
	virtual	void Release() = 0;
	virtual	MouseCreateResult OnMouseEvent( CViewport *view,EMouseEvent event,CPoint &point,int flags ) = 0;
	// Called after accepting an object to see if new object creation mode should be continued.
	virtual	bool ContinueCreation() = 0;
};

// Flags used as CBaseObject::InvalidateTM why flags.
enum EXFormWhyFlags
{
	TM_USER_INPUT  = 0x0001,
	TM_POS_CHANGED = 0x0002,
	TM_ROT_CHANGED = 0x0004,
	TM_SCL_CHANGED = 0x0008,
	TM_NOT_INVALIDATE = 0x0100, // Do not cause InvalidateTM call.
	TM_PARENT_CHANGED = 0x0200, // When parent transformation change.
	TM_UNDO           = 0x0400, // When doing undo operation.
	TM_RESTORE_UNDO   = 0x0800, // When doing RestoreUndo operation (This is different from normal undo).
	TM_ANIMATION      = 0x1000, // When doing animation.
};

#define OBJECT_TEXTURE_ICON_SIZEX 32
#define OBJECT_TEXTURE_ICON_SIZEY 32


/*!
 *	CBaseObject is the base class for all objects which can be placed in map.
 *	Every object belongs to class specified by ClassDesc.
 *	Specific object classes must ovveride this class, to provide specific functionality.
 *	Objects are reference counted and only destroyed when last reference to object
 *	is destroyed.
 *
 */
class CBaseObject : public CVarObject
{
	DECLARE_DYNAMIC(CBaseObject);
public:
	//! Events sent by object to listeners in EventCallback.
	enum EObjectListenerEvent
	{
		ON_DELETE = 0,// Sent after object was deleted from object manager.
		ON_ADD,       // Sent after object was added to object manager.
		ON_SELECT,		// Sent when objects becomes selected.
		ON_UNSELECT,	// Sent when objects unselected.
		ON_TRANSFORM, // Sent when object transformed.
		ON_VISIBILITY, // Sent when object visibility changes.
		ON_RENAME,		// Sent when object changes name.
    ON_CHILDATTACHED, // Sent when object gets a child attached.
	};

	//! This callback will be called if object is deleted.
	typedef Functor2<CBaseObject*,int> EventCallback;

	//! Childs structure.
	typedef std::vector<TSmartPtr<CBaseObject> > Childs;

	//! Retrieve class description of this object.
	CObjectClassDesc*	GetClassDesc() const { return m_classDesc; };

	/** Check if both object are of same class.
	*/
	virtual bool IsSameClass( CBaseObject *obj );

	ObjectType GetType() const { return m_classDesc->GetObjectType(); };
	const char* GetTypeName() const { return m_classDesc->ClassName(); };
	virtual CString GetTypeDescription() const { return m_classDesc->ClassName(); };

	//////////////////////////////////////////////////////////////////////////
	// Layer support.
	//////////////////////////////////////////////////////////////////////////
	void SetLayer( CObjectLayer *layer );
	CObjectLayer* GetLayer() const { return m_layer; };

	//////////////////////////////////////////////////////////////////////////
	// Flags.
	//////////////////////////////////////////////////////////////////////////
	void SetFlags( int flags ) { m_flags |= flags; };
	void ClearFlags( int flags ) { m_flags &= ~flags; };
	bool CheckFlags( int flags ) const { return (m_flags&flags) != 0; };

	//! Returns true if object hidden.
	bool IsHidden() const;
	//! Returns true if object frozen.
	bool IsFrozen() const;
	//! Returns true if object is selected.
	bool IsSelected() const { return CheckFlags(OBJFLAG_SELECTED); }
	//! Returns true if object is shared between missions.
	bool IsShared()	const { return CheckFlags(OBJFLAG_SHARED); }

	// Check if object potentially can be selected.
	bool IsSelectable() const;

	// Return texture icon.
	bool HaveTextureIcon() const { return m_nTextureIcon != 0; };
	void SetTextureIcon( int nTexIcon ) { m_nTextureIcon = nTexIcon; }

	//! Set shared between missions flag.
	virtual void SetShared( bool bShared );
	//! Set object hidden status.
	virtual void SetHidden( bool bHidden );
	//! Set object frozen status.
	virtual void SetFrozen( bool bFrozen );
	//! Set object selected status.
	virtual void SetSelected( bool bSelect );

	//! Set object highlighted (Note: not selected)
	void SetHighlight( bool bHighlight );
	//! Check if object is highlighted.
	bool IsHighlighted() const { return CheckFlags(OBJFLAG_HIGHLIGHT); }

	//////////////////////////////////////////////////////////////////////////
	// Object Id.
	//////////////////////////////////////////////////////////////////////////
	//! Get uniq object id.
	//! Every object will have it own uniq id assigned.
	REFGUID GetId() const { return m_guid; };

	//////////////////////////////////////////////////////////////////////////
	// Name.
	//////////////////////////////////////////////////////////////////////////
	//! Get name of object.
	const CString& GetName() const;

	//! Change name of object.
	virtual void SetName( const CString &name );
	//! Set object name and make sure it is uniq.
	void SetUniqName( const CString &name );	

	//////////////////////////////////////////////////////////////////////////
	// Geometry.
	//////////////////////////////////////////////////////////////////////////
	//! Set object position.
	bool SetPos( const Vec3 &pos,int nWhyFlags=0 );
	//! Set object rotation angles.
	bool SetRotation( const Quat& rotate,int nWhyFlags=0 );
	//! Set object scale.
	bool SetScale( const Vec3 &scale,int nWhyFlags=0 );

	//! Get object position.
	const Vec3& GetPos() const { return m_pos; }
	//! Get object local rotation quaternion.
	const Quat& GetRotation() const { return m_rotate; }
	//! Get object scale.
	const Vec3& GetScale() const { return m_scale; }

	//! Set flatten area.
	void SetArea( float area );
	float GetArea() const { return m_flattenArea; };

	//! Assign display color to the object.
	virtual void	SetColor( COLORREF color );
	//! Get object color.
	COLORREF GetColor() const { return m_color; };

	//////////////////////////////////////////////////////////////////////////
	// CHILDS
	//////////////////////////////////////////////////////////////////////////

	//! Return true if node have childs.
	bool HaveChilds() const { return !m_childs.empty(); };
	//! Return true if have attached childs.
	int GetChildCount() const { return m_childs.size(); };

	//! Get child by index.
	CBaseObject* GetChild( int i ) const;
	//! Return parent node if exist.
	CBaseObject* GetParent() const { return m_parent; };
	//! Scans hiearachy up to determine if we child of specified node.
	virtual bool IsChildOf( CBaseObject* node );
	
	//! Attach new child node.
	//! @param bKeepPos if true Child node will keep its world space position.
	virtual void AttachChild( CBaseObject* child,bool bKeepPos=true );
	//! Detach all childs of this node.
	virtual void DetachAll( bool bKeepPos=true );
	// Detach this node from parent.
	virtual void DetachThis( bool bKeepPos=true );

	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////
	//! Get objects's local transformation matrix.
	Matrix34 GetLocalTM() const { Matrix34 tm; CalcLocalTM(tm); return tm; };

	//! Get objects's world-space transformation matrix.
	const Matrix34& GetWorldTM() const;

	//! Get position in world space.
	Vec3 GetWorldPos() const { return GetWorldTM().GetTranslation(); };
	Ang3 GetWorldAngles() const;

	//! Set xform of object givven in world space.
	void SetWorldTM( const Matrix34 &tm, int nWhyFlags=0 );

	//! Set object xform.
	void SetLocalTM( const Matrix34 &tm, int nWhyFlags=0 );
	
	// Set object xform.
	void SetLocalTM( const Vec3 &pos,const Quat &rotate,const Vec3& scale, int nWhyFlags=0 );

	//////////////////////////////////////////////////////////////////////////
	// Interface to be implemented in plugins.
	//////////////////////////////////////////////////////////////////////////

	//! Called when object is being created (use GetMouseCreateCallback for more advanced mouse creation callback).
	virtual	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	// Return pointer to the callback object used when creating object by the mouse.
	// If this function return NULL MouseCreateCallback method will be used instead.
	virtual	IMouseCreateCallback* GetMouseCreateCallback() { return 0; };
	
	//! Draw object to specified viewport.
	virtual	void Display( DisplayContext &disp ) = 0;
	
	//! Perform intersection testing of this object.
	//! Return true if was hit.
	virtual bool HitTest( HitContext &hc ) { return false; };

	//! Perform intersection testing of this object with rectangle.
	//! Return true if was hit.
	virtual bool HitTestRect( HitContext &hc );

	//! Called when user starts editing object parameters.
	//! Flags is comnination of ObjectEditFlags flags.
	//! Prev is a previous created base object.
	virtual void BeginEditParams( IEditor *ie,int flags );
	//! Called when user ends editing object parameters.
	virtual void EndEditParams( IEditor *ie );

	//! Called when user starts editing of multiple selected objects.
	//! @param bAllOfSameType true if all objects in selection are of same type.
	virtual void BeginEditMultiSelParams( bool bAllOfSameType );
	//! Called when user ends editing of multiple selected objects.
	virtual void EndEditMultiSelParams();

	//! Get bounding box of object in world coordinate space.
	virtual void GetBoundBox( BBox &box );

	//! Get bounding box of object in local object space.
	virtual void GetLocalBounds( BBox &box );
	
	//! Called after some parameter been modified.
	virtual void SetModified();

	//! Called when UI of objects is updated.
	virtual void OnUIUpdate();

	//! Called when visibility of this object changes.
	//! Derived classs may ovverride this to respond to new visibility setting.
	virtual void UpdateVisibility( bool visible );

	//! Serialize object to/from xml.
	//! @param xmlNode XML node to load/save serialized data to.
	//! @param bLoading true if loading data from xml.
	//! @param bUndo true if loading or saving data for Undo/Redo purposes.
	virtual void Serialize( CObjectArchive &ar );

	//// Pre load called before serialize after all objects where completly loaded.
	//virtual void PreLoad( CObjectArchive &ar ) {};
	// Post load called after all objects where completly loaded.
	virtual void PostLoad( CObjectArchive &ar ) {};

	//! Export object to xml.
	//! Return created object node in xml.
	virtual XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	//! Handle events recieved by object.
	//! Override in derived classes, to handle specific events.
	virtual void OnEvent( ObjectEvent event );

	//////////////////////////////////////////////////////////////////////////
	// Group support.
	//////////////////////////////////////////////////////////////////////////
	//! Set group this object belong to.
	void	SetGroup( CGroup *group ) { m_group = group; };
	//! Get group this object belnog to.
	CGroup*	GetGroup() const { return m_group; };
	//! Check to see if this object part of any group.
	bool IsInGroup() const { return m_group != NULL; }

	//////////////////////////////////////////////////////////////////////////
	// LookAt Target.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetLookAt( CBaseObject *target );
	CBaseObject* GetLookAt() const { return m_lookat; };
	//! Returns true if this object is a look-at target.
	bool IsLookAtTarget() const;

	//! Get Animation node assigned to this object.
	virtual struct IAnimNode* GetAnimNode() const { return 0; };
	// Creates or destroy an animation node for the object.
	virtual void EnableAnimNode( bool bEnable ) {};

	//! Gets physics collision entity of this object.
	virtual struct IPhysicalEntity* GetCollisionEntity() const { return 0; };
	
	//////////////////////////////////////////////////////////////////////////
	//! Return IEditor interface.
	IEditor* GetIEditor() const { return ::GetIEditor(); }

	IObjectManager*	GetObjectManager() const;

	//! Store undo information for this object.
	void StoreUndo( const char *UndoDescription,bool minimal=false );

	//! Add event listener callback.
	void AddEventListener( const EventCallback &cb );
	//! Remove event listener callback.
	void RemoveEventListener( const EventCallback &cb );

	//////////////////////////////////////////////////////////////////////////
	//! Material handlaing for this base object.
	//! Override in derived classes.
	//////////////////////////////////////////////////////////////////////////
	//! Assign new material to this object.
	virtual void SetMaterial( CMaterial *mtl );
	//! Get assigned material for this object.
	virtual CMaterial* GetMaterial() const { return m_pMaterial; };
	// Get actual rendering material for this object.
	virtual CMaterial* GetRenderMaterial() const { return m_pMaterial; };

	//////////////////////////////////////////////////////////////////////////
	//! Analyze errors for this object.
	virtual void Validate( CErrorReport *report );

	//////////////////////////////////////////////////////////////////////////
	//! Gather resources of this object.
	virtual void GatherUsedResources( CUsedResources &resources );

	//////////////////////////////////////////////////////////////////////////
	//! Check if specified object is very similar to this one.
	virtual bool IsSimilarObject( CBaseObject *pObject );

	//////////////////////////////////////////////////////////////////////////
	// Material Layers Mask.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterialLayersMask( uint32 nLayersMask ) { m_nMaterialLayersMask = nLayersMask; }
	uint32 GetMaterialLayersMask() { return m_nMaterialLayersMask; };

	//////////////////////////////////////////////////////////////////////////
	// Object minimal usage spec (All/Low/Medium/High)
	//////////////////////////////////////////////////////////////////////////
	uint32 GetMinSpec() const { return m_nMinSpec; }
	virtual void SetMinSpec( uint32 nSpec ) { m_nMinSpec = nSpec; }

	//////////////////////////////////////////////////////////////////////////
	// SubObj selection.
	//////////////////////////////////////////////////////////////////////////
	// Return true if object support selecting of this sub object element type.
	virtual bool StartSubObjSelection( int elemType ) { return false; };
	virtual void EndSubObjectSelection() {};
	virtual void CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator ) { };
	virtual void ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx ) {};
	virtual void AcceptSubObjectModify() {};
	
	// Request a geometry pointer from the object.
	// Return NULL if geometry can not be retrieved or object does not support geometries.
	virtual CEdGeometry* GetGeometry() { return 0; };

	//! In This function variables of the object must be initialized.
	virtual void InitVariables() {};

protected:
	friend CObjectManager;
	friend CObjectLayer;

	//! Ctor is protected to restrict direct usage.
	CBaseObject();
	//! Dtor is protected to restrict direct usage.
	virtual ~CBaseObject();

	//! Initialize Object.
	//! If previous object specified it must be of exactly same class as this object.
	//! All data is copied from previous object.
	//! Optional file parameter specify initial object or script for this object.
	virtual bool Init( IEditor *ie,CBaseObject *prev,const CString &file );


	//////////////////////////////////////////////////////////////////////////
	//! Must be called after cloning the object on clone of object.
	//! This will make sure object references are cloned correctly.
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );

	//! Must be implemented by derived class to create game related objects.
	virtual bool CreateGameObject() { return true; };

	/** Called when object is about to be deleted.
			All Game resources should be freed in this function.
	*/
	virtual void Done();

	/** Change current id of object.
	*/
	//virtual void SetId( uint objectId ) { m_id = objectId; };
	
	//! Call this to delete an object.
	virtual void DeleteThis() = 0;

	//! Called when object need to be converted from different object.
	virtual bool ConvertFromObject( CBaseObject *object );

	// Invalidates cached transformation matrix.
	// nWhyFlags - Flags that indicate the reason for matrix invalidation.
	virtual void InvalidateTM( int nWhyFlags );

	//! Called when local transformation matrix is calculated.
	void CalcLocalTM( Matrix34 &tm ) const;

	//! Update editable object UI params.
	void UpdateEditParams();

	//! Called when child position changed.
	virtual void OnChildModified() {};

	//! Called when GUID of object is changed for any reason.
	virtual void OnChangeGUID( REFGUID newGUID );

	//! Remove child from our childs list.
	virtual void RemoveChild( CBaseObject *node );
	
	//! Resolve parent from callback.
	void ResolveParent( CBaseObject *object );

	//! Draw default object items.
	virtual void DrawDefault( DisplayContext &dc,COLORREF labelColor=RGB(255,255,255) );
	//! Draw object label.
	void	DrawLabel( DisplayContext &dc,const Vec3 &pos,COLORREF labelColor=RGB(255,255,255) );
	//! Draw 3D Axis at object position.
	void	DrawAxis( DisplayContext &dc,const Vec3 &pos,float size );
	//! Draw area arround object.
	void	DrawArea( DisplayContext &dc );

	//! Draw highlight.
	virtual void DrawHighlight( DisplayContext &dc );

	// Do hit testing on specified bounding box.
	// Function can be used by derived classes.
	bool HitTestRectBounds( HitContext &hc,const AABB &box );

	CBaseObject* FindObject( REFGUID id ) const;

	// Returns true if game objects should be created.
	bool IsCreateGameObjects() const;

	// Helper gizmo funcitons.
	void AddGizmo( CGizmo *gizmo );
	void RemoveGizmo( CGizmo *gizmo );

	//! Notify all listeners about event.
	void NotifyListeners( EObjectListenerEvent event );

	//! Only used by ObjectManager.
	bool IsPotentiallyVisible() const;

	// Check against min spec.
	bool IsHiddenBySpec() const;

	//////////////////////////////////////////////////////////////////////////
	// Material Layers Mask.
	//////////////////////////////////////////////////////////////////////////
	// Call this function at constructor of derived object to enable material layer mask ui for object.
	void UseMaterialLayersMask( bool bUse=true ) { m_bUseMaterialLayersMask = bUse; }

	//////////////////////////////////////////////////////////////////////////
	// May be overrided in derived classes to handle helpers scaling.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetHelperScale( float scale ) {};
	virtual float GetHelperScale() { return 1; };

	// Update UI variables.
	void UpdateUIVars();

	//////////////////////////////////////////////////////////////////////////
	// Insert a new CDialog based page into the roll up bar
	virtual int AddUIPage( const char* sCaption, CDialog *pDialog,bool bAutoDestroy=true,int iIndex=-1,bool bAutoExpand=true );
	// Remove a dialog page from the roll up bar
	virtual void RemoveUIPage(int iIndex);
	//////////////////////////////////////////////////////////////////////////

private:
	friend class CUndoBaseObject;
	friend class CPropertiesPanel;
	friend class CObjectArchive;
	friend class CSelectionGroup;
	
	//! Modify object properties, from modified property template.
	//! @templ Template to take parameters from.
	//! @modifiedNode XML Node that was modified in properties.
	void ModifyProperties( XmlNodeRef &templ,const XmlNodeRef &modifiedNode );

	//! Set class description for this object,
	//! Only called once after creation by ObjectManager.
	void SetClassDesc( CObjectClassDesc *classDesc );

	// From CObject, (not implemented)
	virtual void Serialize(CArchive& ar) {};

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE FIELDS
	//////////////////////////////////////////////////////////////////////////
private:
	//! ID Assigned to this object.
	//uint m_id;
	//! Unique object Id.
	GUID m_guid;

	//! Layer this object is assigned to.
	CObjectLayer *m_layer;

	//! Flags of this object.
	int m_flags;

	// Id of the texture icon for this object.
	int m_nTextureIcon;

	//! World space object's position.
	Vec3 m_pos;
	//! Object's Rotation angles.
	Quat m_rotate;
	//! Object's scale value.
	Vec3 m_scale;
	
	//! Display color.
	COLORREF m_color;

	//! World transformation matrix of this object.
	mutable Matrix34 m_worldTM;

	//////////////////////////////////////////////////////////////////////////
	//! Look At target entity.
	TSmartPtr<CBaseObject>	m_lookat;
	//! If we are lookat target. this is pointer to source.
	CBaseObject* m_lookatSource;

	//////////////////////////////////////////////////////////////////////////
	//! Area radius arround object, where terrain is flatten and static objects removed.
	float m_flattenArea;
	//! Every object keeps for itself height above terrain.
	float m_height;
	//! Object's name.
	CString m_name;
	//! Class description for this object.
	CObjectClassDesc*	m_classDesc;
	//! If object belongs to group, this will point to parent group.
	CGroup*	m_group;

	//! Number of reference to this object.
	//! When reference count reach zero, object will delete itself.
	int m_numRefs;
	//////////////////////////////////////////////////////////////////////////
	//! Child animation nodes.
	Childs m_childs;
	//! Pointer to parent node.
	mutable CBaseObject *m_parent;

	//! Material of this object.
	CMaterial* m_pMaterial;

	BBox m_worldBounds;

	//////////////////////////////////////////////////////////////////////////
	// Listeners.
	std::list<EventCallback> m_eventListeners;
	std::list<EventCallback>::iterator m_nextListener;

	//////////////////////////////////////////////////////////////////////////
	// Flags and bit masks.
	//////////////////////////////////////////////////////////////////////////
	mutable uint32 m_bMatrixInWorldSpace : 1;
	mutable uint32 m_bMatrixValid : 1;
	mutable uint32 m_bWorldBoxValid : 1;
	uint32 m_bUseMaterialLayersMask : 1;
	uint32 m_nMaterialLayersMask : 8;
	uint32 m_nMinSpec : 8;
};

//////////////////////////////////////////////////////////////////////////
typedef TSmartPtr<CBaseObject> CBaseObjectPtr;

#endif // __BaseObject_h__
