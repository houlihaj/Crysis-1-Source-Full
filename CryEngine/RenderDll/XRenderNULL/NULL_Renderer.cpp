//////////////////////////////////////////////////////////////////////
//
//  Crytek CryENGINE Source code
//  
//  File:NULL_Renderer.cpp
//  Description: Implementation of the NULL renderer API
//
//  History:
//  -Jan 31,2001:Originally created by Marco Corbetta
//	-: taken over by Andrey Khonich
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "NULL_Renderer.h"

// init memory pool usage

// Included only once per DLL module.
#include <platform_impl.h>

CCryName CTexture::m_sClassName = CCryName("CTexture");

CCryName CHWShader::m_sClassNameVS = CCryName("CHWShader_VS");
CCryName CHWShader::m_sClassNamePS = CCryName("CHWShader_PS");

CCryName CShader::m_sClassName = CCryName("CShader");

CNULLRenderer *gcpNULL = NULL;

extern Matrix44 sIdentityMatrix;

#ifdef WIN32
IDirectBee *CRenderer::m_pDirectBee=0;		// connection to D3D9 wrapper DLL, 0 if not established
#endif

//////////////////////////////////////////////////////////////////////
CNULLRenderer::CNULLRenderer()
{
  gcpNULL = this;
	m_pNULLRenderAuxGeom = CNULLRenderAuxGeom::Create(*this);
}


