/*=============================================================================
PostProcess.cpp : Post process main interface
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tiago Sousa

=============================================================================*/

#include "StdAfx.h"
#include "PostEffects.h"

using namespace NPostEffects;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Ensure proper creation order
CPostEffectsMgr CPostEffectsMgr::m_pInstance;

// Initialize all techniques (create in the order required for rendering)

CDistantRain CDistantRain::m_pInstance;

CAlphaTestAA CAlphaTestAA::m_pInstance;
CSunShafts CSunShafts::m_pInstance;
CGlow CGlow::m_pInstance;
CChameleonCloak CChameleonCloak::m_pInstance;
CDepthOfField CDepthOfField::m_pInstance;
CMotionBlur CMotionBlur::m_pInstance;
CGlittering CGlittering::m_pInstance;
CUnderwaterGodRays CUnderwaterGodRays::m_pInstance;
CVolumetricScattering CVolumetricScattering::m_pInstance;


// For this kind of game specific stuff would be nice to expose some post process functionality to lua
CRainDrops CRainDrops::m_pInstance;  
CWaterDroplets CWaterDroplets::m_pInstance;
CWaterFlow CWaterFlow::m_pInstance;
CBloodSplats CBloodSplats::m_pInstance;
CScreenFrost CScreenFrost::m_pInstance;
CScreenCondensation CScreenCondensation::m_pInstance;

CViewMode CViewMode::m_pInstance;
CAlienInterference CAlienInterference::m_pInstance;
CFlashBang CFlashBang::m_pInstance;

// Colors and filters
CFilterDepthEnhancement CFilterDepthEnhancement::m_pInstance;
CColorCorrection CColorCorrection::m_pInstance;  
CFilterChromaShift CFilterChromaShift::m_pInstance;
CFilterSharpening CFilterSharpening::m_pInstance;
CFilterBlurring CFilterBlurring::m_pInstance;
CFilterMaskedBlurring CFilterMaskedBlurring::m_pInstance;
CFilterRadialBlurring CFilterRadialBlurring::m_pInstance;
CFilterGrain CFilterGrain::m_pInstance;
CFilterStylized CFilterStylized::m_pInstance;
CColorGrading CColorGrading::m_pInstance;
CNightVision CNightVision::m_pInstance;
CCryVision CCryVision::m_pInstance;


CPerspectiveWarp CPerspectiveWarp::m_pInstance;   
CWaterPuddles CWaterPuddles::m_pInstance;
CWaterVolume CWaterVolume::m_pInstance;
CFillrateProfile CFillrateProfile::m_pInstance;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CParamFloat::SetParam(float fParam, bool bForceValue)
{
  if( m_bSmoothTransition && CRenderer::CV_r_postprocess_effects_params_blending )
  {    
		if (!bForceValue)
		{
	    if( !m_nFrameSetCount)
	    {    
	      m_fFrameParamAcc = m_fParamDefault;
	    }

	    m_fFrameParamAcc += fParam;    
	    if( !m_nFrameSetCount && m_fParamDefault )
	      m_nFrameSetCount += 2;    // Make sure to initialize frame set count to 2, for non 0 default values
	    else
	      m_nFrameSetCount++;
		}
		else
		{
			m_fParam = m_fFrameParamAcc = fParam;
			m_nFrameSetCount = 0;
		}
  }
  else  
    m_fParam = fParam;  
}

float CParamFloat::GetParam()
{
  if( m_bSmoothTransition && CRenderer::CV_r_postprocess_effects_params_blending )
  {
    if( m_nFrameSetCount )              
    {
      m_fFrameParamAcc /= (float) m_nFrameSetCount;

      m_nFrameSetCount = 0;
    }

    float fFrameTime = clamp_tpl<float>(iSystem->GetITimer()->GetFrameTime()*4.0f, 0.0f, 1.0f);  
    m_fParam += (m_fFrameParamAcc - m_fParam) * fFrameTime;
  }

  return m_fParam;
}

void CParamVec4::SetParamVec4( const Vec4 &pParam, bool bForceValue )
{
  if( m_bSmoothTransition && CRenderer::CV_r_postprocess_effects_params_blending )
  {
		if (!bForceValue)
		{
			if( !m_nFrameSetCount)
				m_pFrameParamAcc = m_pParamDefault;

			m_pFrameParamAcc += pParam;

			if( !m_nFrameSetCount && (m_pParamDefault.x +  m_pParamDefault.y + m_pParamDefault.z + m_pParamDefault.w ))
				m_nFrameSetCount += 2;  // Make sure to initialize frame set count to 2, for non 0 default values
			else
				m_nFrameSetCount++;
		}
		else
		{
			m_pParam = m_pFrameParamAcc = pParam;
			m_nFrameSetCount = 0;
		}
  }
  else
    m_pParam = pParam;  
}
 
