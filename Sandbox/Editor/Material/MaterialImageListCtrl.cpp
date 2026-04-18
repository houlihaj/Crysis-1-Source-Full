////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialImageListCtrl.cpp
//  Version:     v1.00
//  Created:     9/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialImageListCtrl.h"
#include "MaterialManager.h"

BEGIN_MESSAGE_MAP(CMaterialImageListCtrl,CImageListCtrl)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
int CMaterialImageListCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int nRes = __super::OnCreate(lpCreateStruct);

	CRect rc(0,0,m_itemSize.cx,m_itemSize.cy);
	m_renderCtrl.Create( this,rc,WS_CHILD );
	m_renderCtrl.SetGrid(false);
	m_renderCtrl.SetAxis(false);
	m_renderCtrl.LoadFile( "Editor/Objects/Sphere.cgf" );
	m_renderCtrl.SetCameraLookAt( 2.2f,Vec3(0,1.0f,0) );
	m_renderCtrl.SetCameraRadius( 2.2f );
	m_renderCtrl.SetClearColor( ColorF(0,0,0) );
	m_nModel = 1;
	m_nColor = 0;
	return nRes;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::OnSize(UINT nType, int cx, int cy) 
{
	m_itemSize = CSize(cy-8,cy-8);
	InvalidateAllBitmaps();

	__super::OnSize(nType, cx, cy);
}

//////////////////////////////////////////////////////////////////////////
CMaterialImageListCtrl::CMaterialImageListCtrl()
{
	//m_pStatObj = gEnv->p3DEngine->LoadStatObj( "Editor/Objects/Sphere.cgf" );
	m_bkgrBrush.DeleteObject();
	m_bkgrBrush.CreateSysColorBrush( COLOR_3DFACE );
}

