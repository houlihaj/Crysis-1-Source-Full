////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   LightmapCompilerDialog.h
//  Version:     v1.00
//  Created:     22/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "LightmapCompilerDialog.h"
#include "IViewPane.h"
#include "CryEditDoc.h"
#include "LightmapGen.h"

class CLightmapCompilerViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {CD780A61-F265-444c-89BD-E83D6B91E49A}
		static const GUID guid = { 0xcd780a61, 0xf265, 0x444c, { 0x89, 0xbd, 0xe8, 0x3d, 0x6b, 0x91, 0xe4, 0x9a } };
		return guid;
	}
	virtual const char* ClassName() { return "RAM Compiler"; };
	virtual const char* Category() { return "RAM Compiler"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CLightmapCompilerDialog); };
	virtual const char* GetPaneTitle() { return _T("RAM Compiler"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,800,700); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return false; };
};

//////////////////////////////////////////////////////////////////////////
class CLightmapParamsUI
{

public:
	_smart_ptr<CVarBlock> m_pVarBlock;
	
	CSmartVariableEnum<int> paramLightmapMode;
	CSmartVariableEnum<int> paramLightmapQuality;
	CSmartVariable<bool> paramUseSuperSampling;
	CSmartVariable<bool> paramDistributedMap;

	CSmartVariableArray mode_table;
	CSmartVariableArray trace_table;
	CSmartVariableArray irrad_table;
	CSmartVariableArray direct_table;
	CSmartVariableArray indirect_table;
	CSmartVariableArray sun_table;
	CSmartVariableArray rasterization_table;
	CSmartVariableArray ambientocclusion_table;

	CSmartVariable<int> paramDiffuseDepth;
	CSmartVariable<int> paramSpecularDepth;
	CSmartVariable<int> paramEmitPhotonNum;

	CSmartVariable<int> paramPhotonEstimate;
	CSmartVariable<float> paramMinSearchRadius;
	CSmartVariable<float> paramMaxSearchRadius;

	CSmartVariable<float> paramGridJitterBias;
	CSmartVariable<int> paramIndirectSamplesNum;
	CSmartVariable<float> paramMaxBrightness;
	CSmartVariable<bool> paramFinalRegatharing;

	CSmartVariable<bool> paramDirectSampleNum;

	CSmartVariable<bool> paramUseSunLight;
	CSmartVariable<int> paramNumSunDirectSamples;
	CSmartVariable<int> paramNumSunPhotons;

	CSmartVariableEnum<int> paramLightPageWidthHight;
	CSmartVariable<float> paramLumenPerUnit;

//	CSmartVariable<bool> paramMakeOcclMap;
//	CSmartVariable<bool> paramMakeLightMap;

