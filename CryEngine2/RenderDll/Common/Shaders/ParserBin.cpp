/*=============================================================================
  ParserBin.cpp : Script parser implementations.
  Copyright (c) 2001 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"

extern byte sSkipChars[256];
char *g_KeyTokens[eT_max];
TArray<bool> sfxIFDef;
TArray<bool> sfxIFIgnore;
bool CParserBin::m_bD3D10;
bool CParserBin::m_bNewLightSetup;

Crc32Gen& GetCRC32Gen()
{
	static Crc32Gen& sCrc32Gen = *GetISystem()->GetCrc32Gen();
	return sCrc32Gen;
}

CParserBin::CParserBin(SShaderBin *pBin)
{
  m_pCurBinShader = pBin;
  m_pCurShader = NULL;
  //m_bEmbeddedSearchInfo = false;
}
CParserBin::CParserBin(SShaderBin *pBin, CShader *pSH)
{
  m_pCurBinShader = pBin;
  m_pCurShader = pSH;
  //m_bEmbeddedSearchInfo = false;
}

uint32 CParserBin::GetCRC32(const char *szStr)
{
  uint32 nGen = GetCRC32Gen().GetCRC32(szStr);
  assert(nGen >= eT_user_first);

  return nGen;
}

FXMacroBin CParserBin::m_StaticMacros;

void CParserBin::Init()
{
  // Register key tokens
  fxTokenKey("#include", eT_include);
  fxTokenKey("#define", eT_define);
  fxTokenKey("#undefine", eT_undefine);
  fxTokenKey("#define", eT_define_2);
  fxTokenKey("#if", eT_if);
  fxTokenKey("#ifdef", eT_ifdef);
  fxTokenKey("#ifndef", eT_ifndef);
  fxTokenKey("#if", eT_if_2);
  fxTokenKey("#ifdef", eT_ifdef_2);
  fxTokenKey("#ifndef", eT_ifndef_2);
  fxTokenKey("#endif", eT_endif);
  fxTokenKey("#else", eT_else);
  fxTokenKey("#elif", eT_elif);
  fxTokenKey("#warning", eT_warning);
  fxTokenKey("#register_env", eT_register_env);
  fxTokenKey("#ifcvar", eT_ifcvar);
  fxTokenKey("#ifncvar", eT_ifncvar);
  fxTokenKey("#elifcvar", eT_elifcvar);

  fxTokenKey("|", eT_or);
  fxTokenKey("&", eT_and);

  fxTokenKey("(", eT_br_rnd_1);
  fxTokenKey(")", eT_br_rnd_2);
  fxTokenKey("[", eT_br_sq_1);
  fxTokenKey("]", eT_br_sq_2);
  fxTokenKey("{", eT_br_cv_1);
  fxTokenKey("}", eT_br_cv_2);
  fxTokenKey("<", eT_br_tr_1);
  fxTokenKey(">", eT_br_tr_2);
  fxTokenKey(",", eT_comma);
  fxTokenKey(".", eT_dot);
  fxTokenKey(":", eT_colon);
  fxTokenKey(";", eT_semicolumn);
  fxTokenKey("!", eT_excl);
  fxTokenKey("\"", eT_quote);
  fxTokenKey("'", eT_sing_quote);

  fxTokenKey("//", eT_comment);

  fxTokenKey("?", eT_question);
  fxTokenKey("=", eT_eq);
  fxTokenKey("+", eT_plus);
  fxTokenKey("-", eT_minus);
  fxTokenKey("/", eT_div);
  fxTokenKey("*", eT_mul);
  fxTokenKey("dot", eT_dot_math);
  fxTokenKey("mul", eT_mul_math);
  fxTokenKey("sqrt", eT_sqrt_math);
  fxTokenKey("exp", eT_exp_math);
  fxTokenKey("log", eT_log_math);
  fxTokenKey("log2", eT_log2_math);
  fxTokenKey("sin", eT_sin_math);
  fxTokenKey("cos", eT_cos_math);
  fxTokenKey("sincos", eT_sincos_math);
  fxTokenKey("floor", eT_floor_math);
  fxTokenKey("floor", eT_ceil_math);
  fxTokenKey("frac", eT_frac_math);
  fxTokenKey("lerp", eT_lerp_math);
  fxTokenKey("abs", eT_abs_math);
  fxTokenKey("clamp", eT_clamp_math);
  fxTokenKey("min", eT_min_math);
  fxTokenKey("max", eT_max_math);
  fxTokenKey("length", eT_length_math);

  fxTokenKey("%_LT_LIGHTS", eT__LT_LIGHTS);
  fxTokenKey("%_LT_NUM", eT__LT_NUM);
  fxTokenKey("%_LT_HASPROJ", eT__LT_HASPROJ);
  fxTokenKey("%_LT_0_TYPE", eT__LT_0_TYPE);
  fxTokenKey("%_LT_1_TYPE", eT__LT_1_TYPE);
  fxTokenKey("%_LT_2_TYPE", eT__LT_2_TYPE);
  fxTokenKey("%_LT_3_TYPE", eT__LT_3_TYPE);
  fxTokenKey("%_TT0_TCM", eT__TT0_TCM);
  fxTokenKey("%_TT1_TCM", eT__TT1_TCM);
  fxTokenKey("%_TT2_TCM", eT__TT2_TCM);
  fxTokenKey("%_TT3_TCM", eT__TT3_TCM);
  fxTokenKey("%_TT0_TCG_TYPE", eT__TT0_TCG_TYPE);
  fxTokenKey("%_TT1_TCG_TYPE", eT__TT1_TCG_TYPE);
  fxTokenKey("%_TT2_TCG_TYPE", eT__TT2_TCG_TYPE);
  fxTokenKey("%_TT3_TCG_TYPE", eT__TT3_TCG_TYPE);
  fxTokenKey("%_TT0_TCPROJ", eT__TT0_TCPROJ);
  fxTokenKey("%_TT1_TCPROJ", eT__TT1_TCPROJ);
  fxTokenKey("%_TT2_TCPROJ", eT__TT2_TCPROJ);
  fxTokenKey("%_TT3_TCPROJ", eT__TT3_TCPROJ);
  fxTokenKey("%_TT0_TCUBE", eT__TT0_TCUBE);
  fxTokenKey("%_TT1_TCUBE", eT__TT1_TCUBE);
  fxTokenKey("%_TT2_TCUBE", eT__TT2_TCUBE);
  fxTokenKey("%_TT3_TCUBE", eT__TT3_TCUBE);
  fxTokenKey("%_VT_TYPE", eT__VT_TYPE);
  fxTokenKey("%_VT_TYPE_MODIF", eT__VT_TYPE_MODIF);
  fxTokenKey("%_VT_BEND", eT__VT_BEND);
  fxTokenKey("%_VT_DET_BEND", eT__VT_DET_BEND);
  fxTokenKey("%_VT_GRASS", eT__VT_GRASS);
  fxTokenKey("%_VT_WIND", eT__VT_WIND);
  fxTokenKey("%_VT_DEPTH_OFFSET", eT__VT_DEPTH_OFFSET);
  fxTokenKey("%_VT_TERRAIN_ADAPT", eT__VT_TERRAIN_ADAPT);
  fxTokenKey("%_FT_TEXTURE", eT__FT_TEXTURE);
  fxTokenKey("%_FT_DIFFUSE", eT__FT_DIFFUSE);
  fxTokenKey("%_FT_SPECULAR", eT__FT_SPECULAR);
  fxTokenKey("%_FT0_COP", eT__FT0_COP);
  fxTokenKey("%_FT0_AOP", eT__FT0_AOP);
  fxTokenKey("%_FT0_CARG1", eT__FT0_CARG1);
  fxTokenKey("%_FT0_CARG2", eT__FT0_CARG2);
  fxTokenKey("%_FT0_AARG1", eT__FT0_AARG1);
  fxTokenKey("%_FT0_AARG2", eT__FT0_AARG2);

  fxTokenKey("%_VS", eT__VS);
  fxTokenKey("%_PS", eT__PS);
  fxTokenKey("%_GS", eT__GS);

  FX_REGISTER_TOKEN(_g_SkinQuat);
  FX_REGISTER_TOKEN(_g_ShapeDeformationData);

  FX_REGISTER_TOKEN(tex2D);
  FX_REGISTER_TOKEN(tex2Dproj);
  FX_REGISTER_TOKEN(tex3D);
  FX_REGISTER_TOKEN(texCUBE);
  FX_REGISTER_TOKEN(sampler1D);
  FX_REGISTER_TOKEN(sampler2D);
  FX_REGISTER_TOKEN(sampler3D);
  FX_REGISTER_TOKEN(samplerCUBE);
  FX_REGISTER_TOKEN(SamplerState);
  FX_REGISTER_TOKEN(SamplerComparisonState);
  FX_REGISTER_TOKEN(sampler_state);
  FX_REGISTER_TOKEN(Texture2D);
  FX_REGISTER_TOKEN(Texture2DArray);
  FX_REGISTER_TOKEN(Texture2DMS);

  FX_REGISTER_TOKEN(float);
  FX_REGISTER_TOKEN(float2);
  FX_REGISTER_TOKEN(float3);
  FX_REGISTER_TOKEN(float4);
  FX_REGISTER_TOKEN(float4x4);
  FX_REGISTER_TOKEN(float3x4);
  FX_REGISTER_TOKEN(float2x4);
  FX_REGISTER_TOKEN(float3x3);
  FX_REGISTER_TOKEN(half);
  FX_REGISTER_TOKEN(half2);
  FX_REGISTER_TOKEN(half3);
  FX_REGISTER_TOKEN(half4);
  FX_REGISTER_TOKEN(half4x4);
  FX_REGISTER_TOKEN(half3x4);
  FX_REGISTER_TOKEN(half2x4);
  FX_REGISTER_TOKEN(half3x3);
  FX_REGISTER_TOKEN(bool);
  FX_REGISTER_TOKEN(int);

  FX_REGISTER_TOKEN(inout);

  FX_REGISTER_TOKEN(vs_1_1);
  FX_REGISTER_TOKEN(vs_2_0);
  FX_REGISTER_TOKEN(vs_3_0);
  FX_REGISTER_TOKEN(vs_4_0);
  FX_REGISTER_TOKEN(vs_Auto);
  FX_REGISTER_TOKEN(ps_1_1);
  FX_REGISTER_TOKEN(ps_2_0);
  FX_REGISTER_TOKEN(ps_2_a);
  FX_REGISTER_TOKEN(ps_2_b);
  FX_REGISTER_TOKEN(ps_2_x);
  FX_REGISTER_TOKEN(ps_3_0);
  FX_REGISTER_TOKEN(ps_4_0);
  FX_REGISTER_TOKEN(ps_Auto);
  FX_REGISTER_TOKEN(gs_4_0);
  FX_REGISTER_TOKEN(struct);
  FX_REGISTER_TOKEN(sampler);
  FX_REGISTER_TOKEN(const);
  FX_REGISTER_TOKEN(TEXCOORDN);
  FX_REGISTER_TOKEN(TEXCOORD0);
  FX_REGISTER_TOKEN(TEXCOORD1);
  FX_REGISTER_TOKEN(TEXCOORD2);
  FX_REGISTER_TOKEN(TEXCOORD3);
  FX_REGISTER_TOKEN(TEXCOORD4);
  FX_REGISTER_TOKEN(TEXCOORD5);
  FX_REGISTER_TOKEN(TEXCOORD6);
  FX_REGISTER_TOKEN(TEXCOORD7);
  FX_REGISTER_TOKEN(TEXCOORD8);
  FX_REGISTER_TOKEN(TEXCOORD9);
  FX_REGISTER_TOKEN(TEXCOORD10);
  FX_REGISTER_TOKEN(TEXCOORD11);
  FX_REGISTER_TOKEN(TEXCOORD12);
  FX_REGISTER_TOKEN(TEXCOORD13);
  FX_REGISTER_TOKEN(TEXCOORD14);
  FX_REGISTER_TOKEN(TEXCOORD15);
  FX_REGISTER_TOKEN(TEXCOORDN_centroid);
  FX_REGISTER_TOKEN(TEXCOORD0_centroid);
  FX_REGISTER_TOKEN(TEXCOORD1_centroid);
  FX_REGISTER_TOKEN(TEXCOORD2_centroid);
  FX_REGISTER_TOKEN(TEXCOORD3_centroid);
  FX_REGISTER_TOKEN(TEXCOORD4_centroid);
  FX_REGISTER_TOKEN(TEXCOORD5_centroid);
  FX_REGISTER_TOKEN(TEXCOORD6_centroid);
  FX_REGISTER_TOKEN(TEXCOORD7_centroid);
  FX_REGISTER_TOKEN(TEXCOORD8_centroid);
  FX_REGISTER_TOKEN(TEXCOORD9_centroid);
  FX_REGISTER_TOKEN(TEXCOORD10_centroid);
  FX_REGISTER_TOKEN(TEXCOORD11_centroid);
  FX_REGISTER_TOKEN(TEXCOORD12_centroid);
  FX_REGISTER_TOKEN(TEXCOORD13_centroid);
  FX_REGISTER_TOKEN(TEXCOORD14_centroid);
  FX_REGISTER_TOKEN(TEXCOORD15_centroid);
  FX_REGISTER_TOKEN(COLOR0);

  FX_REGISTER_TOKEN(static);
  FX_REGISTER_TOKEN(packoffset);
  FX_REGISTER_TOKEN(register);
  FX_REGISTER_TOKEN(return);
  FX_REGISTER_TOKEN(vsregister);
  FX_REGISTER_TOKEN(psregister);
  FX_REGISTER_TOKEN(gsregister);
  FX_REGISTER_TOKEN(color);

  FX_REGISTER_TOKEN(Position);
  FX_REGISTER_TOKEN(Allways);

  FX_REGISTER_TOKEN(STANDARDSGLOBAL);

  FX_REGISTER_TOKEN(compile);
  FX_REGISTER_TOKEN(technique);
  FX_REGISTER_TOKEN(string);
  FX_REGISTER_TOKEN(UIWidget);
  FX_REGISTER_TOKEN(UIWidget0);
  FX_REGISTER_TOKEN(UIWidget1);
  FX_REGISTER_TOKEN(UIWidget2);
  FX_REGISTER_TOKEN(UIWidget3);

  FX_REGISTER_TOKEN(Texture);
  FX_REGISTER_TOKEN(MinFilter);
  FX_REGISTER_TOKEN(MagFilter);
  FX_REGISTER_TOKEN(MipFilter);
  FX_REGISTER_TOKEN(AddressU);
  FX_REGISTER_TOKEN(AddressV);
  FX_REGISTER_TOKEN(AddressW);
  FX_REGISTER_TOKEN(BorderColor);
  FX_REGISTER_TOKEN(sRGBLookup);

  FX_REGISTER_TOKEN(LINEAR);
  FX_REGISTER_TOKEN(POINT);
  FX_REGISTER_TOKEN(NONE);
  FX_REGISTER_TOKEN(ANISOTROPIC);

  FX_REGISTER_TOKEN(Clamp);
  FX_REGISTER_TOKEN(Border);
  FX_REGISTER_TOKEN(Wrap);
  FX_REGISTER_TOKEN(Mirror);

  FX_REGISTER_TOKEN(Script);

  FX_REGISTER_TOKEN(RenderOrder);
  FX_REGISTER_TOKEN(ProcessOrder);
  FX_REGISTER_TOKEN(RenderCamera);
  FX_REGISTER_TOKEN(RenderType);
  FX_REGISTER_TOKEN(RenderFilter);
  FX_REGISTER_TOKEN(RenderColorTarget1);
  FX_REGISTER_TOKEN(RenderDepthStencilTarget);
  FX_REGISTER_TOKEN(ClearSetColor);
  FX_REGISTER_TOKEN(ClearSetDepth);
  FX_REGISTER_TOKEN(ClearTarget);
  FX_REGISTER_TOKEN(RenderTarget_IDPool);
  FX_REGISTER_TOKEN(RenderTarget_UpdateType);
  FX_REGISTER_TOKEN(RenderTarget_Width);
  FX_REGISTER_TOKEN(RenderTarget_Height);
  FX_REGISTER_TOKEN(GenerateMips);

  FX_REGISTER_TOKEN(PreProcess);
  FX_REGISTER_TOKEN(PostProcess);
  FX_REGISTER_TOKEN(PreDraw);

  FX_REGISTER_TOKEN(WaterReflection);
  FX_REGISTER_TOKEN(Panorama);

  FX_REGISTER_TOKEN(WaterPlaneReflected);
  FX_REGISTER_TOKEN(PlaneReflected);
  FX_REGISTER_TOKEN(Current);

  FX_REGISTER_TOKEN(CurObject);
  FX_REGISTER_TOKEN(CurScene);
  FX_REGISTER_TOKEN(RecursiveScene);
  FX_REGISTER_TOKEN(CopyScene);

  FX_REGISTER_TOKEN(Refractive);
  FX_REGISTER_TOKEN(Glow);
  FX_REGISTER_TOKEN(Heat);

  FX_REGISTER_TOKEN(DepthBuffer);
  FX_REGISTER_TOKEN(DepthBufferTemp);
  FX_REGISTER_TOKEN(DepthBufferOrig);

  FX_REGISTER_TOKEN($ScreenSize);
  FX_REGISTER_TOKEN(WaterReflect);
  FX_REGISTER_TOKEN(FogColor);

  FX_REGISTER_TOKEN(Color);
  FX_REGISTER_TOKEN(Depth);

  FX_REGISTER_TOKEN($RT_2D);
  FX_REGISTER_TOKEN($RT_CM);
  FX_REGISTER_TOKEN($RT_Cube);

  FX_REGISTER_TOKEN(pass);
  FX_REGISTER_TOKEN(CustomRE);
  FX_REGISTER_TOKEN(Style);

  FX_REGISTER_TOKEN(VertexShader);
  FX_REGISTER_TOKEN(PixelShader);
  FX_REGISTER_TOKEN(GeometryShader);
  FX_REGISTER_TOKEN(ZEnable);
  FX_REGISTER_TOKEN(ZWriteEnable);
  FX_REGISTER_TOKEN(CullMode);
  FX_REGISTER_TOKEN(SrcBlend);
  FX_REGISTER_TOKEN(DestBlend);
  FX_REGISTER_TOKEN(AlphaBlendEnable);
  FX_REGISTER_TOKEN(AlphaFunc);
  FX_REGISTER_TOKEN(AlphaRef);
  FX_REGISTER_TOKEN(ZFunc);
  FX_REGISTER_TOKEN(ColorWriteEnable);
  FX_REGISTER_TOKEN(IgnoreMaterialState);

  FX_REGISTER_TOKEN(None);
  FX_REGISTER_TOKEN(Disable);
  FX_REGISTER_TOKEN(CCW);
  FX_REGISTER_TOKEN(CW);
  FX_REGISTER_TOKEN(Back);
  FX_REGISTER_TOKEN(Front);

  FX_REGISTER_TOKEN(Never);
  FX_REGISTER_TOKEN(Less);
  FX_REGISTER_TOKEN(Equal);
  FX_REGISTER_TOKEN(LEqual);
  FX_REGISTER_TOKEN(LessEqual);
  FX_REGISTER_TOKEN(NotEqual);
  FX_REGISTER_TOKEN(GEqual);
  FX_REGISTER_TOKEN(GreaterEqual);
  FX_REGISTER_TOKEN(Greater)
  FX_REGISTER_TOKEN(Always);

  FX_REGISTER_TOKEN(RED);
  FX_REGISTER_TOKEN(GREEN);
  FX_REGISTER_TOKEN(BLUE);
  FX_REGISTER_TOKEN(ALPHA);

  FX_REGISTER_TOKEN(ONE);
  FX_REGISTER_TOKEN(ZERO);
  FX_REGISTER_TOKEN(SRC_COLOR);
  FX_REGISTER_TOKEN(SrcColor);
  FX_REGISTER_TOKEN(ONE_MINUS_SRC_COLOR);
  FX_REGISTER_TOKEN(InvSrcColor);
  FX_REGISTER_TOKEN(SRC_ALPHA);
  FX_REGISTER_TOKEN(SrcAlpha);
  FX_REGISTER_TOKEN(ONE_MINUS_SRC_ALPHA);
  FX_REGISTER_TOKEN(InvSrcAlpha);
  FX_REGISTER_TOKEN(DST_ALPHA);
  FX_REGISTER_TOKEN(DestAlpha);
  FX_REGISTER_TOKEN(ONE_MINUS_DST_ALPHA);
  FX_REGISTER_TOKEN(InvDestAlpha);
  FX_REGISTER_TOKEN(DST_COLOR);
  FX_REGISTER_TOKEN(DestColor);
  FX_REGISTER_TOKEN(ONE_MINUS_DST_COLOR);
  FX_REGISTER_TOKEN(InvDestColor);
  FX_REGISTER_TOKEN(SRC_ALPHA_SATURATE);

  FX_REGISTER_TOKEN(NULL);

  FX_REGISTER_TOKEN(cbuffer);
  FX_REGISTER_TOKEN(PER_BATCH);
  FX_REGISTER_TOKEN(PER_INSTANCE);
  FX_REGISTER_TOKEN(PER_FRAME);
  FX_REGISTER_TOKEN(PER_MATERIAL);
  FX_REGISTER_TOKEN(PER_LIGHT);
  FX_REGISTER_TOKEN(PER_SHADOWGEN);
  FX_REGISTER_TOKEN(SKIN_DATA);
  FX_REGISTER_TOKEN(SHAPE_DATA);
  FX_REGISTER_TOKEN(INSTANCE_DATA);

  FX_REGISTER_TOKEN(ShaderType);
  FX_REGISTER_TOKEN(ShaderDrawType);
  FX_REGISTER_TOKEN(PreprType);
  FX_REGISTER_TOKEN(Public);
  FX_REGISTER_TOKEN(StartTecnique);
  FX_REGISTER_TOKEN(NoPreview);
  FX_REGISTER_TOKEN(LocalConstants);
  FX_REGISTER_TOKEN(Cull);
  FX_REGISTER_TOKEN(SupportsAttrInstancing);
  FX_REGISTER_TOKEN(SupportsConstInstancing);
  FX_REGISTER_TOKEN(Decal);
  FX_REGISTER_TOKEN(DecalNoDepthOffset);
  FX_REGISTER_TOKEN(EnvLighting);
  FX_REGISTER_TOKEN(NoChunkMerging);
  FX_REGISTER_TOKEN(ForceTransPass);
  FX_REGISTER_TOKEN(AfterHDRPostProcess);
  FX_REGISTER_TOKEN(ForceZpass);
  FX_REGISTER_TOKEN(ForceWaterPass);
  FX_REGISTER_TOKEN(ForceDrawLast);
  FX_REGISTER_TOKEN(ForceDrawAfterWater);
  FX_REGISTER_TOKEN(SupportsSubSurfaceScattering);
	FX_REGISTER_TOKEN(ScatterBlend);
	FX_REGISTER_TOKEN(NoScatterBlend);
  FX_REGISTER_TOKEN(SupportsReplaceBasePass);
  FX_REGISTER_TOKEN(SingleLightPass);
  FX_REGISTER_TOKEN(SkipGeneralPass);
  FX_REGISTER_TOKEN(DetailBumpMapping);
  FX_REGISTER_TOKEN(VT_DetailBendingGrass);
  FX_REGISTER_TOKEN(VT_DetailBending);
  FX_REGISTER_TOKEN(VT_WindBending);
  FX_REGISTER_TOKEN(VT_TerrainAdapt);

  FX_REGISTER_TOKEN(Light);
  FX_REGISTER_TOKEN(Shadow);
  FX_REGISTER_TOKEN(Fur);
  FX_REGISTER_TOKEN(General);
  FX_REGISTER_TOKEN(Terrain);
  FX_REGISTER_TOKEN(Overlay);
  FX_REGISTER_TOKEN(NoDraw);
  FX_REGISTER_TOKEN(Custom);
  FX_REGISTER_TOKEN(Sky);
  FX_REGISTER_TOKEN(OceanShore);
  FX_REGISTER_TOKEN(Hair);
  FX_REGISTER_TOKEN(ForceGeneralPass);

  FX_REGISTER_TOKEN(Metal);
  FX_REGISTER_TOKEN(Ice);
  FX_REGISTER_TOKEN(Water);
  FX_REGISTER_TOKEN(FX);
  FX_REGISTER_TOKEN(HDR);
  FX_REGISTER_TOKEN(Glass);
  FX_REGISTER_TOKEN(Vegetation);
  FX_REGISTER_TOKEN(Particle);
  FX_REGISTER_TOKEN(GenerateSprites);

  FX_REGISTER_TOKEN(NoLights);
  FX_REGISTER_TOKEN(NoMaterialState);
  FX_REGISTER_TOKEN(PositionInvariant);
  FX_REGISTER_TOKEN(TechniqueZ);
  FX_REGISTER_TOKEN(TechniqueScatterPass);
  FX_REGISTER_TOKEN(TechniqueShadowPass);
  FX_REGISTER_TOKEN(TechniqueShadowGen);
  FX_REGISTER_TOKEN(TechniqueShadowGenDX10);
  FX_REGISTER_TOKEN(TechniqueDetail);
  FX_REGISTER_TOKEN(TechniqueCaustics);
  FX_REGISTER_TOKEN(TechniqueGlow);
  FX_REGISTER_TOKEN(TechniqueMotionBlur);
  FX_REGISTER_TOKEN(TechniqueCustomRender);
  FX_REGISTER_TOKEN(TechniqueRainPass);

  FX_REGISTER_TOKEN(ValueString);
  FX_REGISTER_TOKEN(Speed);

  FX_REGISTER_TOKEN(Beam);
  FX_REGISTER_TOKEN(Flare);
  FX_REGISTER_TOKEN(Cloud);
  FX_REGISTER_TOKEN(Ocean);

  FX_REGISTER_TOKEN(Model);
  FX_REGISTER_TOKEN(StartRadius);
  FX_REGISTER_TOKEN(EndRadius);
  FX_REGISTER_TOKEN(StartColor);
  FX_REGISTER_TOKEN(EndColor);
  FX_REGISTER_TOKEN(LightStyle);
  FX_REGISTER_TOKEN(Length);

  FX_REGISTER_TOKEN(RGBStyle);
  FX_REGISTER_TOKEN(Scale);
  FX_REGISTER_TOKEN(Blind);
  FX_REGISTER_TOKEN(SizeBlindScale);
  FX_REGISTER_TOKEN(SizeBlindBias);
  FX_REGISTER_TOKEN(IntensBlindScale);
  FX_REGISTER_TOKEN(IntensBlindBias);
  FX_REGISTER_TOKEN(MinLight);
  FX_REGISTER_TOKEN(DistFactor);
  FX_REGISTER_TOKEN(DistIntensityFactor);
  FX_REGISTER_TOKEN(FadeTime);
  FX_REGISTER_TOKEN(Layer);
  FX_REGISTER_TOKEN(Importance);
  FX_REGISTER_TOKEN(VisAreaScale);

  FX_REGISTER_TOKEN(Poly);
  FX_REGISTER_TOKEN(Identity);
  FX_REGISTER_TOKEN(FromObj);
  FX_REGISTER_TOKEN(FromLight);
  FX_REGISTER_TOKEN(Fixed);

  FX_REGISTER_TOKEN(ParticlesFile);

  FX_REGISTER_TOKEN(Gravity);
  FX_REGISTER_TOKEN(WindDirection);
  FX_REGISTER_TOKEN(WindSpeed);
  FX_REGISTER_TOKEN(WaveHeight);
  FX_REGISTER_TOKEN(DirectionalDependence);
  FX_REGISTER_TOKEN(ChoppyWaveFactor);
  FX_REGISTER_TOKEN(SuppressSmallWavesFactor);

  FX_REGISTER_TOKEN(x);
  FX_REGISTER_TOKEN(y);
  FX_REGISTER_TOKEN(z);
  FX_REGISTER_TOKEN(w);
  FX_REGISTER_TOKEN(r);
  FX_REGISTER_TOKEN(g);
  FX_REGISTER_TOKEN(b);
  FX_REGISTER_TOKEN(a);

  FX_REGISTER_TOKEN(true);
  FX_REGISTER_TOKEN(false);
  FX_REGISTER_TOKEN(0);
  FX_REGISTER_TOKEN(1);
  FX_REGISTER_TOKEN(2);
  FX_REGISTER_TOKEN(3);
  FX_REGISTER_TOKEN(4);
  FX_REGISTER_TOKEN(5);
  FX_REGISTER_TOKEN(6);
  FX_REGISTER_TOKEN(7);
  FX_REGISTER_TOKEN(8);
  FX_REGISTER_TOKEN(9);
  FX_REGISTER_TOKEN(10);
  FX_REGISTER_TOKEN(11);
  FX_REGISTER_TOKEN(12);
  FX_REGISTER_TOKEN(13);
  FX_REGISTER_TOKEN(14);
  FX_REGISTER_TOKEN(15);
  FX_REGISTER_TOKEN(16);
  FX_REGISTER_TOKEN(17);
  FX_REGISTER_TOKEN(18);
  FX_REGISTER_TOKEN(19);

  FX_REGISTER_TOKEN(D3D10);
  FX_REGISTER_TOKEN(DIRECT3D10);
  FX_REGISTER_TOKEN(D3D9);
  FX_REGISTER_TOKEN(DIRECT3D9);

  FX_REGISTER_TOKEN(STANDARDSGLOBAL);

  FXMacroItor it;
  for (it=sStaticMacros.begin(); it!=sStaticMacros.end(); it++)
  {
    bool bKey = false;
    uint32 nName = CParserBin::fxToken(it->first.c_str(), &bKey);
    if (!bKey)
      nName = GetCRC32(it->first.c_str());
    SMacroFX *pr = &it->second;
    uint32 nMacros = 0;
    uint32 Macro[64];
    if (pr->m_szMacro[0])
    {
      char *szBuf = (char *)pr->m_szMacro.c_str();
      SkipCharacters (&szBuf, " ");
      if (!szBuf[0])
        break;
      char com[1024];
      bool bKey = false;
      uint32 dwToken = CParserBin::NextToken(szBuf, com, bKey);
      if (!bKey)
        dwToken = GetCRC32(com);
      Macro[nMacros++] = dwToken;
    }
    AddMacro(nName, Macro, nMacros, pr->m_nMask, m_StaticMacros);
  }
  sStaticMacros.clear();
#if defined (PS3)
  SetupForPS3();
#elif defined (DIRECT3D10)
  SetupForD3D10();
#elif defined (DIRECT3D9)
  SetupForD3D9();
#endif
}

void CParserBin::SetupForPS3()
{
  RemoveMacro(CParserBin::fxToken("D3D10"), m_StaticMacros);
  RemoveMacro(CParserBin::fxToken("DIRECT3D10"), m_StaticMacros);
  uint32 nMacro[1] = {eT_1};
  AddMacro(CParserBin::fxToken("DIRECT3D9"), nMacro, 1, 0, m_StaticMacros);
  AddMacro(CParserBin::fxToken("D3D9"), nMacro, 1, 0, m_StaticMacros);
  m_bD3D10 = false;
  gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/D3D10/";
  gRenDev->m_cEF.m_ShadersFilter = "D3D10";
}

void CParserBin::SetupForD3D9()
{
  RemoveMacro(CParserBin::fxToken("D3D10"), m_StaticMacros);
  RemoveMacro(CParserBin::fxToken("DIRECT3D10"), m_StaticMacros);
  uint32 nMacro[1] = {eT_1};
  AddMacro(CParserBin::fxToken("DIRECT3D9"), nMacro, 1, 0, m_StaticMacros);
  AddMacro(CParserBin::fxToken("D3D9"), nMacro, 1, 0, m_StaticMacros);
  m_bD3D10 = false;
  gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/D3D9/";
  gRenDev->m_cEF.m_ShadersFilter = "D3D9";
}

void CParserBin::SetupForD3D10()
{
  RemoveMacro(CParserBin::fxToken("D3D10"), m_StaticMacros);
  RemoveMacro(CParserBin::fxToken("DIRECT3D10"), m_StaticMacros);
  uint32 nMacro[1] = {eT_1};
  AddMacro(CParserBin::fxToken("DIRECT3D10"), nMacro, 1, 0, m_StaticMacros);
  AddMacro(CParserBin::fxToken("D3D10"), nMacro, 1, 0, m_StaticMacros);
  m_bD3D10 = true;
  gRenDev->m_cEF.m_ShadersCache = "Shaders/Cache/D3D10/";
  gRenDev->m_cEF.m_ShadersFilter = "D3D10";
}

void CParserBin::SetupForPS20(bool bEnable)
{
  if (!bEnable)
  {
    RemoveMacro(CParserBin::fxToken("PS20Only"), m_StaticMacros);
    gRenDev->m_RP.m_nMaxLightsPerPass = 4;
    return;
  }
  uint32 nMacro[1] = {eT_1};
  AddMacro(CParserBin::fxToken("PS20Only"), nMacro, 1, 0, m_StaticMacros);
  gRenDev->m_RP.m_nMaxLightsPerPass = 1;
}

uint32 CParserBin::fxTokenKey(char *szToken, EToken eTC)
{
  g_KeyTokens[eTC] = szToken;
  return eTC;
}

uint32 CParserBin::fxToken(const char *szToken, bool* bKey)
{
  for (int i=0; i<eT_max; i++)
  {
    if (!g_KeyTokens[i])
      continue;
    if (!strcmp(szToken, g_KeyTokens[i]))
    {
      if (bKey)
        *bKey = true;
      return i;
    }
  }
  if (bKey)
    *bKey = false;
  return eT_unknown;
}

uint32 CParserBin::NewUserToken(uint32 nToken, const char *szToken, bool bUseFinalTable)
{
  if (nToken != eT_unknown)
    return nToken;
  nToken = GetCRC32(szToken);
  string strToken = szToken;
  if (bUseFinalTable)
  {
    FXShaderTokenItor itor = m_Table.find(nToken);
    if (itor != m_Table.end())
    {
      assert(itor->second.SToken == strToken);
      return nToken;
    }
    STokenD TD;
    TD.SToken = strToken;
    m_Table.insert(FXShaderTokenItor::value_type(nToken, TD));
  }
  else
  {
    SShaderBin *pBin = m_pCurBinShader;
    assert(pBin);
    FXShaderTokenItor itor = pBin->m_Table.find(nToken);
    if (itor != pBin->m_Table.end())
    {
      assert(itor->second.SToken == strToken);
      return nToken;
    }
    STokenD TD;
    TD.SToken = strToken;
    pBin->m_Table.insert(FXShaderTokenItor::value_type(nToken, TD));
  }
  
  return nToken;
}

uint32 CParserBin::NextToken(char*& buf, char *com, bool& bKey)
{
  char ch;
  int n = 0;
  while ((ch=*buf) != 0)
  {
    if (sSkipChars[ch] == 1)
      break;
    com[n++] = ch;
    if (n == 1024)
    {
      int nnn = 0;
    }
    ++buf;
    if (ch == '/')
      break;
  }
  if (!n)
  {
    if (ch != ' ')
    {
      com[n++] = ch;
      ++buf;
    }
  }
  com[n] = 0;
  uint32 dwToken = fxToken(com, &bKey);
  return dwToken;
}

bool CParserBin::AddMacro(uint32 dwName, uint32 *pMacro, int nMacroTokens, uint32 nMask, FXMacroBin& Macro)
{
  FXMacroBinItor it = Macro.find(dwName);
  if (it != Macro.end())
    Macro.erase(it);
  SMacroBinFX macro;
  macro.m_nMask = nMask;
  if (nMacroTokens)
  {
    macro.m_Macro.resize(nMacroTokens);
    memcpy(&macro.m_Macro[0], pMacro, nMacroTokens*sizeof(uint32));
  }
  Macro.insert(FXMacroBinItor::value_type(dwName, macro));
  return true;
}

bool CParserBin::RemoveMacro(uint32 dwName, FXMacroBin& Macro)
{
  FXMacroBinItor it = Macro.find(dwName);
  if (it == Macro.end())
    return false;
  else
    Macro.erase(dwName);
  return true;
}

const SMacroBinFX *CParserBin::FindMacro(uint32 dwName, FXMacroBin& Macro)
{
  FXMacroBinItor it = Macro.find(dwName);
  if (it != Macro.end())
    return &it->second;
  return NULL;
}

bool CParserBin::GetBool(SParserFrame& Frame)
{
  if (Frame.IsEmpty())
    return true;
  EToken eT = GetToken(Frame);
  if (eT == eT_true || eT == eT_1)
    return true;
  if (eT == eT_false || eT == eT_0)
    return false;
  assert(0);
  return false;
}

string CParserBin::GetString(SParserFrame& Frame)
{
  if (Frame.IsEmpty())
    return "";

  string Str;
  int nCur = Frame.m_nFirstToken;
  int nLast = Frame.m_nLastToken;
  while (nCur <= nLast)
  {
    int nTok = m_Tokens[nCur++];
    const char *szStr = GetString(nTok);
    if (Str.size() && !sSkipChars[Str[Str.size()-1]] && !sSkipChars[szStr[0]])
      Str += " ";
    Str += szStr;
  }
  return Str;
}

const char *CParserBin::GetString(uint32 nToken, FXShaderToken& Table, bool bOnlyKey)
{
  FXShaderTokenItor it;
  if (nToken < eT_user_first)
  {
    assert(g_KeyTokens[nToken]);
    return g_KeyTokens[nToken];
  }
  if (!bOnlyKey)
  {
    it = Table.find(nToken);
    if (it != Table.end())
    {
      return it->second.SToken.c_str();
    }
  }

  assert(0);
  return "";
}

const char *CParserBin::GetString(uint32 nToken, bool bOnlyKey)
{
  return GetString(nToken, m_Table, bOnlyKey);
}

static void sCR(TArray<char>& Text, int nLevel)
{
  Text.AddElem('\n');
  for (int i=0; i<nLevel; i++)
  {
    Text.AddElem(' ');
    Text.AddElem(' ');
  }
}

bool CParserBin::ConvertToAscii(uint32 *pTokens, uint32 nT, FXShaderToken& Table, TArray<char>& Text)
{
  uint32 i;
  bool bRes = true;

  const char *szPrev = " ";
  int nLevel = 0;
  for (i=0; i<nT; i++)
  {
    uint32 nToken = pTokens[i];
    if (nToken == 0)
    {
      Text.Copy("\n", 1);
      continue;
    }
    if (nToken == eT_skip)
    {
      i++;
      continue;
    }
    if (nToken == eT_skip_1)
    {
      while (i < nT)
      {
        nToken = pTokens[i];
        if (nToken == eT_skip_2)
          break;
        i++;
      }
      assert(i < nT);
      continue;
    }
    const char *szStr = GetString(nToken, Table, false);
    assert(szStr);
    if (!szStr)
      bRes = false;
    else
    {
      if (nToken == eT_semicolumn || nToken == eT_br_cv_1)
      {
        if (nToken == eT_br_cv_1)
        {
          sCR(Text, nLevel);
          nLevel++;
        }
        Text.Copy(szStr, strlen(szStr));
        if (nToken == eT_semicolumn)
        {
          if (i+1<nT && pTokens[i+1]==eT_br_cv_2)
            sCR(Text, nLevel-1);
          else
            sCR(Text, nLevel);
        }
        else
        if (i+1 < nT)
        {
          if (pTokens[i+1] < eT_br_rnd_1 || pTokens[i+1]>=eT_float)
            sCR(Text, nLevel);
        }
      }
      else
      {
        if (i+1 < nT)
        {
          if (Text.Num())
          {
            char cPrev = Text[Text.Num()-1];
            if (!sSkipChars[cPrev] && !sSkipChars[szStr[0]])
              Text.AddElem(' ');
          }
        }
        Text.Copy(szStr, strlen(szStr));
        if (nToken == eT_br_cv_2)
        {
          nLevel--;
          if (i+1<nT && pTokens[i+1]!=eT_semicolumn)
            sCR(Text, nLevel);
        }
      }
    }
  }
  Text.AddElem(0);

  return bRes;
}

void CParserBin::MergeTable(SShaderBin *pBin)
{
  FXShaderTokenItor itor;
  for (itor=pBin->m_Table.begin(); itor!=pBin->m_Table.end(); itor++)
  {
    uint32 nTok = itor->first;
    assert(nTok >= eT_user_first);
    FXShaderTokenItor it = m_Table.find(nTok);
    if (it != m_Table.end())
    {
      STokenD &TD = itor->second;
      STokenD &TD2 = it->second;
      assert(TD.SToken == TD.SToken);
      continue;
    }
    m_Table.insert(FXShaderTokenItor::value_type(nTok, itor->second));
  }
}

bool CParserBin::IgnorePreprocessBlock(uint32 *pTokens, uint32& nT, int nMaxTokens)
{
  int nLevel = 0;
  bool bEnded = false;
  int nTFirst = nT;
  while (*pTokens != 0)
  {
    if (nT >= nMaxTokens)
      break;
    uint32 nToken = NextToken(pTokens, nT, nMaxTokens-1);
    if (nToken >= eT_if && nToken <= eT_ifndef_2)
    {
      nLevel++;
      continue;
    }
    if (nToken == eT_endif)
    {
      if (!nLevel)
      {
        bEnded = true;
        nT--;
        break;
      }
      nLevel--;
    }
    else
    if (nToken == eT_else || nToken == eT_elif)
    {
      if (!nLevel)
      {
        nT--;
        break;
      }
    }
  }
  if (nT >= nMaxTokens)
  {
    assert(0);
    Warning("Couldn't find #endif directive for associated #ifdef");
    return false;
  }

  return bEnded;
}

void CParserBin::InsertSkipTokens(uint32 *pTokens, uint32 nStart, uint32 nTokens, bool bSingle)
{
  SParserFrame Fr;
  Fr.m_nFirstToken = nStart;
  Fr.m_nLastToken = nStart;

  if (!bSingle)
  {
    uint32 nS = nStart+1;
    CheckIfExpression(pTokens, nS, 0);
    Fr.m_nLastToken = nS-1;
  }

  if (Fr.m_nLastToken-Fr.m_nFirstToken == 0)
  {
    m_Tokens.push_back(eT_skip);
    m_Tokens.push_back(pTokens[Fr.m_nFirstToken]);
  }
  else
  {
    m_Tokens.push_back(eT_skip_1);
    while(Fr.m_nFirstToken <= Fr.m_nLastToken)
    {
      m_Tokens.push_back(pTokens[Fr.m_nFirstToken]);
      Fr.m_nFirstToken++;
    }
    m_Tokens.push_back(eT_skip_2);
  }
}

bool CParserBin::CheckIfExpression(uint32 *pTokens, uint32& nT, int nPass)
{
  uint32 tmpBuf[64];
  byte bRes[64];
  byte bOr[64];
  int nLevel = 0;
  int i;
  while (true)
  {
    uint32 nToken = pTokens[nT];
    if (nToken == eT_br_rnd_1) // check for '('
    {
      nT++;
      int n = 0;
      int nD = 0;
      while (true)
      {
        int nTok = pTokens[nT];
        if (nTok == eT_br_rnd_1) // check for '('
          n++;
        else
        if (nTok == eT_br_rnd_2) // check for ')'
        {
          if (!n)
          {
            tmpBuf[nD] = 0;
            nT++;
            break;
          }
          n--;
        }
        else
        if (nTok == 0)
          return false;
        tmpBuf[nD++] = nTok;
        nT++;
      }
      uint32 nT2 = 0;
      bRes[nLevel] = CheckIfExpression(&tmpBuf[0], nT2, nPass);
      nLevel++;
      bOr[nLevel] = 255;
    }
    else
    {
      uint32 nTok = pTokens[nT++];
      bool bNeg = false;
      if (nTok == eT_excl)
      {
        bNeg = true;
        nTok = pTokens[nT++];
      }
      const SMacroBinFX *pFound = FindMacro(nTok, m_Macros[nPass]);
      if (!pFound)
        pFound = FindMacro(nTok, m_StaticMacros);
      if (!pFound && !nPass)
        pFound = FindMacro(nTok, m_Macros[1]);
      bRes[nLevel] = (pFound) ? true : false;
      if (bNeg)
        bRes[nLevel] = !bRes[nLevel];
      nLevel++;
      bOr[nLevel] = 255;
    }
    uint32 nTok = pTokens[nT];
    if (nTok == eT_or)
    {
      bOr[nLevel] = true;
      nT++;
      assert (pTokens[nT] == eT_or);
      if (pTokens[nT] == eT_or)
        nT++;
    }
    else
    if (nTok == eT_and)
    {
      bOr[nLevel] = false;
      nT++;
      assert (pTokens[nT] == eT_and);
      if (pTokens[nT] == eT_and)
        nT++;
    }
    else
      break;
  }
  byte Res = false;
  for (i=0; i<nLevel; i++)
  {
    if (!i)
      Res = bRes[i];
    else
    {
      assert(bOr[i] != 255);
      if (bOr[i])
        Res = Res | bRes[i];
      else
        Res = Res & bRes[i];
    }
  }
  return Res != 0;
}

void CParserBin::BuildSearchInfo()
{
/*  int i;
  for (i=0; i<m_Tokens.size(); i++)
  {
    uint32 nToken = m_Tokens[i];
    if (nToken < eT_max)
    {
      if (m_KeyOffsets.size() <= nToken)
        m_KeyOffsets.resize(nToken+1);
      std::vector<int>& Dst = m_KeyOffsets[nToken];
      Dst.push_back(i);
    }
    else
    {
      FXShaderTokenItor itor = m_Table.find(nToken);
      if (itor != m_Table.end())
        itor->second.Offsets.push_back(i);
      else
      {
        assert(0);
      }
    }
  }
  m_bEmbeddedSearchInfo = true;*/
}

