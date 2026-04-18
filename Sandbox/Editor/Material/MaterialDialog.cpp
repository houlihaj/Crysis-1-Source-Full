
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialDialog.cpp
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialDialog.h"

#include "StringDlg.h"
#include "NumberDlg.h"

#include "MaterialManager.h"
#include "MaterialLibrary.h"
#include "Material.h"

#include "Objects\BrushObject.h"
#include "Objects\Entity.h"
#include "Objects\ObjectManager.h"
#include "ViewManager.h"
#include "Clipboard.h"
#include "IEntityRenderState.h"

#include "Controls\PropertyItem.h"
#include "ShaderEnum.h"

#include <I3DEngine.h>
//#include <IEntityRenderState.h>
#include <IEntitySystem.h>
#include <ICryAnimation.h>
#include <IViewPane.h>
#include <EditTool.h>


#include "resource.h"
#include "MatEditPreviewDlg.h"

#define IDC_MATERIAL_TREE AFX_IDW_PANE_FIRST

#define EDITOR_OBJECTS_PATH CString("Objects\\Editor\\")

class CMaterialEditorClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {C7891863-1665-45ac-AE51-486671BC8B12}
		static const GUID guid = 
		{ 0xc7891863, 0x1665, 0x45ac, { 0xae, 0x51, 0x48, 0x66, 0x71, 0xbc, 0x8b, 0x12 } };
		return guid;
	}
	virtual const char* ClassName() { return "Material Editor"; };
	virtual const char* Category() { return "Material Editor"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CMaterialDialog); };
	virtual const char* GetPaneTitle() { return _T("Material Editor"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,1000,800); };
	virtual bool SinglePane() { return true; };
	virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::RegisterViewClass()
{
  GetIEditor()->GetClassFactory()->RegisterClass( new CMaterialEditorClass );
}

IMPLEMENT_DYNCREATE(CMaterialDialog,CBaseLibraryDialog);
//////////////////////////////////////////////////////////////////////////
// Material structures.
//////////////////////////////////////////////////////////////////////////
static struct
{
  int texId;
  const char *name;
} sUsedTextures[] =
{
  { EFTT_DIFFUSE,		"Diffuse" },
  { EFTT_GLOSS,			"Specular" },
  { EFTT_BUMP,			"Bumpmap" },
  { EFTT_NORMALMAP,	"Normalmap" },
  { EFTT_ENV,		"Environment" },
  { EFTT_DETAIL_OVERLAY,"Detail" },
  { EFTT_OPACITY,		"Opacity" },
  { EFTT_DECAL_OVERLAY,	"Decal" },
  { EFTT_SUBSURFACE,	"SubSurface" },
  { EFTT_CUSTOM,	"Custom" },
  { EFTT_CUSTOM_SECONDARY,	"[1] Custom" },
};

static struct
{
  const char *name;
} sUsedMtlLayers[] =
{
  { "Frozen" },
  { "Wet" },
  { "Cloak" },
  { "Unused" },
  { "Unused" },
  { "Unused" },
  { "Unused" },
};

#ifndef _countof
#define _countof(array) (sizeof(array)/sizeof(array[0]))
#endif

struct STextureVars
{
  CSmartVariable<int> amount;
  CSmartVariable<bool> is_tile[2];

  CSmartVariableEnum<int> etcgentype;
  CSmartVariableEnum<int> etcmrotatetype;
  CSmartVariableEnum<int> etcmumovetype;
  CSmartVariableEnum<int> etcmvmovetype;
  CSmartVariableEnum<int> etextype;
  CSmartVariableEnum<int> filter;

  CSmartVariable<bool> is_tcgprojected;
  CSmartVariable<float> tiling[3];
  CSmartVariable<float> rotate[3];
  CSmartVariable<float> offset[3];
  CSmartVariable<float> tcmuoscrate;
  CSmartVariable<float> tcmvoscrate;
  CSmartVariable<float> tcmuoscamplitude;
  CSmartVariable<float> tcmvoscamplitude;
  CSmartVariable<float> tcmuoscphase;
  CSmartVariable<float> tcmvoscphase;
  CSmartVariable<float> tcmrotoscrate[3];
  CSmartVariable<float> tcmrotoscamplitude[3];
  CSmartVariable<float> tcmrotoscphase[3];
  CSmartVariable<float> tcmrotosccenter[3];

  CSmartVariableArray tableTiling;
  CSmartVariableArray tableOscillator;
  CSmartVariableArray tableRotator;
};

struct SMaterialLayerVars
{   
  CSmartVariable<bool> bNoDraw; // disable layer rendering (useful in some cases)  
  CSmartVariableEnum<CString> shader; // shader layer name
};

struct SVertexWaveFormUI
{
  CSmartVariableArray table;
  CSmartVariableEnum<int> waveFormType;
  CSmartVariable<float> level;
  CSmartVariable<float> amplitude;
  CSmartVariable<float> phase;
  CSmartVariable<float> frequency;
};

//////////////////////////////////////////////////////////////////////////
struct SVertexModUI
{
  CSmartVariableEnum<int> type;
  CSmartVariable<float>   fDividerX;
  CSmartVariable<float>   fDividerY;
  CSmartVariable<Vec3>    vNoiseScale;

  SVertexWaveFormUI wave[2];
};

/** User Interface definition of material.
*/
class CMaterialUI
{
public:
  CSmartVariableEnum<CString> shader;
  CSmartVariable<bool> bNoShadow;
  CSmartVariable<bool> bAdditive;
  CSmartVariable<bool> bAdditiveDecal;
  CSmartVariable<bool> bWire;
  CSmartVariable<bool> b2Sided;
  CSmartVariable<float> opacity;
  CSmartVariable<float> alphaTest;
  CSmartVariable<float> glowAmount;  
  CSmartVariable<bool> bScatter;
  CSmartVariable<bool> bHideAfterBreaking;
	CSmartVariable<bool> bForceShadowBias;
  //CSmartVariable<bool> bTranslucenseLayer;
  CSmartVariableEnum<CString> surfaceType;

  //////////////////////////////////////////////////////////////////////////
  // Lighting
  //////////////////////////////////////////////////////////////////////////
  CSmartVariable<Vec3> diffuse;					// Diffuse color 0..1
  CSmartVariable<Vec3> specular;				// Specular color 0..1
  CSmartVariable<float> shininess;			// Specular shininess.
  CSmartVariable<float> speclevel;			// Specular level (multiplier)
  CSmartVariable<Vec3> emissive;				// Emissive color 0..1

  //////////////////////////////////////////////////////////////////////////
  // Textures.
  //////////////////////////////////////////////////////////////////////////
  //CSmartVariableArrayT<CString> textureVars[EFTT_MAX];
  CSmartVariableArray textureVars[EFTT_MAX];
  STextureVars textures[EFTT_MAX];

  //////////////////////////////////////////////////////////////////////////
  // Material layers settings
  //////////////////////////////////////////////////////////////////////////

  // 8 max for now. change this later  
  SMaterialLayerVars materialLayers[MTL_LAYER_MAX_SLOTS];

  //////////////////////////////////////////////////////////////////////////
  // Post effects 
  //////////////////////////////////////////////////////////////////////////

  CSmartVariableEnum< int > postEffectType;
  CVarEnumList< int > *enumPostEffects;

  //////////////////////////////////////////////////////////////////////////

  SVertexModUI vertexMod;

  CSmartVariableArray tableShader;
  CSmartVariableArray tableOpacity;
  CSmartVariableArray tableLighting;
  CSmartVariableArray tableTexture;
  CSmartVariableArray tableVertexMod;

  CSmartVariableArray tableShaderParams;
  CSmartVariableArray tableShaderGenParams;

  CVarEnumList<int> *enumTexType;
  CVarEnumList<int> *enumTexGenType;
  CVarEnumList<int> *enumTexModRotateType;
  CVarEnumList<int> *enumTexModUMoveType;
  CVarEnumList<int> *enumTexModVMoveType;
  CVarEnumList<int> *enumTexFilterType;

  CVarEnumList<int> *enumVertexMod;
  CVarEnumList<int> *enumWaveType;

  //////////////////////////////////////////////////////////////////////////
  int texUsageMask;

  CVarBlockPtr m_vars;
  CVarBlockPtr m_varsMtlLayersPresets;
  CVarBlockPtr m_varsMtlLayersShader[MTL_LAYER_MAX_SLOTS];

  //////////////////////////////////////////////////////////////////////////
  void SetFromMaterial( CMaterial *mtl );
  void SetToMaterial( CMaterial *mtl );
  void SetTextureNames( CMaterial *mtl );

  void SetShaderResources( const SInputShaderResources &sr );
  void GetShaderResources( SInputShaderResources &sr );

  void SetVertexDeform( const SInputShaderResources &sr );
  void GetVertexDeform( SInputShaderResources &sr );

  //////////////////////////////////////////////////////////////////////////
  CMaterialUI()
  {
  }
  ~CMaterialUI()
  {
  }

