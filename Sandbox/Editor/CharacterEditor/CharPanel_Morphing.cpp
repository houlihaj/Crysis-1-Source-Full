// CharMorphingDlg.cpp : implementation file
//

#include "stdafx.h"

#include <I3DEngine.h>
#include "ICryAnimation.h"
#include "ModelViewportCE.h"
#include "CharPanel_Morphing.h"

//#include ".\charpanel_morphing.h"
//#include ".\charpanel_morphing.h"

/*
static COLORREF ButtonColors[8] = 
{
	RGB(40,0,0),
	RGB(80,0,0),
	RGB(100,0,0),
	RGB(120,0,0),
	RGB(160,0,0),
	RGB(180,0,0),
	RGB(210,0,0),
	RGB(255,0,0),
};*/

IMPLEMENT_DYNAMIC(CMorphingDlg, CDialog)

void CMorphingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control( pDX,IDC_CHECK0,m_button[0] );
	DDX_Control( pDX,IDC_CHECK1,m_button[1] );
	DDX_Control( pDX,IDC_CHECK2,m_button[2] );
	DDX_Control( pDX,IDC_CHECK3,m_button[3] );
	DDX_Control( pDX,IDC_CHECK4,m_button[4] );
	DDX_Control( pDX,IDC_CHECK5,m_button[5] );
	DDX_Control( pDX,IDC_CHECK6,m_button[6] );
	DDX_Control( pDX,IDC_CHECK7,m_button[7] );
}


BEGIN_MESSAGE_MAP(CMorphingDlg, CDialog)

	ON_CONTROL(WMU_FS_CHANGED, IDC_DEFORMATION_SLIDER, OnDeformationSlider)
	ON_EN_CHANGE(IDC_DEFORMATION_NUMBER, OnDeformationNumber)

	ON_CONTROL(WMU_FS_CHANGED, IDC_UNIFORM_SCALING_SLIDER, OnUniformScalingSlider)
	ON_EN_CHANGE(IDC_UNIFORM_SCALING_NUMBER, OnUniformScalingNumber)

	ON_BN_CLICKED(IDC_CHECK0, OnBnClicked_COLOR0)
	ON_BN_CLICKED(IDC_CHECK1, OnBnClicked_COLOR1)
	ON_BN_CLICKED(IDC_CHECK2, OnBnClicked_COLOR2)
	ON_BN_CLICKED(IDC_CHECK3, OnBnClicked_COLOR3)
	ON_BN_CLICKED(IDC_CHECK4, OnBnClicked_COLOR4)
	ON_BN_CLICKED(IDC_CHECK5, OnBnClicked_COLOR5)
	ON_BN_CLICKED(IDC_CHECK6, OnBnClicked_COLOR6)
	ON_BN_CLICKED(IDC_CHECK7, OnBnClicked_COLOR7)
	ON_BN_CLICKED(IDC_BUTTON_WIREFRAME, OnBnClicked_WIREFRAME)
END_MESSAGE_MAP()


// CMorphingDlg message handlers
BOOL CMorphingDlg::OnInitDialog()
{
	m_DeformationSlider.SubclassDlgItem( IDC_DEFORMATION_SLIDER,this );
	m_DeformationSlider.SetRangeFloat( -2,2 );
	m_DeformationSlider.SetValue(0);
	m_DeformationSlider.EnableWindow(FALSE);
	m_DeformationNumber.Create( this,IDC_DEFORMATION_NUMBER );
	m_DeformationNumber.SetRange( -2,2 );
	m_DeformationNumber.SetInternalPrecision( 3 );
	m_DeformationNumber.EnableWindow(FALSE);

	m_UniformScalingSlider.SubclassDlgItem( IDC_UNIFORM_SCALING_SLIDER,this );
	m_UniformScalingSlider.SetRangeFloat( 0.1f,2.0f );
	m_UniformScalingSlider.SetValue(1.0f);
	m_UniformScalingSlider.EnableWindow(TRUE);
	m_UniformScalingNumber.Create( this,IDC_UNIFORM_SCALING_NUMBER );
	m_UniformScalingNumber.SetRange( 0.1f,2.0f );
	m_UniformScalingNumber.SetValue(1.0f);
	m_UniformScalingNumber.SetInternalPrecision( 3 );
	m_UniformScalingNumber.EnableWindow(TRUE);

	UpdateData(FALSE);
	return TRUE;
}