	CSmartVariable<bool> paramAmbientOcclusionSamplesNum;
	CSmartVariable<float> paramAmbientOcclusionMinSearchRadius;
	CSmartVariable<float> paramAmbientOcclusionMaxSearchRadius;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* GetVarBlock() { return m_pVarBlock; 	}
	CLightmapParamsUI()
	{
		m_pVarBlock = new CVarBlock;


		AddVariable( m_pVarBlock,rasterization_table,"Parameters" );
//		AddVariable( rasterization_table,paramAmbientOcclusionSamplesNum,"Compile RAE maps" );
//		AddVariable( rasterization_table,paramDirectSampleNum,"Compile Occlusion maps" );
		AddVariable( rasterization_table,paramUseSuperSampling,"Use supersampling (9 sample)");
		AddVariable( rasterization_table,paramDistributedMap,"Save distributable datas");
		AddVariable( rasterization_table, paramAmbientOcclusionMaxSearchRadius, "RAE Max distance" );
		AddVariable( rasterization_table,paramLumenPerUnit,"Lumen Per World Unit" );
		paramLumenPerUnit->SetLimits(0.01f,256.0f);
/*
		AddVariable( m_pVarBlock,rasterization_table,"Rasterization parameters" );
/*
		AddVariable( m_pVarBlock,mode_table,"Lightmap mode");
		AddVariable( m_pVarBlock,trace_table,"Photon trace parameters" );
		AddVariable( m_pVarBlock,irrad_table,"Irradiance estimate parameters" );
		AddVariable( m_pVarBlock,indirect_table,"Indirect light parameters" );
		AddVariable( m_pVarBlock,sun_table,"Sun light parameters" );
* /
		AddVariable( m_pVarBlock,direct_table,"Direct Area light parameters" );
		AddVariable( m_pVarBlock,ambientocclusion_table, "Ambient Occlusion parameters" );

		//rasterization parameters
//		AddVariable( rasterization_table,paramMakeOcclMap,"Enable occlusion map rendering");
//		AddVariable( rasterization_table,paramMakeLightMap,"Enable light map rendering");
		AddVariable( rasterization_table,paramUseSuperSampling,"Use supersampling (9 sample)");
/*		AddVariable( rasterization_table,paramLightmapQuality,"Render Quality");
		paramLightmapQuality.AddEnumItem( "Lowest - no GI",LMQUALITY_LOW);
		paramLightmapQuality.AddEnumItem( "Mid quality",LMQUALITY_MID);
		paramLightmapQuality.AddEnumItem( "High quality ",LMQUALITY_HIGH);
* /

		//Lightmap mode
		AddVariable( mode_table,paramLightmapMode,"Lightmap Mode");
		paramLightmapMode.AddEnumItem( "Use photon map only(FASTEST)",LMMODE_PHOTONMAP_ONLY);
		paramLightmapMode.AddEnumItem( "Use Directional pass + photon map(FAST)",LMMODE_DIRECT_PHOTONMAP);
		paramLightmapMode.AddEnumItem( "Use Directional pass + regathering(PRECISE)",LMMODE_DIRECT_REGATHERING);

		//Photon trace parameters
		AddVariable( trace_table,paramDiffuseDepth,"Diffuse Depth"/*,IVariable::DT_ANGLE * /);
		AddVariable( trace_table,paramSpecularDepth,"Specular Depth" );
		paramSpecularDepth->SetFlags(IVariable::UI_DISABLED);		
		AddVariable( trace_table,paramEmitPhotonNum,"Generate Photons Num" );
		paramEmitPhotonNum->SetLimits(0,1000000);

		//Photon map's lookup estimator parameters
		AddVariable( irrad_table,paramPhotonEstimate,"Photons in Estimate" );
		paramPhotonEstimate->SetLimits(1,1000);
		AddVariable( irrad_table,paramMinSearchRadius,"Estimate Min Search Radius" );
		paramMinSearchRadius->SetFlags(IVariable::UI_DISABLED);	
		AddVariable( irrad_table,paramMaxSearchRadius,"Estimate Max Search Radius" );

		//indirect light parameters
		AddVariable( indirect_table,paramGridJitterBias,"Grid Jitter Bias" );
		AddVariable( indirect_table,paramIndirectSamplesNum,"Indirect Samples Number" );
		AddVariable( indirect_table,paramMaxBrightness,"Max Brightness" );
		AddVariable( indirect_table,paramFinalRegatharing,"Final Regathering");

		//direct light parameters
		AddVariable( direct_table,paramDirectSampleNum,"Direct Samples" );

		//sun direct light parameters
		AddVariable( sun_table,paramUseSunLight,"Use Sun Light" );
		AddVariable( sun_table,paramNumSunDirectSamples,"Direct Samples" );
		AddVariable( sun_table,paramNumSunPhotons, "Generate Sun Photons Num" );
		paramNumSunPhotons->SetLimits(0,1000000);

		//rasterization parameters
		AddVariable( rasterization_table,paramLightPageWidthHight,"Lightmap Page Width/Hight" );
		//AddVariable( rasterization_table,paramLightPageHight,"Lightmap Page Height" );
		AddVariable( rasterization_table,paramLumenPerUnit,"Lumen Per World Unit" );
		paramLumenPerUnit->SetLimits(0.01f,64.0f);
		paramLightPageWidthHight.AddEnumItem("1024 x 1024", 1024);
		paramLightPageWidthHight.AddEnumItem("2048 x 2048", 2048);
//		paramLightPageWidthHight.AddEnumItem("4096 x 4096", 4096);

		AddVariable( ambientocclusion_table, paramAmbientOcclusionSamplesNum, "Ambient Occlusion Sample Number" );
		AddVariable( ambientocclusion_table, paramAmbientOcclusionMinSearchRadius, "Ambient Occlusion Min distance" );
		AddVariable( ambientocclusion_table, paramAmbientOcclusionMaxSearchRadius, "Ambient Occlusion Max distance" );
*/
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVariableBase &varArray,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		varArray.AddChildVar(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVarBlock *vars,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		vars->AddVariable(&var);
	}
};

IMPLEMENT_DYNCREATE(CLightmapCompilerDialog,CDialog)

#define IDC_TASKPANEL 1
#define IDC_PROPERTYPANEL 2

#define ID_TASK_GEN_LM 100
#define ID_TASK_GEN_LM_SEL 101
#define ID_TASK_RESTORE_DEFAULT_CTX 102
#define ID_TASK_SETUP_LMQUALITY 103
#define ID_TASK_SETUP_LMQUALITY_SEL 104
#define ID_TASK_SHOW_DENSITY 105
#define ID_TASK_SHOW_DENSITY_SEL 106
#define ID_TASK_GEN_ALL 107
#define ID_TASK_GEN_ALL_SEL 108
#define ID_TASK_GENERATE_VOLUME 109
#define ID_TASK_GENERATE_LIGHTLIST 110

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CLightmapCompilerViewClass );
}