Vec4 CParamVec4::GetParamVec4()
{
  if( m_bSmoothTransition && CRenderer::CV_r_postprocess_effects_params_blending )
  {  
    if( m_nFrameSetCount )              
    {
      m_pFrameParamAcc /= (float) m_nFrameSetCount;
      m_nFrameSetCount = 0;
    }

    float fFrameTime = clamp_tpl<float>(iSystem->GetITimer()->GetFrameTime()*4.0f, 0.0f, 1.0f);  
    m_pParam += (m_pFrameParamAcc - m_pParam) * fFrameTime;
  }

  return m_pParam;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


int CPostEffectsMgr::Create()
{ 
  m_bPostReset = false;

	// Initialize the CRC table
	// This is the official polynomial used by CRC-32
	// in PKZip, WinZip and Ethernet.
	unsigned int ulPolynomial = 0x04c11db7;

	// 256 values representing ASCII character codes.
	for (int i=0;i<=0xFF;i++)
	{
		m_nCRC32Table[i] = CRC32Reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			m_nCRC32Table[i] = (m_nCRC32Table[i] << 1) ^ (m_nCRC32Table[i] & (1 << 31) ? ulPolynomial : 0);
		m_nCRC32Table[i] = CRC32Reflect(m_nCRC32Table[i], 32);
	} //i

  // Register default parameters
  AddParamFloat("Global_Brightness", m_pBrightness, 1.0f); // brightness
  AddParamFloat("Global_Contrast", m_pContrast, 1.0f);  // contrast
  AddParamFloat("Global_Saturation", m_pSaturation, 1.0f);  // saturation  

  AddParamFloat("Global_ColorC", m_pColorC, 0.0f);  // cyan amount
  AddParamFloat("Global_ColorY", m_pColorY, 0.0f);  // yellow amount
  AddParamFloat("Global_ColorM", m_pColorM, 0.0f);  // magenta amount
  AddParamFloat("Global_ColorK", m_pColorK, 0.0f);  // darkness amount

  AddParamFloat("Global_ColorHue", m_pColorHue, 0.0f);  // image hue rotation  

  // User parameters
  AddParamFloat("Global_User_Brightness", m_pUserBrightness, 1.0f); // brightness
  AddParamFloat("Global_User_Contrast", m_pUserContrast, 1.0f);  // contrast
  AddParamFloat("Global_User_Saturation", m_pUserSaturation, 1.0f);  // saturation  

  AddParamFloat("Global_User_ColorC", m_pUserColorC, 0.0f);  // cyan amount
  AddParamFloat("Global_User_ColorY", m_pUserColorY, 0.0f);  // yellow amount
  AddParamFloat("Global_User_ColorM", m_pUserColorM, 0.0f);  // magenta amount
  AddParamFloat("Global_User_ColorK", m_pUserColorK, 0.0f);  // darkness amount

  AddParamFloat("Global_User_ColorHue", m_pUserColorHue, 0.0f);  // image hue rotation  

  AddParamVec4("Global_DirectionalBlur_Vec", m_pDirectionalBlurVec, Vec4(0, 0, 0, 0)); // directional blur


  // Initialize all post process techniques    
	std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectCreate());

	// Initialize parameters
	StringEffectMapItor pItor= m_pNameIdMapGen.begin(), pEnd = m_pNameIdMapGen.end();
	for( ; pItor!=pEnd ; ++pItor)
	{            
		m_pNameIdMap.insert( KeyEffectMapItor::value_type( GetCRC(pItor->first.c_str()), pItor->second ) );
	}
	
	m_pNameIdMapGen.clear(); 

  return 1;
}

void CPostEffectsMgr::Release()
{ 
  // Free all resources

  std::for_each(m_pNameIdMap.begin(), m_pNameIdMap.end(), SContainerKeyEffectParamDelete());  
  m_pNameIdMap.clear(); 
  
  std::for_each(m_pEffects.begin(), m_pEffects.end(), container_object_safe_release());
  m_pEffects.clear();
}

