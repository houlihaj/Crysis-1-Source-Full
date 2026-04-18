#include "stdafx.h"
#include "TVShakeKeyDialog.h"

IMPLEMENT_DYNAMIC(CTVShakeKeyDialog,CDialog)
CTVShakeKeyDialog::CTVShakeKeyDialog(CWnd* pParent) : IKeyDlg(CTVShakeKeyDialog::IDD,pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = -1;
	m_currValue = Vec4(VMAX);
	m_currTens = FLT_MAX;
	m_currCont = FLT_MAX;
	m_currBias = FLT_MAX;
	m_currEaseTo = FLT_MAX;
	m_currEaseFrom = FLT_MAX;
	m_nLButtonState = 0;

	GetIEditor()->RegisterNotifyListener(this);
}

CTVShakeKeyDialog::~CTVShakeKeyDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CTVShakeKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX,IDC_TCBPREVIEW,m_tcbPreview);
}

void CTVShakeKeyDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (!m_node)
		return;
	if (!m_track)
		return;
	if (m_key < 0)
		return;

	if (event == eNotify_OnIdleUpdate)
		SetKeyControls(m_key);
}

BEGIN_MESSAGE_MAP(CTVShakeKeyDialog,CDialog)
	ON_EN_UPDATE(IDC_XVALUE, OnUpdateValue)
	ON_EN_UPDATE(IDC_YVALUE, OnUpdateValue)
	ON_EN_UPDATE(IDC_ZVALUE, OnUpdateValue)
	ON_EN_UPDATE(IDC_WVALUE, OnUpdateValue)
	ON_EN_UPDATE(IDC_TENSION, OnUpdateValue)
	ON_EN_UPDATE(IDC_CONTINUITY, OnUpdateValue)
	ON_EN_UPDATE(IDC_BIAS, OnUpdateValue)
	ON_EN_UPDATE(IDC_EASETO, OnUpdateValue)
	ON_EN_UPDATE(IDC_EASEFROM, OnUpdateValue)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_XVALUE, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_XVALUE, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_YVALUE, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_YVALUE, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_ZVALUE, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_ZVALUE, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_WVALUE, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_WVALUE, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_TENSION, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_TENSION, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_CONTINUITY, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_CONTINUITY, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_BIAS, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_BIAS, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_EASETO, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_EASETO, OnCtrlLBU)
	ON_CONTROL(WMU_LBUTTONDOWN, IDC_EASEFROM, OnCtrlLBD)
	ON_CONTROL(WMU_LBUTTONUP, IDC_EASEFROM, OnCtrlLBU)
END_MESSAGE_MAP()

void CTVShakeKeyDialog::OnCtrlLBD()
{
	m_nLButtonState = 1;
}

void CTVShakeKeyDialog::OnCtrlLBU()
{
	m_nLButtonState = 0;

	SetKeyControls(m_key);
}

BOOL CTVShakeKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_value[0].Create( this,IDC_XVALUE );
	m_value[1].Create( this,IDC_YVALUE );
	m_value[2].Create( this,IDC_ZVALUE );
	m_value[3].Create( this,IDC_WVALUE );

	m_tcb[0].Create( this,IDC_TENSION );
	m_tcb[1].Create( this,IDC_CONTINUITY );
	m_tcb[2].Create( this,IDC_BIAS );
	m_tcb[3].Create( this,IDC_EASETO );
	m_tcb[4].Create( this,IDC_EASEFROM );

	m_value[0].SetRange( -10000,10000 );
	m_value[1].SetRange( -10000,10000 );
	m_value[2].SetRange( -10000,10000 );
	m_value[3].SetRange( -10000,10000 );

	m_tcb[0].SetRange( -1,1 );
	m_tcb[0].SetInternalPrecision(3);
	m_tcb[0].SetStep(0.001);
	m_tcb[1].SetRange( -1,1 );
	m_tcb[1].SetInternalPrecision(3);
	m_tcb[1].SetStep(0.001);
	m_tcb[2].SetRange( -1,1 );
	m_tcb[2].SetInternalPrecision(3);
	m_tcb[2].SetStep(0.001);
	m_tcb[3].SetRange( 0,1 );
	m_tcb[3].SetInternalPrecision(3);
	m_tcb[3].SetStep(0.001);
	m_tcb[4].SetRange( 0,1 );
	m_tcb[4].SetInternalPrecision(3);
	m_tcb[4].SetStep(0.001);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CTVShakeKeyDialog::SetKey(IAnimNode *node,IAnimTrack *track,int nkey)
{
	assert(track);

	// Store previous data.
	if (node != m_node || track != m_track || nkey != m_key)
		OnUpdateValue();

	assert(track->GetType() == ATRACK_TCB_VECTOR4);

	m_track = track;
	m_node = node;
	m_key = nkey;

	SetKeyControls( nkey );
}

