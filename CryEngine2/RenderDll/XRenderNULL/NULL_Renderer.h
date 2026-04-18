/*=============================================================================
DriverD3D9.h : Direct3D8 Render interface declarations.
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#ifndef NULL_RENDERER_H
#define NULL_RENDERER_H

#if _MSC_VER > 1000
# pragma once
#endif

/*
===========================================
The NULLRenderer interface Class
===========================================
*/

#define MAX_TEXTURE_STAGES 4

#include "CryArray.h"
#include "NULLRenderAuxGeom.h"

//////////////////////////////////////////////////////////////////////
class CNULLRenderer : public CRenderer
{
public:	

  CNULLRenderer();
  virtual ~CNULLRenderer();

  virtual WIN_HWND Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd=0, bool bReInit=false, const SCustomRenderInitArgs* pCustomArgs=0);
  virtual bool SetCurrentContext(WIN_HWND hWnd);
  virtual bool CreateContext(WIN_HWND hWnd, bool bAllowFSAA=false);
  virtual bool DeleteContext(WIN_HWND hWnd);
  virtual void MakeCurrent();
  virtual WIN_HWND GetHWND();

  virtual int  CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF=eTF_A8R8G8B8);
  virtual bool DestroyRenderTarget (int nHandle);;
  virtual bool SetRenderTarget (int nHandle);
  void SetTerrainAONodes(PodArray<struct SSectorTextureSet> * terrainAONodes){};

  virtual void GetVideoMemoryUsageStats( size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently ) {}

  virtual void  ShareResources( IRenderer *renderer ) {}
  virtual	void SetRenderTile(f32 nTilesPosX,f32 nTilesPosY,f32 nTilesGridSizeX,f32 nTilesGridSizeY) {}

	//! Fills array of all supported video formats (except low resolution formats)
	//! Returns number of formats, also when called with NULL
  virtual int	EnumDisplayFormats(SDispFormat* Formats);

  //! Return all supported by video card video AA formats
  virtual int	EnumAAFormats(const SDispFormat &rDispFmt, SAAFormat* Formats) { return 0; }

  //! Changes resolution of the window/device (doen't require to reload the level
  virtual bool	ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen);

  virtual EScreenAspectRatio GetScreenAspect(int nWidth, int nHeight) { return eAspect_4_3; }

  virtual void	ShutDown(bool bReInit=false);
  virtual void	ShutDownFast();

  virtual void	BeginFrame();
  virtual void	RenderDebug();	
	virtual void	EndFrame();	

  virtual void	Reset (void) {};

  virtual int  GetDynVBSize( int vertexType = VERTEX_FORMAT_P3F_COL4UB_TEX2F ) { return 0; }
  virtual void *GetDynVBPtr(int nVerts, int &nOffs, int Pool);
  virtual void DrawDynVB(int nOffs, int Pool, int nVerts);
  virtual void DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType);

  virtual	CVertexBuffer	*CreateBuffer(int  vertexcount,int vertexformat, const char *szSource, bool bDynamic=false);
  virtual void  CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource, bool bDynamic=false);
  virtual void	ReleaseBuffer(CVertexBuffer *bufptr, int nVerts);
  virtual void	DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start=0,int vert_stop=0, CRenderChunk *pChunk=NULL);
  virtual void	UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs=0, int Type=0);
  virtual void  UnlockBuffer(CVertexBuffer *buf, int nType, int nVerts) {}

  virtual void  CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount);
  virtual void  UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, int oldIndexCount, bool bUnLock=true, bool bDynamic=false);
  virtual void  ReleaseIndexBuffer(SVertexStream *dest, int IndexCount);

  virtual	void	CheckError(const char *comment);

  virtual void DrawLine(const Vec3 & vPos1, const Vec3 & vPos2) {};
  virtual void Graph(byte *g, int x, int y, int wdt, int hgt, int nC, int type, char *text, ColorF& color, float fScale) {};
  virtual	void	Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType);

  virtual	void	SetCamera(const CCamera &cam);
  virtual	void	SetViewport(int x, int y, int width, int height);
  virtual	void	SetScissor(int x=0, int y=0, int width=0, int height=0);
  virtual void  GetViewport(int *x, int *y, int *width, int *height);

  virtual void	SetCullMode	(int mode=R_CULL_BACK);
  virtual bool	EnableFog	(bool enable);
  virtual void	SetFog		(float density,float fogstart,float fogend,const float *color,int fogmode);
  virtual void  SetFogColor(float * color);
  virtual	void	EnableVSync(bool enable);

  virtual void  DrawTriStrip(CVertexBuffer *src, int vert_num=4);

  virtual void	PushMatrix();
  virtual void	RotateMatrix(float a,float x,float y,float z);
  virtual void	RotateMatrix(const Vec3 & angels);
  virtual void	TranslateMatrix(float x,float y,float z);
  virtual void	ScaleMatrix(float x,float y,float z);
  virtual void	TranslateMatrix(const Vec3 &pos);
  virtual void  MultMatrix(const float * mat);
  virtual	void	LoadMatrix(const Matrix34 *src=0);
  virtual void	PopMatrix();

  virtual	void	EnableTMU(bool enable);
  virtual void	SelectTMU(int tnum);

  virtual bool ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp);
  virtual void ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height);

  virtual	bool SaveTga(unsigned char *sourcedata,int sourceformat,int w,int h,const char *filename,bool flip) { return false; }

  //download an image to video memory. 0 in case of failure
  virtual	unsigned int DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat=true, int filter=FILTER_BILINEAR, int Id=0, char *szCacheName=NULL, int flags=0) { return 0; }
  virtual	void UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF=eTF_X8R8G8B8) {};

  virtual	bool SetGammaDelta(const float fGamma);
  virtual void RestoreGamma(void) {};

  virtual	void RemoveTexture(unsigned int TextureId) {};

  virtual void	Draw2dImage	(float xpos,float ypos,float w,float h,int texture_id,float s0=0,float t0=0,float s1=1,float t1=1,float angle=0,float r=1,float g=1,float b=1,float a=1,float z=1);
  virtual void  DrawImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float r,float g,float b,float a);
  virtual void  DrawImageWithUV(float xpos,float ypos,float z,float w,float h,int texture_id,float s[4],float t[4],float r,float g,float b,float a);

  virtual int	SetPolygonMode(int mode);

  virtual void ResetToDefault();

  virtual int  GenerateAlphaGlowTexture(float k);

  virtual void SetMaterialColor(float r, float g, float b, float a);

  virtual char * GetStatusText(ERendStats type);
  virtual void GetMemoryUsage(ICrySizer* Sizer);

  // Project/UnProject
  virtual void ProjectToScreen( float ptx, float pty, float ptz, 
    float *sx, float *sy, float *sz );
  virtual int UnProject(float sx, float sy, float sz, 
    float *px, float *py, float *pz,
    const float modelMatrix[16], 
    const float projMatrix[16], 
    const int    viewport[4]);
  virtual int UnProjectFromScreen( float  sx, float  sy, float  sz, 
    float *px, float *py, float *pz);

  // Shadow Mapping
  virtual void PrepareDepthMap(ShadowMapFrustum* SMSource, CDLight* pLight=NULL);
  virtual void SetupShadowOnlyPass(int Num, ShadowMapFrustum* pShadowInst, Matrix34 * pObjMat);
  virtual void DrawAllShadowsOnTheScreen();
  virtual void OnEntityDeleted(IRenderNode * pRenderNode) {};

  virtual void SetClipPlane( int id, float * params );
  virtual void EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract);

  virtual void SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) {};

  //for editor
  virtual void  GetModelViewMatrix(float *mat);
  virtual void  GetProjectionMatrix(float *mat);
  virtual Vec3 GetUnProject(const Vec3 &WindowCoords,const CCamera &cam);

  virtual void DrawObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor);
	virtual void GenerateObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor);
  virtual void DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode=0);
  virtual void DrawQuad(float dy,float dx, float dz, float x, float y, float z);

  virtual void ClearBuffer(uint nFlags, ColorF *vColor, float depth = 1.0f);
  virtual void ReadFrameBuffer(unsigned char * pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX=-1, int nScaledY=-1);
	virtual void ReadFrameBufferFast(unsigned int* pDstARGBA8, int dstWidth, int dstHeight);

  //misc 
  virtual bool ScreenShot(const char *filename=NULL, int width=0);

  virtual char*	GetVertexProfile(bool bSupportedProfile) { return "vs_2_0"; }
  virtual char*	GetPixelProfile(bool bSupportedProfile) { return "ps_2_0"; }

  virtual void Set2DMode(bool enable, int ortox, int ortoy,float znear=-1e30f,float zfar=1e30f);

  virtual int ScreenToTexture();

  virtual void DrawPoints(Vec3 v[], int nump, ColorF& col, int flags) {};
  virtual void DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround) {};


  // Shaders/Shaders support
  // RE - RenderElement

  virtual void EF_Release(int nFlags);
  virtual void EF_PipelineShutdown();

  //==========================================================
  // external interface for shaders
  //==========================================================

  virtual bool EF_SetLightHole(Vec3 vPos, Vec3 vNormal, int idTex, float fScale=1.0f, bool bAdditive=true);

  // Draw all shaded REs in the list
  virtual void EF_EndEf3D (int nFlags);

  // 2d interface for shaders
  virtual void EF_EndEf2D(bool bSort);

  virtual void EF_SetState(int st, int AlphaRef=-1);
  void EF_Init();
  void EF_InitVFMergeMap();

  virtual IDynTexture *MakeDynTextureFromShadowBuffer(int nSize, IDynTexture * pDynTexture);
  virtual void MakeSprite( IDynTexture * &rTexturePtr, float _fSpriteDistance, int nTexSize, float angle, float angle2, IStatObj * pStatObj, const float fBrightnessMultiplier, SRendParams& rParms );
  virtual uint RenderOccludersIntoBuffer(const CCamera & viewCam, int nTexSize, PodArray<IRenderNode*> & lstOccluders, float * pBuffer) { return 0; }

  virtual IRenderAuxGeom* GetIRenderAuxGeom()
  {
    return( m_pNULLRenderAuxGeom );
  }

  //////////////////////////////////////////////////////////////////////
  // Replacement functions for the Font engine ( vlad: for font can be used old functions )
  virtual	bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF=eTF_A8R8G8B8);
  virtual	int  FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF=eTF_A8R8G8B8, bool genMips=false);
  virtual	bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData);
  virtual	void FontReleaseTexture(class CFBitmap *pBmp);
  virtual void FontSetTexture(class CFBitmap*, int nFilterMode);
  virtual void FontSetTexture(int nTexId, int nFilterMode);
  virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight);
  virtual void FontSetBlending(int src, int dst);
  virtual void FontRestoreRenderingState();


	//////////////////////////////////////////////////////////////////////////
	// Warhead
	void UnmarkForDepthMapCapture();
	void CNULLRenderer::SetDataForVertexScatterGeneration(struct SScatteringObjectData* const *objects_for_vertexscattergeneration, int numobjects);

#ifndef EXCLUDE_SCALEFORM_SDK
	virtual void SF_ConfigStencilOp( int op ) {}
	virtual void SF_DrawIndexedTriList( int baseVertexIndex, int minVertexIndex, int numVertices, int startIndex, int triangleCount, const SSF_GlobalDrawParams& params ) {}
	virtual void SF_DrawLineStrip( int baseVertexIndex, int lineCount, const SSF_GlobalDrawParams& params ) {}
	virtual void SF_DrawGlyphClear( const SSF_GlobalDrawParams& params ) {}
	virtual void SF_Flush() {}
	virtual bool SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, unsigned char* pData, size_t pitch, ETEX_Format eTF) { return true; }
#endif // #ifndef EXCLUDE_SCALEFORM_SDK

	virtual void GetLogVBuffers(void) {}
  private:
    CNULLRenderAuxGeom* m_pNULLRenderAuxGeom;

};

//=============================================================================

extern CNULLRenderer *gcpNULL;



#endif //NULL_RENDERER
