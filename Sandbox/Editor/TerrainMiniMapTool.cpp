////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainMiniMapTool.cpp
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain Modification Tool implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Viewport.h"
#include "TerrainMiniMapTool.h"
#include "Heightmap.h"
#include "Objects\DisplayContext.h"
#include "Mission.h"
#include "CryEditDoc.h"

#include <I3DEngine.h>

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainMiniMapTool,CEditTool)


//////////////////////////////////////////////////////////////////////////
// Panel.
//////////////////////////////////////////////////////////////////////////
class CTerrainMiniMapPanel : public CDialog
{
public:
	CSliderCtrl	m_radius;
	CTerrainMiniMapTool *m_tool;
	
	CCustomButton	m_btnSelectArea;
	CCustomButton	m_btnGenerate;
	CCustomButton m_btnSave;
	CCustomButton m_btnRestore;
	CNumberCtrl m_cameraHeight;
	CComboBox m_resolutions;

	CTerrainMiniMapPanel(class CTerrainMiniMapTool *tool,CWnd* pParent = NULL) : CDialog(IDD_PANEL_TERRAIN_MINIMAP,pParent)
	{
		m_tool = tool;
		Create( IDD_PANEL_TERRAIN_MINIMAP,pParent );
	}

protected:
	DECLARE_MESSAGE_MAP()

	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL OnInitDialog()
	{
		__super::OnInitDialog();

		int nStart = 8;
		for (int i = 0; i < 12; i++)
		{
			char str[8];
			sprintf( str,"%d",nStart<<i );
			m_resolutions.AddString( str );
		}
		m_cameraHeight.Create(this,IDC_HEIGHT,0);

		ReloadValues();

		return TRUE;  // return TRUE unless you set the focus to a control
	}
	virtual void DoDataExchange(CDataExchange* pDX)
	{
		CDialog::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_AREA, m_btnSelectArea);
		DDX_Control(pDX, IDC_GENERATE, m_btnGenerate);
		DDX_Control(pDX, IDC_RESTORE, m_btnRestore);
		DDX_Control(pDX, IDC_RESOLUTION,m_resolutions );
	}
	
	void ReloadValues()
	{
		m_cameraHeight.SetValue( m_tool->m_minimap.vExtends.x );
		m_resolutions.SetCurSel(-1);
		int nRes = 0;
		int nStart = 8;
		for (int i = 0; i < 10; i++)
		{
			if (m_tool->m_minimap.textureWidth == (nStart << i))
			{
				m_resolutions.SetCurSel(i);
				break;
			}
		}
	}

	afx_msg void OnSelectArea()
	{
		m_tool->m_minimap.vCenter = m_tool->m_vCenter;
	}
	afx_msg void OnRestoreArea()
	{
		m_tool->m_minimap = m_tool->m_minimapOriginal;
		ReloadValues();
	}
	afx_msg void OnGenerateArea()
	{
		m_tool->Generate();
	}
	afx_msg void OnHeightChange()
	{
		m_tool->m_minimap.vExtends	=
		m_tool->m_minimap.vExtends	=	Vec2(m_cameraHeight.GetValue(),m_cameraHeight.GetValue());
	}
	afx_msg void OnResolutionChange()
	{
		int nSel = m_resolutions.GetCurSel();
		if (nSel >=0 )
		{
			int nRes = 0;
			int nStart = 8;
			for (int i = 0; i < 12; i++)
			{
				if (i == nSel)
				{
					m_tool->m_minimap.textureWidth = nStart << i;
					m_tool->m_minimap.textureHeight = nStart << i;
					break;
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CTerrainMiniMapPanel, CDialog)
	ON_BN_CLICKED(IDC_AREA, OnSelectArea)
	ON_BN_CLICKED(IDC_RESTORE, OnRestoreArea)
	ON_BN_CLICKED(IDC_GENERATE, OnGenerateArea)
	ON_EN_CHANGE(IDC_HEIGHT,OnHeightChange)
	ON_CBN_SELENDOK(IDC_RESOLUTION,OnResolutionChange)
END_MESSAGE_MAP()


namespace
{
	int s_panelId = 0;
	CTerrainMiniMapPanel *s_panel = 0;
}


//////////////////////////////////////////////////////////////////////////
// CTerrainMiniMapTool
//////////////////////////////////////////////////////////////////////////
CTerrainMiniMapTool::CTerrainMiniMapTool()
{
	m_minimap = GetIEditor()->GetDocument()->GetCurrentMission()->GetMinimap();
	m_minimapOriginal = m_minimapOriginal;
	m_vCenter = m_minimap.vCenter;
	m_vExtends = m_minimap.vExtends;

	m_moveSide = 0;

	m_bDragging = false;
	b_stateScreenShot = false;
	GetIEditor()->RegisterNotifyListener( this );
}

//////////////////////////////////////////////////////////////////////////
CTerrainMiniMapTool::~CTerrainMiniMapTool()
{
	GetIEditor()->UnregisterNotifyListener( this );
	GetIEditor()->GetDocument()->GetCurrentMission()->SetMinimap(m_minimap);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::BeginEditParams( IEditor *ie,int flags )
{
	if (!s_panelId)
	{
		s_panel = new CTerrainMiniMapPanel(this,AfxGetMainWnd());
		s_panelId = GetIEditor()->AddRollUpPage( ROLLUP_TERRAIN,"Mini Map",s_panel );
		AfxGetMainWnd()->SetFocus();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::EndEditParams()
{
	if (s_panelId)
	{
		GetIEditor()->RemoveRollUpPage(ROLLUP_TERRAIN,s_panelId);
		s_panelId = 0;
		s_panel = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMiniMapTool::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseLDown && (flags&MK_LBUTTON))
	{
		m_mouseDown = point;
		m_bDragging = true;

/*		Vec3 pos = view->ViewToWorld( point,0,true );
		float x1 = fabs(m_min.x-pos.x);
		float x2 = fabs(m_max.x-pos.x);
		float y1 = fabs(m_min.y-pos.y);
		float y2 = fabs(m_max.y-pos.y);
		m_moveSide = 0;
		if (x1<x2 && x1<y1 && x1<y2)
			m_moveSide = 0;
		else if (y1<x1 && y1<x2 && y1<y2)
			m_moveSide = 1;
		else if (x2<x1 && x2<y1 && x2<y2)
			m_moveSide = 2;
		else if (y2<x1 && y2<x2 && y2<y1)
			m_moveSide = 3;

		if (flags&MK_CONTROL)
		{
			m_min = pos;
		}*/
		return true;
	}
	else if (event == eMouseLDown || (event == eMouseMove && (flags&MK_LBUTTON)))
	{
		Vec3 pos = view->ViewToWorld( point,0,true );
		if (m_bDragging)
		{
/*			switch (m_moveSide)
			{
			case 0:
				m_min.x = pos.x;
				break;
			case 1:
				m_min.y = pos.y;
				break;
			case 2:
				m_max.x = pos.x;
				break;
			case 3:
				m_max.y = pos.y;
				break;
			}*/
			m_vCenter.x	=	pos.x;
			m_vCenter.y	=	pos.y;
		}

/*		if (flags&MK_CONTROL)
		{
			m_max = pos;
		}

		if (m_max.x < m_min.x)
			std::swap(m_min.x,m_max.x);
		if (m_max.y < m_min.y)
			std::swap(m_min.y,m_max.y);*/

		return true;
	}
	else
	{
		// Stop.
		if (m_bDragging)
		{
			m_bDragging = false;
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::Display( DisplayContext &dc )
{
	dc.SetColor( 0,0,1 );
	dc.DrawTerrainRect( m_vCenter.x-0.5f,m_vCenter.y-0.5f,
											m_vCenter.x+0.5f,m_vCenter.y+0.5f,
											1.0f	);
	dc.DrawTerrainRect( m_vCenter.x-5.f,m_vCenter.y-5.f,
											m_vCenter.x+5.f,m_vCenter.y+5.f,
											1.0f	);
	dc.DrawTerrainRect( m_vCenter.x-50.f,m_vCenter.y-50.f,
											m_vCenter.x+50.f,m_vCenter.y+50.f,
											1.0f	);

	dc.SetColor( 0,1,0 );
	dc.SetLineWidth( 2 );
	dc.DrawTerrainRect( m_minimap.vCenter.x-m_minimap.vExtends.x,m_minimap.vCenter.y-m_minimap.vExtends.y,
											m_minimap.vCenter.x+m_minimap.vExtends.x,m_minimap.vCenter.y+m_minimap.vExtends.y,1.1f	);
	dc.SetLineWidth( 0 );

	//CHeightmap *heightmap = GetIEditor()->GetHeightmap();
	//int unitSize = heightmap->GetUnitSize();

	/*
	float fx1 = (m_max.y - m_min.x)/unitSize;
	float fy1 = (m_max.x - m_min.x)/unitSize;
	float fx2 = (m_max.y + m_min.x)/unitSize;
	float fy2 = (m_max.x + m_min.x)/unitSize;

	float sx = (m_max.x - m_min.x)/unitSize;
	float sy = (m_max.y - m_min.y)/unitSize;


	int x1 = MAX(fx1,0);
	int y1 = MAX(fy1,0);
	int x2 = MIN(fx2,heightmap->GetWidth()-1);
	int y2 = MIN(fy2,heightmap->GetHeight()-1);

	if (m_bMakeHole)
		dc.renderer->SetMaterialColor( 1,0,0,1 );
	else
		dc.renderer->SetMaterialColor( 0,0,1,1 );
	Vec3 p1,p2,p3,p4;
	// Make hole.
	for (int x = x1; x <= x2; x++)
	{
		for (int y = y1; y <= y2; y++)
		{
			p1.x = y*unitSize;
			p1.y = x*unitSize;
			
			p2.x = y*unitSize+unitSize;
			p2.y = x*unitSize;
			
			p3.x = y*unitSize+unitSize;
			p3.y = x*unitSize+unitSize;
			
			p4.x = y*unitSize;
			p4.y = x*unitSize+unitSize;

			p1.z = dc.engine->GetTerrainElevation(p1.x,p1.y)+0.2f;
			p2.z = dc.engine->GetTerrainElevation(p2.x,p2.y)+0.2f;
			p3.z = dc.engine->GetTerrainElevation(p3.x,p3.y)+0.2f;
			p4.z = dc.engine->GetTerrainElevation(p4.x,p4.y)+0.2f;
			dc.DrawLine( p1,p2 );
			dc.DrawLine( p2,p3 );
			dc.DrawLine( p3,p4 );
			dc.DrawLine( p1,p4 );
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMiniMapTool::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	return false;
}

bool CTerrainMiniMapTool::OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::Generate()
{
	m_ConstClearList.clear();

	GetIEditor()->SetConsoleVar( "e_screenshot_width",m_minimap.textureWidth );
	GetIEditor()->SetConsoleVar( "e_screenshot_height",m_minimap.textureHeight );
	GetIEditor()->SetConsoleVar( "e_screenshot_map_camheight",100000.f );

	GetIEditor()->SetConsoleVar( "e_screenshot_map_center_x",m_minimap.vCenter.x );
	GetIEditor()->SetConsoleVar( "e_screenshot_map_center_y",m_minimap.vCenter.y );

	GetIEditor()->SetConsoleVar( "e_screenshot_map_size_x",m_minimap.vExtends.x );
	GetIEditor()->SetConsoleVar( "e_screenshot_map_size_y",m_minimap.vExtends.y );

	string path = Path::MakeFullPath("Editor");
	string filename = path + "\\MapScreenshotSettings.xml";
	XmlNodeRef root = GetISystem()->LoadXmlFile( filename );
	if(root)
	{
		for(int i=0,nChilds=root->getChildCount();i<nChilds;i++)
		{
			XmlNodeRef ChildNode = root->getChild(i);
			const char* pTagName	=	ChildNode->getTag();
			ICVar* pVar	=	gEnv->pConsole->GetCVar(pTagName);
			if(pVar)
			{
				m_ConstClearList[pTagName]	=	pVar->GetFVal();
				const char* pAttr	=	ChildNode->getAttr("value");
				std::string Value	=	pAttr;
				pVar->Set(Value.c_str());
			}
		}
	}
	else
	{
		m_ConstClearList["r_HDRRendering"]	= gEnv->pConsole->GetCVar("r_HDRRendering")->GetFVal();
		m_ConstClearList["r_PostProcessEffects"]	=	gEnv->pConsole->GetCVar("r_PostProcessEffects")->GetFVal();
		m_ConstClearList["e_lods"]	= gEnv->pConsole->GetCVar("e_lods")->GetFVal();
		m_ConstClearList["e_view_dist_ratio"]	= gEnv->pConsole->GetCVar("e_view_dist_ratio")->GetFVal();
		m_ConstClearList["e_terrain_lod_ratio"]	= gEnv->pConsole->GetCVar("e_terrain_lod_ratio")->GetFVal();
		m_ConstClearList["e_screenshot_quality"]	=	gEnv->pConsole->GetCVar("e_screenshot_quality")->GetFVal();

		gEnv->pConsole->GetCVar("r_HDRRendering")->Set(0);
		gEnv->pConsole->GetCVar("r_PostProcessEffects")->Set(0);
		gEnv->pConsole->GetCVar("e_screenshot_quality")->Set(0);
		gEnv->pConsole->GetCVar("e_view_dist_ratio")->Set(10000.f);
		gEnv->pConsole->GetCVar("e_lods")->Set(0);
		gEnv->pConsole->GetCVar("e_terrain_lod_ratio")->Set(0.f);
		gEnv->pConsole->GetCVar("e_vegetation")->Set(0);
	}
	gEnv->pConsole->GetCVar("e_screenshot_quality")->Set(0);

	GetIEditor()->SetConsoleVar( "e_screenshot",3 );

	b_stateScreenShot = true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMiniMapTool::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
		case eNotify_OnIdleUpdate:
		{
			if(b_stateScreenShot)
			{
				ICVar * pVar = gEnv->pConsole->GetCVar("e_screenshot");
				if(pVar && pVar->GetIVal()==0)
				{
					for(std::map<std::string,float>::iterator it=m_ConstClearList.begin();it!=m_ConstClearList.end();++it)
					{
						ICVar* pVar	=	gEnv->pConsole->GetCVar(it->first.c_str());
						if(pVar)
							pVar->Set(it->second);
					}
					m_ConstClearList.clear();

					b_stateScreenShot = false;
				}
			}
		}
		break;
	}
}