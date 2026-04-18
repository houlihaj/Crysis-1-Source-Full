////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   VoxelObject.h
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __voxelobject_h__
#define __voxelobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"

class CEdMesh;

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */



class CVoxelObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CVoxelObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Display( DisplayContext &dc );

	bool ResetTransformation();
	bool Split();
	void Retriangulate();
	void CopyHM();

	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );
	void SetSelected( bool bSelect );
	void UpdateVisibility( bool visible );
	void SetHidden( bool bHidden );
	void SetFrozen( bool bFrozen );
	virtual void SetMinSpec( uint32 nSpec );

	IPhysicalEntity* GetCollisionEntity() const;

	void Serialize( CObjectArchive &ar );

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	IRenderNode* GetEngineNode() const { return m_pRenderNode; };
	float GetLightmapQuality() const { return mv_lightmapQuality;	}
	void SetLightmapQuality( const f32 fLightmapQuality ) { mv_lightmapQuality = fLightmapQuality;	}

	//////////////////////////////////////////////////////////////////////////
	//float GetRatioLod() const { return mv_ratioLOD; };
	//float GetRatioViewDist() const { return mv_ratioViewDist; };

	//////////////////////////////////////////////////////////////////////////
	//bool IsRecieveLightmap() const { return mv_recvLightmap; };

	IRenderNode * GetRenderNode(){return m_pRenderNode;};

	//////////////////////////////////////////////////////////////////////////
	// Voxel parameters.
	//////////////////////////////////////////////////////////////////////////
/*	CVariable<float> m_width;
	CVariable<float> m_height;
	CVariable<float> m_length;
	CVariable<float> m_unitSize;*/


	CVariable<bool> mv_outdoor;
	CVariable<bool> mv_goodOccluder;
	CVariable<bool> mv_castShadowMaps;
	CVariable<bool> mv_castLightmap;
	CVariable<bool> mv_recvLightmap;
//	CVariable<bool> mv_recvShadowMaps;
	CVariable<bool> mv_hideable;
	CVariable<int> mv_ratioLOD;
	CVariable<int> mv_ratioViewDist;
	CVariable<bool> mv_excludeFromTriangulation;
	CVariable<float> mv_lightmapQuality;
	CVariable<bool> mv_linkToTerrain;
	CVariable<bool> mv_generateLODs;
	CVariable<bool> mv_computeAO;
	CVariable<bool> mv_snapToTerrain;
	CVariable<bool> mv_smartBaseColor;

protected:
	//! Dtor must be protected.
	CVoxelObject();

	//! Convert ray givven in world coordinates to the ray in local Voxel coordinates.
	void DeleteThis() { delete this; };
	void InvalidateTM( int nWhyFlags );


	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );
	void BeginEditMultiSelParams( bool bAllOfSameType );
	void EndEditMultiSelParams();

	void OnFileChange( CString filename );

	void UpdateEngineNode( bool bOnlyTransform=false );

	bool CreateGameObject();
	void Done();

	// size of *ppOut must be 2*size+1
	void ConvertDataToString(char** ppOut, void * pData, int size);
	// size of *ppOut must be size
	void StringToData(void ** ppOut, const char * pStr, int size);

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.

	void OnSizeChange(IVariable *pVar);
	void OnRenderVarChange( IVariable *var );
	//////////////////////////////////////////////////////////////////////////

private:

	BBox m_bbox;
	Matrix34 m_invertTM;

	int m_renderFlags;

	IRenderNode* m_pRenderNode;
	IRenderNode* m_pPrevRenderNode;


friend class CUndoVoxelObject;
};

/*!
 * Class Description of CVoxelObject.	
 */
class CVoxelObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {104a3049-7265-45bc-9b6a-4de17fb57a0c}
		static const GUID guid = { 0x104a3049, 0x7265, 0x45bc, { 0x9b, 0x6a, 0x4d, 0xe1, 0x7f, 0xb5, 0x7a, 0x0c } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_VOXEL; };
	const char* ClassName() { return "VoxelObject"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVoxelObject); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 150; };
};


//////////////////////////////////////////////////////////////////////////
//! Undo object.
class CUndoVoxelObject : public IUndoObject
{
public:
	CUndoVoxelObject( CVoxelObject * obj)
	{
		m_pObj = obj;
		IVoxelObject * pObject = (IVoxelObject*)(m_pObj->m_pRenderNode);
		if(pObject)
    {
      IMemoryBlock * pMB = pObject->GetCompiledData();
      m_pUndo = new CMemoryBlock;
      m_pUndo->Allocate(pMB->GetSize());
      m_pUndo->Copy(pMB->GetData(), pMB->GetSize());
      pMB->Release();
    }
		m_pRedo = 0;
	}

protected:
	virtual void Release() { delete this; };
	virtual int GetSize() {	return sizeof(*this) + (m_pUndo ? m_pUndo->GetSize() : 0) + (m_pRedo ? m_pRedo->GetSize() : 0); };
	virtual const char* GetDescription() { return "Voxel Object Modify"; };

	virtual void Undo( bool bUndo )
	{
		IVoxelObject * pObject = (IVoxelObject*)(m_pObj->m_pRenderNode);
		if(pObject)
		{
			if (bUndo) // Store for redo.
      {
        IMemoryBlock * pMB = pObject->GetCompiledData();
        m_pRedo = new CMemoryBlock;
        m_pRedo->Allocate(pMB->GetSize());
        m_pRedo->Copy(pMB->GetData(), pMB->GetSize());
        pMB->Release();
      }

			// Restore 
			pObject->SetCompiledData((uchar*)m_pUndo->GetBuffer(),m_pUndo->GetSize());
		}
	}
	virtual void Redo()
	{
		IVoxelObject * pObject = (IVoxelObject*)(m_pObj->m_pRenderNode);
		if(pObject)
		{
			if (m_pRedo) // Restore image.
				pObject->SetCompiledData((uchar*)m_pRedo->GetBuffer(),m_pRedo->GetSize());
		}
	}

private:
	//IVoxelObject * m_pObject;
	CVoxelObject * m_pObj;
	_smart_ptr<CMemoryBlock> m_pUndo;
	_smart_ptr<CMemoryBlock> m_pRedo;
};


#endif // __voxelobject_h__