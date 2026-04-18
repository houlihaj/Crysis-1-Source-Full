////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   TimeOfDayDialog.cpp
//  Version:     v1.00
//  Created:     12/3/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TimeOfDayDialog.h"
#include "Controls/SplineCtrl.h"
#include "Controls/ColorGradientCtrl.h"
#include "NumberDlg.h"
#include "Mission.h"
#include "CryEditDoc.h"
#include "IViewPane.h"

#include <ITimer.h>
#include <I3DEngine.h>

#define IDC_TASKPANEL 1
#define IDC_SLIDERSPANEL 2
#define IDC_EXPRESSION_WEIGHT_SLIDER 3

#define IDC_TIME_OF_DAY 5
#define IDC_TIME_OF_DAY_START 6
#define IDC_TIME_OF_DAY_END 7
#define IDC_TIME_OF_DAY_SPEED 8
#define IDC_SKYUPDATE_CHECKBOX 11
#define IDC_PROPERTIES_CTRL 20

#define IDC_SPLINE_CTRL           30
#define IDC_SPLINE_CTRL_TIMELINE  31
#define IDC_COLOR_CTRL            32

#define SLIDER_SCALE 100

#define ID_EXPAND_ALL_SPLINES 1000
#define ID_COLLAPSE_ALL_SPLINES 1001
#define ID_PLAY_TOD_ANIM 1002
#define ID_STOP_TOD_ANIM 1003

#define ID_RESET_TO_DEFAULT 1004


IMPLEMENT_DYNCREATE(CTimeOfDayDialog,CToolbarDialog)

//////////////////////////////////////////////////////////////////////////
class CTimeOfDayViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {85FB1272-D858-4ca5-ABB4-04D484ABF51E}
		static const GUID guid = { 0x85fb1272, 0xd858, 0x4ca5, { 0xab, 0xb4, 0x4, 0xd4, 0x84, 0xab, 0xf5, 0x1e } };
		return guid;
	}
	virtual const char* ClassName() { return "Time Of Day"; };
	virtual const char* Category() { return "Time Of Day"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTimeOfDayDialog); };
	virtual const char* GetPaneTitle() { return _T("Time Of Day"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(100,100,1000,800); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return true; };
};

CTimeOfDayDialog* CTimeOfDayDialog::m_pInstance = 0;

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CTimeOfDayViewClass );
}

//////////////////////////////////////////////////////////////////////////
class CSplineCtrlContainerTOD : public CSplineCtrlEx
{
public:
	virtual void PostNcDestroy() { delete this; };
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTimeOfDayDialog, CToolbarDialog)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
	//ON_NOTIFY_RANGE( NM_RCLICK,FIRST_SLIDER_ID,FIRST_SLIDER_ID+1000,OnSplineRClick )
	ON_NOTIFY_RANGE( SPLN_BEFORE_CHANGE,IDC_SPLINE_CTRL,IDC_SPLINE_CTRL,OnBeforeSplineChange )
	ON_NOTIFY_RANGE( SPLN_CHANGE,IDC_SPLINE_CTRL,IDC_SPLINE_CTRL,OnSplineChange )
	ON_NOTIFY_RANGE( CLRGRDN_BEFORE_CHANGE,IDC_COLOR_CTRL,IDC_COLOR_CTRL,OnBeforeSplineChange )
	ON_NOTIFY_RANGE( CLRGRDN_CHANGE,IDC_COLOR_CTRL,IDC_COLOR_CTRL,OnSplineChange )

	ON_COMMAND(ID_FILE_IMPORT, OnImport)
	ON_COMMAND(ID_FILE_EXPORT, OnExport)
	ON_COMMAND(ID_PLAY,OnPlayAnim)
	ON_UPDATE_COMMAND_UI(ID_PLAY,OnUpdatePlayAnim)
	ON_COMMAND(ID_RECORD,OnRecord)
	ON_UPDATE_COMMAND_UI(ID_RECORD,OnUpdateRecord)
	ON_COMMAND(ID_FACEIT_PLAY_FROM_0,OnPlayAnimFrom0)
	ON_UPDATE_COMMAND_UI(ID_FACEIT_PLAY_FROM_0,OnUpdatePlayAnimFrom0)
	ON_COMMAND(ID_FACEIT_GOTO_MINUS1,OnGotoMinus1)
	ON_COMMAND(ID_FACEIT_GOTO_0,OnGoto0)
	ON_COMMAND(ID_FACEIT_GOTO_1,OnGoto1)
	ON_COMMAND(ID_HOLD,OnHold)
	ON_COMMAND(ID_FETCH,OnFetch)
	ON_COMMAND(ID_UNDO,OnUndo)
	ON_COMMAND(ID_REDO,OnRedo)
	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
	ON_CONTROL_RANGE(TIMEOFDAYN_CHANGE, IDC_TIME_OF_DAY,IDC_TIME_OF_DAY_END, OnChangeCurrentTime)
	ON_EN_CHANGE(IDC_TIME_OF_DAY_SPEED,OnChangeTimeAnimSpeed)
	ON_NOTIFY(PROPERTYCTRL_ONSELECT,IDC_PROPERTIES_CTRL,OnPropertySelected)

	ON_NOTIFY(SPLN_SCROLL_ZOOM,IDC_SPLINE_CTRL,OnSplineCtrlScrollZoom)
	ON_NOTIFY(TLN_CHANGE,IDC_SPLINE_CTRL_TIMELINE,OnTimelineCtrlChange)