//////////////////////////////////////////////////////////////////////////
CMaterialImageListCtrl::~CMaterialImageListCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
CImageListCtrlItem* CMaterialImageListCtrl::AddMaterial( CMaterial *pMaterial,void *pUserData )
{
	CMtlItem *pItem = new CMtlItem;
	pItem->pMaterial = pMaterial;
	pItem->pUserData = pUserData;
	InsertItem(pItem);
	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CMaterialImageListCtrl::CMtlItem* CMaterialImageListCtrl::FindMaterialItem( CMaterial *pMaterial )
{
	Items items;
	GetAllItems(items);
	for (int i = 0; i < items.size(); i++)
	{
		CImageListCtrlItem *pItem = items[i];
		CMtlItem* pMtlItem = (CMtlItem*)pItem;
		if (pMtlItem->pMaterial == pMaterial)
			return pMtlItem;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::SelectMaterial( CMaterial *pMaterial )
{
	ResetSelection();
	CMtlItem* pMtlItem = FindMaterialItem(pMaterial);
	if (pMtlItem)
	{
		SelectItem(pMtlItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::InvalidateMaterial( CMaterial *pMaterial )
{
	CMtlItem* pMtlItem = FindMaterialItem(pMaterial);
	if (pMtlItem)
	{
		InvalidateItemRect(pMtlItem);
		pMtlItem->bBitmapValid = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::OnUpdateItem( CImageListCtrlItem *pItem )
{
	// Make a bitmap from image.

	CMtlItem* pMtlItem = (CMtlItem*)pItem;

	CImage image;

	bool bPreview = false;
	if (pMtlItem->pMaterial)
	{
		if (!(pMtlItem->pMaterial->GetFlags() & MTL_FLAG_NOPREVIEW))
		{
			m_renderCtrl.MoveWindow( pItem->rect );
			m_renderCtrl.ShowWindow(SW_SHOW);
			if(!strcmp(pMtlItem->pMaterial->GetShaderName(), "Terrain.Layer"))
			{
				XmlNodeRef node = CreateXmlNode( "Material" );
				CBaseLibraryItem::SerializeContext ctx( node, false );
				pMtlItem->pMaterial->Serialize( ctx );

				if(!m_pMatPreview)
				{
					int flags = 0;
					if (node->getAttr( "MtlFlags",flags ))
					{
						flags |= MTL_FLAG_UIMATERIAL;
						node->setAttr( "MtlFlags",flags );
					}
					m_pMatPreview = GetIEditor()->GetMaterialManager()->CreateMaterial( "_NewPreview_", node);
				}
				else
				{
					CBaseLibraryItem::SerializeContext ctx( node, true );
					m_pMatPreview->Serialize( ctx );
				}
				m_pMatPreview->SetShaderName("Illum");
				m_pMatPreview->Update();
				m_renderCtrl.SetMaterial( m_pMatPreview );
			}
			else
				m_renderCtrl.SetMaterial( pMtlItem->pMaterial );
			m_renderCtrl.GetImage( image );
			m_renderCtrl.ShowWindow(SW_HIDE);
		}
		bPreview = true;
	}

	if (!bPreview)
	{
		image.Allocate( pItem->rect.Width(),pItem->rect.Height() );
		image.Clear();
	}

	unsigned int *pImageData = image.GetData();
	int w = image.GetWidth();
	int h = image.GetHeight();

	if (pItem->bitmap.GetSafeHandle() != NULL)
		pItem->bitmap.DeleteObject();

	VERIFY(pItem->bitmap.CreateBitmap(w,h,1,32, pImageData));
	pItem->bBitmapValid = true;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::OnSelectItem( CImageListCtrlItem *pItem,bool bSelected )
{
	__super::OnSelectItem(pItem,bSelected);

	if (m_selectMtlFunc && bSelected && pItem)
		m_selectMtlFunc( (CMtlItem*)pItem );
}

#define MENU_USE_BOX 1
#define MENU_USE_SPHERE 2
#define MENU_USE_TEAPOT 3
#define MENU_BG_BLACK 4
#define MENU_BG_GRAY  5
#define MENU_BG_WHITE 6
#define MENU_USE_BACKLIGHT 7

//////////////////////////////////////////////////////////////////////////
void CMaterialImageListCtrl::OnRButtonUp(UINT nFlags, CPoint point)
{
	CMenu menu;
	menu.CreatePopupMenu();

	menu.AppendMenu( MF_STRING|((m_nModel==0)?MF_CHECKED:0),MENU_USE_BOX,"Use Box" );
	menu.AppendMenu( MF_STRING|((m_nModel==1)?MF_CHECKED:0),MENU_USE_SPHERE,"Use Sphere" );
	menu.AppendMenu( MF_STRING|((m_nModel==2)?MF_CHECKED:0),MENU_USE_TEAPOT,"Use Teapot" );
	menu.AppendMenu( MF_SEPARATOR,0,"" );
	menu.AppendMenu( MF_STRING|((m_nColor==0)?MF_CHECKED:0),MENU_BG_BLACK,"Black Background" );
	menu.AppendMenu( MF_STRING|((m_nColor==1)?MF_CHECKED:0),MENU_BG_GRAY,"Gray Background" );
	menu.AppendMenu( MF_STRING|((m_nColor==2)?MF_CHECKED:0),MENU_BG_WHITE,"White Background" );
	menu.AppendMenu( MF_SEPARATOR,0,"" );
	menu.AppendMenu( MF_STRING|((m_renderCtrl.UseBackLight())?MF_CHECKED:0),MENU_USE_BACKLIGHT,"Use Back Light" );

	ClientToScreen(&point);

	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY,point.x,point.y,this );
	switch(cmd) {
	case MENU_USE_BOX:
		m_renderCtrl.LoadFile( "Editor/Objects/MtlBox.cgf" );
		m_renderCtrl.SetCameraRadius( 1.6f );
		m_nModel = 0;
		break;
	case MENU_USE_SPHERE:
		m_renderCtrl.LoadFile( "Editor/Objects/Sphere.cgf" );
		m_renderCtrl.SetCameraRadius( 2.2f );
		m_nModel = 1;
		break;
	case MENU_USE_TEAPOT:
		m_renderCtrl.LoadFile( "Editor/Objects/MtlTeapot.cgf" );
		m_renderCtrl.SetCameraRadius( 1.6f );
		m_nModel = 2;
		break;
	case MENU_BG_BLACK:
		m_renderCtrl.SetClearColor( ColorF(0,0,0) );
		m_nColor = 0;
		break;
	case MENU_BG_GRAY:
		m_renderCtrl.SetClearColor( ColorF(0.5f,0.5f,0.5f) );
		m_nColor = 1;
		break;
	case MENU_BG_WHITE:
		m_renderCtrl.SetClearColor( ColorF(1,1,1) );
		m_nColor = 2;
		break;

	case MENU_USE_BACKLIGHT:
		m_renderCtrl.UseBackLight( !m_renderCtrl.UseBackLight() );
		break;
	}
	InvalidateAllBitmaps();
	Invalidate();
}
