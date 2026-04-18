// TcbKeyDialog.cpp : implementation file
//

#include "stdafx.h"
#include "TcbKeyDialog.h"

#include "AnimationContext.h"

#include "IMovieSystem.h"

#include "ViewManager.h"

// CTcbKeyDialog dialog

IMPLEMENT_DYNAMIC(CTcbKeyDialog, CDialog)
CTcbKeyDialog::CTcbKeyDialog(CWnd* pParent /*=NULL*/)
	: IKeyDlg(CTcbKeyDialog::IDD, pParent)
{
	m_track = 0;
	m_node = 0;
	m_key = -1;
	m_paramId = -1;
	m_currValue = Vec3(VMAX);
	m_currTens = FLT_MAX;
	m_currCont = FLT_MAX;
	m_currBias = FLT_MAX;
	m_currEaseTo = FLT_MAX;
	m_currEaseFrom = FLT_MAX;
	m_nLButtonState = 0;

	GetIEditor()->RegisterNotifyListener(this);
}

CTcbKeyDialog::~CTcbKeyDialog()
{
	GetIEditor()->UnregisterNotifyListener(this);
}

void CTcbKeyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TCBPREVIEW, m_tcbPreview);
}

void CTcbKeyDialog::OnEditorNotifyEvent(EEditorNotifyEvent event)
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

BEGIN_MESSAGE_MAP(CTcbKeyDialog, CDialog)
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


// CTcbKeyDialog message handlers

void CTcbKeyDialog::OnCtrlLBD()
{
	if (m_node != NULL)
	{
		IEntity *pEntity = m_node->GetEntity();

		if (pEntity != NULL)
		{
			m_dragStartM = pEntity->GetWorldTM();
			m_dragStartInvM = m_dragStartM.GetInverted();
		}
	}

	m_nLButtonState = 1;
}

void CTcbKeyDialog::OnCtrlLBU()
{
	m_nLButtonState = 0;

	SetKeyControls(m_key);
}

BOOL CTcbKeyDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_value[0].Create( this,IDC_XVALUE );
	m_value[1].Create( this,IDC_YVALUE );
	m_value[2].Create( this,IDC_ZVALUE );
	//m_value[3].Create( this,IDC_WVALUE );

	m_tcb[0].Create( this,IDC_TENSION );
	m_tcb[1].Create( this,IDC_CONTINUITY );
	m_tcb[2].Create( this,IDC_BIAS );
	m_tcb[3].Create( this,IDC_EASETO );
	m_tcb[4].Create( this,IDC_EASEFROM );

	m_value[0].SetRange( -10000,10000 );
	m_value[1].SetRange( -10000,10000 );
	m_value[2].SetRange( -10000,10000 );
	//m_value[3].SetRange( -10000,10000 );

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
void CTcbKeyDialog::SetKey( IAnimNode *node,IAnimTrack *track,int nkey )
{
	assert( track );

	// Store previous data.
	if (node != m_node || track != m_track || nkey != m_key)
		OnUpdateValue();

	EAnimTrackType trType = track->GetType();
	assert( trType == ATRACK_TCB_FLOAT || trType == ATRACK_TCB_VECTOR || trType == ATRACK_TCB_QUAT );

	m_paramId = -1;

	IAnimBlock *pAnimBlock = node->GetAnimBlock();

	if (pAnimBlock != NULL)
	{
		for (int i = 0; i < pAnimBlock->GetTrackCount(); i++)
		{
			int paramId;
			IAnimTrack *pAnimTrack;

			if (pAnimBlock->GetTrackInfo(i,paramId,&pAnimTrack) && pAnimTrack == track)
			{
				m_paramId = paramId;
				break;
			}
		}
	}

	m_track = track;
	m_node = node;
	m_key = nkey;

	SetKeyControls( nkey );
}

//////////////////////////////////////////////////////////////////////////
inline double Round( double fVal, double fStep )
{
	return int_round(fVal / fStep) * fStep;
}

