/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 07:11:2005   11:00 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _DECALOBJECT_H_
#define _DECALOBJECT_H_

#pragma once


#include "BaseObject.h"


class CEdMesh;
struct IDecalRenderNode;


class CDecalObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE( CDecalObject )

	// CBaseObject overrides
	virtual void Display( DisplayContext& dc );

	virtual bool HitTest( HitContext& hc );
	virtual void BeginEditParams( IEditor* ie, int flags );
	virtual void EndEditParams( IEditor* ie );

	virtual int MouseCreateCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags );
	virtual void GetLocalBounds( BBox& box );
	virtual void SetHidden( bool bHidden );
	virtual void UpdateVisibility( bool visible );
	virtual void SetMaterial( CMaterial* mtl );
	virtual void Serialize( CObjectArchive& ar );
	virtual XmlNodeRef Export( const CString& levelPath, XmlNodeRef& xmlNode );
	virtual void SetMinSpec( uint32 nSpec );
	virtual void SetMaterialLayersMask( uint32 nLayersMask );

	// special input handler (to be reused by this class as well as the decal object tool)
	void MouseCallbackImpl( CViewport* view, EMouseEvent event, CPoint& point, int flags, bool callerIsMouseCreateCallback = false );
	
	int GetProjectionType() const;
	void UpdateEngineNode();

protected:
	// CBaseObject overrides
	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual bool CreateGameObject();
	virtual void Done();
	virtual void DeleteThis() { delete this; };
	virtual void InvalidateTM( int nWhyFlags );

private:
	CDecalObject();	
	bool RayToLineDistance( const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt );
	void OnFileChange( CString filename );
	void OnParamChanged( IVariable* pVar );
	void OnViewDistRatioChanged( IVariable* pVar );
	CMaterial* GetDefaultMaterial() const;

private:
	int m_renderFlags;
	CVariable<int> m_viewDistRatio;
	CVariable<int> m_projectionType;
	CVariable<int> m_sortPrio;
	IDecalRenderNode* m_pRenderNode;
	IDecalRenderNode* m_pPrevRenderNode;
};


class CDecalObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {90eac52c-1b65-4db2-88cd-a3be6508b383}
		static const GUID guid = { 0x90eac52c, 0x1b65, 0x4db2, { 0x88, 0xcd, 0xa3, 0xbe, 0x65, 0x08, 0xb3, 0x83 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_DECAL; };
	const char* ClassName() { return "Decal"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS( CDecalObject ); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 150; };
	virtual const char* GetTextureIcon() { return "Editor/ObjectIcons/Decal.bmp"; };
};


#endif // _DECALOBJECT_H_