bool CParserBin::PreprocessTokens(DynArray<uint32>& Tokens, int nPass)
{
  bool bRes = true;
  uint32 nTokenParam;

  uint32 nT = 0;
  uint32 *pTokens = &Tokens[0];
  while (nT < Tokens.size())
  {
    uint32 nToken = NextToken(pTokens, nT, Tokens.size()-1);
    if (!nToken)
      break;
    bool bFirst = false;
    switch (nToken)
    {
    case eT_include:
      {
        nTokenParam = pTokens[nT++];
        const char *szName = GetString(nTokenParam, false);
        assert(szName);
        SShaderBin *pBin = gRenDev->m_cEF.m_Bin.GetBinShader(szName, true);
        if (!pBin)
        {
          iLog->Log("Warning: Couldn't find include file '%s'", szName);
          assert(0);
        }
        else
        {
          assert(pBin);
          MergeTable(pBin);
          pBin->Lock();
          PreprocessTokens(pBin->m_Tokens, nPass);
          pBin->Unlock();
        }
      }
      break;
    case eT_define:
    case eT_define_2:
      {
        uint32 nMacro = 0;
        int n = nPass;
        uint32 nMask = 0;
        nTokenParam = pTokens[nT++];
        /*const char *szname = GetString(nTokenParam);
        if (!stricmp(szname, "%_SHADOW_GEN"))
        {
          int nnn = 0;
        }*/
        uint32 *pMacro = &pTokens[nT];
        while (pMacro[nMacro])
          nMacro++;
        if (nToken == eT_define_2)
        {
          n = 1;
          if (nMacro)
            nMask = GetInt(pMacro[0]);
        }
        AddMacro(nTokenParam, pMacro, nMacro, nMask, m_Macros[n]);
        nT += nMacro+1;
      }
      break;
    case eT_undefine:
      {
        uint32 nMacro = 0;
        nTokenParam = pTokens[nT++];
        int n = nPass;
        FXMacroBinItor it = m_Macros[nPass].find(nTokenParam);
        if (it == m_Macros[nPass].end() && !nPass)
        {
          it = m_Macros[1].find(nTokenParam);
          n = 1;
        }
        if (it == m_Macros[nPass].end())
        {
          Warning("Couldn't find macro '%s'", GetString(nTokenParam));
        }
        else
        {
          m_Macros[n].erase(nTokenParam);
        }
      }
      break;
    case eT_if:
    case eT_ifdef:
    case eT_ifndef:
      bFirst = true;
    case eT_if_2:
    case eT_ifdef_2:
    case eT_ifndef_2:
      if ((nPass==0 && !bFirst) || (nPass==1 && bFirst))
      {
        if (nPass == 1)
        {
          assert(0);
        }

        sfxIFIgnore.AddElem(true);
        sfxIFDef.AddElem(false);
        m_Tokens.push_back(nToken);
      }
      else
      {
        if (!nPass)
          InsertSkipTokens(pTokens, nT-1, Tokens.size(), false);
        sfxIFIgnore.AddElem(false);
        bool bRes = CheckIfExpression(pTokens, nT, nPass);
        if (nToken == eT_ifndef || nToken == eT_ifndef_2)
          bRes = !bRes;
        if (!bRes)
        {
          IgnorePreprocessBlock(pTokens, nT, Tokens.size());
          sfxIFDef.AddElem(false);
        }
        else
          sfxIFDef.AddElem(true);
      }
      break;
    case eT_elif:
      {
        int nLevel = sfxIFDef.Num()-1;
        if (nLevel < 0)
        {
          assert(0);
          Warning("#elif without #ifdef");
          return false;
        }
        if (!nPass)
          InsertSkipTokens(pTokens, nT-1, Tokens.size(), false);
        if (sfxIFIgnore[nLevel] == true)
          m_Tokens.push_back(nToken);
        else
        {
          if (sfxIFDef[nLevel] == true)
          {
            IgnorePreprocessBlock(pTokens, nT, Tokens.size());
          }
          else
          {
            bool bRes = CheckIfExpression(pTokens, nT, nPass);
            if (!bRes)
              IgnorePreprocessBlock(pTokens, nT, Tokens.size());
            else
              sfxIFDef[nLevel] = true;
          }
        }
      }
      break;
    case eT_else:
      {
        int nLevel = sfxIFDef.Num()-1;
        if (nLevel < 0)
        {
          assert(0);
          Warning("#else without #ifdef");
          return false;
        }
        if (!nPass)
          InsertSkipTokens(pTokens, nT-1, Tokens.size(), true);
        if (sfxIFIgnore[nLevel] == true)
          m_Tokens.push_back(nToken);
        else
        {
          if (sfxIFDef[nLevel] == true)
          {
            bool bEnded = IgnorePreprocessBlock(pTokens, nT, Tokens.size());
            if (!bEnded)
            {
              assert(0);
              Warning("#else or #elif after #else");
              return false;
            }
          }
        }
      }
      break;
    case eT_endif:
      {
        int nLevel = sfxIFDef.Num()-1;
        if (nLevel < 0)
        {
          assert(0);
          Warning( "#endif without #ifdef");
          return false;
        }
        if (sfxIFIgnore[nLevel] == true)
          m_Tokens.push_back(nToken);
        sfxIFDef.Remove(nLevel);
        sfxIFIgnore.Remove(nLevel);
      }
      break;
    case eT_warning:
      {
        const char *szStr = GetString(pTokens[nT++]);
        Warning(szStr);
      }
      break;
    case eT_register_env:
      {
        const char *szStr = GetString(pTokens[nT++]);
        if (szStr && !strcmp(szStr, "LIGHT_SETUP"))
          m_bNewLightSetup = true;
        fxRegisterEnv(szStr);
      }
      break;
    case eT_ifcvar:
    case eT_ifncvar:
      {
        const char *szStr = GetString(pTokens[nT++]);
        sfxIFIgnore.AddElem(false);
        ICVar *pVar = iConsole->GetCVar(szStr);
        int nVal = 0;
        if (!pVar)
          iLog->Log("Warning: couldn't find variable '%s'", szStr);
        else
          nVal = pVar->GetIVal();
        if (nToken == eT_ifncvar)
          nVal = !nVal;
        if (!nVal)
        {
          IgnorePreprocessBlock(pTokens, nT, Tokens.size());
          sfxIFDef.AddElem(false);
        }
        else
          sfxIFDef.AddElem(true);
      }
      break;
    case eT_elifcvar:
      {
        int nLevel = sfxIFDef.Num()-1;
        if (nLevel < 0)
        {
          assert(0);
          Warning("#elifcvar without #ifcvar or #ifdef");
          return false;
        }
        sfxIFIgnore.AddElem(false);
        if (sfxIFDef[nLevel] == true)
        {
          IgnorePreprocessBlock(pTokens, nT, Tokens.size());
        }
        else
        {
          const char *szStr = GetString(pTokens[nT++]);
          ICVar *pVar = iConsole->GetCVar(szStr);
          int nVal = 0;
          if (!pVar)
            iLog->Log("Warning: couldn't find variable '%s'", szStr);
          else
            nVal = pVar->GetIVal();
          if (!nVal)
            IgnorePreprocessBlock(pTokens, nT, Tokens.size());
          else
            sfxIFDef[nLevel] = true;
        }
      }
      break;
    default:
      {
        FXMacroBinItor it = m_Macros[nPass].find(nToken);
        if (it != m_Macros[nPass].end())
        {
          // Found macro
          SMacroBinFX *pM = &it->second;
          for (int i=0; i<pM->m_Macro.size(); i++)
          {
            m_Tokens.push_back(pM->m_Macro[i]);
          }
        }
        else
          m_Tokens.push_back(nToken);
      }
      break;
    }
  }

  return bRes;
}

