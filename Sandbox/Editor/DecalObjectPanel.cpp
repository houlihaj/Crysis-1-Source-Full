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

#include "stdafx.h"
#include "DecalObjectPanel.h"

#include "Objects\ObjectManager.h"
#include "Objects\DecalObject.h"
#include "EditTool.h"


//////////////////////////////////////////////////////////////////////////
// CDecalObjectTool dialog
//////////////////////////////////////////////////////////////////////////


class CDecalObjectTool : public CEditTool
{
public:
	DECLARE_DYNCREATE( CDecalObjectTool )

	CDecalObjectTool();

	virtual void Display( DisplayContext& dc ) {};
	virtual bool MouseCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags );
	virtual bool OnKeyDown( CViewport* view,uint nChar, uint nRepCnt, uint nFlags );
	virtual bool OnKeyUp( CViewport* view, uint nChar, uint nRepCnt, uint nFlags );
	virtual void SetUserData( void* userData );

protected:
	virtual ~CDecalObjectTool();
	void DeleteThis() { delete this; };

private:
	CDecalObject* m_pDecalObj;
};


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE( CDecalObjectTool,CEditTool )

CDecalObjectTool::CDecalObjectTool()
{
}

//////////////////////////////////////////////////////////////////////////
CDecalObjectTool::~CDecalObjectTool()
{
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::OnKeyDown( CViewport* view, uint nChar, uint nRepCnt, uint nFlags )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::OnKeyUp( CViewport* view, uint nChar, uint nRepCnt, uint nFlags )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CDecalObjectTool::MouseCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags )
{
	if( m_pDecalObj )
		m_pDecalObj->MouseCallbackImpl( view, event, point, flags );
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CDecalObjectTool::SetUserData( void* userData )
{
	m_pDecalObj = static_cast< CDecalObject* >( userData );
}


//////////////////////////////////////////////////////////////////////////
// CDecalObjectPanel dialog
//////////////////////////////////////////////////////////////////////////


IMPLEMENT_DYNAMIC( CDecalObjectPanel, CXTResizeDialog )

CDecalObjectPanel::CDecalObjectPanel( CWnd* pParent )
: CXTResizeDialog( CDecalObjectPanel::IDD, pParent )
{
	m_pDecalObj = 0;
	Create( IDD, pParent );
}

//////////////////////////////////////////////////////////////////////////
CDecalObjectPanel::~CDecalObjectPanel()
{
}

//////////////////////////////////////////////////////////////////////////
void CDecalObjectPanel::DoDataExchange( CDataExchange* pDX )
{
	CXTResizeDialog::DoDataExchange( pDX );
	DDX_Control( pDX, IDC_DECAL_REORIENTATE, m_decalReorientateButton );
}

BEGIN_MESSAGE_MAP( CDecalObjectPanel, CXTResizeDialog )
	ON_BN_CLICKED( IDC_DECAL_UPDATE_PROJECTION, OnUpdateProjection )
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CDecalObjectPanel::SetDecalObject( CDecalObject* pObj )
{
	m_pDecalObj = pObj;
	if( m_pDecalObj )
		m_decalReorientateButton.SetToolClass( RUNTIME_CLASS( CDecalObjectTool ), m_pDecalObj );
	else
		m_decalReorientateButton.EnableWindow( FALSE );	
}

//////////////////////////////////////////////////////////////////////////
BOOL CDecalObjectPanel::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();
	return TRUE;  // return TRUE unless you set the focus to a control
}

//////////////////////////////////////////////////////////////////////////
void CDecalObjectPanel::OnUpdateProjection()
{
	if( m_pDecalObj && m_pDecalObj->GetProjectionType() > 0 )
		m_pDecalObj->UpdateEngineNode();
}
