#ifndef _DISTANCECLOUDOBJECT_H_
#define _DISTANCECLOUDOBJECT_H_

#pragma once


#include "BaseObject.h"


struct IDistanceCloudRenderNode;


class CDistanceCloudObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE( CDistanceCloudObject )

	// CBaseObject overrides
	virtual void Display( DisplayContext& dc );

	virtual bool HitTest( HitContext& hc );
	virtual void BeginEditParams( IEditor* ie, int flags );
	virtual void EndEditParams( IEditor* ie );

	virtual void GetLocalBounds( BBox& box );
	virtual void SetHidden( bool bHidden );
	virtual void UpdateVisibility( bool visible );
	virtual void SetMaterial( CMaterial* mtl );
	virtual void Serialize( CObjectArchive& ar );
	virtual XmlNodeRef Export( const CString& levelPath, XmlNodeRef& xmlNode );

	void UpdateEngineNode();

protected:
	// CBaseObject overrides
	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual bool CreateGameObject();
	virtual void Done();
	virtual void DeleteThis() { delete this; };
	virtual void InvalidateTM( int nWhyFlags );

private:
	CDistanceCloudObject();	
	void OnProjectionTypeChange( IVariable* pVar );
	CMaterial* GetDefaultMaterial() const;

private:
	//CVariable< int > m_projectionType;
	IDistanceCloudRenderNode* m_pRenderNode;
	IDistanceCloudRenderNode* m_pPrevRenderNode;
	int m_renderFlags;
};


class CDistanceCloudObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {cd77e11b-6c86-4e9f-8a4b-0cb5b0eb5314}
		static const GUID guid = { 0xcd77e11b, 0x6c86, 0x4e9f, { 0x8a, 0x4b, 0x0c, 0xb5, 0xb0, 0xeb, 0x53, 0x14 } };

		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_DISTANCECLOUD; };
	const char* ClassName() { return "DistanceCloud"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS( CDistanceCloudObject ); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 200; };
	virtual const char* GetTextureIcon() { return "Editor/ObjectIcons/Clouds.bmp"; };
};


#endif // _DISTANCECLOUDOBJECT_H_