bool CParserBin::Preprocess(int nPass, DynArray<uint32>& Tokens, FXShaderToken *pSrcTable)
{
  bool bRes = true;

  sfxIFDef.SetUse(0);
  sfxIFIgnore.SetUse(0);

  m_Macros[nPass].clear();

  m_Table = *pSrcTable;

  /*{
    TArray<char> Text;
    ConvertToAscii(&Tokens[0], Tokens.size(), m_Table, Text);
    FILE *fp;
    if (!nPass)
      fp = fopen("SrcBin1.cg", "wb");
    else
      fp = fopen("SrcBin2.cg", "wb");
    if (fp)
    {
      fwrite(&Text[0], 1, Text.Num()-1, fp);
      fclose(fp);
    }
  }*/

  bRes = PreprocessTokens(Tokens, nPass);
  if (!nPass)
    BuildSearchInfo();

  assert(!sfxIFDef.Num());
  assert(!sfxIFIgnore.Num());

  /*{
    TArray<char> Text;
    ConvertToAscii(&m_Tokens[0], m_Tokens.size(), m_Table, Text);
    FILE *fp;
    if (!nPass)
      fp = fopen("PreprBin1.cg", "wb");
    else
      fp = fopen("PreprBin2.cg", "wb");
    if (fp)
    {
      fwrite(&Text[0], 1, Text.Num()-1, fp);
      fclose(fp);
    }
  }*/

  return bRes;
}

