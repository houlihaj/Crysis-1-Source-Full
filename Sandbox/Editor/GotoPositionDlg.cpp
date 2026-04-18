// GotoPositionDlg.cpp : implementation file
//

#include "stdafx.h"

#include "ViewManager.h"
#include "GotoPositionDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CGotoPositionDlg dialog


CGotoPositionDlg::CGotoPositionDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CGotoPositionDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGotoPositionDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CGotoPositionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGotoPositionDlg)
	// NOTE: the ClassWizard will add DDX and DDV calls here
	//DDX_Check(pDX, IDC_CHECK1, m_bQuality);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGotoPositionDlg, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_EN_CHANGE(IDC_EDIT1, OnChangeEdit)
	ON_EN_UPDATE(IDC_DYMX, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_DYMY, OnUpdateNumbers)
	ON_EN_UPDATE(IDC_DYMZ, OnUpdateNumbers)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGotoPositionDlg message handlers


BOOL CGotoPositionDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	Vec3 pos;

	CViewport *pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
	if (pRenderViewport)
	{
		Matrix34 tm = pRenderViewport->GetViewTM();
		pos = pRenderViewport->GetViewTM().GetTranslation();
	}

	m_dymX.Create( this,IDC_DYMX );
	m_dymX.SetRange( -10000, 10000);
	m_dymX.SetValue(pos.x);

	m_dymY.Create( this,IDC_DYMY );
	m_dymY.SetRange( -10000, 10000);
	m_dymY.SetValue(pos.y);

	m_dymZ.Create( this,IDC_DYMZ );
	m_dymZ.SetRange( -10000, 10000);
	m_dymZ.SetValue(pos.z);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}



void CGotoPositionDlg::OnChangeEdit()
{
	float pos[3] = {0, 0, 0};
	CWnd * pWnd = GetDlgItem(IDC_EDIT1);
	if(pWnd)
	{
		pWnd->GetWindowText(m_sPos);
		if(m_sPos.GetLength()>0)
		{
			char str[1024];
			int len = m_sPos.GetLength();
			strcpy(str, m_sPos);
			str[1023] = 0;
			int i;
			for(i=0; i<len; i++)
				if(str[i] == ' ' || str[i] == ',' || str[i] == ';' || str[i] == '\t')
					str[i] = 0;

			int k;
			for(i=0, k=0; i<len && k<3; i++)
			{
				int ln = strlen(&str[i]);
				if(ln>0)
				{
					sscanf( &str[i],"%f",&pos[k]);
					i+=ln-1;
					k++;
				}
			}
		}
	}
	m_dymX.SetValue(pos[0]);
	m_dymY.SetValue(pos[1]);
	m_dymZ.SetValue(pos[2]);
}


//////////////////////////////////////////////////////////////////////////
void CGotoPositionDlg::OnUpdateNumbers() 
{
	char str[1024];
	sprintf(str, "%.2f, %.2f, %.2f", m_dymX.GetValue(), m_dymY.GetValue(), m_dymZ.GetValue());
	CWnd * pWnd = GetDlgItem(IDC_EDIT1);
	if(pWnd)
		pWnd->SetWindowText(str);
}


void CGotoPositionDlg::OnBnClickedOk()
{
	CViewport *pRenderViewport = GetIEditor()->GetViewManager()->GetGameViewport();
	if (pRenderViewport)
	{
		Matrix34 tm = pRenderViewport->GetViewTM();
		tm.SetTranslation(Vec3(m_dymX.GetValue(), m_dymY.GetValue(), m_dymZ.GetValue()));
		pRenderViewport->SetViewTM(tm);
	}

	OnOK();
}