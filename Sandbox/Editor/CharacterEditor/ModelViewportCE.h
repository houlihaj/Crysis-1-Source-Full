#pragma once

#include "ModelViewport.h"
#include "ArcBall.h"
#include "RenderHelpers\\AxisHelper.h"
#include "Objects\\baseobject.h"
#include "CGFContent.h"

struct IAttachment;
class CCharacterPropertiesDlg;
class ICharacterChangeListener;


class CModelViewportCE : public CModelViewport
{
	DECLARE_DYNCREATE(CModelViewportCE)

public:

	CModelViewportCE() { 
		InitDisplayContext();
		m_pAttachmentsDlg=0;
		m_pMorphingDlg=0;
		m_pTabSelect=0;
		m_pAnimationControlDlg = 0;
		m_pCharacterPropertiesDlg = 0;
		m_pCharacterChangeListener = 0;
		InitModelViewportCE();	
	}


	void InitModelViewportCE()	
	{
		m_ArcBall.InitArcBall();

		//m_arrVertices.resize(0x4000);
		//m_arrIndices.resize(m_arrVertices.size()*3);

		m_WinRect.left	=0;
		m_WinRect.top		=0;
		m_WinRect.right	=0;
		m_WinRect.bottom=0;

		m_Button_MOVE		= 0;
		m_Button_ROTATE	= 0;
		m_MouseButtonL		= false;
		m_MouseButtonR		= false;
		m_SelectionUpdate	= 0;
		m_MouseOnAttachment=-1;
		m_SelectedAttachment=0;

		//init keyboard
		memset(m_bKey, 0x00, sizeof(m_bKey));
		memset(m_keyrcr, 0x00, sizeof(m_keyrcr));
		memset(m_keyflip, 0x00, sizeof(m_keyflip));
	}

	struct ClosestPoint {
		uint32 basemodel;
		f32 distance;
		uint32 selection; 
		ClosestPoint() {
			basemodel = 0;
			distance	=(3.3E38f-200.0f);
			selection	=-1;
		}
	};

	ClosestPoint cp;

	void OnRender();
	void LoadObject( const CString &fileName, float scale = 1.f );
	void SetCharacter( ICharacterInstance *pInstance );
//	void SetCharacterUIInfo();

	void UpdateAnimationList();
	void UpdateBoneList();

	void Update() {	CModelViewport::Update();	};
	Vec3 SnapToGrid( Vec3 vec ) { return vec; }; // No snapping.

	f32 CharacterPicking( const Ray& mray ); 
	f32 AttachmentPicking( const Ray& mray, IAttachment* pIAttachment, const Matrix34& m34 );

	DECLARE_MESSAGE_MAP()


public:

	CRect m_WinRect;

	class CAttachmentsDlg* m_pAttachmentsDlg;
	class CMorphingDlg* m_pMorphingDlg;
	class CXTTabCtrl* m_pTabSelect;
	class CAnimationControlDlg* m_pAnimationControlDlg;

	TFace* m_pFaces;
	uint32 m_numFaces;
	ExtSkinVertex* m_pSkinVertices;
	uint32 m_numSkinVertices;

	CArcBall3D	m_ArcBall;
	CAxisHelper	m_AxisHelper;
	HitContext m_HitContext;

	enum ViewModeCE
	{
		NothingMode = 0,
		SelectMode,
		MoveMode,
		RotateMode,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
	};
	ViewModeCE m_opMode;

	int32		m_Button_MOVE;
	int32		m_Button_ROTATE;
	int32		m_MouseButtonL;
	int32		m_MouseButtonR;
	int32		m_SelectionUpdate;
	int32   m_MouseOnAttachment;
	int32		m_SelectedAttachment;
//	Mouse		m_Mouse;
	uint8		m_bKey[256];
	uint64	m_keyrcr[256];
	uint8		m_keyflip[256];

	afx_msg void OnLButtonDown( UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(		UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown( UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(		UINT nFlags, CPoint point);

	afx_msg void OnMouseMove(		UINT nFlags, CPoint point); 
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);

	afx_msg void OnAnimPlay();

	CCharacterPropertiesDlg* m_pCharacterPropertiesDlg;

	void SetCharacterChangeListener( ICharacterChangeListener* pListener );

protected:
	void SendOnCharacterChanged();
	ICharacterChangeListener* m_pCharacterChangeListener;
};