int CParserBin::CopyTokens(SParserFrame& Fragment, DynArray<uint32>& NewTokens)
{
  if (Fragment.IsEmpty())
    return 0;
  int nCopy = Fragment.m_nLastToken-Fragment.m_nFirstToken+1;
  int nStart = NewTokens.size();
  NewTokens.resize(nStart+nCopy);
  memcpy(&NewTokens[nStart], &m_Tokens[Fragment.m_nFirstToken], nCopy*sizeof(uint32));

  return nCopy;
}

int CParserBin::CopyTokens(SCodeFragment *pCF, DynArray<uint32>& SHData, DynArray<SCodeFragment>& Replaces, DynArray<uint32>& NewTokens, uint32 nID)
{
  int nRepl = -1;
  int i;

  for (i=0; i<Replaces.size(); i++)
  {
    SCodeFragment *pFR = &Replaces[i];

    if (pFR->m_dwName == nID)
      break;
  }
  if (i != Replaces.size())
  {
    assert (!(i & 1));
    nRepl = i;
  }
  int nDst = SHData.size();
  int nSrc = pCF->m_nFirstToken;
  uint32 *pSrc = &m_Tokens[0];
  int nSize = pCF->m_nLastToken-nSrc+1;
  if (nRepl >= 0)
  {
    int nDstStart = nDst;
    uint32 *pSrc2 = &NewTokens[0];
    while (nSrc <= pCF->m_nLastToken)
    {
      if (nRepl >= Replaces.size() || Replaces[nRepl].m_dwName != nID)
      {
        int nCopy = pCF->m_nLastToken - nSrc + 1;
        SHData.resize(nDst+nCopy);
        memcpy(&SHData[nDst], &pSrc[nSrc], nCopy*sizeof(uint32));
        nSrc += nCopy;
        nDst += nCopy;
      }
      else
      {
        int nCopy = Replaces[nRepl].m_nFirstToken - nSrc;
        if (nCopy)
        {
          assert(nCopy > 0);
          SHData.resize(nDst+nCopy);
          memcpy(&SHData[nDst], &pSrc[nSrc], nCopy*sizeof(uint32));
          nSrc += nCopy + (Replaces[nRepl].m_nLastToken - Replaces[nRepl].m_nFirstToken + 1);
          nDst += nCopy;
        }
        nRepl++;
        nCopy = Replaces[nRepl].m_nLastToken - Replaces[nRepl].m_nFirstToken + 1;
        SHData.resize(nDst+nCopy);
        memcpy(&SHData[nDst], &pSrc2[Replaces[nRepl].m_nFirstToken], nCopy*sizeof(uint32));
        nDst += nCopy;
        nRepl++;
      }
    }
    return SHData.size() - nDstStart;
  }
  else
  {
    SHData.resize(nDst+nSize);
    memcpy(&SHData[nDst], &pSrc[nSrc], nSize*sizeof(uint32));
    return nSize;
  }
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, const uint32 *pTokens, uint32 nToken)
{
  while (nStart <= nLast)
  {
    if (pTokens[nStart] == nToken)
      return nStart;
    nStart++;
  }
  return -1;
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, uint32 nToken)
{
  //if (!m_bEmbeddedSearchInfo)
    return FindToken(nStart, nLast, &m_Tokens[0], nToken);
  /*std::vector<int> *pSI;
  if (nToken < eT_max)
  {
    if (nToken >= m_KeyOffsets.size())
      return -1;
    pSI = &m_KeyOffsets[nToken];
  }
  else
  {
    FXShaderTokenItor itor = m_Table.find(nToken);
    if (itor == m_Table.end())
      return -1;
    pSI = &itor->second.Offsets;
  }
  for (int i=0; i<pSI->size(); i++)
  {
    int nOffs = (*pSI)[i];
    if (nOffs>=nStart && nOffs<=nLast)
      return nOffs;
  }
  return -1;*/
}

