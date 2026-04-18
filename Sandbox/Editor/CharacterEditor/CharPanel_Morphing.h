#pragma once


// CMorphingDlg dialog
#include "Controls\FillSliderCtrl.h"
#include "Controls\NumberCtrl.h"
#include "Controls\ColorButton.h"

class CMorphingDlg : public CDialog
{
	DECLARE_DYNAMIC(CMorphingDlg)

public:

	CMorphingDlg::CMorphingDlg( CWnd* pParent = NULL )	: CDialog(CMorphingDlg::IDD, pParent)	
	{
		m_pModelViewportCE=0;
		m_nColor=9;
	}

	virtual ~CMorphingDlg()	{	}

// Dialog Data
	enum { IDD = IDD_CHARACTER_EDITOR_MORPHING };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	class CModelViewportCE *m_pModelViewportCE;


	CColorCheckBox m_button[8];
	int m_nColor;
	afx_msg void OnBnClicked_COLORS(uint32 n);
	afx_msg void OnBnClicked_COLOR0();
	afx_msg void OnBnClicked_COLOR1();
	afx_msg void OnBnClicked_COLOR2();
	afx_msg void OnBnClicked_COLOR3();
	afx_msg void OnBnClicked_COLOR4();
	afx_msg void OnBnClicked_COLOR5();
	afx_msg void OnBnClicked_COLOR6();
	afx_msg void OnBnClicked_COLOR7();


	void SetDeformationSliderPos();
	f32 GetUniformScalingSlider();
	afx_msg void OnDeformationSlider();
	afx_msg void OnDeformationNumber();
	CFillSliderCtrl m_DeformationSlider;
	CNumberCtrl m_DeformationNumber;

	afx_msg void OnUniformScalingSlider();
	afx_msg void OnUniformScalingNumber();
	CFillSliderCtrl m_UniformScalingSlider;
	CNumberCtrl m_UniformScalingNumber;

	afx_msg void OnBnClicked_WIREFRAME();
};