//////////////////////////////////////////////////////////////////////////
void CTcbKeyDialog::SetKeyControls( int nkey )
{
	ITcbKey key;

	if (nkey < 0)
		nkey = 0;
	if (nkey > m_track->GetNumKeys()-1)
		nkey = m_track->GetNumKeys()-1;

	EAnimValue valueType = m_track->GetValueType();
	m_track->GetKey( nkey,&key );
	if (valueType == AVALUE_FLOAT)
	{
		if (m_currValue.x != key.GetFloat())
		{
			m_value[0].SetValue( Round(key.GetFloat(), 1e-4) );
			m_currValue.x = key.GetFloat();
		}
		m_value[0].EnableWindow(TRUE);
		m_value[1].EnableWindow(FALSE);
		m_value[2].EnableWindow(FALSE);
		//m_value[3].EnableWindow(FALSE);
	}
	else if (valueType == AVALUE_VECTOR)
	{
		Vec3 keyValue = key.GetVec3();

		if (m_paramId == APARAM_POS || m_paramId == APARAM_SCL)
		{
			Matrix34 tm;

			if (m_paramId == APARAM_POS)
			{
				tm.SetTranslationMat(keyValue);
			}
			else
			{
				tm.SetScale(keyValue);
			}

			IEntity *pEntity = m_node->GetEntity();

			if (pEntity != NULL)
			{
				int coordSys = GetIEditor()->GetReferenceCoordSys();

				switch (coordSys)
				{
					case COORDS_LOCAL:
						{
							if (m_nLButtonState)
							{
								tm = m_dragStartInvM*tm;
							}
							else
							{
								tm = pEntity->GetWorldTM().GetInverted()*tm;
							}
							break;
						}
					case COORDS_PARENT:
						{
							IEntity *pParent = pEntity->GetParent();

							if (pParent != NULL)
							{
								tm = pParent->GetWorldTM().GetInverted()*tm;
							}
							break;
						}
					case COORDS_USERDEFINED:
						{
							tm = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix().GetInverted()*tm;
							break;
						}
				}
			}

			if (m_paramId == APARAM_POS)
			{
				keyValue = tm.GetTranslation();
			}
			else
			{
				AffineParts ap;

				ap.SpectralDecompose(tm);

				keyValue = ap.scale;
			}
		}

		if (m_currValue != keyValue)
		{
			m_value[0].SetValue( Round(keyValue.x, 1e-4) );
			m_value[1].SetValue( Round(keyValue.y, 1e-4) );
			m_value[2].SetValue( Round(keyValue.z, 1e-4) );
			m_currValue = keyValue;
		}

		m_value[0].EnableWindow(TRUE);
		m_value[1].EnableWindow(TRUE);
		m_value[2].EnableWindow(TRUE);
		//m_value[3].EnableWindow(FALSE);
	}
	else if (valueType == AVALUE_QUAT)
	{
		Quat keyValue = key.GetQuat();

		if (m_paramId == APARAM_ROT)
		{
			Matrix34 tm;

			tm.Set(Vec3(1,1,1),keyValue);

			IEntity *pEntity = m_node->GetEntity();

			if (pEntity != NULL)
			{
				int coordSys = GetIEditor()->GetReferenceCoordSys();

				switch (coordSys)
				{
					case COORDS_LOCAL:
						{
							if (m_nLButtonState)
							{
								tm = m_dragStartInvM*tm;
							}
							else
							{
								tm = pEntity->GetWorldTM().GetInverted()*tm;
							}
							break;
						}
					case COORDS_PARENT:
						{
							IEntity *pParent = pEntity->GetParent();

							if (pParent != NULL)
							{
								tm = pParent->GetWorldTM().GetInverted()*tm;
							}
							break;
						}
					case COORDS_USERDEFINED:
						{
							tm = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix().GetInverted()*tm;
							break;
						}
				}
			}

			AffineParts ap;

			ap.SpectralDecompose(tm);

			keyValue = ap.rot;
		}

		Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(keyValue)) * 180.0f/gf_PI;
		if (m_currValue.x != angles.x || m_currValue.y != angles.y || m_currValue.z != angles.z)
		{
			m_value[0].SetValue( Round(angles.x, 1e-4) );
			m_value[1].SetValue( Round(angles.y, 1e-4) );
			m_value[2].SetValue( Round(angles.z, 1e-4) );
			m_currValue.x = angles.x;
			m_currValue.y = angles.y;
			m_currValue.z = angles.z;
		}
		//m_value[3].SetValue( key.GetQuat().w );

		m_value[0].EnableWindow(TRUE);
		m_value[1].EnableWindow(TRUE);
		m_value[2].EnableWindow(TRUE);
		//m_value[3].EnableWindow(TRUE);
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

void CTcbKeyDialog::OnUpdateValue()
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

	EAnimValue valueType = m_track->GetValueType();
	m_track->GetKey( m_key,&key );
	if (valueType == AVALUE_FLOAT)
	{
		float val = m_value[0].GetValue();
		if (key.GetFloat() != val)
		{
		  if(!bUndoRecorded)
			{
				bUndoRecorded = true;
				RecordTrackUndo();
			}
		}

		key.SetValue(val);
	}
	else if (valueType == AVALUE_VECTOR)
	{
		Vec3 vec(m_value[0].GetValue(),m_value[1].GetValue(),m_value[2].GetValue());

		if (m_paramId == APARAM_POS || m_paramId == APARAM_SCL)
		{
			Matrix34 tm;

			if (m_paramId == APARAM_POS)
			{
				tm.SetTranslationMat(vec);
			}
			else
			{
				tm.SetScale(vec);
			}

			IEntity *pEntity = m_node->GetEntity();

			if (pEntity != NULL)
			{
				int coordSys = GetIEditor()->GetReferenceCoordSys();

				switch (coordSys)
				{
					case COORDS_LOCAL:
						{
							if (m_nLButtonState)
							{
								tm = m_dragStartM*tm;
							}
							else
							{
								tm = pEntity->GetWorldTM()*tm;
							}
							break;
						}
					case COORDS_PARENT:
						{
							IEntity *pParent = pEntity->GetParent();

							if (pParent != NULL)
							{
								tm = pParent->GetWorldTM()*tm;
							}
							break;
						}
					case COORDS_USERDEFINED:
						{
							tm = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix()*tm;
							break;
						}
				}
			}

			if (m_paramId == APARAM_POS)
			{
				vec = tm.GetTranslation();
			}
			else
			{
				AffineParts ap;

				ap.SpectralDecompose(tm);

				vec = ap.scale;
			}
		}

		if (vec.x != key.GetVec3().x || vec.y != key.GetVec3().y || vec.z != key.GetVec3().z)
		{
			if(!bUndoRecorded)
			{
				bUndoRecorded = true;
				RecordTrackUndo();
			}
		}

		key.SetValue(vec);
	}
	else if (valueType == AVALUE_QUAT)
	{
		Ang3 angles( m_value[0].GetValue(),m_value[1].GetValue(),m_value[2].GetValue() );
		Quat quat;
		quat.SetRotationXYZ( angles * gf_PI/180.0f );

		if (m_paramId == APARAM_ROT)
		{
			Matrix34 tm;

			tm.Set(Vec3(1,1,1),quat);

			IEntity *pEntity = m_node->GetEntity();

			if (pEntity != NULL)
			{
				int coordSys = GetIEditor()->GetReferenceCoordSys();

				switch (coordSys)
				{
					case COORDS_LOCAL:
						{
							if (m_nLButtonState)
							{
								tm = m_dragStartM*tm;
							}
							else
							{
								tm = pEntity->GetWorldTM()*tm;
							}
							break;
						}
					case COORDS_PARENT:
						{
							IEntity *pParent = pEntity->GetParent();

							if (pParent != NULL)
							{
								tm = pParent->GetWorldTM()*tm;
							}
							break;
						}
					case COORDS_USERDEFINED:
						{
							tm = GetIEditor()->GetViewManager()->GetGrid()->GetMatrix()*tm;
							break;
						}
				}
			}

			AffineParts ap;

			ap.SpectralDecompose(tm);

			quat = ap.rot;
		}

		if (quat.v.x != key.GetQuat().v.x || quat.v.y != key.GetQuat().v.y || quat.v.z != key.GetQuat().v.z || quat.w != key.GetQuat().w)
		{
			if(!bUndoRecorded)
			{
				bUndoRecorded = true;
				RecordTrackUndo();
			}
		}

		key.SetValue(quat);
	}
	
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