int32 CParserBin::FindToken(uint32 nStart, uint32 nLast, const uint32 *pToks)
{
  uint32 *pTokens = &m_Tokens[0];
  while (nStart <= nLast)
  {
    int n = 0;
    uint32 nTok;
    while (nTok=pToks[n])
    {
      if (pTokens[nStart] == nTok)
        return nStart;
      n++;
    }
    nStart++;
  }
  return -1;
}

int CParserBin::GetNextToken(uint32& nStart)
{
  while (true)
  {
    bool bFunction = false;

    if (m_CurFrame.m_nCurToken >= m_CurFrame.m_nLastToken)
      return -1;

    uint32 nToken;
    if ((nToken=m_Tokens[m_CurFrame.m_nCurToken]) == eT_unknown)
      return -2;

    nStart = m_CurFrame.m_nCurToken;
    bool bFound = false;

    if (nToken == eT_quote)
    {
      m_CurFrame.m_nCurToken++;
      continue;
    }
    if (nToken == eT_skip)
    {
      m_CurFrame.m_nCurToken += 2;
      continue;
    }
    if (nToken == eT_skip_1)
    {
      while (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken)
      {
        nToken = m_Tokens[m_CurFrame.m_nCurToken++];
        if (nToken == eT_skip_2)
          break;
      }
      continue;
    }
    // if preprocessor
    if (nToken >= eT_include && nToken <= eT_elifcvar)
    {
      SCodeFragment Fr;
      Fr.m_nFirstToken = m_CurFrame.m_nCurToken;
      if (nToken >= eT_if && nToken <= eT_elif)
      {
        while (m_Tokens[m_CurFrame.m_nCurToken])
        {
          if (m_CurFrame.m_nCurToken+1 == m_Tokens.size())
            break;
          m_CurFrame.m_nCurToken++;
        }
      }
      Fr.m_nLastToken = m_CurFrame.m_nCurToken++;
      m_CodeFragments.push_back(Fr);
    }
    else
    {
      m_CurFrame.m_nCurToken = nStart;
      // Check for function
      uint32 nLastTok = nStart;
      uint32 nFnName;
      if (m_CurFrame.m_nCurToken+4 < m_CurFrame.m_nLastToken)
      {
        uint32 nFnRet  = m_Tokens[m_CurFrame.m_nCurToken];
        // DX10 stuff
        if (nFnRet == eT_br_sq_1)
        {
          nLastTok = FindToken(m_CurFrame.m_nCurToken+1, m_CurFrame.m_nLastToken, eT_br_sq_2);
          if (nLastTok > 0)
          {
            nLastTok++;
            nFnRet = m_Tokens[nLastTok];
          }

        }
        nFnName = m_Tokens[nLastTok+1];
        if (m_Tokens[nLastTok+2] == eT_br_rnd_1)
        {
          nLastTok = FindToken(nLastTok+3, m_CurFrame.m_nLastToken, eT_br_cv_1);
          int nRecurse = 0;
          while (nLastTok <= m_CurFrame.m_nLastToken)
          {
            uint32 nT = m_Tokens[nLastTok];
            if (nT == eT_br_cv_1)
              nRecurse++;
            else
            if (nT == eT_br_cv_2)
            {
              nRecurse--;
              if (nRecurse == 0)
              {
                bFunction = true;
                break;
              }
            }
            nLastTok++;
          }
        }
      }
      if (bFunction)
      {
        SCodeFragment Fr;
        Fr.m_nFirstToken = m_CurFrame.m_nCurToken;
        Fr.m_nLastToken = nLastTok;
        Fr.m_dwName = nFnName;
#ifdef _DEBUG
        Fr.m_Name = GetString(nFnName);
#endif
        Fr.m_eType = eFT_Function;
        m_CodeFragments.push_back(Fr);
        m_CurFrame.m_nCurToken = nLastTok+1;
      }
      else
      {
        m_eToken = (EToken)nToken;
        assert(m_eToken < eT_user_first);
        m_CurFrame.m_nCurToken++;
        break;
      }
    }
  }
  return 1;
}

