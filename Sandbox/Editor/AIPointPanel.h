#pragma once

#include "Controls\PickObjectButton.h"

class CAIPoint;
// CAIPointPanel dialog

class CAIPointPanel : public CDialog
{
	DECLARE_DYNCREATE(CAIPointPanel)

public:
	CAIPointPanel(CWnd* pParent = NULL);   // standard constructor
	virtual ~CAIPointPanel();

	void SetObject( CAIPoint *object );
	void StartPick();
	void StartPickImpass();

// Dialog Data
	enum { IDD = IDD_PANEL_AIPOINT };

protected:
	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnBnClickedRegenLinks();
	afx_msg void OnBnClickedSelect();
	afx_msg void OnBnClickedRemove();
	afx_msg void OnBnClickedRemoveAll();
	afx_msg void OnBnClickedRemoveAllInArea();
	afx_msg void OnLbnDblclkLinks();
	afx_msg void OnLbnLinksSelChange();

	DECLARE_MESSAGE_MAP()

	void ReloadLinks();
	// Called by SPickObjectCallback
	void OnPick( bool impass, CBaseObject *picked );
	bool OnPickFilter( bool impass, CBaseObject *picked );
	void OnCancelPick( bool impass );

	CAIPoint* m_object;
	CColoredListBox m_links;

	CCustomButton m_regenLinksBtn;
	CPickObjectButton m_pickBtn;
	CPickObjectButton m_pickImpassBtn;
	CCustomButton m_selectBtn;
	CCustomButton m_removeBtn;
	int m_type;
  int m_navigationType;
	int	m_removable;

	// Need this to filter the two pick buttons
	struct SPickObjectCallback : public IPickObjectCallback
	{
		// Do this via Init since MS compiler warnins about passing "this" in initialisers
		void Init(CAIPointPanel* owner, bool impass) {m_owner = owner; m_impass = impass;}
		virtual void OnPick( CBaseObject *picked ) {m_owner->OnPick(m_impass, picked);}
		virtual bool OnPickFilter( CBaseObject *picked ) {return m_owner->OnPickFilter(m_impass, picked);}
		virtual void OnCancelPick() {m_owner->OnCancelPick(m_impass);}
	private:
		CAIPointPanel* m_owner;
		bool m_impass;
	};
	SPickObjectCallback m_pickCallback;
	SPickObjectCallback m_pickImpassCallback;

public:
	afx_msg void OnBnClickedWaypoint();
	afx_msg void OnBnClickedHidepoint();
	afx_msg void OnBnClickedHidepointSecondary();
	afx_msg void OnBnClickedEntrypoint();
	afx_msg void OnBnClickedExitpoint();
	afx_msg void OnBnClickedRemovable();
  afx_msg void OnEnChangeAinavlayer();
  afx_msg void OnBnClickedHuman();
  afx_msg void OnBnClicked3dsurface();
};