void CPostEffectsMgr::Reset()
{
  m_pBrightness->ResetParam(1.0f);
  m_pContrast->ResetParam(1.0f);
  m_pSaturation->ResetParam(1.0f);
  m_pColorC->ResetParam(0.0f);
  m_pColorY->ResetParam(0.0f);
  m_pColorM->ResetParam(0.0f);
  m_pColorK->ResetParam(0.0f);
  m_pColorHue->ResetParam(0.0f);
  m_pUserBrightness->ResetParam(1.0f);
  m_pUserContrast->ResetParam(1.0f);
  m_pUserSaturation->ResetParam(1.0f);
  m_pUserColorC->ResetParam(0.0f);
  m_pUserColorY->ResetParam(0.0f);
  m_pUserColorM->ResetParam(0.0f);
  m_pUserColorK->ResetParam(0.0f);
  m_pUserColorHue->ResetParam(0.0f);
  m_pDirectionalBlurVec->ResetParamVec4(Vec4(0, 0, 0, 0));

  std::for_each(m_pEffects.begin(), m_pEffects.end(), SContainerPostEffectReset());      
}

// Used only when creating the crc table
uint32 CPostEffectsMgr::CRC32Reflect(uint32 ref, char ch)
{		
	uint32 value = 0;

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for (int i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

uint32 CPostEffectsMgr::GetCRC( const char *pszName )
{  
  if( !pszName )
  {
    assert("CPostEffectsMgr::GetCRC() invalid string passed");
    return 0;
  }

	// Once the lookup table has been filled in by the constructor,
	// this function creates all CRCs using only the lookup table.
	// Be sure to use unsigned variables, because negative values introduce high bits where zero bits are required.	
		
	const char *szPtr=pszName;	
	uint32 ulCRC=0xffffffff; // Start out with all bits set high.
	unsigned char c;

	while (*szPtr)
	{
		c=*szPtr++;
		if (c>='a' && c<='z')
			c-=32; //convert to uppercase
		ulCRC = (ulCRC >> 8) ^ m_nCRC32Table[(ulCRC & 0xFF) ^ c]; 
	}

// Exclusive OR the result with the beginning value...avoids the % operator 
	return ulCRC ^ 0xffffffff; 


}

CEffectParam *CPostEffectsMgr::GetByName(const char *pszParam)
{
  assert(pszParam || "mfGetByName: null FX name");

  static uint32 nKeyCache = 0;
  uint32 nCurrKey = GetCRC( pszParam );

  // Check cache first
  static CEffectParam *pParamCache = 0;
  if( nCurrKey == nKeyCache && pParamCache)
    return pParamCache;

  KeyEffectMapItor pItor = m_pNameIdMap.find( nCurrKey );

  if( pItor != m_pNameIdMap.end() )
  {
    pParamCache =  pItor->second;
    nKeyCache = nCurrKey;

    return pParamCache;
  }

  return 0;
}

float CPostEffectsMgr::GetByNameF(const char *pszParam)
{    
  CEffectParam *pParam = GetByName(pszParam);
  if( pParam )
  {
    return pParam->GetParam();
  }

  return 0.0f;
}

Vec4 CPostEffectsMgr::GetByNameVec4(const char *pszParam)
{    
  CEffectParam *pParam = GetByName(pszParam);
  if( pParam )
  {
    return pParam->GetParamVec4();
  }

  return Vec4(0,0,0,0);
}

void CPostEffectsMgr::RegisterEffect( CPostEffect *pEffect )
{
  assert( pEffect );
  m_pEffects.push_back( pEffect );
}

template <typename paramT, typename T> 
void CPostEffectsMgr::RegisterParam(const char *pszName, CEffectParam *&pParam, const T &pParamVal, bool bSmoothTransition)
{
  pParam = CEffectParam::Create< paramT >( pParamVal, bSmoothTransition );
  m_pNameIdMapGen.insert( StringEffectMapItor::value_type( pszName, pParam) );  
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

int CParamTexture::Create(const char *pszFileName)
{
  if(!pszFileName)
  {
    return 0;
  }

  if(m_pTexParam)
  {
    // check if texture is same
    if(!strcmpi(m_pTexParam->GetName(), pszFileName))
    {
      return 0;
    }
    // release texture if required
    SAFE_RELEASE(m_pTexParam)
  }

  m_pTexParam = CTexture::ForName(pszFileName, FT_DONT_RELEASE, eTF_Unknown);
  assert(m_pTexParam || "CParamTexture.Create: texture not found!");

  return 1;
}


void CParamTexture::Release()
{
  SAFE_RELEASE(m_pTexParam);
}