bool CParserBin::FXGetAssignmentData(SParserFrame& Frame)
{
  bool bRes = true;

  Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
  int nLastToken = m_CurFrame.m_nCurToken;
  while (nLastToken <= m_CurFrame.m_nLastToken)
  {
    uint32 nTok = m_Tokens[nLastToken++];
    if (nTok == eT_quote)
    {
      while (nLastToken <= m_CurFrame.m_nLastToken)
      {
        nTok = m_Tokens[nLastToken++];
        if (nTok == eT_quote)
          break;
      }
    }
    else
    if (nTok == eT_semicolumn)
      break;
  }
  Frame.m_nLastToken = nLastToken-2;
  if (m_Tokens[nLastToken] == eT_semicolumn)
    m_CurFrame.m_nCurToken = nLastToken+1;
  else
    m_CurFrame.m_nCurToken = nLastToken;

  return bRes;
}

bool CParserBin::FXGetAssignmentData2(SParserFrame& Frame)
{
  bool bRes = true;

  Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
  int nLastToken = m_CurFrame.m_nCurToken;
  uint32 nTok = m_Tokens[nLastToken];
  if (nTok == eT_br_cv_1)
  {
    nLastToken++;
    while (nLastToken+1 <= m_CurFrame.m_nLastToken)
    {
      nTok = m_Tokens[nLastToken];
      if (nTok == eT_semicolumn)
        break;
      nLastToken++;
    }
  }
  else
  if (nTok == eT_br_rnd_1)
  {
    nLastToken++;
    int n = 1;
    while (nLastToken+1 <= m_CurFrame.m_nLastToken)
    {
      nTok = m_Tokens[nLastToken];
      if (nTok == eT_semicolumn || nTok == eT_br_tr_1 || nTok == eT_eq)
      {
        assert(!n);
        break;
      }
      if (nTok == eT_br_rnd_1)
        n++;
      else
      if (nTok == eT_br_rnd_2)
        n--;
      nLastToken++;
    }
  }
  else
  {
    while (nLastToken <= m_CurFrame.m_nLastToken)
    {
      nTok = m_Tokens[nLastToken];
      if (nTok == eT_semicolumn || nTok == eT_br_rnd_1 || nTok == eT_br_cv_1 || nTok == eT_br_tr_1)
        break;
      nLastToken++;
    }
  }

  Frame.m_nLastToken = nLastToken-1;
  if (m_Tokens[nLastToken] == eT_semicolumn)
    m_CurFrame.m_nCurToken = nLastToken+1;
  else
    m_CurFrame.m_nCurToken = nLastToken;

  return bRes;
}


