// ModelViewSubmeshPanel.cpp : implementation file
//

#include "stdafx.h"
#include "ICryAnimation.h"
#include "ModelViewSubmeshPanel.h"

#include "ModelViewport.h"
#include "CryFile.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ModelViewSubmeshPanel dialog


ModelViewSubmeshPanel::ModelViewSubmeshPanel( CModelViewport *vp,CWnd* pParent /* = NULL */)
	: CXTResizeDialog(ModelViewSubmeshPanel::IDD, pParent)
{
	m_modelView = vp;

	Create(MAKEINTRESOURCE(IDD),pParent);
	//{{AFX_DATA_INIT(ModelViewSubmeshPanel)
	//}}AFX_DATA_INIT
}


void ModelViewSubmeshPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(ModelViewSubmeshPanel, CXTResizeDialog)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ModelViewSubmeshPanel message handlers


