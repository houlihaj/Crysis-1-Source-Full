////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   VoxelObjectpanel.cpp
//  Version:     v1.00
//  Created:     2/12/2002 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VoxelObjectPanel.h"

#include "Objects\ObjectManager.h"
#include "Objects\VoxelObject.h"

// CVoxelObjectPanel dialog

IMPLEMENT_DYNAMIC(CVoxelObjectPanel, CXTResizeDialog)
CVoxelObjectPanel::CVoxelObjectPanel(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CVoxelObjectPanel::IDD, pParent)
{
	m_voxelObj = 0;
	Create( IDD,pParent );
}

//////////////////////////////////////////////////////////////////////////
CVoxelObjectPanel::~CVoxelObjectPanel()
{
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObjectPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	//DDX_Control(pDX, IDC_SAVE_TO_CGF, m_btn1);
	//DDX_Control(pDX, IDC_REFRESH, m_btn2);
	//DDX_Control(pDX, IDC_SIDES, m_sides);
	//DDX_Control(pDX, IDC_GEOMETRY_EDIT, m_subObjEdit);
}

//////////////////////////////////////////////////////////////////////////
void CVoxelObjectPanel::SetVoxelObject( CVoxelObject *obj )
{
	m_voxelObj = obj;
}

BEGIN_MESSAGE_MAP(CVoxelObjectPanel, CXTResizeDialog)
	//ON_CBN_SELENDOK(IDC_SIDES, OnCbnSelendokSides)
	//ON_BN_CLICKED(IDC_SAVE_TO_CGF, OnBnClickedSavetoCGF)
	ON_BN_CLICKED(IDC_VOXEL_RESET_XFORM, OnBnClicked)
	ON_BN_CLICKED(IDC_VOXEL_SPLIT, OnBnSplit)
	ON_BN_CLICKED(IDC_VOXEL_UPDATE, OnUpdate)
	ON_BN_CLICKED(IDC_VOXEL_COPYHM, OnCopyHM)
	
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CVoxelObjectPanel::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	/*
	CString str;
	for (int i = 0; i < 10; i++)
	{
		str.Format( "%d",i );
		m_sides.AddString( str );
	}
	*/


	return TRUE;  // return TRUE unless you set the focus to a control
}


void CVoxelObjectPanel::OnBnClicked()
{
	if (m_voxelObj)
	{
		if(!m_voxelObj->ResetTransformation())
		{
			char out [1024];
			sprintf(out, 
				"Reset rotation operation is not possible on %s voxel object.", (const char*)m_voxelObj->GetName());
			MessageBox(out, "Reset rotation error", MB_OK|MB_ICONSTOP);
		}
	}
  else
	{
		// Reset all selected voxels.
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pBaseObj = selection->GetObject(i);
			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CVoxelObject)))
			{
				bool res = ((CVoxelObject*)pBaseObj)->ResetTransformation();
				if(!res)
				{
					char out [1024];
					sprintf(out, 
						"Reset rotation operation is not possible on %s voxel object.\n"
						"Press OK to continue processing other objects or press Cancel to abort.", (const char*)pBaseObj->GetName());
					if(MessageBox(out, "Reset rotation error", MB_OKCANCEL|MB_ICONSTOP)==IDCANCEL)
					{
						break;
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////
void CVoxelObjectPanel::OnBnSplit()
{
	if (m_voxelObj)
	{
		m_voxelObj->Split();
	}
	else
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pBaseObj = selection->GetObject(i);
			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CVoxelObject)))
			{
				bool res = ((CVoxelObject*)pBaseObj)->Split();
			}
		}
	}
}

////////////////////////////////////////////////////////////
void CVoxelObjectPanel::OnUpdate()
{
	if (m_voxelObj)
	{
		m_voxelObj->Retriangulate();
	}
	else
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pBaseObj = selection->GetObject(i);
			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CVoxelObject)))
			{
				((CVoxelObject*)pBaseObj)->Retriangulate();
			}
		}
	}
}

void CVoxelObjectPanel::OnCopyHM()
{
	if (m_voxelObj)
	{
		m_voxelObj->CopyHM();
	}
	else
	{
		CSelectionGroup *selection = GetIEditor()->GetSelection();
		for (int i = 0; i < selection->GetCount(); i++)
		{
			CBaseObject *pBaseObj = selection->GetObject(i);
			if (pBaseObj->IsKindOf(RUNTIME_CLASS(CVoxelObject)))
			{
				((CVoxelObject*)pBaseObj)->CopyHM();
			}
		}
	}
}