  //////////////////////////////////////////////////////////////////////////
  CVarBlock* CreateVars()
  {
    m_vars = new CVarBlock;

    //////////////////////////////////////////////////////////////////////////
    // Init enums.
    //////////////////////////////////////////////////////////////////////////
    enumTexType = new CVarEnumList<int>;
    enumTexType->AddItem( "2D",eTT_2D );
    enumTexType->AddItem( "Cube-Map",eTT_Cube );
    enumTexType->AddItem( "Auto Cube-Map",eTT_AutoCube );
    enumTexType->AddItem( "Auto 2D-Map",eTT_Auto2D );
    enumTexType->AddItem( "From User Params",eTT_User );

    enumTexGenType = new CVarEnumList<int>;
    enumTexGenType->AddItem( "Stream",ETG_Stream );
    enumTexGenType->AddItem( "World",ETG_World );
    enumTexGenType->AddItem( "Camera",ETG_Camera );
    enumTexGenType->AddItem( "World Environment Map",ETG_WorldEnvMap );
    enumTexGenType->AddItem( "Camera Environment Map",ETG_CameraEnvMap );
    enumTexGenType->AddItem( "Normal Map",ETG_NormalMap );
    enumTexGenType->AddItem( "Sphere Map",ETG_SphereMap );

    enumTexModRotateType = new CVarEnumList<int>;
    enumTexModRotateType->AddItem( "No Change",ETMR_NoChange );
    enumTexModRotateType->AddItem( "Fixed Rotation",ETMR_Fixed );
    enumTexModRotateType->AddItem( "Constant Rotation",ETMR_Constant );
    enumTexModRotateType->AddItem( "Oscillated Rotation",ETMR_Oscillated );

    enumTexModUMoveType = new CVarEnumList<int>;
    enumTexModUMoveType->AddItem( "No Change",ETMM_NoChange );
    enumTexModUMoveType->AddItem( "Fixed Moving",ETMM_Fixed );
    enumTexModUMoveType->AddItem( "Constant Moving",ETMM_Constant );
    enumTexModUMoveType->AddItem( "Jitter Moving",ETMM_Jitter );
    enumTexModUMoveType->AddItem( "Pan Moving",ETMM_Pan );
    enumTexModUMoveType->AddItem( "Stretch Moving",ETMM_Stretch );
    enumTexModUMoveType->AddItem( "Stretch-Repeat Moving",ETMM_StretchRepeat );

    enumTexModVMoveType = new CVarEnumList<int>;
    enumTexModVMoveType->AddItem( "No Change",ETMM_NoChange );
    enumTexModVMoveType->AddItem( "Fixed Moving",ETMM_Fixed );
    enumTexModVMoveType->AddItem( "Constant Moving",ETMM_Constant );
    enumTexModVMoveType->AddItem( "Jitter Moving",ETMM_Jitter );
    enumTexModVMoveType->AddItem( "Pan Moving",ETMM_Pan );
    enumTexModVMoveType->AddItem( "Stretch Moving",ETMM_Stretch );
    enumTexModVMoveType->AddItem( "Stretch-Repeat Moving",ETMM_StretchRepeat );

    enumTexFilterType = new CVarEnumList<int>;
    enumTexFilterType->AddItem( "Default",FILTER_NONE );
    enumTexFilterType->AddItem( "Point",FILTER_POINT );
    enumTexFilterType->AddItem( "Linear",FILTER_LINEAR );
    enumTexFilterType->AddItem( "Bilinear",FILTER_BILINEAR );
    enumTexFilterType->AddItem( "Trilinear",FILTER_TRILINEAR );
    enumTexFilterType->AddItem( "Anisotropic 2x",FILTER_ANISO2X );
    enumTexFilterType->AddItem( "Anisotropic 4x",FILTER_ANISO4X );
    enumTexFilterType->AddItem( "Anisotropic 8x",FILTER_ANISO8X );
    enumTexFilterType->AddItem( "Anisotropic 16x",FILTER_ANISO16X );

    //////////////////////////////////////////////////////////////////////////
    // Vertex Mods.
    //////////////////////////////////////////////////////////////////////////
    enumVertexMod = new CVarEnumList<int>;
    enumVertexMod->AddItem("None",eDT_Unknown);
    enumVertexMod->AddItem("Sin Wave",eDT_SinWave);
    enumVertexMod->AddItem("Bulge",eDT_Bulge);
    enumVertexMod->AddItem("Squeeze",eDT_Squeeze);
    enumVertexMod->AddItem("Perlin 2D",eDT_Perlin2D);
    enumVertexMod->AddItem("Perlin 3D",eDT_Perlin3D);
    enumVertexMod->AddItem("From Center",eDT_FromCenter);
    enumVertexMod->AddItem("Bending",eDT_Bending);
    enumVertexMod->AddItem("Proc. Flare",eDT_ProcFlare);
    enumVertexMod->AddItem("Auto sprite",eDT_AutoSprite);
    enumVertexMod->AddItem("Beam",eDT_Beam);
    enumVertexMod->AddItem("FixedOffset",eDT_FixedOffset);
		
    //////////////////////////////////////////////////////////////////////////    

    enumWaveType = new CVarEnumList<int>;
    enumWaveType->AddItem("None",eWF_None);
    enumWaveType->AddItem("Sin",eWF_Sin);
    enumWaveType->AddItem("Half Sin",eWF_HalfSin);
    enumWaveType->AddItem("Square",eWF_Square);
    enumWaveType->AddItem("Triangle",eWF_Triangle);
    enumWaveType->AddItem("Saw Tooth",eWF_SawTooth);
    enumWaveType->AddItem("Inverse Saw Tooth",eWF_InvSawTooth);
    enumWaveType->AddItem("Hill",eWF_Hill);
    enumWaveType->AddItem("Inverse Hill",eWF_InvHill);
    
    //////////////////////////////////////////////////////////////////////////
    // Fill shaders enum.
    //////////////////////////////////////////////////////////////////////////
    CVarEnumList<CString> *enumShaders  = new CVarEnumList<CString>;
    {
      CShaderEnum *pShaderEnum = GetIEditor()->GetShaderEnum();
      pShaderEnum->EnumShaders();
      for (int i = 0; i < pShaderEnum->GetShaderCount(); i++)
      {
        CString shaderName = pShaderEnum->GetShader(i);
        if (strstri(shaderName,"_Overlay") != 0)
          continue;
        enumShaders->AddItem( shaderName,shaderName );
      }
    }

    //////////////////////////////////////////////////////////////////////////
    // Fill surface types.
    //////////////////////////////////////////////////////////////////////////
    CVarEnumList<CString> *enumSurfaceTypes = new CVarEnumList<CString>;
    {
      std::vector<CString> types;
      types.push_back( "" ); // Push empty surface type.
      ISurfaceTypeEnumerator *pSurfaceTypeEnum = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetEnumerator();
      for (ISurfaceType *pSurfaceType = pSurfaceTypeEnum->GetFirst(); pSurfaceType; pSurfaceType = pSurfaceTypeEnum->GetNext())
      {
        types.push_back( pSurfaceType->GetName() );
      }
      std::sort( types.begin(),types.end() );
      for (int i = 0; i < types.size(); i++)
      {
        enumSurfaceTypes->AddItem( types[i],types[i] );
      }
    }

    //////////////////////////////////////////////////////////////////////////
    // Fill post effects types
    //////////////////////////////////////////////////////////////////////////

    enumPostEffects = new CVarEnumList<int>;
    enumPostEffects->AddItem("None", 0);
    enumPostEffects->AddItem("Ghost", POST_EFFECT_GHOST);
    enumPostEffects->AddItem("Hologram", POST_EFFECT_HOLOGRAM);
    enumPostEffects->AddItem("ChameleonCloak", POST_EFFECT_CHAMELEONCLOAK);

    //////////////////////////////////////////////////////////////////////////
    // Init tables.
    //////////////////////////////////////////////////////////////////////////
    AddVariable( m_vars,tableShader,"Material Settings",IVariable::DT_SIMPLE );
    AddVariable( m_vars,tableOpacity,"Opacity Settings",IVariable::DT_SIMPLE );
    AddVariable( m_vars,tableLighting,"Lighting Settings",IVariable::DT_SIMPLE );
    AddVariable( m_vars,tableTexture,"Texture Maps",IVariable::DT_SIMPLE );        
    AddVariable( m_vars,tableShaderParams,"Shader Params",IVariable::DT_SIMPLE );
    AddVariable( m_vars,tableShaderGenParams,"Shader Generation Params",IVariable::DT_SIMPLE );
    AddVariable( m_vars,tableVertexMod,"Vertex Deformation",IVariable::DT_SIMPLE );


    //////////////////////////////////////////////////////////////////////////
    // Shader.
    //////////////////////////////////////////////////////////////////////////
    AddVariable( tableShader,shader,"Shader" );
    //AddVariable( tableShader,bWire,"Wireframe",IVariable::DT_SIMPLE );
    AddVariable( tableShader,b2Sided,"2 Sided",IVariable::DT_SIMPLE );
    AddVariable( tableShader,bNoShadow,"No Shadow",IVariable::DT_SIMPLE );
    AddVariable( tableShader,bScatter,"Use Scattering",IVariable::DT_SIMPLE );
    AddVariable( tableShader,bHideAfterBreaking,"Hide After Breaking",IVariable::DT_SIMPLE );
		AddVariable( tableShader,bForceShadowBias,"Force using shadow bias",IVariable::DT_SIMPLE );
    AddVariable( tableShader,glowAmount,"Glow Amount",IVariable::DT_SIMPLE );        
    glowAmount->SetLimits( 1,1000 );
    glowAmount->SetLimits( 0,100 );

    postEffectType->SetEnumList( enumPostEffects );
    AddVariable( tableShader, postEffectType,"Post effect type" );

    AddVariable( tableShader,surfaceType,"Surface Type" );

    shader->SetEnumList( enumShaders );

    surfaceType->SetEnumList( enumSurfaceTypes );

    //////////////////////////////////////////////////////////////////////////
    // Opacity.
    //////////////////////////////////////////////////////////////////////////
    AddVariable( tableOpacity,opacity,"Opacity",IVariable::DT_PERCENT );
    AddVariable( tableOpacity,alphaTest,"AlphaTest",IVariable::DT_PERCENT );
    AddVariable( tableOpacity,bAdditive,"Additive",IVariable::DT_SIMPLE );

    //////////////////////////////////////////////////////////////////////////
    // Lighting.
    //////////////////////////////////////////////////////////////////////////
    AddVariable( tableLighting,diffuse,"Diffuse Color",IVariable::DT_COLOR );
    AddVariable( tableLighting,specular,"Specular Color",IVariable::DT_COLOR );
    AddVariable( tableLighting,shininess,"Glossiness",IVariable::DT_SIMPLE );
    AddVariable( tableLighting,speclevel,"Specular Level",IVariable::DT_SIMPLE );
    AddVariable( tableLighting,emissive,"Emissive Color",IVariable::DT_COLOR );

    speclevel->SetLimits( 0,100 );
    shininess->SetLimits( 0,255 );

    //////////////////////////////////////////////////////////////////////////
    // Init texture variables.
    //////////////////////////////////////////////////////////////////////////
    int i;
    for (i = 0; i < _countof(sUsedTextures); i++)
    {
      InitTextureVars( sUsedTextures[i].texId,sUsedTextures[i].name );
    }

    //////////////////////////////////////////////////////////////////////////
    // Init Vertex Deformation.
    //////////////////////////////////////////////////////////////////////////
    vertexMod.type->SetEnumList( enumVertexMod );
    AddVariable( tableVertexMod,vertexMod.type,"Type" );
    AddVariable( tableVertexMod,vertexMod.fDividerX,"Wave Length X" );
    AddVariable( tableVertexMod,vertexMod.fDividerY,"Wave Length Y" );
    AddVariable( tableVertexMod,vertexMod.vNoiseScale,"Noise Scale" );

    AddVariable( tableVertexMod,vertexMod.wave[0].table,"Wave X" );
    AddVariable( tableVertexMod,vertexMod.wave[1].table,"Wave Y" );

    for (int i = 0; i < 2; i++)
    {
      vertexMod.wave[i].waveFormType->SetEnumList( enumWaveType );
      AddVariable( vertexMod.wave[i].table,vertexMod.wave[i].waveFormType,"Type" );
      AddVariable( vertexMod.wave[i].table,vertexMod.wave[i].level,"Level" );
      AddVariable( vertexMod.wave[i].table,vertexMod.wave[i].amplitude,"Amplitude" );
      AddVariable( vertexMod.wave[i].table,vertexMod.wave[i].phase,"Phase" );
      AddVariable( vertexMod.wave[i].table,vertexMod.wave[i].frequency,"Frequency" );
    }

    return m_vars;
  }