//////////////////////////////////////////////////////////////////////////
inline double Round(double fVal, double fStep)
{
	return int_round(fVal / fStep) * fStep;
}

//////////////////////////////////////////////////////////////////////////
void CTVShakeKeyDialog::SetKeyControls(int nkey)
{
	ITcbKey key;

	if (nkey < 0)
		nkey = 0;
	if (nkey > m_track->GetNumKeys()-1)
		nkey = m_track->GetNumKeys()-1;

	m_track->GetKey( nkey,&key );

	Vec4 keyValue = key.GetVec4();

	if (m_currValue != keyValue)
	{
		m_value[0].SetValue( Round(keyValue.x, 1e-4) );
		m_value[1].SetValue( Round(keyValue.y, 1e-4) );
		m_value[2].SetValue( Round(keyValue.z, 1e-4) );
		m_value[3].SetValue( Round(keyValue.w, 1e-4) );
		m_currValue = keyValue;
	}

	bool updatePreview = false;

	if (m_currTens != key.tens)
	{
		m_tcb[0].SetValue( key.tens );
		m_currTens = key.tens;
		updatePreview = true;
	}
	if (m_currCont != key.cont)
	{
		m_tcb[1].SetValue( key.cont );
		m_currCont = key.cont;
		updatePreview = true;
	}
	if (m_currBias != key.bias)
	{
		m_tcb[2].SetValue( key.bias );
		m_currBias = key.bias;
		updatePreview = true;
	}
	if (m_currEaseTo != key.easeto)
	{
		m_tcb[3].SetValue( key.easeto );
		m_currEaseTo = key.easeto;
		updatePreview = true;
	}
	if (m_currEaseFrom != key.easefrom)
	{
		m_tcb[4].SetValue( key.easefrom );
		m_currEaseFrom = key.easefrom;
		updatePreview = true;
	}
	
	if (updatePreview)
	{
		m_tcbPreview.SetTcb( key.tens,key.cont,key.bias,key.easeto,key.easefrom );
	}
}

void CTVShakeKeyDialog::OnUpdateValue()
{
	if (!m_node)
		return;
	if (!m_track)
		return;
	if (m_key < 0)
		return;

	ITcbKey key;
	m_track->GetKey( m_key,&key );

	bool bUndoRecorded = false;

	if(m_nLButtonState)
	{
		if(m_nLButtonState==2)
			bUndoRecorded = true;
		else
			m_nLButtonState = 2;
	}

	Vec4 vec(m_value[0].GetValue(),m_value[1].GetValue(),m_value[2].GetValue(),m_value[3].GetValue());

	if (vec.x != key.GetVec4().x || vec.y != key.GetVec4().y || vec.z != key.GetVec4().z || vec.w != key.GetVec4().w)
	{
		if(!bUndoRecorded)
		{
			bUndoRecorded = true;
			RecordTrackUndo();
		}
	}

	key.SetValue(vec);
	
	if (!bUndoRecorded)
	{
		if (key.tens != m_tcb[0].GetValue() ||
				key.cont != m_tcb[1].GetValue() ||
				key.bias != m_tcb[2].GetValue() ||
				key.easeto != m_tcb[3].GetValue() ||
				key.easefrom != m_tcb[4].GetValue()
				)
		{
			bUndoRecorded = true;
			RecordTrackUndo();
		}
	}
	key.tens =  m_tcb[0].GetValue();
	key.cont = m_tcb[1].GetValue();
	key.bias = m_tcb[2].GetValue();
	key.easeto = m_tcb[3].GetValue();
	key.easefrom = m_tcb[4].GetValue();

	m_tcbPreview.SetTcb( key.tens,key.cont,key.bias,key.easeto,key.easefrom );

	m_track->SetKey( m_key,&key );
	GetIEditor()->GetAnimation()->ForceAnimation();
	RefreshTrackView();
}