bool CParserBin::GetAssignmentData(SParserFrame& Frame)
{
  bool bRes = true;

  Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
  int nLastToken = m_CurFrame.m_nCurToken;
  uint32 *pTokens = &m_Tokens[0];
  uint32 nTok = pTokens[nLastToken+1];
  if (nTok == eT_br_sq_1 || nTok == eT_br_rnd_1)
  {
    EToken eTClose = nTok == eT_br_sq_1 ? eT_br_sq_2 : eT_br_rnd_2;
    nLastToken += 2;
    while (nLastToken <= m_CurFrame.m_nLastToken)
    {
      nTok = pTokens[nLastToken];
      if (nTok == eTClose || nTok == eT_semicolumn)
      {
        if (nTok == eT_semicolumn)
          nLastToken--;
        break;
      }
      nLastToken++;
    }
  }
  Frame.m_nLastToken = nLastToken;

  nLastToken++;

  if (pTokens[nLastToken] == eT_semicolumn)
    m_CurFrame.m_nCurToken = nLastToken+1;
  else
    m_CurFrame.m_nCurToken = nLastToken;

  return bRes;
}

bool CParserBin::GetSubData(SParserFrame& Frame, EToken eT1, EToken eT2)
{
  bool bRes = true;

  Frame.m_nFirstToken = 0;
  Frame.m_nLastToken = 0;
  uint32 nTok = m_Tokens[m_CurFrame.m_nCurToken];
  if (nTok != eT1)
    return false;
  m_CurFrame.m_nCurToken++;
  Frame.m_nFirstToken = m_CurFrame.m_nCurToken;
  uint32 nCurToken = m_CurFrame.m_nCurToken;
  int skip = 1;
  while (nCurToken <= m_CurFrame.m_nLastToken)
  {
    nTok = m_Tokens[nCurToken];
    if (nTok == eT1)
      skip++;
    else
    if (nTok == eT2)
    {
      if (--skip == 0)
      {
        Frame.m_nLastToken = nCurToken-1; 
        nCurToken++;
        break;
      }
    }
    nCurToken++;
  }
  if (Frame.IsEmpty())
    Frame.Reset();
  if (nCurToken <= m_CurFrame.m_nLastToken && m_Tokens[nCurToken] == eT_semicolumn)
    m_CurFrame.m_nCurToken = nCurToken+1;
  else
    m_CurFrame.m_nCurToken = nCurToken;

  return (Frame.m_nFirstToken <= Frame.m_nLastToken);
}


bool CParserBin::ParseObject(SFXTokenBin *pTokens)
{
  bool bRes = true;

  assert(m_CurFrame.m_nFirstToken <= m_CurFrame.m_nLastToken);

  if (m_CurFrame.m_nCurToken+1 >= m_CurFrame.m_nLastToken)
    return false;
  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_unknown)
    return false;

  int nRes = GetNextToken(m_nFirstToken);
  if (nRes < 0)
    return false;

  bool bSamplerMS = false;

  if (m_Tokens[m_nFirstToken] == eT_Texture2DMS)
  {
    bSamplerMS = true;
  }

  m_Name.Reset();
  m_Assign.Reset();
  m_Value.Reset();
  m_Data.Reset();
  m_Annotations.Reset();

  SFXTokenBin *pT = pTokens;
  while (pTokens->id != 0)
  {
    if (pTokens->id == m_eToken)
      break;
    pTokens++;
  }
  if (pTokens->id == 0)
  {
    pTokens = pT;
    Warning ("FXBin parser found token '%s' which was not one of the list (Skipping).\n", GetString(m_eToken));
    while (pTokens->id != 0)
    {
#if !defined(PS3)
      Warning("    %s\n", GetString(pTokens->id));
#endif
      pTokens++;
    }
#if !defined(PS3)
    assert(0);
#endif
#ifdef _DEBUG
    TArray<char> Text;
    SParserFrame Fr;
    Fr.m_nFirstToken = max(m_CurFrame.m_nFirstToken, m_CurFrame.m_nCurToken-5);
    Fr.m_nLastToken = m_CurFrame.m_nLastToken;
    ConvertToAscii(&m_Tokens[Fr.m_nFirstToken], Fr.m_nLastToken-Fr.m_nFirstToken+1, m_Table, Text);
#endif
    return 0;                 
  }


  GetAssignmentData(m_Name);
  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_colon)
  {
    m_CurFrame.m_nCurToken++;                       // skip the ':'
    GetAssignmentData(m_Assign);
  }

  GetSubData(m_Annotations, eT_br_tr_1, eT_br_tr_2); 
  if (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken)
  {
    if (m_Tokens[m_CurFrame.m_nCurToken] == eT_eq)
    {
      m_CurFrame.m_nCurToken++;                       // skip the '='
      FXGetAssignmentData2(m_Value);
    }

    if (bSamplerMS)
    {
      m_CurFrame.m_nCurToken += 5;
    }

    GetSubData(m_Data, eT_br_cv_1, eT_br_cv_2);
  }

  if (m_CurFrame.m_nCurToken <= m_CurFrame.m_nLastToken && m_Tokens[m_CurFrame.m_nCurToken] == eT_semicolumn)
    m_CurFrame.m_nCurToken++;

  return bRes;
}

