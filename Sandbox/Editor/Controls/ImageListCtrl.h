////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CImageListCtrl.h
//  Version:     v1.00
//  Created:     9/10/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CImageListCtrl_h__
#define __CImageListCtrl_h__
#pragma once

struct CImageListCtrlItem : public CRefCountBase
{
	// Where to draw item.
	CRect rect;
	// Items image.
	CImage *pImage;
	CBitmap bitmap;
	void *pUserData;
	void *pUserData2;
	bool bBitmapValid;
	bool bSelected;

	CImageListCtrlItem();
	~CImageListCtrlItem();
};

//////////////////////////////////////////////////////////////////////////
// CImageListCtrl styles.
//////////////////////////////////////////////////////////////////////////
enum EImageListCtrl
{
	ILC_STYLE_HORZ        = 0x0001, // Horizontal only.
	ILC_STYLE_VERT        = 0x0002, // Vertical only.
	ILC_MULTI_SELECTION   = 0x0004, // Multiple items can be selected.
};

//////////////////////////////////////////////////////////////////////////
// Custom control to display list of images.
//////////////////////////////////////////////////////////////////////////
class CImageListCtrl : public CWnd
{
public:
	typedef std::vector<_smart_ptr<CImageListCtrlItem> > Items;

	CImageListCtrl();
	~CImageListCtrl();

	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd,UINT nID );
	void SetItemSize( CSize size );
	void SetBorderSize( CSize size );

	// Get all items inside specified rectangle.
	void GetItemsInRect( const CRect &rect,Items &items ) const;
	void GetAllItems( Items &items ) const;

	// Test if point is within an item. return NULL if point is not hit any item.
	CImageListCtrlItem* HitTest( CPoint point );

	virtual void InsertItem( CImageListCtrlItem *pItem );
	virtual void DeleteAllItems();
	
	virtual void SelectItem( CImageListCtrlItem *pItem,bool bSelect=true );
	virtual void SelectItem( int nIndex,bool bSelect=true );
	virtual void ResetSelection();
	
	void InvalidateItemRect( CImageListCtrlItem *pItem );
	void InvalidateAllItems();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg void OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar );
	afx_msg void OnVScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar );
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	virtual void DrawControl( CDC *pDC,const CRect &rcUpdate );
	virtual void DrawItem( CDC *pDC,const CRect &rcUpdate,CImageListCtrlItem *pItem );
	virtual void CalcLayout( bool bUpdateScrollBar=true );
	
	virtual void OnUpdateItem( CImageListCtrlItem *pItem );
	virtual void OnSelectItem( CImageListCtrlItem *pItem,bool bSelected );
	void InvalidateAllBitmaps();

	//////////////////////////////////////////////////////////////////////////
	// Member Variables
	//////////////////////////////////////////////////////////////////////////
	// Items.
	Items m_items;

	CBitmap m_offscreenBitmap;
	CSize m_itemSize;
	CSize m_borderSize;
	CPoint m_scrollOffset;
	CBrush m_bkgrBrush;
	CPen m_selectedPen;
};

#endif //__CImageListCtrl_h__