//////////////////////////////////////////////////////////////////////////
// CLightmapCompilerDialog implementation.
//////////////////////////////////////////////////////////////////////////
CLightmapCompilerDialog::CLightmapCompilerDialog()
: CDialog( IDD )
{
	m_pParamsUI = new CLightmapParamsUI;
	UpdateParams();
	Create(IDD);
}

CLightmapCompilerDialog::~CLightmapCompilerDialog()
{
	delete m_pParamsUI;
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::PostNcDestroy()
{
	delete this;
}

void CLightmapCompilerDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CLightmapCompilerDialog, CDialog)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::OnDestroy()
{
	GetIEditor()->UnregisterNotifyListener( this );
	__super::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CLightmapCompilerDialog::OnInitDialog()
{
	__super::OnInitDialog();

	CRect rc;
	m_wndTaskPanel.Create( WS_CHILD|WS_BORDER|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,CRect(0,0,150,200),this,IDC_TASKPANEL );

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(TRUE);

	//////////////////////////////////////////////////////////////////////////
	// Generate Group.
	//////////////////////////////////////////////////////////////////////////
	// Add default tasks.
	CXTPTaskPanelGroup *pGenGroup = m_wndTaskPanel.AddGroup(1);
	pGenGroup->SetCaption( "Generate ALL" );

	CXTPTaskPanelGroupItem *pItem =  NULL;
/*
	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_ALL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Param & Rendering" );
*/
	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_LM);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start Rendering" );

	pGenGroup = m_wndTaskPanel.AddGroup(2);
	pGenGroup->SetCaption( "Generate SELECTED" );
/*
	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_ALL_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Param & rendering" );
*/
	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_LM_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start Rendering" );

/*
	pItem =  pGenGroup->AddLinkItem(ID_TASK_SETUP_LMQUALITY);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Parametrize OcclusionampQuality" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_LM);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start rendering Occlusionmaps" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_ALL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start param & rendering" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_SHOW_DENSITY);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start TexelDensity rendering" );

	pGenGroup = m_wndTaskPanel.AddGroup(2);
	pGenGroup->SetCaption( "Generate SELECTED" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_SETUP_LMQUALITY_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Parametrize OcclusionampQuality" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_LM_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start rendering Occlusionmaps" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GEN_ALL_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start param & rendering" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_SHOW_DENSITY_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start TexelDensity rendering" );
*/
	pGenGroup = m_wndTaskPanel.AddGroup(3);
	pGenGroup->SetCaption( "Parameter management" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_RESTORE_DEFAULT_CTX);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Restore Default Parameters" );

	pGenGroup = m_wndTaskPanel.AddGroup(4);
	pGenGroup->SetCaption( "BETA STUFFS" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GENERATE_VOLUME);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Start volume compiling" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_GENERATE_LIGHTLIST);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Generate LightList" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_SETUP_LMQUALITY);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Parametrize OcclusionampQuality ALL" );

	pItem =  pGenGroup->AddLinkItem(ID_TASK_SETUP_LMQUALITY_SEL);
	pItem->SetType(xtpTaskItemTypeLink);
	pItem->SetCaption( "Parametrize OcclusionampQuality SELECTED" );



	//////////////////////////////////////////////////////////////////////////
	// Statistics Group.
	//////////////////////////////////////////////////////////////////////////
