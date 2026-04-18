/*=============================================================================
  ParserBin.h : Script parser declarations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#ifndef PARSERBIN_H
#define PARSERBIN_H

extern TArray<bool> sfxIFDef;
extern TArray<bool> sfxIFIgnore;

// key tokens
enum EToken
{
  eT_unknown = 0,
  eT_include = 1,
  eT_define  = 2,
  eT_define_2 = 3,
  eT_undefine = 4,

  eT_if      = 5,
  eT_ifdef   = 6,
  eT_ifndef  = 7,
  eT_if_2    = 8,
  eT_ifdef_2 = 9,
  eT_ifndef_2 = 10,
  eT_elif    = 11,

  eT_endif   = 12,
  eT_else    = 13,
  eT_or      = 14,
  eT_and     = 15,
  eT_warning = 16,
  eT_register_env = 17,
  eT_ifcvar  = 18,
  eT_ifncvar = 19,
  eT_elifcvar = 20,
  eT_skip     = 21,
  eT_skip_1   = 22,
  eT_skip_2   = 23,

  eT_br_rnd_1 = 24,
  eT_br_rnd_2 = 25,
  eT_br_sq_1  = 26,
  eT_br_sq_2  = 27,
  eT_br_cv_1  = 28,
  eT_br_cv_2  = 29,
  eT_br_tr_1  = 30,
  eT_br_tr_2  = 31,
  eT_comma    = 32,
  eT_dot      = 33,
  eT_colon    = 34,
  eT_semicolumn = 35,
  eT_excl     = 36,    // !
  eT_quote    = 37,
  eT_sing_quote = 38,

  eT_question = 39,
  eT_eq       = 40,
  eT_plus     = 41,
  eT_minus    = 42,
  eT_div      = 43,
  eT_mul      = 44,
  eT_dot_math = 45,
  eT_mul_math = 46,
  eT_sqrt_math = 47,
  eT_exp_math = 48,
  eT_log_math = 49,
  eT_log2_math = 50,
  eT_sin_math = 51,
  eT_cos_math = 52,
  eT_sincos_math = 53,
  eT_floor_math  = 54,
  eT_ceil_math   = 55,
  eT_frac_math   = 56,
  eT_lerp_math   = 57,
  eT_abs_math    = 58,
  eT_clamp_math  = 59,
  eT_min_math    = 60,
  eT_max_math    = 61,
  eT_length_math = 62,

  eT_tex2D    = 63,
  eT_tex2Dproj = 64,
  eT_tex3D    = 65,
  eT_texCUBE  = 66,
  eT_SamplerState = 67,
  eT_SamplerComparisonState = 68,
  eT_sampler_state = 69,
  eT_Texture2D  = 70,
  eT_Texture2DArray  = 71,
  eT_Texture2DMS = 72,

  eT_float    = 73,
  eT_float2   = 74,
  eT_float3   = 75,
  eT_float4   = 76,
  eT_float4x4 = 77,
  eT_float3x4 = 78,
  eT_float2x4 = 79,
  eT_float3x3 = 80,
  eT_half     = 81,
  eT_half2    = 82,
  eT_half3    = 83,
  eT_half4    = 84,
  eT_half4x4  = 85,
  eT_half3x4  = 86,
  eT_half2x4  = 87,
  eT_half3x3  = 88,
  eT_bool     = 89,
  eT_int      = 90,
  eT_sampler1D    = 91,
  eT_sampler2D    = 92,
  eT_sampler3D    = 93,
  eT_samplerCUBE  = 94,
  eT_const        = 95,

  eT_inout    = 96,

  eT_vs_1_1   = 97,
  eT_vs_2_0   = 98,
  eT_vs_3_0   = 99,
  eT_vs_4_0   = 100,
  eT_ps_1_1   = 101,
  eT_ps_2_0   = 102,
  eT_ps_2_a   = 103,
  eT_ps_2_b   = 104,
  eT_ps_2_x   = 105,
  eT_ps_3_0   = 106,
  eT_ps_4_0   = 107,
  eT_gs_4_0   = 108,
  eT_vs_Auto  = 109,
  eT_ps_Auto  = 110,
  eT_struct   = 111,
  eT_sampler  = 112,
  eT_TEXCOORDN = 113,
  eT_TEXCOORD0 = 114,
  eT_TEXCOORD1 = 115,
  eT_TEXCOORD2 = 116,
  eT_TEXCOORD3 = 117,
  eT_TEXCOORD4 = 118,
  eT_TEXCOORD5 = 119,
  eT_TEXCOORD6 = 120,
  eT_TEXCOORD7 = 121,
  eT_TEXCOORD8 = 122,
  eT_TEXCOORD9 = 123,
  eT_TEXCOORD10 = 124,
  eT_TEXCOORD11 = 125,
  eT_TEXCOORD12 = 126,
  eT_TEXCOORD13 = 127,
  eT_TEXCOORD14 = 128,
  eT_TEXCOORD15 = 129,
  eT_TEXCOORDN_centroid = 130,
  eT_TEXCOORD0_centroid = 131,
  eT_TEXCOORD1_centroid = 132,
  eT_TEXCOORD2_centroid = 133,
  eT_TEXCOORD3_centroid = 134,
  eT_TEXCOORD4_centroid = 135,
  eT_TEXCOORD5_centroid = 136,
  eT_TEXCOORD6_centroid = 137,
  eT_TEXCOORD7_centroid = 138,
  eT_TEXCOORD8_centroid = 139,
  eT_TEXCOORD9_centroid = 140,
  eT_TEXCOORD10_centroid = 141,
  eT_TEXCOORD11_centroid = 142,
  eT_TEXCOORD12_centroid = 143,
  eT_TEXCOORD13_centroid = 144,
  eT_TEXCOORD14_centroid = 145,
  eT_TEXCOORD15_centroid = 146,
  eT_COLOR0,
  eT_static,
  eT_packoffset,
  eT_register,
  eT_return,
  eT_vsregister,
  eT_psregister,
  eT_gsregister,

  eT_color,
  eT_Position,
  eT_Allways,

  eT_STANDARDSGLOBAL,

  eT_compile,
  eT_technique,
  eT_string,
  eT_UIWidget,
  eT_UIWidget0,
  eT_UIWidget1,
  eT_UIWidget2,
  eT_UIWidget3,

  eT_Texture,
  eT_MinFilter,
  eT_MagFilter,
  eT_MipFilter,
  eT_AddressU,
  eT_AddressV,
  eT_AddressW,
  eT_BorderColor,
  eT_sRGBLookup,

  eT_LINEAR,
  eT_POINT,
  eT_NONE,
  eT_ANISOTROPIC,

  eT_Clamp,
  eT_Border,
  eT_Wrap,
  eT_Mirror,

  eT_Script,
  eT_comment,

  eT_RenderOrder,
  eT_ProcessOrder,
  eT_RenderCamera,
  eT_RenderType,
  eT_RenderFilter,
  eT_RenderColorTarget1,
  eT_RenderDepthStencilTarget,
  eT_ClearSetColor,
  eT_ClearSetDepth,
  eT_ClearTarget,
  eT_RenderTarget_IDPool,
  eT_RenderTarget_UpdateType,
  eT_RenderTarget_Width,
  eT_RenderTarget_Height,
  eT_GenerateMips,

  eT_PreProcess,
  eT_PostProcess,
  eT_PreDraw,

  eT_WaterReflection,
  eT_Panorama,

  eT_WaterPlaneReflected,
  eT_PlaneReflected,
  eT_Current,

  eT_CurObject,
  eT_CurScene,
  eT_RecursiveScene,
  eT_CopyScene,

  eT_Refractive,
  eT_Glow,
  eT_Heat,

  eT_DepthBuffer,
  eT_DepthBufferTemp,
  eT_DepthBufferOrig,

  eT_$ScreenSize,
  eT_WaterReflect,
  eT_FogColor,

  eT_Color,
  eT_Depth,

  eT_$RT_2D,
  eT_$RT_CM,
  eT_$RT_Cube,

  eT_pass,
  eT_CustomRE,
  eT_Style,

  eT_VertexShader,
  eT_PixelShader,
  eT_GeometryShader,
  eT_ZEnable,
  eT_ZWriteEnable,
  eT_CullMode,
  eT_SrcBlend,
  eT_DestBlend,
  eT_AlphaBlendEnable,
  eT_AlphaFunc,
  eT_AlphaRef,
  eT_ZFunc,
  eT_ColorWriteEnable,
  eT_IgnoreMaterialState,

  eT_None,
  eT_Disable,
  eT_CCW,
  eT_CW,
  eT_Back,
  eT_Front,

  eT_Never,
  eT_Less,
  eT_Equal,
  eT_LEqual,
  eT_LessEqual,
  eT_NotEqual,
  eT_GEqual,
  eT_GreaterEqual,
  eT_Greater,
  eT_Always,

  eT_RED,
  eT_GREEN,
  eT_BLUE,
  eT_ALPHA,

  eT_ONE,
  eT_ZERO,
  eT_SRC_COLOR,
  eT_SrcColor,
  eT_ONE_MINUS_SRC_COLOR,
  eT_InvSrcColor,
  eT_SRC_ALPHA,
  eT_SrcAlpha,
  eT_ONE_MINUS_SRC_ALPHA,
  eT_InvSrcAlpha,
  eT_DST_ALPHA,
  eT_DestAlpha,
  eT_ONE_MINUS_DST_ALPHA,
  eT_InvDestAlpha,
  eT_DST_COLOR,
  eT_DestColor,
  eT_ONE_MINUS_DST_COLOR,
  eT_InvDestColor,
  eT_SRC_ALPHA_SATURATE,

  eT_NULL,

  eT_cbuffer, 
  eT_PER_BATCH,
  eT_PER_INSTANCE,
  eT_PER_FRAME,
  eT_PER_MATERIAL,
  eT_PER_LIGHT,
  eT_PER_SHADOWGEN,
  eT_SKIN_DATA,
  eT_SHAPE_DATA,
  eT_INSTANCE_DATA,

  eT_ShaderType,
  eT_ShaderDrawType,
  eT_PreprType,
  eT_Public,
  eT_StartTecnique,
  eT_NoPreview,
  eT_LocalConstants,
  eT_Cull,
  eT_SupportsAttrInstancing,
  eT_SupportsConstInstancing,
  eT_Decal,
  eT_DecalNoDepthOffset,
  eT_EnvLighting,
  eT_NoChunkMerging,
  eT_ForceTransPass,
  eT_AfterHDRPostProcess,
  eT_ForceZpass,
  eT_ForceWaterPass,
  eT_ForceDrawLast,
  eT_ForceDrawAfterWater,
  eT_SupportsSubSurfaceScattering,
  eT_SupportsReplaceBasePass,
  eT_SingleLightPass,
  eT_SkipGeneralPass,
  eT_DetailBumpMapping,

  eT_Light,
  eT_Shadow,
  eT_Fur,
  eT_General,
  eT_Terrain,
  eT_Overlay,
  eT_NoDraw,
  eT_Custom,
  eT_Sky,
  eT_OceanShore,
  eT_Hair,
  eT_ForceGeneralPass,

  eT_Metal,
  eT_Ice,
  eT_Water,
  eT_FX,
  eT_HDR,
  eT_Glass,
  eT_Vegetation,
  eT_Particle,
  eT_GenerateSprites,

  eT_NoLights,
  eT_NoMaterialState,
  eT_PositionInvariant,
  eT_TechniqueZ,
  eT_TechniqueShadowPass,
  eT_TechniqueScatterPass,
  eT_TechniqueShadowGen,
  eT_TechniqueShadowGenDX10,
  eT_TechniqueDetail,
  eT_TechniqueCaustics,
  eT_TechniqueGlow,
  eT_TechniqueMotionBlur,
  eT_TechniqueCustomRender,
  eT_TechniqueRainPass,

  eT_ValueString,
  eT_Speed,

  eT_Beam,
  eT_Flare,
  eT_Cloud,
  eT_Ocean,

  eT_Model,
  eT_StartRadius,
  eT_EndRadius,
  eT_StartColor,
  eT_EndColor,
  eT_LightStyle,
  eT_Length,

  eT_RGBStyle,
  eT_Scale,
  eT_Blind,
  eT_SizeBlindScale,
  eT_SizeBlindBias,
  eT_IntensBlindScale,
  eT_IntensBlindBias,
  eT_MinLight,
  eT_DistFactor,
  eT_DistIntensityFactor,
  eT_FadeTime,
  eT_Layer,
  eT_Importance,
  eT_VisAreaScale,

  eT_Poly,
  eT_Identity,
  eT_FromObj,
  eT_FromLight,
  eT_Fixed,

  eT_ParticlesFile,

  eT_Gravity,
  eT_WindDirection,
  eT_WindSpeed,
  eT_WaveHeight,
  eT_DirectionalDependence,
  eT_ChoppyWaveFactor,
  eT_SuppressSmallWavesFactor,

  eT__LT_LIGHTS,
  eT__LT_NUM,
  eT__LT_HASPROJ,
  eT__LT_0_TYPE,
  eT__LT_1_TYPE,
  eT__LT_2_TYPE,
  eT__LT_3_TYPE,
  eT__TT0_TCM,
  eT__TT1_TCM,
  eT__TT2_TCM,
  eT__TT3_TCM,
  eT__TT0_TCG_TYPE,
  eT__TT1_TCG_TYPE,
  eT__TT2_TCG_TYPE,
  eT__TT3_TCG_TYPE,
  eT__TT0_TCPROJ,
  eT__TT1_TCPROJ,
  eT__TT2_TCPROJ,
  eT__TT3_TCPROJ,
  eT__TT0_TCUBE,
  eT__TT1_TCUBE,
  eT__TT2_TCUBE,
  eT__TT3_TCUBE,
  eT__VT_TYPE,
  eT__VT_TYPE_MODIF,
  eT__VT_BEND,
  eT__VT_DET_BEND,
  eT__VT_GRASS,
  eT__VT_WIND,
  eT__VT_DEPTH_OFFSET,
  eT__VT_TERRAIN_ADAPT,
  eT__FT_TEXTURE,
  eT__FT_DIFFUSE,
  eT__FT_SPECULAR,
  eT__FT0_COP,
  eT__FT0_AOP,
  eT__FT0_CARG1,
  eT__FT0_CARG2,
  eT__FT0_AARG1,
  eT__FT0_AARG2,

  eT__VS,
  eT__PS,
  eT__GS,

  eT__g_SkinQuat,
  eT__g_ShapeDeformationData,

  eT_x,
  eT_y,
  eT_z,
  eT_w,
  eT_r,
  eT_g,
  eT_b,
  eT_a,

  eT_true,
  eT_false,
  eT_0,
  eT_1,
  eT_2,
  eT_3,
  eT_4,
  eT_5,
  eT_6,
  eT_7,
  eT_8,
  eT_9,
  eT_10,
  eT_11,
  eT_12,
  eT_13,
  eT_14,
  eT_15,
  eT_16,
  eT_17,
  eT_18,
  eT_19,

  eT_AnisotropyLevel,

  eT_D3D10,
  eT_DIRECT3D10,
  eT_D3D9,
  eT_DIRECT3D9,

  eT_VT_DetailBendingGrass,
  eT_VT_DetailBending,
  eT_VT_WindBending,
  eT_VT_TerrainAdapt,

	eT_ScatterBlend,
	eT_NoScatterBlend,

  eT_max,
  eT_user_first = eT_max+1
};

struct SFXTokenBin
{
  uint32 id;
};

#define FX_BEGIN_TOKENS \
  static SFXTokenBin sCommands[] = {

#define FX_END_TOKENS \
{ eT_unknown }        \
};

#define FX_TOKEN(id) \
{ Parser.fxTokenKey(#id, eT_##id) },

#define FX_REGISTER_TOKEN(id) fxTokenKey(#id, eT_##id);


extern char *g_KeyTokens[];

struct SMacroBinFX
{
  DynArray<uint32> m_Macro;
  uint m_nMask;
};

class CParserBin;

typedef std::map<uint32, SMacroBinFX> FXMacroBin;
typedef FXMacroBin::iterator FXMacroBinItor;

struct SParserFrame
{
  uint32 m_nFirstToken;
  uint32 m_nLastToken;
  uint32 m_nCurToken;
  SParserFrame(uint32 nFirstToken, uint32 nLastToken)
  {
    m_nFirstToken = nFirstToken;
    m_nLastToken = nLastToken;
    m_nCurToken = m_nFirstToken;
  }
  SParserFrame()
  {
    m_nFirstToken = 0;
    m_nLastToken = 0;
    m_nCurToken = m_nFirstToken;
  }
  _inline void Reset()
  {
    m_nFirstToken = 0;
    m_nLastToken = 0;
    m_nCurToken = m_nFirstToken;
  }
  _inline bool IsEmpty()
  {
    if (!m_nFirstToken && !m_nLastToken)
      return true;
    return (m_nLastToken < m_nFirstToken);
  }
};

enum EFragmentType
{
  eFT_Unknown,
  eFT_Function,
  eFT_Structure,
  eFT_Sampler,
};

struct SCodeFragment
{
  uint32 m_nFirstToken;
  uint32 m_nLastToken;
  uint32 m_dwName;
  EFragmentType m_eType;
#ifdef _DEBUG
  string m_Name;
#endif
  SCodeFragment()
  {
    m_nFirstToken = 0;
    m_nLastToken = 0;
    m_dwName = 0;
    m_eType = eFT_Unknown;
  }
};

class CParserBin
{
  friend class CShaderManBin;
  friend class CHWShader_D3D;
  friend struct SFXParam;
  friend class CREBeam;
  friend class CRECloud;
  friend class CREFlare;
  friend class CREOcean;

  //bool m_bEmbeddedSearchInfo;
  struct SShaderBin *m_pCurBinShader;
  CShader *m_pCurShader;
  DynArray<uint32> m_Tokens;
  FXMacroBin m_Macros[2];
  FXShaderToken m_Table;
  //std::vector<std::vector<int>> m_KeyOffsets;
  EToken m_eToken;
  uint32 m_nFirstToken;
  DynArray<SCodeFragment> m_CodeFragments;
  DynArray<SFXParam> m_Parameters;
  DynArray<STexSampler> m_Samplers;

  SParserFrame m_CurFrame;

  SParserFrame m_Name;
  SParserFrame m_Assign;
  SParserFrame m_Annotations;
  SParserFrame m_Value;
  SParserFrame m_Data;

  static FXMacroBin m_StaticMacros;

public:
  CParserBin(SShaderBin *pBin);
  CParserBin(SShaderBin *pBin, CShader *pSH);

  static const char *GetString(uint32 nToken, FXShaderToken& Table, bool bOnlyKey = false);
  const char *GetString(uint32 nToken, bool bOnlyKey = false);
  string GetString(SParserFrame& Frame);
  void BuildSearchInfo();
  bool PreprocessTokens(DynArray<uint32>& Tokens, int nPass);
  bool Preprocess(int nPass, DynArray<uint32>& Tokens, FXShaderToken *pSrcTable);
  const SMacroBinFX *FindMacro(uint32 dwName, FXMacroBin& Macro);
  static bool AddMacro(uint32 dwToken, uint32 *pMacro, int nMacroTokens, uint32 nMask, FXMacroBin& Macro);
  static bool RemoveMacro(uint32 dwToken, FXMacroBin& Macro);
  uint32 NewUserToken(uint32 nToken, const char *szToken, bool bUseFinalTable);
  void MergeTable(SShaderBin *pBin);
  bool CheckIfExpression(uint32 *pTokens, uint32& nT, int nPass);
  bool IgnorePreprocessBlock(uint32 *pTokens, uint32& nT, int nMaxTokens);
  static bool ConvertToAscii(uint32 *pTokens, uint32 nT, FXShaderToken& Table, TArray<char>& Text);
  bool GetBool(SParserFrame& Frame);
  _inline uint32 *GetTokens(int nStart) { return &m_Tokens[nStart]; }
  _inline int GetNumTokens() { return m_Tokens.size(); }
  _inline EToken GetToken() { return m_eToken; }
  _inline EToken GetToken(SParserFrame& Frame)
  {
    assert(!Frame.IsEmpty());
    return (EToken)m_Tokens[Frame.m_nFirstToken];
  }
  _inline uint32 FirstToken() { return m_nFirstToken; }
  _inline int GetInt(uint32 nToken)
  {
    const char *szStr = GetString(nToken);
    if (szStr[0] == '0' && szStr[1] == 'x')
    {
      int i;
      sscanf(&szStr[2], "%x", &i);
      return i;
    }
    return atoi(szStr);
  }
  _inline float GetFloat(SParserFrame& Frame)
  {
    return (float)atof(GetString(Frame));
  }
  static _inline uint32 NextToken(uint32 *pTokens, uint& nCur, uint32 nLast)
  {
    while (nCur <= nLast)
    {
      uint32 nToken = pTokens[nCur++];
      if (nToken == eT_skip)
      {
        nCur++;
        continue;
      }
      if (nToken == eT_skip_1)
      {
        while (nCur <= nLast)
        {
          nToken = pTokens[nCur++];
          if (nToken == eT_skip_2)
            break;
        }
        continue;
      }
      return nToken;
    }
    return 0;
  }

  SParserFrame BeginFrame(SParserFrame& Frame);
  void EndFrame(SParserFrame& Frame);

  byte GetCompareFunc(EToken eT);
  int  GetSrcBlend(EToken eT);
  int  GetDstBlend(EToken eT);

  void InsertSkipTokens(uint32 *pTokens, uint32 nStart, uint32 nTokens, bool bSingle);
  bool ParseObject(SFXTokenBin *pTokens, int& nIndex);
  bool ParseObject(SFXTokenBin *pTokens);
  int  GetNextToken(uint32& nStart);
  bool FXGetAssignmentData(SParserFrame& Frame);
  bool FXGetAssignmentData2(SParserFrame& Frame);
  bool GetAssignmentData(SParserFrame& Frame);
  bool GetSubData(SParserFrame& Frame, EToken eT1, EToken eT2);
  static int32 FindToken(uint32 nStart, uint32 nLast, const uint32 *pTokens, uint32 nToken);
  int32 FindToken(uint32 nStart, uint32 nLast, uint32 nToken);
  int32 FindToken(uint32 nStart, uint32 nLast, const uint32 *pTokens);
  int  CopyTokens(SParserFrame& Fragment, DynArray<uint32>& NewTokens);
  int  CopyTokens(SCodeFragment *pCF, DynArray<uint32>& SHData, DynArray<SCodeFragment>& Replaces, DynArray<uint32>& NewTokens, uint32 nID);
  static _inline void AddDefineToken(uint32 dwToken, DynArray<uint32>& Tokens)
  {
    Tokens.push_back(eT_define);
    Tokens.push_back(dwToken);
    Tokens.push_back(0);
  }
  static _inline void AddDefineToken(uint32 dwToken, uint32 dwToken2, DynArray<uint32>& Tokens)
  {
    Tokens.push_back(eT_define);
    Tokens.push_back(dwToken);
    Tokens.push_back(dwToken2);
    Tokens.push_back(0);
  }
  bool JumpSemicolumn(uint32& nStart, uint32 nEnd);

  static uint32 fxToken(const char *szToken, bool* bKey=NULL);
  static uint32 fxTokenKey(char *szToken, EToken eT=eT_unknown);
  static uint32 GetCRC32(const char *szStr);
  static uint32 NextToken(char*& buf, char *com, bool& bKey);
  static void Init();
  static void SetupForPS3();
  static void SetupForD3D9();
  static void SetupForD3D10();
  static void SetupForPS20(bool bEnable);

  static bool m_bD3D10;
  static bool m_bNewLightSetup;
};

char *fxFillPr (char **buf, char *dst);

#endif