bool CParserBin::ParseObject(SFXTokenBin *pTokens, int& nIndex)
{
  bool bRes = true;

  assert(m_CurFrame.m_nFirstToken <= m_CurFrame.m_nLastToken);

  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_unknown)
    return false;
  if (m_CurFrame.m_nCurToken+1 >= m_CurFrame.m_nLastToken)
    return false;

  int nRes = GetNextToken(m_nFirstToken);
  if (nRes < 0)
    return false;

  m_Name.Reset();
  m_Assign.Reset();
  m_Value.Reset();
  m_Data.Reset();
  m_Annotations.Reset();

  SFXTokenBin *pT = pTokens;
  while (pTokens->id != 0)
  {
    if (pTokens->id == m_eToken)
      break;
    pTokens++;
  }
  if (pTokens->id == 0)
  {
    pTokens = pT;
    Warning ("Warning: FXBin parser found token '%s' which was not one of the list (Skipping).\n", GetString(m_eToken));
    while (pTokens->id != 0)
    {
#if !defined(PS3)
      Warning("    %s\n", GetString(pTokens->id));
#endif
      pTokens++;
    }
#if !defined(PS3)
    assert(0);
#endif
#ifdef _DEBUG
    TArray<char> Text;
    SParserFrame Fr;
    Fr.m_nFirstToken = max(m_CurFrame.m_nFirstToken, m_CurFrame.m_nCurToken-5);
    Fr.m_nLastToken = m_CurFrame.m_nLastToken;
    ConvertToAscii(&m_Tokens[Fr.m_nFirstToken], Fr.m_nLastToken-Fr.m_nFirstToken+1, m_Table, Text);
#endif
    return 0;                 
  }
  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_br_sq_1)
  {
    m_CurFrame.m_nCurToken++;
    nIndex = GetInt(m_Tokens[m_CurFrame.m_nCurToken++]);
  }
  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_sing_quote)
    GetSubData(m_Name, eT_sing_quote, eT_sing_quote);
  else
  if (m_Tokens[m_CurFrame.m_nCurToken] != eT_eq)
    m_Name.m_nFirstToken = m_Name.m_nLastToken = m_CurFrame.m_nCurToken++;

  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_eq)
  {
    m_CurFrame.m_nCurToken++;
    FXGetAssignmentData(m_Data);
  }
  else
    GetSubData(m_Data, eT_br_cv_1, eT_br_cv_2);

  if (m_Tokens[m_CurFrame.m_nCurToken] == eT_semicolumn || m_Tokens[m_CurFrame.m_nCurToken] == eT_quote)
    m_CurFrame.m_nCurToken++;

  return bRes;
}

bool CParserBin::JumpSemicolumn(uint32& nStart, uint32 nEnd)
{
  while (nStart <= nEnd)
  {
    uint32 nTok = m_Tokens[nStart++];
    if (nTok == eT_semicolumn)
      return true;
  }
  return false;
}

SParserFrame CParserBin::BeginFrame(SParserFrame& Frame)
{
  SParserFrame RetFrame = m_CurFrame;
  m_CurFrame = Frame;
  m_eToken = eT_unknown;
  m_CurFrame.m_nCurToken = Frame.m_nFirstToken;

  return RetFrame;
}

void CParserBin::EndFrame(SParserFrame& Frame)
{
  m_CurFrame = Frame;
}

void SFXParam::PostLoad(CParserBin& Parser, SParserFrame& Name, SParserFrame& Annotations, SParserFrame& Values, SParserFrame& Assign)
{
  m_Annotations = Parser.GetString(Annotations);
  if (!Values.IsEmpty())
  {
    if (Parser.GetToken(Values) == eT_br_cv_1)
    {
      Values.m_nFirstToken++;
      int32 nFind = Parser.FindToken(Values.m_nFirstToken, Values.m_nLastToken, eT_br_cv_2);
      assert(nFind > 0 && Values.m_nLastToken==nFind);
      if (nFind > 0)
        Values.m_nLastToken--;
    }
    m_Values = Parser.GetString(Values);
  }
  m_Assign = Parser.GetString(Assign);
  m_Name = Parser.GetString(Name);
                    
  m_nFlags = 0;
  if (m_nComps == 1 && m_nParameters <= 1)
    m_nFlags |= PF_SCALAR;
  if (m_eType == eType_INT)
    m_nFlags |= PF_INTEGER;
  else
  if (m_eType == eType_BOOL)
    m_nFlags |= PF_BOOL;
  else
  if (m_eType != eType_FLOAT)
  {
    assert(0);
  }

  if (Annotations.IsEmpty())
    return;

  uint32 nCur  = Annotations.m_nFirstToken;
  uint32 nLast = Annotations.m_nLastToken;
  uint32 *pTokens = &Parser.m_Tokens[0];
  while (nCur <= nLast)
  {
    uint32 nTok = pTokens[nCur++];
    if (nTok == eT_psregister || nTok == eT_vsregister || nTok == eT_gsregister)
    {
      m_nFlags |= PF_CUSTOM_BINDED;
      {
        uint32 nTok2 = pTokens[nCur++];
        if (nTok2 != eT_eq)
        {
          assert(0);
        }
        else
        {
          nTok2 = pTokens[nCur++];
          const char *szReg = Parser.GetString(nTok2);
          assert(szReg[0] == 'c');
          if (nTok == eT_vsregister)
            m_nRegister[eHWSC_Vertex] = atoi(&szReg[1]);
          else
          if (nTok == eT_psregister)
            m_nRegister[eHWSC_Pixel] = atoi(&szReg[1]);
          else
          if (CParserBin::m_bD3D10 && nTok == eT_gsregister)
            m_nRegister[eHWSC_Geometry] = atoi(&szReg[1]);
        }
      }
    }
    else
    if (nTok == eT_Position)
      m_nFlags |= PF_POSITION;
    else
    if (nTok == eT_Allways)
      m_nFlags |= PF_ALWAYS;
    else
    {
      if (nTok == eT_string)
      {
        uint32 nTokName = pTokens[nCur++];
        if (nTokName == eT_UIWidget || nTokName == eT_UIWidget0)
        {
          uint32 nTok0 = pTokens[nCur++];
          uint32 nTok1 = pTokens[nCur++];
          if (nTok0 == eT_eq && nTok1 == eT_quote)
          {
            nTok = pTokens[nCur++];
            if (nTok == eT_color)
              m_nFlags |= PF_TWEAKABLE_MASK;
            else
              m_nFlags |= PF_TWEAKABLE_0;
          }
        }
        else
        if (nTokName == eT_UIWidget1)
          m_nFlags |= PF_TWEAKABLE_1;
        else
        if (nTokName == eT_UIWidget2)
          m_nFlags |= PF_TWEAKABLE_2;
        else
        if (nTokName == eT_UIWidget3)
          m_nFlags |= PF_TWEAKABLE_3;
      }
    }
    if (!Parser.JumpSemicolumn(nCur, nLast))
      return;
  }
}

byte CParserBin::GetCompareFunc(EToken eT)
{
  switch (eT)
  {
    case eT_None:
    case eT_Disable:
      return eCF_Disable;
    case eT_Never:
      return eCF_Never;
    case eT_Less:
      return eCF_Less;
    case eT_Equal:
      return eCF_Equal;
    case eT_LEqual:
    case eT_LessEqual:
      return eCF_LEqual;
    case eT_Greater:
      return eCF_Greater;
    case eT_NotEqual:
      return eCF_NotEqual;
    case eT_GEqual:
    case eT_GreaterEqual:
      return eCF_NotEqual;
    case eT_Always:
      return eCF_Always;
    default:
      Warning("unknown CompareFunc parameter '%s' (Skipping)\n", GetString(eT));
  }
  return eCF_Less;
}

int CParserBin::GetSrcBlend(EToken eT)
{
  switch (eT)
  {
    case eT_ONE:
      return GS_BLSRC_ONE;
    case eT_ZERO:
      return GS_BLSRC_ZERO;
    case eT_DST_COLOR:
    case eT_DestColor:
      return GS_BLSRC_DSTCOL;
    case eT_ONE_MINUS_DST_COLOR:
    case eT_InvDestColor:
      return GS_BLSRC_ONEMINUSDSTCOL;
    case eT_SRC_ALPHA:
    case eT_SrcAlpha:
      return GS_BLSRC_SRCALPHA;
    case eT_ONE_MINUS_SRC_ALPHA:
    case eT_InvSrcAlpha:
      return GS_BLSRC_ONEMINUSSRCALPHA;
    case eT_DST_ALPHA:
    case eT_DestAlpha:
      return GS_BLSRC_DSTALPHA;
    case eT_ONE_MINUS_DST_ALPHA:
    case eT_InvDestAlpha:
      return GS_BLSRC_ONEMINUSDSTALPHA;
    case eT_SRC_ALPHA_SATURATE:
      return GS_BLSRC_ALPHASATURATE;

  default:
    {
      Warning("unknown SrcBlend parameter '%s' (Skipping)\n", GetString(eT));
      assert(0);
    }
  }
  return GS_BLSRC_ONE;
}

int CParserBin::GetDstBlend(EToken eT)
{
  switch (eT)
  {
    case eT_ONE:
      return GS_BLDST_ONE;
    case eT_ZERO:
      return GS_BLDST_ZERO;
    case eT_SRC_COLOR:
    case eT_SrcColor:
      return GS_BLDST_SRCCOL;
    case eT_ONE_MINUS_SRC_COLOR:
    case eT_InvSrcColor:
      return GS_BLDST_ONEMINUSSRCCOL;
    case eT_SRC_ALPHA:
    case eT_SrcAlpha:
      return GS_BLDST_SRCALPHA;
    case eT_ONE_MINUS_SRC_ALPHA:
    case eT_InvSrcAlpha:
      return GS_BLDST_ONEMINUSSRCALPHA;
    case eT_DST_ALPHA:
    case eT_DestAlpha:
      return GS_BLDST_DSTALPHA;
    case eT_ONE_MINUS_DST_ALPHA:
    case eT_InvDestAlpha:
      return GS_BLDST_ONEMINUSDSTALPHA;

  default:
    {
      Warning("unknown DstBlend parameter '%s' (Skipping)\n", GetString(eT));
      assert(0);
    }
  }
  return GS_BLDST_ONE;

}