/*
	CXTPTaskPanelGroup *pStatsGroup = m_wndTaskPanel.AddGroup(4);
	pStatsGroup->SetCaption( "Statistics" );
	pItem =  pStatsGroup->AddLinkItem(0);
	pItem->SetType(xtpTaskItemTypeText);
	pItem->SetCaption( "This is our statistics string, here you can report number of traces photons, unwrapped geometry and so on..." );
*/
	// If properties window is already created.
	XmlNodeRef xmlnode = GetIEditor()->GetDocument()->GetEnvironmentTemplate();

	m_wndProps.Create( WS_VISIBLE|WS_CHILD|WS_BORDER,rc,this,IDC_PROPERTYPANEL );
	//m_wndProps.CreateItems( xmlnode );
	//m_wndProps.SetUpdateCallback( functor(*this,OnPropertyChanged) );
	m_wndProps.AddVarBlock( m_pParamsUI->GetVarBlock() );

	//expand only the "basic" stuffs
	CVariableArray &RT = m_pParamsUI->rasterization_table;
	m_wndProps.ExpandVariableAndChilds( &RT, true );
	//m_wndProps.ExpandAll();

	m_wndProps.SetUpdateCallback(functor(*this, &CLightmapCompilerDialog::FillSceneContext));

	GetIEditor()->RegisterNotifyListener( this );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType,cx,cy);

	if (m_wndTaskPanel.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		rc.DeflateRect(2,2,2,2);

		CRect taskRc(rc);
		taskRc.right = taskRc.left + 200;
		m_wndTaskPanel.MoveWindow( taskRc,TRUE );

		CRect propRc(rc);
		propRc.left = taskRc.right + 2;
		m_wndProps.MoveWindow( propRc,TRUE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
		RestoreDefaultParams();
		break;
	case eNotify_OnEndSceneOpen:
		UpdateParams();
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
LRESULT CLightmapCompilerDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam) {
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			switch (nCmdID)
			{
			case ID_TASK_GEN_LM:
				Generate( false );
				break;
			case ID_TASK_GEN_LM_SEL:
				Generate( true );
				break;
			case ID_TASK_SETUP_LMQUALITY:
				SetupLightmapQuality( false );
				break;
			case ID_TASK_SETUP_LMQUALITY_SEL:
				SetupLightmapQuality( true );
				break;
			case ID_TASK_GEN_ALL:
				SetupAndGenerate( false );
				break;
			case ID_TASK_GEN_ALL_SEL:
				SetupAndGenerate( true );
				break;
			case ID_TASK_SHOW_DENSITY:
				ShowTexelDensity( false );
				break;
			case ID_TASK_SHOW_DENSITY_SEL:
				ShowTexelDensity( true );
				break;
			case ID_TASK_GENERATE_VOLUME:
				GenerateLightVolume();
				break;
			case ID_TASK_GENERATE_LIGHTLIST:
				GenerateLightList();
				break;

			case ID_TASK_RESTORE_DEFAULT_CTX:
				RestoreDefaultParams();
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
void CLightmapCompilerDialog::FillSceneContext(XmlNodeRef pNode)
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	sceneCtx.m_bUseSuperSampling = m_pParamsUI->paramUseSuperSampling;
	sceneCtx.m_bDistributedMap = m_pParamsUI->paramDistributedMap;

	sceneCtx.m_nMaxDiffuseDepth = m_pParamsUI->paramDiffuseDepth;
	sceneCtx.m_nMaxSpecularDepth = m_pParamsUI->paramSpecularDepth;
	sceneCtx.m_numEmitPhotons =	m_pParamsUI->paramEmitPhotonNum;

	sceneCtx.m_nPhotonEstimator =	m_pParamsUI->paramPhotonEstimate;

	sceneCtx.m_fMaxPhotonSearchRadius =	m_pParamsUI->paramMaxSearchRadius/10000.0f;

	sceneCtx.m_fGridJitterBias = m_pParamsUI->paramGridJitterBias;
	sceneCtx.m_numIndirectSamples = m_pParamsUI->paramIndirectSamplesNum;
	sceneCtx.m_fMaxBrightness =	m_pParamsUI->paramMaxBrightness;
	sceneCtx.m_bFinalRegatharing	=	m_pParamsUI->paramFinalRegatharing;

	sceneCtx.m_numDirectSamples =	0; //m_pParamsUI->paramDirectSampleNum ? 1 : 0;

	sceneCtx.m_bUseSunLight = m_pParamsUI->paramUseSunLight;
	sceneCtx.m_numSunDirectSamples = m_pParamsUI->paramNumSunDirectSamples;
	sceneCtx.m_numSunPhotons = m_pParamsUI->paramNumSunPhotons;

	sceneCtx.m_nLightmapPageWidth =	1024;//m_pParamsUI->paramLightPageWidthHight;
	sceneCtx.m_nLightmapPageHeight = 1024;//m_pParamsUI->paramLightPageWidthHight;
	sceneCtx.m_fLumenPerUnitU =	m_pParamsUI->paramLumenPerUnit;
	sceneCtx.m_fLumenPerUnitV =	m_pParamsUI->paramLumenPerUnit;

	//set proper mode
	sceneCtx.m_eLightmapMode = m_pParamsUI->paramLightmapMode;
	sceneCtx.m_bMakeRAE = false;//m_pParamsUI->paramMakeOcclMap;
	sceneCtx.m_bMakeOcclMap = false;//m_pParamsUI->paramMakeOcclMap;
	sceneCtx.m_bMakeLightMap = false;//m_pParamsUI->paramMakeLightMap;

	sceneCtx.m_numAmbientOcclusionSamples = 6; //m_pParamsUI->paramAmbientOcclusionSamplesNum ? 6 : 0;
	sceneCtx.m_fMinAmbientOcclusionSearchRadius = 0;//m_pParamsUI->paramAmbientOcclusionMinSearchRadius;
	sceneCtx.m_fMaxAmbientOcclusionSearchRadius = m_pParamsUI->paramAmbientOcclusionMaxSearchRadius;

	//override the values, if the default quality exchanged...
	//sceneCtx.SetPredefinedState( m_pParamsUI->paramLightmapQuality );
}


//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::SetupAndGenerate( bool bOnlySelected )
{
	{
		CWaitCursor wait;
		SSharedLMEditorData sSharedData;
		CLightmapGen cLightmapGen(&sSharedData);

		if (bOnlySelected)
			cLightmapGen.SetupLightmapQualitySelected( GetIEditor(),NULL );
		else
			cLightmapGen.SetupLightmapQualityAll( GetIEditor(),NULL );
	}
	{
		CWaitCursor wait;
		SSharedLMEditorData sSharedData;
		CLightmapGen cLightmapGen(&sSharedData);
		if (bOnlySelected)
			cLightmapGen.GenerateSelected( GetIEditor(),NULL );
		else
			cLightmapGen.GenerateAll( GetIEditor(),NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::SetupLightmapQuality( bool bOnlySelected )
{
	CWaitCursor wait;

	SSharedLMEditorData sSharedData;
	CLightmapGen cLightmapGen(&sSharedData);

	if (bOnlySelected)
	{
		cLightmapGen.SetupLightmapQualitySelected( GetIEditor(),NULL );
	}
	else
	{
		cLightmapGen.SetupLightmapQualityAll( GetIEditor(),NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::GenerateLightList()
{
	CWaitCursor wait;

	SSharedLMEditorData sSharedData;
	CLightmapGen cLightmapGen(&sSharedData);

	cLightmapGen.GenerateLightList( GetIEditor(),NULL );
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::GenerateLightVolume()
{
	CWaitCursor wait;

	SSharedLMEditorData sSharedData;
	CLightmapGen cLightmapGen(&sSharedData);

	cLightmapGen.GenerateLightVolume( GetIEditor(),NULL );
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::ShowTexelDensity( bool bOnlySelected )
{
	CWaitCursor wait;

	SSharedLMEditorData sSharedData;
	CLightmapGen cLightmapGen(&sSharedData);

	if (bOnlySelected)
	{
		cLightmapGen.ShowTexelDensitySelected( GetIEditor(),NULL );
	}
	else
	{
		cLightmapGen.ShowTexelDensityAll( GetIEditor(),NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLightmapCompilerDialog::Generate( bool bOnlySelected )
{
	CWaitCursor wait;

	SSharedLMEditorData sSharedData;
	CLightmapGen cLightmapGen(&sSharedData);

	//FillSceneContext();

	if (bOnlySelected)
	{
		cLightmapGen.GenerateSelected( GetIEditor(),NULL );
	}
	else
	{
		cLightmapGen.GenerateAll( GetIEditor(),NULL );
	}
	
	//CWaitProgress aa;

	//aa.SetText();
}

void CLightmapCompilerDialog::RestoreDefaultParams()
{
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	sceneCtx.RestoreDefault();
	UpdateParams();
}

void CLightmapCompilerDialog::UpdateParams()
{
	//disable update the sceneCtx...
	m_wndProps.EnableUpdateCallback(false);
	CSceneContext& sceneCtx = CSceneContext::GetInstance();
	m_pParamsUI->paramUseSuperSampling = sceneCtx.m_bUseSuperSampling;
	m_pParamsUI->paramDistributedMap = sceneCtx.m_bDistributedMap;

//	m_pParamsUI->paramMakeOcclMap = sceneCtx.m_bMakeOcclMap;
	//m_pParamsUI->paramMakeLightMap = sceneCtx.m_bMakeLightMap;

	m_pParamsUI->paramDiffuseDepth = sceneCtx.m_nMaxDiffuseDepth;
	m_pParamsUI->paramSpecularDepth = sceneCtx.m_nMaxSpecularDepth; 
	m_pParamsUI->paramEmitPhotonNum = sceneCtx.m_numEmitPhotons;

	m_pParamsUI->paramPhotonEstimate = sceneCtx.m_nPhotonEstimator;
	m_pParamsUI->paramMinSearchRadius = 0.0f;
	m_pParamsUI->paramMaxSearchRadius = sceneCtx.m_fMaxPhotonSearchRadius*10000.0f;

	m_pParamsUI->paramGridJitterBias = sceneCtx.m_fGridJitterBias;
	m_pParamsUI->paramIndirectSamplesNum = sceneCtx.m_numIndirectSamples;
	m_pParamsUI->paramMaxBrightness = sceneCtx.m_fMaxBrightness;
	m_pParamsUI->paramFinalRegatharing = sceneCtx.m_bFinalRegatharing;

	m_pParamsUI->paramUseSunLight = sceneCtx.m_bUseSunLight;
	m_pParamsUI->paramNumSunDirectSamples = sceneCtx.m_numSunDirectSamples;
	m_pParamsUI->paramNumSunPhotons = sceneCtx.m_numSunPhotons;

	m_pParamsUI->paramDirectSampleNum = sceneCtx.m_numDirectSamples ? true : false;

	m_pParamsUI->paramLightPageWidthHight = sceneCtx.m_nLightmapPageWidth;
	//m_pParamsUI->paramLightPageHight = sceneCtx.m_nLightmapPageHeight;
	m_pParamsUI->paramLumenPerUnit = sceneCtx.m_fLumenPerUnitU;

	m_pParamsUI->paramLightmapMode = sceneCtx.m_eLightmapMode;

	m_pParamsUI->paramAmbientOcclusionSamplesNum = sceneCtx.m_numAmbientOcclusionSamples ? true : false;
	m_pParamsUI->paramAmbientOcclusionMinSearchRadius = sceneCtx.m_fMinAmbientOcclusionSearchRadius;
	m_pParamsUI->paramAmbientOcclusionMaxSearchRadius = sceneCtx.m_fMaxAmbientOcclusionSearchRadius;

	//enable update the sceneCtx...
	m_wndProps.EnableUpdateCallback(true);
}