#include <stdio.h>
//////////////////////////////////////////////////////////////////////
CNULLRenderer::~CNULLRenderer()
{ 
  ShutDown(); 
	delete m_pNULLRenderAuxGeom;
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::EnableTMU(bool enable)
{ 
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::CheckError(const char *comment)
{
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::BeginFrame()
{
	m_nFrameID++;
	m_nFrameUpdateID++;

	m_pNULLRenderAuxGeom->BeginFrame();
}

//////////////////////////////////////////////////////////////////////
bool CNULLRenderer::ChangeDisplay(unsigned int width,unsigned int height,unsigned int bpp)
{
  return false;
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height)
{
}

void CNULLRenderer::RenderDebug()
{
}

void CNULLRenderer::EndFrame()
{
	//m_pNULLRenderAuxGeom->Flush(true);
	m_pNULLRenderAuxGeom->EndFrame();
}

void CNULLRenderer::GetMemoryUsage(ICrySizer* Sizer)
{
}

WIN_HWND CNULLRenderer::GetHWND()
{
#if defined(WIN32)
  return GetDesktopWindow();
#else
	return NULL;
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
//IMAGES DRAWING
////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::Draw2dImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float angle,float r,float g,float b,float a, float z)
{ 
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::DrawImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float r,float g,float b,float a)
{ 
}

void CNULLRenderer::DrawImageWithUV(float xpos,float ypos,float z,float w,float h,int texture_id,float s[4],float t[4],float r,float g,float b,float a)
{
}

///////////////////////////////////////////
void CNULLRenderer::SetCullMode(int mode)
{
}

///////////////////////////////////////////
void CNULLRenderer::SetFog(float density,float fogstart,float fogend,const float *color,int fogmode)
{
}

///////////////////////////////////////////
bool CNULLRenderer::EnableFog(bool enable)
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//MISC EXTENSIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////
void CNULLRenderer::EnableVSync(bool enable)
{
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::SelectTMU(int tnum)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//MATRIX FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////
void CNULLRenderer::PushMatrix()
{
}

///////////////////////////////////////////
void CNULLRenderer::RotateMatrix(float a,float x,float y,float z)
{
}

void CNULLRenderer::RotateMatrix(const Vec3 & angles)
{
}

///////////////////////////////////////////
void CNULLRenderer::TranslateMatrix(float x,float y,float z)
{
}

void CNULLRenderer::MultMatrix(const float * mat)
{
}

void CNULLRenderer::TranslateMatrix(const Vec3 &pos)
{
}

///////////////////////////////////////////
void CNULLRenderer::ScaleMatrix(float x,float y,float z)
{
}

///////////////////////////////////////////
void CNULLRenderer::PopMatrix()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNULLRenderer::LoadMatrix(const Matrix34 *src)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//MISC
////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////
void CNULLRenderer::Draw3dBBox(const Vec3 &mins,const Vec3 &maxs, int nPrimType)
{
}


///////////////////////////////////////////
int CNULLRenderer::SetPolygonMode(int mode)
{
  return 0;
}


///////////////////////////////////////////
void CNULLRenderer::SetCamera(const CCamera &cam)
{
  m_cam = cam;
}

void CNULLRenderer::GetViewport(int *x, int *y, int *width, int *height)
{
  *x = 0;
  *y = 0;
  *width = m_width;
  *height = m_height;
}

void CNULLRenderer::SetViewport(int x, int y, int width, int height)
{
}

void CNULLRenderer::SetScissor(int x, int y, int width, int height)
{
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::GetModelViewMatrix(float * mat)
{
  memcpy(mat, &sIdentityMatrix, sizeof(sIdentityMatrix));
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::GetProjectionMatrix(float *mat)
{
  memcpy(mat, &sIdentityMatrix, sizeof(sIdentityMatrix));
}

//////////////////////////////////////////////////////////////////////
Vec3 CNULLRenderer::GetUnProject(const Vec3 &WindowCoords,const CCamera &cam)
{
  return (Vec3(1,1,1));
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipmode/*=0*/)
{
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::ProjectToScreen( float ptx, float pty, float ptz, float *sx, float *sy, float *sz )
{
}

int CNULLRenderer::UnProject(float sx, float sy, float sz, 
              float *px, float *py, float *pz,
              const float modelMatrix[16], 
              const float projMatrix[16], 
              const int    viewport[4])
{
  return 0;
}

//////////////////////////////////////////////////////////////////////
int CNULLRenderer::UnProjectFromScreen( float  sx, float  sy, float  sz, 
                                      float *px, float *py, float *pz)
{
  return 0;
}

//////////////////////////////////////////////////////////////////////
bool CNULLRenderer::ScreenShot(const char *filename, int width)
{
  return true;
}

int CNULLRenderer::ScreenToTexture()
{
  return 0;
}

void CNULLRenderer::ResetToDefault()
{
}

///////////////////////////////////////////
void CNULLRenderer::SetMaterialColor(float r, float g, float b, float a)
{
}

char * CNULLRenderer::GetStatusText(ERendStats type)
{
  return "NULL";
}

//////////////////////////////////////////////////////////////////////

void CNULLRenderer::ClearBuffer(uint nFlags, ColorF *vColor, float depth)
{
}

void CNULLRenderer::ReadFrameBuffer(unsigned char * pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY)
{
}

void CNULLRenderer::ReadFrameBufferFast(unsigned int* pDstARGBA8, int dstWidth, int dstHeight)
{
}

void CNULLRenderer::SetFogColor(float * color)
{
}

void CNULLRenderer::SetClipPlane( int id, float * params )
{
}

void CNULLRenderer::DrawQuad(float dy,float dx, float dz, float x, float y, float z)
{
}

//////////////////////////////////////////////////////////////////////
void CNULLRenderer::Set2DMode(bool enable, int ortox, int ortoy, float znear, float zfar)
{ 
}

int CNULLRenderer::CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF)
{
  return 0;
}

bool CNULLRenderer::DestroyRenderTarget (int nHandle)
{
  return true;
}

bool CNULLRenderer::SetRenderTarget (int nHandle)
{
  return true;
}



//////////////////////////////////////////////////////////////////////
void CNULLRenderer::UnmarkForDepthMapCapture()
{ 
}

void CNULLRenderer::SetDataForVertexScatterGeneration(struct SScatteringObjectData* const *objects_for_vertexscattergeneration, int numobjects)
{ 
}


//=========================================================================================


ILog     *iLog;
IConsole *iConsole;
ITimer   *iTimer;
ISystem  *iSystem;

extern "C" DLL_EXPORT IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp);
DLL_EXPORT IRenderer* PackageRenderConstructor(int argc, char* argv[], SCryRenderInterface *sp)
{
  gbRgb = false;

  iConsole  = sp->ipConsole;
  iLog      = sp->ipLog;
  iTimer    = sp->ipTimer;
  iSystem   = sp->ipSystem;

  ModuleInitISystem(iSystem);

  CRenderer *rd = (CRenderer *) (new CNULLRenderer());

#ifdef LINUX
  srand( clock() );
#else
  srand( GetTickCount() );
#endif

  return rd;
}

void *gGet_D3DDevice()
{
  return NULL;
}
void *gGet_glReadPixels()
{
  return NULL;
}

IVideoPlayer* CRenderer::CreateVideoPlayerInstance() const 
{ 
	return 0; 
}