void CMorphingDlg::OnDeformationSlider()
{
	float val = m_DeformationSlider.GetValue();
	m_DeformationNumber.SetValue( val );

	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) 
	{
		if ( (m_nColor>=0) && (m_nColor<=7) ) 
		{
			f32* pMorphValues = pCharacter->GetShapeDeformArray();
			pMorphValues[m_nColor]=val;
		}
	}
}

void CMorphingDlg::OnDeformationNumber()
{
	float val = m_DeformationNumber.GetValue();
	m_DeformationSlider.SetValue( val );

	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) 
	{
		if ( (m_nColor>=0) && (m_nColor<=7) ) 
		{
			f32* pMorphValues = pCharacter->GetShapeDeformArray();
			pMorphValues[m_nColor]=val;
		}
	}
}





void CMorphingDlg::SetDeformationSliderPos()
{
	if (m_pModelViewportCE)
	{
		ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
		if (pCharacter) 
		{
			int test=0;
			test |= m_button[0].GetCheck();
			test |= m_button[1].GetCheck();
			test |= m_button[2].GetCheck();
			test |= m_button[3].GetCheck();
			test |= m_button[4].GetCheck();
			test |= m_button[5].GetCheck();
			test |= m_button[6].GetCheck();
			test |= m_button[7].GetCheck();

			if (test) 
			{
				pCharacter->SetResetMode(0);
				m_pModelViewportCE->m_Button_ROTATE=0;
				m_pModelViewportCE->m_Button_MOVE=0;
				f32* pMorphValues = pCharacter->GetShapeDeformArray();
				f32 blend=pMorphValues[m_nColor];
				m_DeformationSlider.SetValue( blend );
				m_DeformationSlider.EnableWindow(TRUE);
				m_DeformationNumber.EnableWindow(TRUE);
			} 
			else 
			{
				m_DeformationSlider.EnableWindow(FALSE);
				m_DeformationNumber.EnableWindow(FALSE);
			}

		}
	}
}


void CMorphingDlg::OnBnClicked_COLORS(uint32 n)
{
	m_nColor=n;
	m_button[0].SetCheck(0);
	m_button[1].SetCheck(0);
	m_button[2].SetCheck(0);
	m_button[3].SetCheck(0);
	m_button[4].SetCheck(0);
	m_button[5].SetCheck(0);
	m_button[6].SetCheck(0);
	m_button[7].SetCheck(0);

	m_button[n].SetCheck(1);

	SetDeformationSliderPos();
	float val = m_DeformationSlider.GetValue();
	m_DeformationNumber.SetValue( val );

}

void CMorphingDlg::OnBnClicked_COLOR0()
{
	OnBnClicked_COLORS(0);
}

void CMorphingDlg::OnBnClicked_COLOR1()
{
	OnBnClicked_COLORS(1);
}

void CMorphingDlg::OnBnClicked_COLOR2()
{
	OnBnClicked_COLORS(2);
}

void CMorphingDlg::OnBnClicked_COLOR3()
{
	OnBnClicked_COLORS(3);
}

void CMorphingDlg::OnBnClicked_COLOR4()
{
	OnBnClicked_COLORS(4);
}

void CMorphingDlg::OnBnClicked_COLOR5()
{
	OnBnClicked_COLORS(5);
}

void CMorphingDlg::OnBnClicked_COLOR6()
{
	OnBnClicked_COLORS(6);
}

void CMorphingDlg::OnBnClicked_COLOR7()
{
	OnBnClicked_COLORS(7);
}

void CMorphingDlg::OnBnClicked_WIREFRAME()
{
	CCustomButton* pButton=(CCustomButton*)GetDlgItem(IDC_BUTTON_WIREFRAME);
	int check = pButton->GetCheck();
	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawWireframe")->Set( check );
}



f32 CMorphingDlg::GetUniformScalingSlider()
{
	return m_UniformScalingSlider.GetValue();
}

void CMorphingDlg::OnUniformScalingSlider()
{
	f32 val = m_UniformScalingSlider.GetValue();
	m_UniformScalingNumber.SetValue( val );

	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) 
		m_pModelViewportCE->SetUniformScaling(val);
}

void CMorphingDlg::OnUniformScalingNumber()
{
	f32 val = m_UniformScalingNumber.GetValue();
	m_UniformScalingSlider.SetValue( val );

	ICharacterInstance* pCharacter = m_pModelViewportCE->GetCharacterBase();
	if (pCharacter) 
		m_pModelViewportCE->SetUniformScaling(val);
}
