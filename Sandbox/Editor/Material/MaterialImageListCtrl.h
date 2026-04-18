////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialImageListCtrl.h
//  Version:     v1.00
//  Created:     9/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MaterialImageListCtrl_h__
#define __MaterialImageListCtrl_h__
#pragma once

#include "Controls\ImageListCtrl.h"
#include "Controls\PreviewModelCtrl.h"
#include "Material.h"

//////////////////////////////////////////////////////////////////////////
class CMaterialImageListCtrl : public CImageListCtrl
{
public:
	typedef Functor1<CImageListCtrlItem*> SelectCallback;

	struct CMtlItem : public CImageListCtrlItem
	{
		_smart_ptr<CMaterial> pMaterial;
	};

	CMaterialImageListCtrl();
	~CMaterialImageListCtrl();

	CImageListCtrlItem* AddMaterial( CMaterial *pMaterial,void *pUserData=NULL );
	void SelectMaterial( CMaterial *pMaterial );
	CMtlItem* FindMaterialItem( CMaterial *pMaterial );
	void InvalidateMaterial( CMaterial *pMaterial );

	void SetSelectMaterialCallback( SelectCallback func ) { m_selectMtlFunc = func; };

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	virtual void OnSelectItem( CImageListCtrlItem *pItem,bool bSelected );
	virtual void OnUpdateItem( CImageListCtrlItem *pItem );

private:
	_smart_ptr<CMaterial> m_pMatPreview;

	CPreviewModelCtrl m_renderCtrl;
	IStatObj* m_pStatObj;
	SelectCallback m_selectMtlFunc;
	int m_nModel;
	int m_nColor;
};

#endif //__MaterialImageListCtrl_h__