  CVarBlock* CreateMtlLayersPresetsVars()
  {
    m_varsMtlLayersPresets = new CVarBlock;    
    return m_varsMtlLayersPresets;
  }

  CVarBlock* CreateMtlLayerShaderVars( int nLayer )
  {
    m_varsMtlLayersShader[nLayer] = new CVarBlock;

    //////////////////////////////////////////////////////////////////////////
    // Init Material Layers
    //////////////////////////////////////////////////////////////////////////

    AddVariable(  m_varsMtlLayersShader[nLayer], materialLayers[nLayer].shader, "Shader");

    //////////////////////////////////////////////////////////////////////////
    // Fill shaders enum.
    //////////////////////////////////////////////////////////////////////////
    CVarEnumList<CString> *enumShaders  = new CVarEnumList<CString>;
    {
      CShaderEnum *pShaderEnum = GetIEditor()->GetShaderEnum();
      pShaderEnum->EnumShaders();
      for (int i = 0; i < pShaderEnum->GetShaderCount(); i++)
      {
        CString shaderName = pShaderEnum->GetShader(i);
        if (strstri(shaderName,".layer") !=0 || strstri(shaderName,"layer") == 0)
          continue;
        
        enumShaders->AddItem( shaderName,shaderName );
      }
    }
    
    //enumShaders->AddItem( "NoDraw","NoDraw" );

    materialLayers[nLayer].shader->SetEnumList( enumShaders );    
    
    AddVariable( m_varsMtlLayersShader[nLayer], materialLayers[nLayer].bNoDraw, "No Draw", IVariable::DT_SIMPLE);    
    
    return m_varsMtlLayersShader[nLayer];
  }

private:
  //////////////////////////////////////////////////////////////////////////
  void InitTextureVars( int id,const CString &name )
  {
    textureVars[id]->SetFlags( IVariable::UI_BOLD );
    AddVariable( tableTexture,textureVars[id],name,IVariable::DT_TEXTURE );
    // Add variables from STextureVars structure.
    if (id == EFTT_BUMP || id == EFTT_ENV)
    {
      AddVariable( textureVars[id],textures[id].amount,"Amount" );
      textures[id].amount->SetLimits( 0,255 );
    }
    AddVariable( textureVars[id],textures[id].etextype,"TexType",IVariable::DT_SIMPLE );
    AddVariable( textureVars[id],textures[id].filter,"Filter",IVariable::DT_SIMPLE );

    if (id == EFTT_DECAL_OVERLAY)
    {
      AddVariable( textureVars[id],bAdditiveDecal,"Additive Decal",IVariable::DT_SIMPLE );
    }

    AddVariable( textureVars[id],textures[id].is_tcgprojected,"IsProjectedTexGen",IVariable::DT_SIMPLE );
    AddVariable( textureVars[id],textures[id].etcgentype,"TexGenType",IVariable::DT_SIMPLE );

    //////////////////////////////////////////////////////////////////////////
    // Tiling table.
    AddVariable( textureVars[id],textures[id].tableTiling,"Tiling" );
    {
      CVariableArray& table = textures[id].tableTiling;
      table.SetFlags( IVariable::UI_BOLD );
      AddVariable( table,textures[id].is_tile[0],"IsTileU" );
      AddVariable( table,textures[id].is_tile[1],"IsTileV" );


      AddVariable( table,textures[id].tiling[0],"TileU" );
      AddVariable( table,textures[id].tiling[1],"TileV" );
      AddVariable( table,textures[id].offset[0],"OffsetU" );
      AddVariable( table,textures[id].offset[1],"OffsetV" );
      AddVariable( table,textures[id].rotate[0],"RotateU" );
      AddVariable( table,textures[id].rotate[1],"RotateV" );
      AddVariable( table,textures[id].rotate[2],"RotateW" );
    }

    //////////////////////////////////////////////////////////////////////////
    // Rotator tables.
    AddVariable( textureVars[id],textures[id].tableRotator,"Rotator" );
    {
      CVariableArray& table = textures[id].tableRotator;
      table.SetFlags( IVariable::UI_BOLD );
      AddVariable( table,textures[id].etcmrotatetype,"Type" );
      AddVariable( table,textures[id].tcmrotoscrate[0],"RateU" );
      AddVariable( table,textures[id].tcmrotoscrate[1],"RateV" );
      AddVariable( table,textures[id].tcmrotoscrate[2],"RateW" );
      AddVariable( table,textures[id].tcmrotoscphase[0],"PhaseU" );
      AddVariable( table,textures[id].tcmrotoscphase[1],"PhaseV" );
      AddVariable( table,textures[id].tcmrotoscphase[2],"PhaseW" );
      AddVariable( table,textures[id].tcmrotoscamplitude[0],"AmplitudeU" );
      AddVariable( table,textures[id].tcmrotoscamplitude[1],"AmplitudeV" );
      AddVariable( table,textures[id].tcmrotoscamplitude[2],"AmplitudeW" );
      AddVariable( table,textures[id].tcmrotosccenter[0],"CenterU" );
      AddVariable( table,textures[id].tcmrotosccenter[1],"CenterV" );
      AddVariable( table,textures[id].tcmrotosccenter[2],"CenterW" );
    }

    //////////////////////////////////////////////////////////////////////////
    // Oscillator table
    AddVariable( textureVars[id],textures[id].tableOscillator,"Oscillator" );
    {
      CVariableArray& table = textures[id].tableOscillator;
      table.SetFlags( IVariable::UI_BOLD );
      AddVariable( table,textures[id].etcmumovetype,"TypeU" );
      AddVariable( table,textures[id].etcmvmovetype,"TypeV" );
      AddVariable( table,textures[id].tcmuoscrate,"RateU" );
      AddVariable( table,textures[id].tcmvoscrate,"RateV" );
      AddVariable( table,textures[id].tcmuoscphase,"PhaseU" );
      AddVariable( table,textures[id].tcmvoscphase,"PhaseV" );
      AddVariable( table,textures[id].tcmuoscamplitude,"AmplitudeU" );
      AddVariable( table,textures[id].tcmvoscamplitude,"AmplitudeV" );
    }

    //////////////////////////////////////////////////////////////////////////
    // Assign enums tables to variable.
    //////////////////////////////////////////////////////////////////////////
    textures[id].etextype->SetEnumList( enumTexType );
    textures[id].etcgentype->SetEnumList( enumTexGenType );
    textures[id].etcmrotatetype->SetEnumList( enumTexModRotateType );
    textures[id].etcmumovetype->SetEnumList( enumTexModUMoveType );
    textures[id].etcmvmovetype->SetEnumList( enumTexModVMoveType );
    textures[id].filter->SetEnumList( enumTexFilterType );
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

  void SetTextureResources( const SInputShaderResources &sr,int texid );
  void GetTextureResources( SInputShaderResources &sr,int texid );
  Vec3 ToVec3( const ColorF &col ) { return Vec3(col.r,col.g,col.b); }
  ColorF ToCFColor( const Vec3 &col ) { return ColorF(col); }
};

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetShaderResources( const SInputShaderResources &sr )
{
  opacity = sr.m_Opacity;
  alphaTest = sr.m_AlphaRef;
  glowAmount = sr.m_GlowAmount;
  postEffectType = sr.m_PostEffects;

  diffuse = ToVec3(sr.m_LMaterial.m_Diffuse);

  speclevel = max(sr.m_LMaterial.m_Specular.r,max(sr.m_LMaterial.m_Specular.g,sr.m_LMaterial.m_Specular.b));
  if(speclevel>1.0f)
    specular = ToVec3(sr.m_LMaterial.m_Specular*(1.0f/speclevel));
  else
  {
    specular = ToVec3(sr.m_LMaterial.m_Specular);
    speclevel=1.0f;
  }

  emissive = ToVec3(sr.m_LMaterial.m_Emission);

  shininess = sr.m_LMaterial.m_SpecShininess;

  SetVertexDeform( sr );

  for (int i = 0; i < _countof(sUsedTextures); i++)
  {
    SetTextureResources( sr,sUsedTextures[i].texId );
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetShaderResources( SInputShaderResources &sr )
{
  sr.m_Opacity = opacity;
  sr.m_AlphaRef = alphaTest;
  sr.m_GlowAmount = glowAmount;
  sr.m_PostEffects = postEffectType;

  sr.m_LMaterial.m_Diffuse = ToCFColor(diffuse);
  sr.m_LMaterial.m_Specular = ToCFColor(specular) * speclevel;
  sr.m_LMaterial.m_Emission = ToCFColor(emissive);
  sr.m_LMaterial.m_SpecShininess = shininess;

  GetVertexDeform( sr );
  //int vmdType = vertexModif;
  //sr.m_DeformInfo.m_eType = (EDeformType)vmdType;

  for (int i = 0; i < _countof(sUsedTextures); i++)
  {
    GetTextureResources( sr,sUsedTextures[i].texId );
  }
}

inline float RoundDegree( float val )
{
  //double v = floor(val*100.0f);
  //return v*0.01f;
  return (float)((int)(val*100+0.5f)) * 0.01f;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetTextureResources( const SInputShaderResources &sr,int tex )
{
  /*
  // Enable/Disable texture map, depending on the mask.
  int flags = textureVars[tex].GetFlags();
  if ((1 << tex) & texUsageMask)
  flags &= ~IVariable::UI_DISABLED;
  else
  flags |= IVariable::UI_DISABLED;
  textureVars[tex].SetFlags( flags );
  */

  CString texFilename = sr.m_Textures[tex].m_Name.c_str();

  texFilename = Path::ToUnixPath(texFilename);

  textureVars[tex]->Set( texFilename );
  textures[tex].amount = sr.m_Textures[tex].m_Amount;
  textures[tex].is_tile[0] = sr.m_Textures[tex].m_bUTile;
  textures[tex].is_tile[1] = sr.m_Textures[tex].m_bVTile;

  textures[tex].tiling[0] = sr.m_Textures[tex].m_TexModificator.m_Tiling[0];
  textures[tex].tiling[1] = sr.m_Textures[tex].m_TexModificator.m_Tiling[1];
  textures[tex].offset[0] = sr.m_Textures[tex].m_TexModificator.m_Offs[0];
  textures[tex].offset[1] = sr.m_Textures[tex].m_TexModificator.m_Offs[1];
  textures[tex].filter = (int)sr.m_Textures[tex].m_Filter;
  textures[tex].etextype = sr.m_Textures[tex].m_Sampler.m_eTexType;
  textures[tex].etcgentype = sr.m_Textures[tex].m_TexModificator.m_eTGType;
  textures[tex].etcmumovetype = sr.m_Textures[tex].m_TexModificator.m_eUMoveType;
  textures[tex].etcmvmovetype = sr.m_Textures[tex].m_TexModificator.m_eVMoveType;
  textures[tex].etcmrotatetype = sr.m_Textures[tex].m_TexModificator.m_eRotType;
  textures[tex].is_tcgprojected = sr.m_Textures[tex].m_TexModificator.m_bTexGenProjected;
  textures[tex].tcmuoscrate = sr.m_Textures[tex].m_TexModificator.m_UOscRate;
  textures[tex].tcmuoscphase = sr.m_Textures[tex].m_TexModificator.m_UOscPhase;
  textures[tex].tcmuoscamplitude = sr.m_Textures[tex].m_TexModificator.m_UOscAmplitude;
  textures[tex].tcmvoscrate = sr.m_Textures[tex].m_TexModificator.m_VOscRate;
  textures[tex].tcmvoscphase = sr.m_Textures[tex].m_TexModificator.m_VOscPhase;
  textures[tex].tcmvoscamplitude = sr.m_Textures[tex].m_TexModificator.m_VOscAmplitude;
  for (int i=0; i<3; i++)
  {
    textures[tex].rotate[i] = RoundDegree(Word2Degr(sr.m_Textures[tex].m_TexModificator.m_Rot[i]));
    textures[tex].tcmrotoscrate[i] = RoundDegree(Word2Degr(sr.m_Textures[tex].m_TexModificator.m_RotOscRate[i]));
    textures[tex].tcmrotoscphase[i] = RoundDegree(Word2Degr(sr.m_Textures[tex].m_TexModificator.m_RotOscPhase[i]));
    textures[tex].tcmrotoscamplitude[i] = RoundDegree(Word2Degr(sr.m_Textures[tex].m_TexModificator.m_RotOscAmplitude[i]));
    textures[tex].tcmrotosccenter[i] = sr.m_Textures[tex].m_TexModificator.m_RotOscCenter[i];
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetTextureResources( SInputShaderResources &sr,int tex )
{
  CString texFilename;
  textureVars[tex]->Get(texFilename);
  texFilename = Path::ToUnixPath(texFilename);
  sr.m_Textures[tex].m_Name = (const char*)texFilename;
  sr.m_Textures[tex].m_Amount = textures[tex].amount;
  sr.m_Textures[tex].m_bUTile = textures[tex].is_tile[0];
  sr.m_Textures[tex].m_bVTile = textures[tex].is_tile[1];

  sr.m_Textures[tex].m_TexModificator.m_bTexGenProjected = textures[tex].is_tcgprojected;
  sr.m_Textures[tex].m_TexModificator.m_Tiling[0] = textures[tex].tiling[0];
  sr.m_Textures[tex].m_TexModificator.m_Tiling[1] = textures[tex].tiling[1];
  sr.m_Textures[tex].m_TexModificator.m_Offs[0] = textures[tex].offset[0];
  sr.m_Textures[tex].m_TexModificator.m_Offs[1] = textures[tex].offset[1];
  sr.m_Textures[tex].m_Filter = (int)textures[tex].filter;
  sr.m_Textures[tex].m_Sampler.m_eTexType = textures[tex].etextype;
  sr.m_Textures[tex].m_TexModificator.m_eRotType = textures[tex].etcmrotatetype;
  sr.m_Textures[tex].m_TexModificator.m_eTGType = textures[tex].etcgentype;
  sr.m_Textures[tex].m_TexModificator.m_eUMoveType = textures[tex].etcmumovetype;
  sr.m_Textures[tex].m_TexModificator.m_eVMoveType = textures[tex].etcmvmovetype;
  sr.m_Textures[tex].m_TexModificator.m_UOscRate = textures[tex].tcmuoscrate;
  sr.m_Textures[tex].m_TexModificator.m_UOscPhase = textures[tex].tcmuoscphase;
  sr.m_Textures[tex].m_TexModificator.m_UOscAmplitude = textures[tex].tcmuoscamplitude;
  sr.m_Textures[tex].m_TexModificator.m_VOscRate = textures[tex].tcmvoscrate;
  sr.m_Textures[tex].m_TexModificator.m_VOscPhase = textures[tex].tcmvoscphase;
  sr.m_Textures[tex].m_TexModificator.m_VOscAmplitude = textures[tex].tcmvoscamplitude;
  for (int i=0; i<3; i++)
  {
    sr.m_Textures[tex].m_TexModificator.m_Rot[i] = Degr2Word(textures[tex].rotate[i]);
    sr.m_Textures[tex].m_TexModificator.m_RotOscRate[i] = Degr2Word(textures[tex].tcmrotoscrate[i]);
    sr.m_Textures[tex].m_TexModificator.m_RotOscPhase[i] = Degr2Word(textures[tex].tcmrotoscphase[i]);
    sr.m_Textures[tex].m_TexModificator.m_RotOscAmplitude[i] = Degr2Word(textures[tex].tcmrotoscamplitude[i]);
    sr.m_Textures[tex].m_TexModificator.m_RotOscCenter[i] = textures[tex].tcmrotosccenter[i];
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::SetVertexDeform( const SInputShaderResources &sr )
{
  vertexMod.type = (int)sr.m_DeformInfo.m_eType;
  vertexMod.fDividerX = sr.m_DeformInfo.m_fDividerX;
  vertexMod.fDividerY = sr.m_DeformInfo.m_fDividerY;
  vertexMod.vNoiseScale = sr.m_DeformInfo.m_vNoiseScale;

  vertexMod.wave[0].waveFormType = sr.m_DeformInfo.m_WaveX.m_eWFType;
  vertexMod.wave[0].amplitude = sr.m_DeformInfo.m_WaveX.m_Amp;
  vertexMod.wave[0].level = sr.m_DeformInfo.m_WaveX.m_Level;
  vertexMod.wave[0].phase = sr.m_DeformInfo.m_WaveX.m_Phase;
  vertexMod.wave[0].frequency = sr.m_DeformInfo.m_WaveX.m_Freq;

  vertexMod.wave[1].waveFormType = sr.m_DeformInfo.m_WaveY.m_eWFType;
  vertexMod.wave[1].amplitude = sr.m_DeformInfo.m_WaveY.m_Amp;
  vertexMod.wave[1].level = sr.m_DeformInfo.m_WaveY.m_Level;
  vertexMod.wave[1].phase = sr.m_DeformInfo.m_WaveY.m_Phase;
  vertexMod.wave[1].frequency = sr.m_DeformInfo.m_WaveY.m_Freq;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialUI::GetVertexDeform( SInputShaderResources &sr )
{
  sr.m_DeformInfo.m_eType = (EDeformType)((int)vertexMod.type);
  sr.m_DeformInfo.m_fDividerX = vertexMod.fDividerX;
  sr.m_DeformInfo.m_fDividerY = vertexMod.fDividerY;
  sr.m_DeformInfo.m_vNoiseScale = vertexMod.vNoiseScale;

  sr.m_DeformInfo.m_WaveX.m_eWFType = (EWaveForm)((int)vertexMod.wave[0].waveFormType);
  sr.m_DeformInfo.m_WaveX.m_Amp = vertexMod.wave[0].amplitude;
  sr.m_DeformInfo.m_WaveX.m_Level = vertexMod.wave[0].level;
  sr.m_DeformInfo.m_WaveX.m_Phase = vertexMod.wave[0].phase;
  sr.m_DeformInfo.m_WaveX.m_Freq = vertexMod.wave[0].frequency;

  sr.m_DeformInfo.m_WaveY.m_eWFType = (EWaveForm)((int)vertexMod.wave[1].waveFormType);
  sr.m_DeformInfo.m_WaveY.m_Amp = vertexMod.wave[1].amplitude;
  sr.m_DeformInfo.m_WaveY.m_Level = vertexMod.wave[1].level;
  sr.m_DeformInfo.m_WaveY.m_Phase = vertexMod.wave[1].phase;
  sr.m_DeformInfo.m_WaveY.m_Freq = vertexMod.wave[1].frequency;
}

void CMaterialUI::SetFromMaterial( CMaterial *mtl )
{
  CString shaderName = mtl->GetShaderName();
  if (!shaderName.IsEmpty())
  {
    // Capitalize first letter.
    shaderName = CString((char)toupper(shaderName[0])) + shaderName.Mid(1);
  }

  shader = shaderName;

  int mtlFlags = mtl->GetFlags();
  bNoShadow = (mtlFlags & MTL_FLAG_NOSHADOW);
  bAdditive = (mtlFlags & MTL_FLAG_ADDITIVE);
  bAdditiveDecal = (mtlFlags & MTL_FLAG_ADDITIVE_DECAL);
  bWire = (mtlFlags & MTL_FLAG_WIRE);  
  b2Sided = (mtlFlags & MTL_FLAG_2SIDED);  
  bScatter = (mtlFlags & MTL_FLAG_SCATTER);
  bHideAfterBreaking = (mtlFlags & MTL_FLAG_HIDEONBREAK);
	bForceShadowBias = (mtlFlags & MTL_FLAG_FORCESHADOWBIAS);

  texUsageMask = mtl->GetTexmapUsageMask();
  // Detail, decal and custom textures are always active.
  texUsageMask |= 1 << EFTT_DETAIL_OVERLAY;
  texUsageMask |= 1 << EFTT_DECAL_OVERLAY;
  texUsageMask |= 1 << EFTT_CUSTOM;
  texUsageMask |= 1 << EFTT_CUSTOM_SECONDARY;
  if ((texUsageMask & (1<<EFTT_BUMP)) || (texUsageMask & (1<<EFTT_NORMALMAP)))
  {
    texUsageMask |= 1 << EFTT_BUMP;
    texUsageMask |= 1 << EFTT_NORMALMAP;
  }
  surfaceType = mtl->GetSurfaceTypeName();

  SetShaderResources( mtl->GetShaderResources() );

  // set each material layer
  SMaterialLayerResources *pMtlLayerResources = mtl->GetMtlLayerResources();
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)     
  {    
    materialLayers[l].shader = pMtlLayerResources[l].m_shaderName;    
    materialLayers[l].bNoDraw = pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_NODRAW;    
  }
}

void CMaterialUI::SetToMaterial( CMaterial *mtl )
{
  int mtlFlags = mtl->GetFlags();
  if (bNoShadow)
    mtlFlags |= MTL_FLAG_NOSHADOW;
  else
    mtlFlags &= ~MTL_FLAG_NOSHADOW;
  if (bAdditive)
    mtlFlags |= MTL_FLAG_ADDITIVE;
  else
    mtlFlags &= ~MTL_FLAG_ADDITIVE;
  if (bAdditiveDecal)
    mtlFlags |= MTL_FLAG_ADDITIVE_DECAL;
  else
    mtlFlags &= ~MTL_FLAG_ADDITIVE_DECAL;
  if (bWire)
    mtlFlags |= MTL_FLAG_WIRE;
  else
    mtlFlags &= ~MTL_FLAG_WIRE;
  if (b2Sided)
    mtlFlags |= MTL_FLAG_2SIDED;
  else
    mtlFlags &= ~MTL_FLAG_2SIDED;

  if (bScatter)
    mtlFlags |= MTL_FLAG_SCATTER;
  else
    mtlFlags &= ~MTL_FLAG_SCATTER;

  if (bHideAfterBreaking)
    mtlFlags |= MTL_FLAG_HIDEONBREAK;
  else
    mtlFlags &= ~MTL_FLAG_HIDEONBREAK;

	if (bForceShadowBias)
		mtlFlags |= MTL_FLAG_FORCESHADOWBIAS;
	else
		mtlFlags &= ~MTL_FLAG_FORCESHADOWBIAS;

  mtl->SetFlags(mtlFlags);

  // set each material layer
  SMaterialLayerResources *pMtlLayerResources = mtl->GetMtlLayerResources();
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {    
    if( pMtlLayerResources[l].m_shaderName != materialLayers[l].shader)
    {
      pMtlLayerResources[l].m_shaderName = materialLayers[l].shader;
      pMtlLayerResources[l].m_bRegetPublicParams = true;
    }

    if ( materialLayers[l].bNoDraw  )
      pMtlLayerResources[l].m_nFlags |= MTL_LAYER_USAGE_NODRAW;
    else
      pMtlLayerResources[l].m_nFlags &= ~MTL_LAYER_USAGE_NODRAW;
  }

  mtl->SetSurfaceTypeName( surfaceType );

  // If shader name is different reload shader.
  mtl->SetShaderName( shader );
  GetShaderResources( mtl->GetShaderResources() );

}

void CMaterialUI::SetTextureNames( CMaterial *mtl )
{
  SInputShaderResources &sr = mtl->GetShaderResources();
  for (int i = 0; i < _countof(sUsedTextures); i++)
  {
    int tex = sUsedTextures[i].texId;
    CString texFilename = sr.m_Textures[tex].m_Name.c_str();
    textureVars[tex]->Set( texFilename );
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CMtlPickCallback : public IPickObjectCallback
{
public:
  CMtlPickCallback() { m_bActive = true; };
  //! Called when object picked.
  virtual void OnPick( CBaseObject *picked )
  {
    m_bActive = false;
    CMaterial *pMtl = picked->GetMaterial();
    if (pMtl)
      GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pMtl );
    delete this;
  }
  //! Called when pick mode canceled.
  virtual void OnCancelPick()
  {
    m_bActive = false;
    delete this;
  }
  //! Return true if specified object is pickable.
  virtual bool OnPickFilter( CBaseObject *filterObject )
  {
    // Check if object have material.
    if (filterObject->GetMaterial())
      return true;
    else
      return false;
  }
  static bool IsActive() { return m_bActive; };
private:
  static bool m_bActive;
};
bool CMtlPickCallback::m_bActive = false;
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// CMaterialDialog implementation.
//////////////////////////////////////////////////////////////////////////
CMaterialDialog::CMaterialDialog()
: CBaseLibraryDialog(IDD_DB_ENTITY, NULL)
{
  m_pMatManager = GetIEditor()->GetMaterialManager();
  m_pItemManager = m_pMatManager;

  m_publicVarsItems = 0;

  m_shaderGenParamsVars = 0;
  m_shaderGenParamsVarsItem = 0;

  m_varsMtlLayersPresets = 0;
  m_varsMtlLayersPresetsItems = 0;

  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {
    m_varsMtlLayersShader[l] = 0;
    m_varsMtlLayersShaderItems[l] = 0;

    m_varsMtlLayersShaderParams[l] = 0;
    m_varsMtlLayersShaderParamsItems[l] = 0;
  }

  m_dragImage = 0;
  m_hDropItem = 0;

  m_pPreviewCtrl = 0;

  m_pMaterialUI = new CMaterialUI;

	m_lastUpdateTime = 0.0f;

  // Immediately create dialog.
  Create( IDD_DATABASE,NULL );
}

CMaterialDialog::~CMaterialDialog()
{
  m_wndMtlBrowser.SetImageListCtrl(NULL);

  delete m_pMaterialUI;
  m_vars = 0;
  m_publicVars = 0;
  m_publicVarsItems = 0;
  m_shaderGenParamsVars = 0;
  m_shaderGenParamsVarsItem = 0;

  m_varsMtlLayersPresets = 0;
  m_varsMtlLayersPresetsItems = 0;

  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {
    m_varsMtlLayersShader[l] = 0;
    m_varsMtlLayersShaderItems[l] = 0;

    m_varsMtlLayersShaderParams[l] = 0;
    m_varsMtlLayersShaderParamsItems[l] = 0;
  }

  m_propsCtrl.RemoveAllItems();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::PostNcDestroy()
{
  delete this;
}

void CMaterialDialog::DoDataExchange(CDataExchange* pDX)
{
  CBaseLibraryDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMaterialDialog, CBaseLibraryDialog)
  ON_COMMAND( ID_DB_ADD,OnAddItem )


  ON_COMMAND( ID_DB_SELECTASSIGNEDOBJECTS,OnSelectAssignedObjects )
  ON_COMMAND( ID_DB_MTL_ASSIGNTOSELECTION,OnAssignMaterialToSelection )
  ON_COMMAND( ID_DB_MTL_GETFROMSELECTION,OnGetMaterialFromSelection )
  ON_COMMAND( ID_DB_MTL_RESETMATERIAL,OnResetMaterialOnSelection )
  ON_CBN_SELENDOK( IDC_MATERIAL_BROWSER_LISTTYPE,OnChangedBrowserListType )
  ON_UPDATE_COMMAND_UI( ID_DB_MTL_ASSIGNTOSELECTION,OnUpdateAssignMtlToSelection )
  ON_UPDATE_COMMAND_UI( ID_DB_SELECTASSIGNEDOBJECTS,OnUpdateMtlSelected )
  ON_UPDATE_COMMAND_UI( ID_DB_MTL_GETFROMSELECTION,OnUpdateObjectSelected )
  ON_UPDATE_COMMAND_UI( ID_DB_MTL_RESETMATERIAL,OnUpdateObjectSelected )

  ON_COMMAND( ID_DB_MTL_PICK,OnPickMtl )
  ON_UPDATE_COMMAND_UI( ID_DB_MTL_PICK,OnUpdatePickMtl )

  ON_COMMAND( ID_DB_MTL_GENCUBEMAP,OnGenCubemap )

  ON_COMMAND( ID_DB_MTL_PREVIEW, OnMaterialPreview )

  ON_WM_SIZE()
  ON_WM_DESTROY()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnDestroy()
{
  int temp;
  int VSplitter;
  m_wndVSplitter.GetColumnInfo( 0,VSplitter,temp );
  AfxGetApp()->WriteProfileInt("Dialogs\\Materials","VSplitter",VSplitter );

  m_wndMtlBrowser.SetImageListCtrl(NULL);

  CBaseLibraryDialog::OnDestroy();
}

// CTVSelectKeyDialog message handlers
BOOL CMaterialDialog::OnInitDialog()
{
  CBaseLibraryDialog::OnInitDialog();

  InitToolbar(IDR_DB_MATERIAL_BAR);

  CRect rc;

  GetClientRect(rc);

  // Create status bar.
  {
    UINT indicators[] =
    {
      ID_SEPARATOR,           // status line indicator
    };
    VERIFY( m_statusBar.Create( this, WS_CHILD|WS_VISIBLE|CBRS_BOTTOM) );
    VERIFY( m_statusBar.SetIndicators( indicators,sizeof(indicators)/sizeof(UINT) ) );
  }

  //int h2 = rc.Height()/2;
  int h2 = 200;

  int VSplitter = AfxGetApp()->GetProfileInt("Dialogs\\Materials","VSplitter",200 );

  m_wndVSplitter.CreateStatic( this,1,2,WS_CHILD|WS_VISIBLE );
  m_wndRightSplitter.CreateStatic( this,2,1,WS_CHILD|WS_VISIBLE,AFX_IDW_PANE_FIRST+1 );

  //m_imageList.Create(IDB_MATERIAL_TREE, 16, 1, RGB (255, 0, 255));
  CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_MATERIAL_TREE,16,RGB(255,0,255) );

  m_wndMtlBrowser.Create( rc,&m_wndVSplitter,AFX_IDW_PANE_FIRST );
  m_wndMtlBrowser.SetListener(this);
  m_wndMtlBrowser.ShowWindow(SW_SHOW);

  // TreeCtrl must be already created.
  //m_treeCtrl.SetParent( &m_wndVSplitter );
  //m_treeCtrl.SetImageList(&m_imageList,TVSIL_NORMAL);

  m_propsCtrl.Create( WS_CHILD|WS_BORDER,rc,&m_wndVSplitter,AFX_IDW_PANE_FIRST+1 );

  m_vars = m_pMaterialUI->CreateVars();
  CPropertyItem *varsItems = m_propsCtrl.AddVarBlock( m_vars );

  m_publicVarsItems = m_propsCtrl.FindItemByVar( m_pMaterialUI->tableShaderParams.GetVar() );
  m_shaderGenParamsVarsItem = m_propsCtrl.FindItemByVar( m_pMaterialUI->tableShaderGenParams.GetVar() );

  // Create material layers presets property item
  m_varsMtlLayersPresets =  m_pMaterialUI->CreateMtlLayersPresetsVars();
  m_varsMtlLayersPresetsItems = m_propsCtrl.AddVarBlockAt( m_varsMtlLayersPresets, "Material Layers Presets", varsItems);

  // Create each material layers shader property item
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {        
    m_varsMtlLayersShader[l] =  m_pMaterialUI->CreateMtlLayerShaderVars( l );    

    CString no;
    no.Format("%d",l);        
    CString pLayerName = sUsedMtlLayers[l].name;//CString("[") + no + CString("] Shader");
    m_varsMtlLayersShaderItems[l] = m_propsCtrl.AddVarBlockAt( m_varsMtlLayersShader[l], pLayerName, m_varsMtlLayersPresetsItems, true);
  }

  m_propsCtrl.EnableWindow(FALSE);
  m_propsCtrl.ExpandAllChilds( m_propsCtrl.GetRootItem(),false );

  CPropertyItem *pItem = m_propsCtrl.FindItemByVar(& (*m_pMaterialUI->tableVertexMod));
  if(pItem)
    m_propsCtrl.Expand(pItem, false);

  m_mtlImageListCtrl.Create( ILC_STYLE_HORZ|WS_CHILD|WS_VISIBLE,CRect(0,0,300,200),this,1 );
  m_wndMtlBrowser.SetImageListCtrl( &m_mtlImageListCtrl );

  m_wndRightSplitter.SetPane( 0,0,&m_mtlImageListCtrl,CSize(VSplitter,70) );
  m_wndRightSplitter.SetPane( 1,0,&m_propsCtrl,CSize(VSplitter,70) );

  //m_wndVSplitter.SetPane( 0,0,&m_treeCtrl,CSize(VSplitter,100) );
  m_wndVSplitter.SetPane( 0,0,&m_wndMtlBrowser,CSize(VSplitter,100) );
  //m_wndVSplitter.SetPane( 0,1,&m_propsCtrl,CSize(VSplitter,100) );
  m_wndVSplitter.SetPane( 0,1,&m_wndRightSplitter,CSize(VSplitter,100) );

  RecalcLayout();

  ReloadLibs();

	m_propsCtrl.ShowWindow( SW_SHOW );

	m_wndVSplitter.MoveWindow(rc,FALSE);

	RecalcLayout();

  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
UINT CMaterialDialog::GetDialogMenuID()
{
  return IDR_DB_ENTITY;
};

//////////////////////////////////////////////////////////////////////////
// Create the toolbar
void CMaterialDialog::InitToolbar( UINT nToolbarResID )
{
	// Create Library toolbar.
	CXTPToolBar *pMtlToolBar = GetCommandBars()->Add( _T("MaterialToolBar"),xtpBarTop );
	pMtlToolBar->EnableCustomization(FALSE);
	VERIFY(pMtlToolBar->LoadToolBar( nToolbarResID ));

	{
		CRect rc(0,0,150,16);
		m_listTypeCtrl.Create( WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,rc,this,IDC_MATERIAL_BROWSER_LISTTYPE );
		m_listTypeCtrl.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
		m_listTypeCtrl.AddString( "Materials Library" );
		m_listTypeCtrl.AddString( "All Materials" );
		m_listTypeCtrl.AddString( "Used In Level" );
		m_listTypeCtrl.SetCurSel(0);
	}

	// Create library control.
	{
		CXTPControl *pCtrl = pMtlToolBar->GetControls()->FindControl(xtpControlButton, IDC_MATERIAL_BROWSER_LISTTYPE, TRUE, FALSE);
		if (pCtrl)
		{
			CXTPControlCustom* pCustomCtrl = (CXTPControlCustom*)pMtlToolBar->GetControls()->SetControlType(pCtrl->GetIndex(),xtpControlCustom);
			pCustomCtrl->SetSize( CSize(150,16) );
			pCustomCtrl->SetControl( &m_listTypeCtrl );
			pCustomCtrl->SetFlags(xtpFlagManualUpdate);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	if (m_wndVSplitter)
	{
		m_wndVSplitter.MoveWindow(CRect(0,0,cx,cy),FALSE);
	}
	RecalcLayout();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
  switch (event)
  {
  case eNotify_OnBeginNewScene:
    m_wndMtlBrowser.ClearItems();
    break;

  case eNotify_OnBeginSceneOpen:
    m_wndMtlBrowser.ClearItems();
    break;

  case eNotify_OnCloseScene:
    m_wndMtlBrowser.ClearItems();
    break;

	case eNotify_OnIdleUpdate:
		OnIdleUpdate();
		break;
  }
  CBaseLibraryDialog::OnEditorNotifyEvent(event);
}


//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnIdleUpdate()
{
	if(m_lastUpdateTime>0.0001f)
	{
		if(gEnv->pTimer->GetCurrTime() - m_lastUpdateTime>1.0f)
		{
			CMaterial *mtl = m_pMatManager->GetCurrentMaterial();
			mtl->Save();
			m_lastUpdateTime = 0.0f;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
HTREEITEM CMaterialDialog::InsertItemToTree( CBaseLibraryItem *pItem,HTREEITEM hParent )
{
  /*
  CMaterial *pMtl = (CMaterial*)pItem;
  if (pMtl->GetParent())
  {
  if (!hParent || hParent == TVI_ROOT || m_treeCtrl.GetItemData(hParent) == 0)
  return 0;
  }

  HTREEITEM hMtlItem = CBaseLibraryDialog::InsertItemToTree( pItem,hParent );

  for (int i = 0; i < pMtl->GetSubMaterialCount(); i++)
  {
  CMaterial *pSubMtl = pMtl->GetSubMaterial(i);
  CBaseLibraryDialog::InsertItemToTree( pSubMtl,hMtlItem );
  }
  */
  return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::ReloadItems()
{
  m_wndMtlBrowser.ReloadItems( CMaterialBrowserCtrl::VIEW_MATLIB );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnAddItem()
{
  m_wndMtlBrowser.OnAddNewMaterial();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::SetMaterialVars( CMaterial *mtl )
{
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::UpdateShaderParamsUI( CMaterial *pMtl )
{
  //////////////////////////////////////////////////////////////////////////
  // Shader Gen Mask.
  //////////////////////////////////////////////////////////////////////////
  if (m_shaderGenParamsVarsItem)
  {
    m_shaderGenParamsVars = pMtl->GetShaderGenParamsVars();
    if (m_shaderGenParamsVars)
      m_propsCtrl.ReplaceVarBlock( m_shaderGenParamsVarsItem,m_shaderGenParamsVars );
    else
      m_propsCtrl.RemoveAllChilds( m_shaderGenParamsVarsItem );
  }

  //////////////////////////////////////////////////////////////////////////
  // Shader Public Params.
  //////////////////////////////////////////////////////////////////////////
  if (m_publicVarsItems)
  {
    m_publicVars = pMtl->GetPublicVars( pMtl->GetShaderResources() );
    if (m_publicVars)
      m_propsCtrl.ReplaceVarBlock( m_publicVarsItems,m_publicVars );
    else
      m_propsCtrl.RemoveAllChilds( m_publicVarsItems );
  }
  //////////////////////////////////////////////////////////////////////////

}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::SelectItem( CBaseLibraryItem *item,bool bForceReload )
{
  static bool bNoRecursiveSelect = false;
  if (bNoRecursiveSelect)
    return;

  m_propsCtrl.Flush();

  bool bChanged = item != m_pCurrentItem || bForceReload;
  CBaseLibraryDialog::SelectItem( item,bForceReload );

  if (!bChanged)
    return;

  bNoRecursiveSelect = true;
  m_wndMtlBrowser.SelectItem(item);
  bNoRecursiveSelect = false;

  // Empty preview control.
  //m_previewCtrl.SetEntity(0);
  if(m_pPreviewCtrl)
    m_pPreviewCtrl->SetEntity(0);
  m_pMatManager->SetCurrentMaterial( (CMaterial*)item );

  if (!item)
  {
    m_statusBar.SetWindowText("");
    m_propsCtrl.EnableWindow(FALSE);
    return;
  }

  m_wndMtlBrowser.SelectItem(item);

  // Render preview geometry with current material
  CMaterial *mtl = m_pMatManager->GetCurrentMaterial();
  m_statusBar.SetWindowText(mtl->GetName());

  if (mtl->IsFromEngine())
  {
    m_statusBar.SetWindowText( mtl->GetName() + " (Old FC Material)" );
  }
  else if (mtl->IsDummy())
  {
    m_statusBar.SetWindowText( mtl->GetName() + " (Not Found)" );
  }
  else if (!mtl->CanModify())
  {
    m_statusBar.SetWindowText( mtl->GetName() + " (Read Only)" );
  }
  else if (mtl->IsPureChild())
  {
    m_statusBar.SetWindowText( mtl->GetName() + " (SubMaterial)" );
  }

  if (mtl->IsMultiSubMaterial())
  {
    // Cannot edit it.
    m_propsCtrl.EnableWindow(FALSE);
    m_propsCtrl.EnableUpdateCallback(false);
    return;
  }

  m_propsCtrl.EnableWindow(TRUE);
  m_propsCtrl.EnableUpdateCallback(false);

  UpdatePreview();

  // Update variables.
  m_propsCtrl.EnableUpdateCallback(false);
  m_pMaterialUI->SetFromMaterial( mtl);
  m_propsCtrl.EnableUpdateCallback(true);

  //////////////////////////////////////////////////////////////////////////
  // Set Shader Params for material layers
  //////////////////////////////////////////////////////////////////////////

  SMaterialLayerResources *pMtlLayerResources = mtl->GetMtlLayerResources();
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {
    SMaterialLayerResources *pCurrResource = &pMtlLayerResources[l];    
    // delete old property item
    if ( m_varsMtlLayersShaderParamsItems[l] )
    {
      m_propsCtrl.DeleteItem( m_varsMtlLayersShaderParamsItems[l] );
      m_varsMtlLayersShaderParamsItems[l] = 0;      
    }

    m_varsMtlLayersShaderParams[l] = mtl->GetPublicVars( pCurrResource->m_shaderResources );

    if ( m_varsMtlLayersShaderParams[l] )
    {
      m_varsMtlLayersShaderParamsItems[l] = m_propsCtrl.AddVarBlockAt( m_varsMtlLayersShaderParams[l], "Shader Params", m_varsMtlLayersShaderItems[l] );
    }
  }

  //////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////
  // Set Shader Gen Params.
  //////////////////////////////////////////////////////////////////////////
  UpdateShaderParamsUI(mtl);
  //////////////////////////////////////////////////////////////////////////

  m_propsCtrl.SetUpdateCallback( functor(*this,&CMaterialDialog::OnUpdateProperties) );
  m_propsCtrl.EnableUpdateCallback(true);

  if (mtl->IsDummy())
  {
    m_propsCtrl.EnableWindow(FALSE);
  }
  else
  {
    m_propsCtrl.EnableWindow(TRUE);
    m_propsCtrl.SetUpdateCallback( functor(*this,&CMaterialDialog::OnUpdateProperties) );
    m_propsCtrl.EnableUpdateCallback(true);
    if (mtl->CanModify())
    {
      // Material can be modified.
      m_propsCtrl.SetFlags( m_propsCtrl.GetFlags() & (~CPropertyCtrl::F_GRAYED) );
    }
    else
    {
      // Material cannot be modified.
      m_propsCtrl.SetFlags( m_propsCtrl.GetFlags() | CPropertyCtrl::F_GRAYED );
    }
  }
  if (mtl)
  {
    m_mtlImageListCtrl.SelectMaterial( mtl );
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateProperties( IVariable *var )
{
  CMaterial *mtl = GetSelectedMaterial();
  if (!mtl)
    return;
	
	m_lastUpdateTime = gEnv->pTimer->GetCurrTime();

  bool bShaderChanged = (m_pMaterialUI->shader == var);
  bool bShaderGenMaskChanged = false;
  if (m_shaderGenParamsVars)
    bShaderGenMaskChanged = m_shaderGenParamsVars->IsContainVariable( var );

  bool bMtlLayersChanged = false;  
  SMaterialLayerResources *pMtlLayerResources = mtl->GetMtlLayerResources();  
  int nCurrLayer = -1;

  // Check for shader changes
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
  {
    if( (m_pMaterialUI->materialLayers[l].shader == var) )
    {
      bMtlLayersChanged = true;    
      nCurrLayer = l;
      break;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Assign modified Shader Gen Params to shader.
  //////////////////////////////////////////////////////////////////////////
  if (bShaderGenMaskChanged)
  {
    mtl->SetShaderGenParamsVars(m_shaderGenParamsVars);
  }

  //////////////////////////////////////////////////////////////////////////
  // Invalidate material and save changes.
  //m_pMatManager->MarkMaterialAsModified(mtl);
  m_pMaterialUI->SetToMaterial( mtl );
  mtl->Update();

  //////////////////////////////////////////////////////////////////////////
  // Assign new public vars to material.
  // Must be after material update.
  //////////////////////////////////////////////////////////////////////////

  if (bShaderChanged||bShaderGenMaskChanged || bMtlLayersChanged)
    GetIEditor()->SuspendUndo();

  if ( m_publicVars != NULL && !bShaderChanged)
  {	
    mtl->SetPublicVars( m_publicVars, mtl->GetShaderResources(), mtl->GetShaderItem().m_pShaderResources, mtl->GetShaderItem().m_pShader);

  }

  bool bUpdateLayers = false;  
  for(int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)  
  {
    if ( m_varsMtlLayersShaderParams[l] != NULL && l != nCurrLayer)
    {
      SMaterialLayerResources *pCurrResource = &pMtlLayerResources[l];    
      SShaderItem &pCurrShaderItem = pCurrResource->m_pMatLayer->GetShaderItem();
      mtl->SetPublicVars( m_varsMtlLayersShaderParams[l], pCurrResource->m_shaderResources, pCurrShaderItem.m_pShaderResources, pCurrShaderItem.m_pShader); 
      bUpdateLayers = true;            
    }
  }

  if( bUpdateLayers )
  {
    mtl->UpdateMaterialLayers();
  }

  if (bShaderChanged||bShaderGenMaskChanged || bMtlLayersChanged)
    GetIEditor()->ResumeUndo();

  //////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////
  if (bShaderChanged || bShaderGenMaskChanged || bMtlLayersChanged)
  {
    m_pMaterialUI->SetFromMaterial( mtl );
  }
  //m_pMaterialUI->SetTextureNames( mtl );

  UpdatePreview();

  // When shader changed.
  if (bShaderChanged || bShaderGenMaskChanged || bMtlLayersChanged)
  {
    //////////////////////////////////////////////////////////////////////////
    // Set material layers params
    //////////////////////////////////////////////////////////////////////////

    if( bMtlLayersChanged) // only update changed shader in material layers 
    {      
      SMaterialLayerResources *pCurrResource = &pMtlLayerResources[nCurrLayer];

      // delete old property item
      if ( m_varsMtlLayersShaderParamsItems[nCurrLayer] )
      {
        m_propsCtrl.DeleteItem( m_varsMtlLayersShaderParamsItems[nCurrLayer] );
        m_varsMtlLayersShaderParamsItems[nCurrLayer] = 0;      
      } 

      m_varsMtlLayersShaderParams[nCurrLayer] = mtl->GetPublicVars( pCurrResource->m_shaderResources );

      if ( m_varsMtlLayersShaderParams[nCurrLayer] )
      {
        m_varsMtlLayersShaderParamsItems[nCurrLayer] = m_propsCtrl.AddVarBlockAt( m_varsMtlLayersShaderParams[nCurrLayer], "Shader Params", m_varsMtlLayersShaderItems[nCurrLayer] );
      }
    }

    UpdateShaderParamsUI(mtl);
  }

  if (bShaderGenMaskChanged || bShaderChanged || bMtlLayersChanged)
    m_propsCtrl.Invalidate();

  m_mtlImageListCtrl.InvalidateMaterial(mtl); 
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialDialog::GetSelectedMaterial()
{
  CBaseLibraryItem *pItem = m_pCurrentItem;
  return (CMaterial*)pItem;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnAssignMaterialToSelection()
{
  GetIEditor()->ExecuteCommand("Material.AssignToSelection");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnSelectAssignedObjects()
{
  GetIEditor()->ExecuteCommand("Material.SelectAssignedObjects");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnResetMaterialOnSelection()
{
  GetIEditor()->ExecuteCommand("Material.ResetSelection");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnGetMaterialFromSelection()
{
  GetIEditor()->ExecuteCommand("Material.SelectFromObject");
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::DeleteItem( CBaseLibraryItem *pItem )
{
  m_wndMtlBrowser.DeleteItem();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateMtlSelected( CCmdUI* pCmdUI )
{
  if (GetSelectedMaterial())
  {
    pCmdUI->Enable( TRUE );
  }
  else
  {
    pCmdUI->Enable( FALSE );
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateAssignMtlToSelection( CCmdUI* pCmdUI )
{
  if (GetSelectedMaterial() && (!GetIEditor()->GetSelection()->IsEmpty() || GetIEditor()->IsInPreviewMode()))
  {
    pCmdUI->Enable( TRUE );
  }
  else
  {
    pCmdUI->Enable( FALSE );
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdateObjectSelected( CCmdUI* pCmdUI )
{
  if (!GetIEditor()->GetSelection()->IsEmpty() || GetIEditor()->IsInPreviewMode())
  {
    pCmdUI->Enable( TRUE );
  }
  else
  {
    pCmdUI->Enable( FALSE );
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnPickMtl()
{
  if (GetIEditor()->GetEditTool() && strcmp(GetIEditor()->GetEditTool()->GetClassDesc()->ClassName(),"EditTool.PickMaterial") == 0)
  {
    GetIEditor()->SetEditTool( NULL );
  }
  else
    GetIEditor()->SetEditTool( "EditTool.PickMaterial" );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnUpdatePickMtl( CCmdUI* pCmdUI )
{
  if (GetIEditor()->GetEditTool() && strcmp(GetIEditor()->GetEditTool()->GetClassDesc()->ClassName(),"EditTool.PickMaterial") == 0)
  {
    pCmdUI->SetCheck(1);
  }
  else
  {
    pCmdUI->SetCheck(0);
  }
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnCopy()
{
  m_wndMtlBrowser.OnCopy();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnPaste()
{
  m_wndMtlBrowser.OnPaste();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnMaterialPreview()
{
  m_pPreviewDlg = new CMatEditPreviewDlg(0, this);
  //m_pPreviewDlg->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnGenCubemap()
{
  CMaterial *pMtl = GetSelectedMaterial();
  if (!pMtl)
    return;

  CBaseObject *pObject = GetIEditor()->GetSelectedObject();
  if (!pObject)
  {
    AfxMessageBox( "Select One Object to Generate Cubemap",MB_OK|MB_APPLMODAL|MB_ICONWARNING );
    return;
  }
  CString filename;
  if (!CFileUtil::SelectSaveFile( "*.*","dds","Textures",filename ))
    return;
  filename = Path::GetRelativePath(filename);
  if (filename.IsEmpty())
  {
    AfxMessageBox( _T("Texture Must be inside MasterCD folder!"),MB_OK|MB_APPLMODAL|MB_ICONWARNING );
    return;
  }

  CNumberDlg dlg( this,256,"Enter Cubemap Resolution" );
  dlg.SetInteger( true );
  if (dlg.DoModal() != IDOK)
    return;

  int res = 1;
  int size = dlg.GetValue();
  // Make size power of 2.
  for (int i = 0; i < 16; i++)
  {
    if (res*2 > size)
      break;
    res *= 2;
  }
  if (res > 4096)
  {
    AfxMessageBox( "Bad texture resolution.\nMust be power of 2 and less or equal to 4096",MB_OK|MB_APPLMODAL|MB_ICONWARNING );
    return;
  }
  // Hide object before Cubemap generation.
  pObject->SetHidden( true );
  GetIEditor()->GetRenderer()->EF_ScanEnvironmentCM( filename,res,pObject->GetWorldPos() );
  pObject->SetHidden( false );
  CString texname = Path::GetFileName(filename);
  CString path = Path::GetPath(filename);
  texname = Path::Make( path,texname+".dds" );
  // Assign this texname to current material.
  texname = Path::ToUnixPath(texname);

  pMtl->GetShaderResources().m_Textures[EFTT_ENV].m_Name = texname;
  pMtl->Update();
  // Update variables.
  m_propsCtrl.EnableUpdateCallback(false);
  m_pMaterialUI->SetFromMaterial( pMtl );
  m_propsCtrl.EnableUpdateCallback(true);
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialDialog::SetItemName( CBaseLibraryItem *item,const CString &groupName,const CString &itemName )
{
  assert( item );
  // Make prototype name.
  CString fullName = groupName + "/" + itemName;
  IDataBaseItem *pOtherItem = m_pItemManager->FindItemByName( fullName );
  if (pOtherItem && pOtherItem != item)
  {
    // Ensure uniqness of name.
    Warning( "Duplicate Item Name %s",(const char*)fullName );
    return false;
  }
  else
  {
    item->SetName( fullName );
  }
  return true;
}


//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnBrowserSelectItem( IDataBaseItem* pItem,bool bForce )
{
  SelectItem( (CBaseLibraryItem*)pItem,bForce );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::UpdatePreview()
{
};

//////////////////////////////////////////////////////////////////////////
void CMaterialDialog::OnChangedBrowserListType()
{
  int sel = m_listTypeCtrl.GetCurSel();
  if (sel == 0)
    m_wndMtlBrowser.ReloadItems( CMaterialBrowserCtrl::VIEW_MATLIB );
  else if (sel == 1)
    m_wndMtlBrowser.ReloadItems( CMaterialBrowserCtrl::VIEW_ALL );
  else if (sel == 2)
    m_wndMtlBrowser.ReloadItems( CMaterialBrowserCtrl::VIEW_LEVEL );

	m_pMatManager->SetCurrentMaterial(0);
}
