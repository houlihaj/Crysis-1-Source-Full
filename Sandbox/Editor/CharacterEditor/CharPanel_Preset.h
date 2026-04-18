#if !defined(AFX_CHARPANEL_PRESET_H__3C441EB6_B02A_4D2F_A5E9_587E331E9E48__INCLUDED_)
#define AFX_CHARPANEL_PRESET_H__3C441EB6_B02A_4D2F_A5E9_587E331E9E48__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Controls\FillSliderCtrl.h"

#define CBA_FILE "/Animations/Animations.cba"

struct SAnimationDefinition;

/////////////////////////////////////////////////////////////////////////////
// CharPanel_Preset dialog
/////////////////////////////////////////////////////////////////////////////
class CharPanel_Preset : public CDialog
{

public:
	CharPanel_Preset( class CModelViewport *vp,CWnd* pParent = NULL);   // standard constructor
	virtual ~CharPanel_Preset();

	enum { IDD = IDD_CHARPANEL_PRESET };

	void FillChildrenNodes(HTREEITEM root, int16 idRoot);
	void InitNodes();
	void EnableNodesError(bool bEnable);
	void UpdateBonePreset();
	void ChangeCurPreset(const char * pName);
	void SaveCBAFile();

	CTreeCtrlEx m_tree_nodes;

protected:

	void UpdateCurBoneRot(float val);
	void UpdateCurBonePos(float val);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog();
	afx_msg void OnTvnSelchangedTreeNodes(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void OnSlider_RotError();
	afx_msg void OnNumber_RotError();

	afx_msg void OnSlider_PosError();
	afx_msg void OnNumber_PosError();

	afx_msg void OnBnClickedSavePreset();

	DECLARE_MESSAGE_MAP()

	CModelViewport *m_ModelViewPort;

	class CAnimationInfoLoader * m_animLoader;
	SAnimationDefinition * m_animDef;

	CFillSliderCtrl m_RotError;
	CNumberCtrl m_RotErrorNumber;

	CFillSliderCtrl m_PosError;
	CNumberCtrl m_PosErrorNumber;
	struct SAnimationDesc * m_pAnimDesc;
	struct BonePreset * m_pCurBone;
	struct CompressionPreset * m_pBonePreset;

	float m_fMinRot;
	float m_fMaxRot;
	float m_fMinPos;
	float m_fMaxPos;
};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CHARPANEL_PRESET_H__3C441EB6_B02A_4D2F_A5E9_587E331E9E48__INCLUDED_)