END_MESSAGE_MAP()

/** Undo object stored when track is modified.
*/
class CUndoTimeOfDayObject : public IUndoObject
{
public:
	CUndoTimeOfDayObject()
	{
		m_undo = CreateXmlNode("Undo");
		m_redo = CreateXmlNode("Redo");
		GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( m_undo,false );
	}
protected:
	virtual int GetSize() { return sizeof(*this); }
	virtual const char* GetDescription() { return "Time Of Day"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( m_redo,false );
		}
		GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( m_undo,true );
		CTimeOfDayDialog::UpdateValues();
	}
	virtual void Redo()
	{
		GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( m_redo,true );
		CTimeOfDayDialog::UpdateValues();
	}

private:
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
};

//////////////////////////////////////////////////////////////////////////
// Adapter for Multi element interpolators that allow to split it to several 
// different interpolators for each element separately.
//////////////////////////////////////////////////////////////////////////
class CMultiElementSplineInterpolatorAdapter : public ISplineInterpolator
{
public:
	CMultiElementSplineInterpolatorAdapter() : m_pInterpolator(0),m_element(0) {}
	CMultiElementSplineInterpolatorAdapter( ISplineInterpolator *pSpline,int element )
	{
		m_pInterpolator = pSpline;
		m_element = element;
	}
	// Dimension of the spline from 0 to 3, number of parameters used in ValueType.
	virtual int GetNumDimensions() { return m_pInterpolator->GetNumDimensions(); }
	virtual ESplineType GetSplineType() { return m_pInterpolator->GetSplineType(); }
	virtual int  InsertKey( float time,ValueType value )
	{
		ValueType v = {0,0,0,0};
		v[m_element] = value[0];
		return m_pInterpolator->InsertKey(time,v);
	};
	virtual void RemoveKey( int key ) { m_pInterpolator->RemoveKey(key); };

	virtual void FindKeysInRange(float startTime, float endTime, int& firstFoundKey, int& numFoundKeys)
		{ m_pInterpolator->FindKeysInRange(startTime,endTime,firstFoundKey,numFoundKeys); }
	virtual void RemoveKeysInRange(float startTime, float endTime) { m_pInterpolator->RemoveKeysInRange(startTime,endTime); }

	virtual int   GetKeyCount() { return m_pInterpolator->GetKeyCount(); }
	virtual void  SetKeyTime( int key,float time ) { return m_pInterpolator->SetKeyTime(key,time); };
	virtual float GetKeyTime( int key ) { return m_pInterpolator->GetKeyTime(key); }
	
	virtual void  SetKeyValue( int key,ValueType value )
	{
		ValueType v = {0,0,0,0};
		m_pInterpolator->GetKeyValue(key,v);
		v[m_element] = value[0];
		m_pInterpolator->SetKeyValue(key,v);
	}
	virtual bool  GetKeyValue( int key,ValueType &value )
	{
		ValueType v = {0,0,0,0};
		v[m_element] = value[0];
		return m_pInterpolator->GetKeyValue(key,value);
		value[0] = v[m_element];
	}

	virtual void  SetKeyTangents( int key,ValueType tin,ValueType tout ) {};
	virtual bool  GetKeyTangents( int key,ValueType &tin,ValueType &tout ) { return false; };

	// Changes key flags, @see ESplineKeyFlags
	virtual void  SetKeyFlags( int key,int flags ) { return m_pInterpolator->SetKeyFlags(key,flags); };
	// Retrieve key flags, @see ESplineKeyFlags
	virtual int   GetKeyFlags( int key ) { return m_pInterpolator->GetKeyFlags(key); }

	virtual void Interpolate( float time,ValueType &value )
	{
		m_pInterpolator->Interpolate(time,value);
		value[0] = value[m_element];
	};

	virtual void SerializeSpline( XmlNodeRef &node,bool bLoading ) { return m_pInterpolator->SerializeSpline(node,bLoading); };

	virtual ISplineBackup* Backup() { return m_pInterpolator->Backup(); };
	virtual void Restore(ISplineBackup* pBackup) { return m_pInterpolator->Restore(pBackup); };

public:
	ISplineInterpolator *m_pInterpolator;
	int m_element;
};

//////////////////////////////////////////////////////////////////////////
class CTimeOfDaySplineSet : public ISplineSet
{
public:
	void AddSpline( ISplineInterpolator *pSpline ) { m_splines.push_back(pSpline); };
	void RemoveAllSplines() { m_splines.clear(); }

	virtual ISplineInterpolator* GetSplineFromID(const string& id) {
		int i = atoi(id.c_str());
		if (i >= 0 && i < (int)m_splines.size())
			return m_splines[i] ;
		return 0;
	};
	virtual string GetIDFromSpline(ISplineInterpolator* pSpline)
	{
		for (int i = 0; i < (int)m_splines.size(); i++)
		{
			if (m_splines[i] == pSpline)
			{
				string s;
				s.Format("%d",i);
				return s;
			}
		}
		return "";
	}
	virtual int GetSplineCount() const { return (int)m_splines.size(); }
	virtual int GetKeyCountAtTime(float time, float threshold) const
	{
		int count = 0;
		for (int i = 0; i < (int)m_splines.size(); i++)
		{
			if (m_splines[i]->FindKey(time,threshold) > 0)
				count++;
		}
		return count;
	}

public:
	std::vector<ISplineInterpolator*> m_splines;
};

