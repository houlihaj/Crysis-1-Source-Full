////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CloudGroup.h
//  Version:     v1.00
//  Created:     01/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CloudGroup object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CloudGroup_h__
#define __CloudGroup_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Group.h"

class CCloudSprite;
struct ICloudRenderNode;

/*!
 *	CCloudGroup groups object together.
 *  Object can only be assigned to one group.
 *
 */
class CCloudGroup : public CGroup
{
public:
	DECLARE_DYNCREATE(CCloudGroup)

	CCloudGroup();
	virtual ~CCloudGroup();

	virtual bool CreateGameObject();
	void Display( DisplayContext &dc );
	void OnChildModified();

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	void InvalidateTM( int nWhyFlags );
	void UpdateVisibility( bool visible );

	void Serialize( CObjectArchive &ar );

	void UpdateRenderObject();
	XmlNodeRef GenerateXml();
	void Generate();
	void Export();
	void Import();
	bool ImportFS(const CString & name);
	bool ImportExportFolder(const CString & fileName);
	void AddSprite(CCloudSprite * pSprite);
	void ReleaseCloudSprites();
	void BrowseFile();
	const char * GetExportFileName();
	void UpdateSpritesBox();

protected:

	void OnParamChange(IVariable *pVar);
	void OnPreviewChange(IVariable *pVar);
	void FillBox(CBaseObject * pObj);

	CVariableArray varTextureArray;
	CVariable<int> mv_numRows;
	CVariable<int> mv_numCols;

	CVariableArray varCloudArray;
	CVariable<int> mv_spriteRow;
	CVariable<int> mv_numSprites;
	CVariable<float> mv_sizeSprites;
	CVariable<float> mv_cullDistance;
	CVariable<float> mv_randomSizeValue;
	CVariable<float> m_angleVariations;
	CVariable<bool> mv_spritePerBox;
	CVariable<float> mv_density;

	CVariableArray varVisArray;
	//CVariable<bool> mv_isRegeneratePos;
	CVariable<bool> mv_bShowSpheres;
	CVariable<bool> mv_bPreviewCloud;
	CVariable<bool> mv_bAutoUpdate;

	CRenderObject * pObj;
	SShaderItem SI;

	CString m_exportFile;

	CCloudSprite ** ppCloudSprites;
	int nNumCloudSprites;

	float m_boxTotArea;

	BBox m_SpritesBox;

	ICloudRenderNode *m_pCloudRenderNode;
};

/*!
 * Class Description of Group.
 */
class CCloudGroupClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {d397bd6a-07d1-4673-ba11-c1fa64bc2f21}
		static const GUID guid = { 0xd397bd6a, 0x07d1, 0x4673, { 0xba, 0x11, 0xc1, 0xfa, 0x64, 0xbc, 0x2f, 0x21 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_CLOUDGROUP; };
	const char* ClassName() { return "Cloud"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCloudGroup); };
};

#endif // __CloudGroup_h__