//////////////////////////////////////////////////////////////////////////
CTimeOfDayDialog::CTimeOfDayDialog()
: CToolbarDialog(IDD,NULL)
{
	m_pInstance = this;
	m_bAnimation = false;
	m_bPlayFrom0 = false;
	m_bIgnoreScroll = false;
	m_splineCtrlHeight = 400;
	m_bRecording = true;
	m_pSplineCtrl = 0;
	m_pColorGradientCtrl = 0;

	GetIEditor()->RegisterNotifyListener( this );
	Create( CTimeOfDayDialog::IDD,AfxGetMainWnd() );
}

//////////////////////////////////////////////////////////////////////////
CTimeOfDayDialog::~CTimeOfDayDialog()
{
	m_pInstance = 0;
	GetIEditor()->UnregisterNotifyListener( this );
	ClearCtrls();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnClose()
{
	//SavePlacement( "Dialogs\\TimeOfDay" );
	DestroyWindow();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType,cx,cy);

	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::UpdateValues()
{
	if (!m_pInstance)
		return;

	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	ITimeOfDay::SAdvancedInfo advInfo;
	pTimeOfDay->GetAdvancedInfo( advInfo );

	m_pInstance->SetTimeRange( advInfo.fStartTime,advInfo.fEndTime,advInfo.fAnimSpeed );
	m_pInstance->SetTime( m_pInstance->GetTime(),true,true );
	m_pInstance->RedrawWindow( NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::RecalcLayout()
{
	if (m_wndTaskPanel.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);

		DWORD dwMode = LM_HORZ|LM_HORZDOCK|LM_STRETCH|LM_COMMIT;
		CSize sz = m_wndToolBar.CalcDockingLayout(32000, dwMode);

		CRect rcToolbar(rc);
		rcToolbar.bottom = sz.cy;
		m_wndToolBar.MoveWindow(rcToolbar);
		rc.top = rcToolbar.bottom+1;

		rc.right -= 300;

		CRect rcTask(rc);
		rcTask.right = 180;
		rc.left = rcTask.right+1;
		m_wndTaskPanel.MoveWindow(rcTask);

		CRect rcsldr(rc);
		int sh = 20;
		m_weightSlider.MoveWindow( CRect(rcsldr.left,rcsldr.top+10,rcsldr.right,rcsldr.top+10+sh) );
		m_textMinus100.MoveWindow( CRect(rcsldr.left+10,rcsldr.top+32,rcsldr.left+100,rcsldr.top+45) );
		m_textPlus100.MoveWindow( CRect(rcsldr.right-100,rcsldr.top+32,rcsldr.right-10,rcsldr.top+45) );
		m_text0.MoveWindow( CRect((rcsldr.right+rcsldr.left)/2-4,rcsldr.top+32,(rcsldr.right+rcsldr.left)/2+40,rcsldr.top+45) );

		//m_weightSlider.MoveWindow( CRect(0,0,0,0) );
		//m_textMinus100.MoveWindow( CRect(0,0,0,0) );
		//m_textPlus100.MoveWindow( CRect(0,0,0,0) );
		//m_text0.MoveWindow( CRect(0,0,0,0) );

		//m_timelineCtrl.MoveWindow( CRect(rcsldr.left,rcsldr.top+10,rcsldr.right,rcsldr.top+10+sh) );
		//m_timelineCtrl.SetZoom( (rcsldr.right-rcsldr.left)/24.01f );

		rc.top += 50;

		CRect rcColorRc(rc);
		rcColorRc.bottom = rcColorRc.top+40;
		rcColorRc.left += 40;
		if (m_pColorGradientCtrl)
			m_pColorGradientCtrl->MoveWindow(rcColorRc);

		CRect rcSplineRc(rc);
		rcSplineRc.top += 40;
		if (m_pSplineCtrl)
			m_pSplineCtrl->MoveWindow(rcSplineRc);

		GetClientRect(rc);
		rc.top = rcToolbar.bottom+1;
		rc.left = rc.right - 299;
		m_propsCtrl.MoveWindow(rc);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CTimeOfDayDialog::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	CRect rc;
	GetClientRect(rc);

	//////////////////////////////////////////////////////////////////////////
	VERIFY(m_wndToolBar.CreateToolBar(WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS|CBRS_ALIGN_TOP, this, AFX_IDW_TOOLBAR));
	VERIFY(m_wndToolBar.LoadToolBar(IDR_TIME_OF_DAY_BAR));
	m_wndToolBar.SetFlags(xtpFlagAlignTop|xtpFlagStretched);
	m_wndToolBar.EnableCustomization(FALSE);

	//VERIFY(m_wndToolBar.Create(this,WS_VISIBLE|WS_CHILD|CBRS_TOOLTIPS,AFX_IDW_TOOLBAR));
	//VERIFY(m_wndToolBar.LoadToolBar(IDR_AVI_RECORDER_BAR));
	//m_wndToolBar.SetFlags(xtpFlagStretched);

	//m_timelineCtrl.Create( WS_CHILD|WS_VISIBLE,CRect(0,0,1,1),this,IDC_TIME_CTRL );
	//m_timelineCtrl.SetTimeRange( Range(0,24) );

	m_timelineCtrl2.Create( WS_CHILD|WS_VISIBLE,CRect(0,0,1,1),this,IDC_SPLINE_CTRL_TIMELINE );
	m_timelineCtrl2.SetTimeRange( Range(0,1) );
	m_timelineCtrl2.SetTicksTextScale( 24.0f );

	m_weightSlider.Create( WS_CHILD|WS_VISIBLE|TBS_BOTH|TBS_AUTOTICKS|TBS_NOTICKS,CRect(0,0,1,1),this,IDC_EXPRESSION_WEIGHT_SLIDER );
	m_weightSlider.SetRange(0,24*SLIDER_SCALE);
	m_weightSlider.SetTicFreq( 1*SLIDER_SCALE );

	m_textMinus100.Create( "0",WS_CHILD|WS_VISIBLE,rc,this,IDC_STATIC );
	m_textMinus100.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	m_textPlus100.Create( "24",WS_CHILD|WS_VISIBLE|SS_RIGHT,rc,this,IDC_STATIC );
	m_textPlus100.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	m_text0.Create( "12",WS_CHILD|WS_VISIBLE,rc,this,IDC_STATIC );
	m_text0.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );

	{
		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		// Task panel.
		//////////////////////////////////////////////////////////////////////////
		m_wndTaskPanel.Create( WS_CHILD|WS_BORDER|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,IDC_TASKPANEL );

		//m_taskImageList.Create( IDR_DB_GAMETOKENS_BAR,16,1,RGB(192,192,192) );
		//VERIFY( CMFCUtils::LoadTrueColorImageList( m_taskImageList,IDR_DB_GAMETOKENS_BAR,15,RGB(192,192,192) ) );
		//m_wndTaskPanel.SetImageList( &m_taskImageList );
		m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
		m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
		m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
		m_wndTaskPanel.AllowDrag(TRUE);
		m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);

		// Add default tasks.
		CXTPTaskPanelGroupItem *pItem =  NULL;
		CXTPTaskPanelGroup *pGroup = NULL;
		{
			pGroup = m_wndTaskPanel.AddGroup(1);
			pGroup->SetCaption( "Time of Day Tasks" );

			pItem =  pGroup->AddLinkItem(ID_FILE_IMPORT); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Import From File" );
			pItem =  pGroup->AddLinkItem(ID_FILE_EXPORT); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Export To File" );

			pItem =  pGroup->AddLinkItem(ID_RESET_TO_DEFAULT); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Reset Values" );

			pItem =  pGroup->AddLinkItem(ID_EXPAND_ALL_SPLINES); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Expand All" );
			pItem =  pGroup->AddLinkItem(ID_COLLAPSE_ALL_SPLINES); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Collapse All" );
		}

		m_timeEdit.Create( WS_CHILD|WS_VISIBLE|ES_CENTER,CRect(0,0,200,16),this,IDC_TIME_OF_DAY );
		m_timeEdit.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
		m_timeEdit.SetParent(&m_wndTaskPanel);
		m_timeEdit.SetOwner(this);

		m_timeEditStart.Create( WS_CHILD|WS_VISIBLE|ES_CENTER,CRect(0,0,200,16),this,IDC_TIME_OF_DAY_START );
		m_timeEditStart.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
		m_timeEditStart.SetParent(&m_wndTaskPanel);
		m_timeEditStart.SetOwner(this);

		m_timeEditEnd.Create( WS_CHILD|WS_VISIBLE|ES_CENTER,CRect(0,0,200,16),this,IDC_TIME_OF_DAY_END );
		m_timeEditEnd.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
		m_timeEditEnd.SetParent(&m_wndTaskPanel);
		m_timeEditEnd.SetOwner(this);

		m_timeAnimSpeedCtrl.Create( this,CRect(0,0,200,18),IDC_TIME_OF_DAY_SPEED,CNumberCtrl::NOBORDER|CNumberCtrl::CENTER_ALIGN );
		m_timeAnimSpeedCtrl.SetParent(&m_wndTaskPanel);
		m_timeAnimSpeedCtrl.SetOwner(this);
		{
			pGroup = m_wndTaskPanel.AddGroup(2);
			pGroup->SetCaption( "Current Time" );

			pItem =  pGroup->AddControlItem(m_timeEdit);
			pItem->SetCaption( "Time: " );

			pItem =  pGroup->AddTextItem(_T("Start Time:"));
			pItem =  pGroup->AddControlItem(m_timeEditStart);
			pItem =  pGroup->AddTextItem(_T("End Time:"));
			pItem =  pGroup->AddControlItem(m_timeEditEnd);
			pItem =  pGroup->AddTextItem(_T("Play Speed:"));
			pItem =  pGroup->AddControlItem(m_timeAnimSpeedCtrl);
		}

		m_forceUpdateCheck.Create( "Force sky update",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,CRect(0,0,60,16),&m_wndTaskPanel,IDC_SKYUPDATE_CHECKBOX );
		m_forceUpdateCheck.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
		{
			pGroup = m_wndTaskPanel.AddGroup(3);
			pGroup->SetCaption( "Update Tasks" );

			pItem =  pGroup->AddLinkItem(ID_PLAY_TOD_ANIM); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Play" );
			pItem =  pGroup->AddLinkItem(ID_STOP_TOD_ANIM); pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( "Stop" );
			pItem =  pGroup->AddControlItem(m_forceUpdateCheck);
		}

		//////////////////////////////////////////////////////////////////////////
	}

	m_propsCtrl.Create( WS_CHILD|WS_VISIBLE,CRect(0,0,100,100),this,IDC_PROPERTIES_CTRL );
	m_propsCtrl.ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	m_propsCtrl.SetUpdateCallback( functor(*this,&CTimeOfDayDialog::OnUpdateProperties) );

	CreateProperties();


	RecalcLayout();
	ReloadCtrls();

	UpdateValues();
	SetTime( GetTime(),true );

	//AutoLoadPlacement( "Dialogs\\TimeOfDay" );

	return res;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::CreateProperties()
{
	m_pVars = new CVarBlock;

	std::map<CString,IVariable*> groups;

	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	for (int i = 0; i < pTimeOfDay->GetVariableCount(); i++)
	{
		ITimeOfDay::SVariableInfo varInfo;
		if (!pTimeOfDay->GetVariableInfo( i,varInfo ))
			continue;

		{
			if (!varInfo.pInterpolator)
				continue;

			IVariable *pGroupVar = stl::find_in_map(groups,varInfo.group,0);
			if (!pGroupVar)
			{
				// Create new group
				pGroupVar = new CVariableArray;
				pGroupVar->SetName( varInfo.group );
				m_pVars->AddVariable( pGroupVar );
				groups[varInfo.group] = pGroupVar;
			}

			IVariable *pVar = 0;
			if (varInfo.type == ITimeOfDay::TYPE_COLOR)
			{
				//////////////////////////////////////////////////////////////////////////
				// Add Var.
				pVar = new CVariable<Vec3>;
				pVar->SetDataType( IVariable::DT_COLOR );
				pVar->Set( Vec3(varInfo.fValue[0],varInfo.fValue[1],varInfo.fValue[2]) );
			}
			else if (varInfo.type == ITimeOfDay::TYPE_FLOAT)
			{
				// Add Var.
				pVar = new CVariable<float>;
				pVar->Set( varInfo.fValue[0] );
				pVar->SetLimits( varInfo.fValue[1],varInfo.fValue[2] );
			}
			if (pVar)
			{
				pVar->SetName( varInfo.name );
				pVar->SetHumanName( varInfo.displayName );
				pGroupVar->AddChildVar( pVar );
			}
		}
	}

	m_propsCtrl.AddVarBlock( m_pVars );
	m_propsCtrl.ExpandAll();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::ClearCtrls()
{
	m_controllers.clear();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::ReloadCtrls()
{
	ClearCtrls();
		
	if (!m_pSplineCtrl)
	{
		m_pSplineCtrl = new CSplineCtrlContainerTOD;
		m_pSplineCtrl->Create( WS_CHILD|WS_VISIBLE|WS_BORDER|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,CRect(0,0,100,400),this,IDC_SPLINE_CTRL );
		m_pSplineCtrl->ModifyStyleEx( 0,WS_EX_CLIENTEDGE );

		//m_pSplineCtrl->SetGrid( 24,10 );

		m_pSplineCtrl->SetZoom( Vec2(50.0f,100.0f) );
		m_pSplineCtrl->SetTimeRange( Range(0,1.0f) );
		m_pSplineCtrl->SetValueRange( Range(-1.0f,1.0f) );

		m_pSplineCtrl->SetTimelineCtrl(&m_timelineCtrl2);
		

		//m_pSplineCtrl->SetTimeRange( 0,1.0f );
		//m_pSplineCtrl->LockFirstAndLastKeys(true);

		//m_pSplineCtrl->SetValueRange( 0,1 );
		m_pSplineCtrl->SetTooltipValueScale(24,1);
	}

	if (!m_pColorGradientCtrl )
	{
		CRect sliderRc(0,0,1,40);
		CColorGradientCtrl *pColorCtrl = new CColorGradientCtrl;
		pColorCtrl->Create( WS_CHILD|WS_VISIBLE|WS_BORDER,sliderRc,this,IDC_COLOR_CTRL );
		pColorCtrl->ModifyStyleEx( 0,WS_EX_CLIENTEDGE );

		pColorCtrl->SetNoZoom(false);
		pColorCtrl->SetTimeRange( 0,1.0f );
		pColorCtrl->LockFirstAndLastKeys(true);
		pColorCtrl->SetTooltipValueScale(24,1);

		m_pColorGradientCtrl = pColorCtrl;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnHScroll( UINT nSBCode,UINT nPos,CScrollBar* pScrollBar )
{
	if (m_bIgnoreScroll)
		return;
	if (pScrollBar == (CScrollBar*)&m_weightSlider)
	{
		float fWeight = (float)m_weightSlider.GetPos()/SLIDER_SCALE;
		m_bIgnoreScroll = true;
		SetTime( fWeight );
		m_bIgnoreScroll = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnSplineRClick( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult )
{
	
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnBeforeSplineChange( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult )
{
	if (CUndo::IsRecording())
		CUndo::Record( new CUndoTimeOfDayObject );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnSplineChange( UINT nID, NMHDR* pNMHDR, LRESULT* lpResult )
{
	RefreshPropertiesValues();

	CWnd *pWnd = CWnd::FromHandle(pNMHDR->hwndFrom);
	if (pWnd->IsKindOf(RUNTIME_CLASS(CSplineCtrlEx)))
	{
		CSplineCtrlEx *pSlineCtrl = (CSplineCtrlEx*)pWnd;

		// Spline changes
		if (m_pColorGradientCtrl)
			m_pColorGradientCtrl->Invalidate();
	}
	else
	{
		// Color control changes.
		if (m_pSplineCtrl)
			m_pSplineCtrl->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
float CTimeOfDayDialog::GetTime() const
{
	float fTime = 0;
	if (GetIEditor()->GetDocument()->GetCurrentMission())
		fTime = GetIEditor()->GetDocument()->GetCurrentMission()->GetTime();
	return fTime;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::SetTimeRange( float fTimeStart,float fTimeEnd,float fSpeed )
{
	{
		int nHour = floor(fTimeStart);
		int nMins = (fTimeStart - floor(fTimeStart)) * 60.0f;
		m_timeEditStart.SetTime( nHour,nMins );
	}
	{
		int nHour = floor(fTimeEnd);
		int nMins = (fTimeEnd - floor(fTimeEnd)) * 60.0f;
		m_timeEditEnd.SetTime( nHour,nMins );
	}
	m_timeAnimSpeedCtrl.SetValue( fSpeed );

	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	ITimeOfDay::SAdvancedInfo advInfo;
	pTimeOfDay->GetAdvancedInfo( advInfo );
	advInfo.fStartTime = fTimeStart;
	advInfo.fEndTime = fTimeEnd;
	advInfo.fAnimSpeed = fSpeed;
	pTimeOfDay->SetAdvancedInfo( advInfo );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::RefreshPropertiesValues( bool bSetTime )
{
	m_propsCtrl.EnableUpdateCallback(false);

	// Interpolate internal values
	if (bSetTime)
		gEnv->p3DEngine->GetTimeOfDay()->SetTime( GetTime(),false );

	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	for (int i = 0,numVars = pTimeOfDay->GetVariableCount(); i < numVars; i++)
	{
		ITimeOfDay::SVariableInfo varInfo;
		if (!pTimeOfDay->GetVariableInfo( i,varInfo ))
			continue;
		IVariable *pVar = m_pVars->FindVariable(varInfo.name);
		if (!pVar)
			continue;
		switch (varInfo.type)
		{
		case ITimeOfDay::TYPE_FLOAT:
			pVar->Set( varInfo.fValue[0] );
			break;
		case ITimeOfDay::TYPE_COLOR:
			pVar->Set( Vec3(varInfo.fValue[0],varInfo.fValue[1],varInfo.fValue[2]) );
			break;
		}
	}
	m_propsCtrl.EnableUpdateCallback(true);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::SetTime( float fHour,bool bOnlyUpdateUI,bool bNoPropertiesUpdate )
{
	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();

	bool bForceUpdate = m_forceUpdateCheck.GetCheck() == BST_CHECKED;

	bool bTimeWasSet = false;
	if (!bOnlyUpdateUI)
	{
		pTimeOfDay->SetTime( fHour,bForceUpdate );
		bTimeWasSet = true;
	}

	if (GetIEditor()->GetDocument()->GetCurrentMission())
		GetIEditor()->GetDocument()->GetCurrentMission()->SetTime( fHour );

	int p = fHour*SLIDER_SCALE;
	if (!m_bIgnoreScroll)
	{
		m_bIgnoreScroll = true;
		m_weightSlider.SetPos( p );
		m_bIgnoreScroll = false;
	}
	if (p < 0)
		m_weightSlider.SetSelection(p,0);
	else if (p > 0)
		m_weightSlider.SetSelection(0,p);

	//if (m_timelineCtrl2)
		//m_timelineCtrl2.SetTimeMarker(fHour/24.0f);

	int nHour = floor(fHour);
	int nMins = (fHour - floor(fHour)) * 60.0f;
	m_timeEdit.SetTime( nHour,nMins );

	if (m_pSplineCtrl)
		m_pSplineCtrl->SetTimeMarker(fHour/24.0f);
	if (m_pColorGradientCtrl)
		m_pColorGradientCtrl->SetTimeMarker(fHour/24.0f);

	if (!bNoPropertiesUpdate)
	{
		RefreshPropertiesValues(!bTimeWasSet);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnPlayAnim()
{
	m_bAnimation = !m_bAnimation;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnUpdatePlayAnim( CCmdUI* pCmdUI )
{
	pCmdUI->SetCheck( (m_bAnimation)?BST_CHECKED:BST_UNCHECKED );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnRecord()
{
	m_bRecording = !m_bRecording;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnUpdateRecord( CCmdUI* pCmdUI )
{
	pCmdUI->SetCheck( (m_bRecording)?BST_CHECKED:BST_UNCHECKED );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnPlayAnimFrom0()
{
	m_bPlayFrom0 = !m_bPlayFrom0;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnUpdatePlayAnimFrom0( CCmdUI* pCmdUI )
{
	pCmdUI->SetCheck(m_bPlayFrom0);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnGotoMinus1()
{
	SetTime(0);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnGoto0()
{
	SetTime(12);
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnGoto1()
{
	SetTime(24);
}


//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
		case eNotify_OnEndSceneOpen:
		case eNotify_OnEndNewScene:
			{
				ReloadCtrls();
				UpdateValues();
			}
		break;

		case eNotify_OnIdleUpdate:
			if (m_bAnimation)
			{
				ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
				float fHour = pTimeOfDay->GetTime();

				ITimeOfDay::SAdvancedInfo advInfo;
				pTimeOfDay->GetAdvancedInfo( advInfo );

				float dt = gEnv->pTimer->GetFrameTime();
				float fTime = fHour + dt*advInfo.fAnimSpeed;
				if (fTime > advInfo.fEndTime)
					fTime = advInfo.fStartTime;
				if (fTime < advInfo.fStartTime)
					fTime = advInfo.fEndTime;
				if (m_bPlayFrom0 && fTime < 0)
					fTime = 0;
				SetTime( fTime );
			}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnTimelineCtrlChange( NMHDR* pNMHDR, LRESULT* pResult )
{
	float fTime = m_timelineCtrl2.GetTimeMarker();
	SetTime( fTime*24.0f );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnChangeCurrentTime( UINT nID )
{
	switch (nID)
	{
	case IDC_TIME_OF_DAY:
		{
			CString str;
			m_timeEdit.GetWindowText(str);
			int h,m;
			sscanf( str,_T("%02d:%02d"),&h,&m );
			float fTime = (float)h + (float)m/60.0f;
			SetTime( fTime );
		}
		break;
	case IDC_TIME_OF_DAY_START:
		{
			CString str;
			m_timeEditStart.GetWindowText(str);
			int h,m;
			sscanf( str,_T("%02d:%02d"),&h,&m );
			float fTime = (float)h + (float)m/60.0f;
			ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
			ITimeOfDay::SAdvancedInfo advInfo;
			pTimeOfDay->GetAdvancedInfo( advInfo );
			advInfo.fStartTime = fTime;
			pTimeOfDay->SetAdvancedInfo( advInfo );
		}
		break;
	case IDC_TIME_OF_DAY_END:
		{
			CString str;
			m_timeEditEnd.GetWindowText(str);
			int h,m;
			sscanf( str,_T("%02d:%02d"),&h,&m );
			float fTime = (float)h + (float)m/60.0f;
			ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
			ITimeOfDay::SAdvancedInfo advInfo;
			pTimeOfDay->GetAdvancedInfo( advInfo );
			advInfo.fEndTime = fTime;
			pTimeOfDay->SetAdvancedInfo( advInfo );
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnChangeTimeAnimSpeed()
{
	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	ITimeOfDay::SAdvancedInfo advInfo;
	pTimeOfDay->GetAdvancedInfo( advInfo );
	advInfo.fAnimSpeed = m_timeAnimSpeedCtrl.GetValue();
	pTimeOfDay->SetAdvancedInfo( advInfo );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnSplineCtrlScrollZoom( NMHDR* pNMHDR, LRESULT* pResult )
{
	if (m_pSplineCtrl && m_pColorGradientCtrl)
	{
		m_pColorGradientCtrl->SetZoom( m_pSplineCtrl->GetZoom().x );
		m_pColorGradientCtrl->SetOrigin( m_pSplineCtrl->GetScrollOffset().x );
		m_pColorGradientCtrl->RedrawWindow();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnImport()
{
	char szFilters[] = "Time Of Day Settings (*.tod)|*.tod||";
	CString fileName;

	if (CFileUtil::SelectFile( szFilters,GetIEditor()->GetLevelFolder(),fileName ))
	{
		XmlNodeRef root = GetISystem()->LoadXmlFile( fileName );
		if (root)
		{
			ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
			float fTime = GetTime();
			pTimeOfDay->Serialize( root,true );
			pTimeOfDay->SetTime( fTime,true );

			UpdateValues();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnExport()
{
	char szFilters[] = "Time Of Day Settings (*.tod)|*.tod||";
	CString fileName;
	if (CFileUtil::SelectSaveFile( szFilters,"tod",GetIEditor()->GetLevelFolder(),fileName ))
	{
		// Write the light settings into the archive
		XmlNodeRef node = CreateXmlNode( "TimeOfDay" );
		ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
		pTimeOfDay->Serialize( node,false );
		SaveXmlNode( node,fileName );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnExpandAll()
{
	m_propsCtrl.ExpandAll();
}
//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnResetToDefaultValues()
{
	if (!m_pInstance)
		return;

	if (MessageBox("Are you sure you want to reset all values to their default values?","Reset values",MB_YESNO)==IDYES)
	{
		ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
		pTimeOfDay->ResetVariables();
		RefreshPropertiesValues(false);
	}
}
//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnCollapseAll()
{
	// Must implement.
}

//////////////////////////////////////////////////////////////////////////
LRESULT CTimeOfDayDialog::OnTaskPanelNotify( WPARAM wParam, LPARAM lParam )
{
	switch(wParam) {
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			switch (nCmdID)
			{
				case ID_PLAY_TOD_ANIM:
					m_bAnimation = true;
					break;
				case ID_STOP_TOD_ANIM:
					m_bAnimation = false;
					break;
				case ID_FILE_IMPORT:
					OnImport();
					break;
				case ID_FILE_EXPORT:
					OnExport();
					break;

				case ID_RESET_TO_DEFAULT:
					OnResetToDefaultValues();
				break;

				case ID_EXPAND_ALL_SPLINES:
					OnExpandAll();
					break;
				case ID_COLLAPSE_ALL_SPLINES:
					OnCollapseAll();
					break;
			}
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnUpdateProperties( IVariable *pVar )
{
	ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();

	int nIndex = 0;
	int numVars = pTimeOfDay->GetVariableCount();

	ITimeOfDay::SVariableInfo varInfo;
	for (; nIndex < numVars; nIndex++)
	{
		if (!pTimeOfDay->GetVariableInfo( nIndex,varInfo ))
			continue;
		if (strcmp(varInfo.name,pVar->GetName()) == 0)
		{
			break;
		}
	}
	if (nIndex == numVars)
		return;

	float fTime = GetTime();
	float fSplineTime = fTime / 24;

	int nKey = varInfo.pInterpolator->FindKey(fSplineTime);
	int nLastKey = varInfo.pInterpolator->GetKeyCount()-1;

	if (CUndo::IsRecording())
		CUndo::Record( new CUndoTimeOfDayObject );

	switch (varInfo.type)
	{
	case ITimeOfDay::TYPE_FLOAT:
		{
			float fVal = 0;
			pVar->Get(fVal);
			if (m_bRecording)
			{
				if (nKey < 0)
					varInfo.pInterpolator->InsertKeyFloat(fSplineTime,fVal);
				else
				{
					varInfo.pInterpolator->SetKeyValueFloat(nKey,fVal);
					if (nKey == 0)
						varInfo.pInterpolator->SetKeyValueFloat(nLastKey,fVal);
					else if (nKey == nLastKey)
						varInfo.pInterpolator->SetKeyValueFloat(0,fVal);
				}
			}

			float v3[3] = {fVal,varInfo.fValue[1],varInfo.fValue[2]};
			pTimeOfDay->SetVariableValue( nIndex,v3 );
		}
		break;
	case ITimeOfDay::TYPE_COLOR:
		{
			Vec3 vVal;
			pVar->Get(vVal);
			float v3[3] = {vVal.x,vVal.y,vVal.z};
			if (m_bRecording)
			{
				if (nKey < 0)
					varInfo.pInterpolator->InsertKeyFloat3(fSplineTime,v3);
				else
				{
					varInfo.pInterpolator->SetKeyValueFloat3(nKey,v3);
					if (nKey == 0)
						varInfo.pInterpolator->SetKeyValueFloat3(nLastKey,v3);
					else if (nKey == nLastKey)
						varInfo.pInterpolator->SetKeyValueFloat3(0,v3);
				}
			}
			pTimeOfDay->SetVariableValue( nIndex,v3 );
			if (m_pColorGradientCtrl)
				m_pColorGradientCtrl->Invalidate();
		}
		break;
	}

	if (m_pSplineCtrl)
		m_pSplineCtrl->Invalidate();

	bool bForceUpdate = m_forceUpdateCheck.GetCheck() == BST_CHECKED;
	pTimeOfDay->Update( false,bForceUpdate );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnPropertySelected( NMHDR* pNMHDR, LRESULT* pResult )
{
	CPropertyCtrlNotify *pNotify = (CPropertyCtrlNotify*)pNMHDR;
	if (pNotify->pVariable)
	{
		IVariable *pVar = pNotify->pVariable;

		ITimeOfDay *pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
		for (int i = 0,numVars = pTimeOfDay->GetVariableCount(); i < numVars; i++)
		{
			ITimeOfDay::SVariableInfo varInfo;
			if (!pTimeOfDay->GetVariableInfo( i,varInfo ))
			{
				continue;
			}
			if (strcmp(varInfo.name,pVar->GetName()) == 0)
			{
				m_pSplineCtrl->SetTimeRange( Range(0,1.0f) );

				m_pSplineCtrl->RemoveAllSplines();
				if (varInfo.type == ITimeOfDay::TYPE_COLOR)
				{
					COLORREF	afColorArray[4];
					afColorArray[0]=RGB(255,0,0);
					afColorArray[1]=RGB(0,255,0);
					afColorArray[2]=RGB(0,0,255);
					afColorArray[3]=RGB(255,0,255); //Pink... so you know it's wrong if you see it.
					m_pSplineCtrl->AddSpline( varInfo.pInterpolator,0,afColorArray);
					m_pSplineCtrl->SetValueRange( Range(0,1) );


					m_pColorGradientCtrl->SetSpline( varInfo.pInterpolator,TRUE );
					m_pColorGradientCtrl->EnableWindow(TRUE);
					m_pColorGradientCtrl->RedrawWindow();

				}
				else
				{	
					m_pColorGradientCtrl->EnableWindow(FALSE);
					m_pColorGradientCtrl->Invalidate();

					m_pSplineCtrl->SetValueRange( Range(varInfo.fValue[1],varInfo.fValue[2]) );
					m_pSplineCtrl->AddSpline( varInfo.pInterpolator,0,RGB(0,255,0) );
				}
				m_pSplineCtrl->SetSplineSet( 0 );
				m_pSplineCtrl->RedrawWindow();

				break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnHold()
{
	XmlNodeRef node = CreateXmlNode("TimeOfDay");
	GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( node,false );
	node->saveToFile( "%USER%/Sandbox/TimeOfDayHold.xml" );
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnFetch()
{
	XmlNodeRef node = GetISystem()->LoadXmlFile( "%USER%/Sandbox/TimeOfDayHold.xml" );
	if (node)
	{
		GetIEditor()->Get3DEngine()->GetTimeOfDay()->Serialize( node,true );
		UpdateValues();
		SetTime( GetTime(),true );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnUndo()
{
	GetIEditor()->Undo();
}

//////////////////////////////////////////////////////////////////////////
void CTimeOfDayDialog::OnRedo()
{
	GetIEditor()->Redo();
}
