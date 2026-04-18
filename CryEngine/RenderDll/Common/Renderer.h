
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Renderer.h - API Indipendent
//
//	History:
//	-Jan 31,2001:Originally Created by Marco Corbetta
//	-: taken over by Andrey Khonich
//
//////////////////////////////////////////////////////////////////////

#ifndef _RENDERER_H
#define _RENDERER_H

#if _MSC_VER > 1000
# pragma once
#endif

#ifdef PS2
#include "File.h"
#endif

typedef void (PROCRENDEF)(SShaderPass *l, int nPrimType);

#define USE_HDR 1
#define USE_FX 1

struct ICVar;
struct ShadowMapFrustum;
struct IStatObj;
struct SShaderPass;
class CREParticle;
struct IDirectBee;

typedef int (*pDrawModelFunc)(void);

//=============================================================

struct SMoveClip;
struct STrace;

//try_to_merge flags
#define TTM_NEEDLM						1
#define TTM_NEEDOCCLM					2
#define TTM_NEEDSHADOWCASTERS	4
#define TTM_NEEDMATERIAL			8
#define TTM_NEEDDYNLIHGTS     16

struct SFogState
{
  int m_nFogMode;
  int m_nCurFogMode;
  bool m_bEnable;
  float m_FogDensity;
  float m_FogStart;
  float m_FogEnd;
  ColorF m_FogColor;
  ColorF m_CurColor;

  bool operator != (SFogState fs)
  {
    if (m_nFogMode!=fs.m_nFogMode || m_FogDensity!=fs.m_FogDensity || m_FogStart!=fs.m_FogStart || m_FogEnd!=fs.m_FogEnd || m_FogColor!=fs.m_FogColor)
      return true;
    return false;
  }
};

struct SSpriteInfo
{
  IDynTexture *m_pTex;
	struct SSectorTextureSet * m_pTerrainTexInfo;
  Vec3 m_vPos;
  float m_fDX;
  float m_fDY;
  float m_fScaleV;
  UCol m_Color;
  uint8	m_ucTexCoordMinX;			// 0..128 used for the full range (0..1) in the texture (to fit in byte)
  uint8	m_ucTexCoordMinY;			// 0..128 used for the full range (0..1) in the texture (to fit in byte)
  uint8	m_ucTexCoordMaxX;			// 0..128 used for the full range (0..1) in the texture (to fit in byte)
  uint8	m_ucTexCoordMaxY;			// 0..128 used for the full range (0..1) in the texture (to fit in byte)
	uchar m_AngleY;							// 0/60 vertical tilt
};

struct SSpriteGenInfo
{
  float fAngle;								// horizonal rotation in degree
  float fAngle2;							// vertical tilt 0/60 in degree
  float fGenDist;
  float fBrightnessMultiplier;
  int nMaterialLayers;
  IMaterial *pMaterial;
  IDynTexture **ppTexture;
  CStatObj *pStatObj;
  int nSP;
};

struct SObjsState
{
  int nNumVisObjects;
  int nNumTempObjects;
};

class CMatrixStack
{
public:
	CMatrixStack(uint32 maxDepth,uint32 dirtyFlag);
	~CMatrixStack();

	// Pops the top of the stack, returns the current top
	// *after* popping the top.
	bool Pop();

	// Pushes the stack by one, duplicating the current matrix.
	bool Push();

	// Loads identity in the current matrix.
	bool LoadIdentity();

	// Loads the given matrix into the current matrix
	bool LoadMatrix(const Matrix44* pMat );

	// Right-Multiplies the given matrix to the current matrix.
	// (transformation is about the current world origin)
	bool MultMatrix(const Matrix44* pMat );

	// Left-Multiplies the given matrix to the current matrix
	// (transformation is about the local origin of the object)
	bool MultMatrixLocal(const Matrix44* pMat );

	// Right multiply the current matrix with the computed rotation
	// matrix, counterclockwise about the given axis with the given angle.
	// (rotation is about the current world origin)
	bool RotateAxis	(const Vec3* pV, f32 Angle);

	// Left multiply the current matrix with the computed rotation
	// matrix, counterclockwise about the given axis with the given angle.
	// (rotation is about the local origin of the object)
	bool RotateAxisLocal(const Vec3* pV, f32 Angle);

	// Right multiply the current matrix with the computed rotation
	// matrix. All angles are counterclockwise. (rotation is about the
	// current world origin)

	// Right multiply the current matrix with the computed scale
	// matrix. (transformation is about the current world origin)
	bool Scale(f32 x, f32 y, f32 z);

	// Left multiply the current matrix with the computed scale
	// matrix. (transformation is about the local origin of the object)
	bool ScaleLocal(f32 x, f32 y, f32 z);

	// Right multiply the current matrix with the computed translation
	// matrix. (transformation is about the current world origin)
	bool Translate(f32 x, f32 y, f32 z);

	// Left multiply the current matrix with the computed translation
	// matrix. (transformation is about the local origin of the object)
	bool TranslateLocal(f32 x, f32 y, f32 z);

	// Obtain the current matrix at the top of the stack
	Matrix44* GetTop();

  int GetDepth() { return m_nDepth; }

private:
		Matrix44 *m_pTop;		//top of the stack
		Matrix44 *m_pStack; // array of Matrix44
		uint32 m_nDepth;
		uint32 m_nMaxDepth;
		uint32 m_nDirtyFlag; //flag for new matrices
};

//////////////////////////////////////////////////////////////////////
class CRenderer : public IRenderer
{
friend struct IDirect3D9;

public:	
	DEFINE_ALIGNED_DATA( Matrix44, m_ViewMatrix, 16 );
	DEFINE_ALIGNED_DATA( Matrix44, m_CameraMatrix, 16 );
  DEFINE_ALIGNED_DATA( Matrix44, m_CameraZeroMatrix, 16 );
  DEFINE_ALIGNED_DATA( Matrix44, m_CameraMatrixPrev, 16 );
	DEFINE_ALIGNED_DATA( Matrix44, m_ProjMatrix, 16 );
	DEFINE_ALIGNED_DATA( Matrix44, m_TranspOrigCameraProjMatrix, 16 );
  DEFINE_ALIGNED_DATA( Matrix44, m_CameraProjMatrix, 16 );  
  DEFINE_ALIGNED_DATA( Matrix44, m_CameraProjZeroMatrix, 16 );  
	DEFINE_ALIGNED_DATA( Matrix44, m_InvCameraProjMatrix, 16 );
  DEFINE_ALIGNED_DATA (Matrix44, m_TempMatrices[4][8], 16);
  DEFINE_ALIGNED_DATA (Vec4, m_vMeshScale, 16);

	struct SRenderTileInfo { SRenderTileInfo() { nPosX=nPosY=nGridSizeX=nGridSizeY=0.f; } f32 nPosX, nPosY, nGridSizeX, nGridSizeY; };

  //_declspec(align(16)) Matrix44 m_ViewMatrix;
  //_declspec(align(16)) Matrix44 m_CameraMatrix;
  //_declspec(align(16)) Matrix44 m_ProjMatrix;
  //_declspec(align(16)) Matrix44 m_CameraProjMatrix;
  //_declspec(align(16)) Matrix44 m_CameraProjTransposedMatrix;
  //_declspec(align(16)) Matrix44 m_InvCameraProjMatrix;

  bool              m_bDeviceLost;

  // Shaders pipeline status
  //=============================================================================================================
  SRenderPipeline m_RP;
  //=============================================================================================================

  int              m_MaxVertBufferSize;
  int              m_CurVertBufferSize;
  int              m_CurIndexBufferSize;

  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F m_DynVB[2048];
  struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *m_TempDynVB;

  int  m_VSync;
  int  m_Predicated;
  int  m_FSAA;
  int  m_FSAA_quality;
  int  m_FSAA_samples;
	int		m_deskwidth, m_deskheight, m_deskbpp;
  int  m_nHDRType;

  int m_nStencilMaskRef;

  byte  m_bDeviceSupportsATOC;
  byte  m_bDeviceSupportsInstancing;

  uint  m_bDeviceSupports_ATI_DF24 : 1;
  uint  m_bDeviceSupports_ATI_DF16 : 1;
  uint  m_bDeviceSupports_NV_D24S8_SM : 1;

  uint  m_bDeviceSupports_NVDBT : 1;

  uint  m_bDeviceSupportsG16R16Filter : 1;
  uint  m_bDeviceSupportsFP16Separate : 1;
  uint  m_bDeviceSupportsFP16Filter : 1;
  uint  m_bDeviceSupportsDynBranching : 1;
	uint  m_bDeviceSupportsR16FRendertarget : 1;
  uint  m_bDeviceSupports_G16R16_FSAA : 1;
  uint  m_bDeviceSupports_A16B16G16R16_FSAA : 1;
  uint  m_bDeviceSupportsVertexTexture : 1;

  uint m_bEditor : 1; // Render instance created from editor
  uint m_bUseHWSkinning : 1;
  uint m_bUseDynBranching : 1;
  uint m_bUseStatBranching : 1;
  uint m_bShadersPresort : 1;
  uint m_bEnd : 1;
	uint m_bUsePOM : 1;
  uint m_bInResolutionChange : 1;

  ICVar *m_CVWidth;
  ICVar *m_CVHeight;
  ICVar *m_CVFullScreen;
  ICVar *m_CVColorBits;
  ICVar *m_CVDisplayInfo;

  ColorF m_CurFontColor;

  SFogState m_FS;
  SFogState m_FSStack[8];
  int m_nCurFSStackLevel;

  SObjsState m_ObjsStack[16];

  byte m_iDeviceSupportsComprNormalmaps; // 0:none(DXT5 will be used) 1: 3DC, 2: V8U8, 3: CxV8U8

  DWORD m_Features;
  int m_MaxTextureSize;
  size_t m_MaxTextureMemory;
  int m_nShadowTexSize;

  unsigned short m_GammaTable[256];
  float m_fLastGamma;
  float m_fLastBrightness;
  float m_fLastContrast;
  float m_fDeltaGamma;

protected:

	typedef std::list<IRendererEventListener *> TListRendererEventListeners;
	TListRendererEventListeners m_listRendererEventListeners;
	bool m_bMarkForDepthMapCapture;
	struct SScatteringObjectData* const *m_ObjectsForVertexScatterGeneration;
	int m_nNumObjectsForVertexScatterGeneration;

public:

  CRenderer();
	virtual ~CRenderer();

	virtual void AddListener		(IRendererEventListener *pRendererEventListener);
	virtual void RemoveListener	(IRendererEventListener *pRendererEventListener);

	virtual ERenderType GetRenderType() const;

#ifndef PS2
  virtual WIN_HWND Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd=0, bool bReInit=false, const SCustomRenderInitArgs* pCustomArgs=0)=0;
#else	//PS2
	virtual bool Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool bReInit=false)=0;
#endif	//PS2
  virtual bool SetCurrentContext(WIN_HWND hWnd)=0;
  virtual bool CreateContext(WIN_HWND hWnd, bool bAllowFSAA=false)=0;
  virtual bool DeleteContext(WIN_HWND hWnd)=0;

  virtual int  CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF=eTF_A8R8G8B8)=0;
  virtual bool DestroyRenderTarget (int nHandle)=0;
  virtual bool SetRenderTarget (int nHandle)=0;

  virtual int GetFeatures() {return m_Features;}
	virtual int GetCurrentNumberOfDrawCalls()
  {
    int nDIPs = 0;
    for (int i=0; i<EFSLIST_NUM; i++)
    {
      nDIPs += m_RP.m_PS.m_nDIPs[i];
    }
    return nDIPs;
  }
	
	//! Fills array of all supported video formats (except low resolution formats)
	//! Returns number of formats, also when called with NULL
	virtual int	EnumDisplayFormats(SDispFormat *formats)=0;

  //! Return all supported by video card video AA formats
  virtual int	EnumAAFormats(const SDispFormat &rDispFmt, SAAFormat* formats)=0;

  //! Changes resolution of the window/device (doen't require to reload the level
  virtual bool	ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen)=0;
  virtual bool CheckDeviceLost() { return false; };

  virtual EScreenAspectRatio GetScreenAspect(int nWidth, int nHeight);

	virtual void	Release();
  virtual void  FreeResources(int nFlags);

	virtual void	BeginFrame()=0;
	virtual void	RenderDebug()=0;	
	virtual void	EndFrame()=0;	

  virtual void	Reset (void) = 0;

	virtual int  GetDynVBSize( int vertexType = VERTEX_FORMAT_P3F_COL4UB_TEX2F ) = 0;
  virtual void *GetDynVBPtr(int nVerts, int &nOffs, int Pool) = 0;
  virtual void DrawDynVB(int nOffs, int Pool, int nVerts) = 0;
  virtual void DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType) = 0;
	virtual	CVertexBuffer	*CreateBuffer(int  vertexcount,int vertexformat, const char *szSource, bool bDynamic=false)=0;
  virtual void  CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource, bool bDynamic=false)=0;
	virtual void	DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start=0,int vert_stop=0, CRenderChunk *pChunk=NULL)=0;
	virtual void	UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs=0, int Type=0)=0;

	virtual	void	CheckError(const char *comment)=0;

	virtual	void	Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType)=0;
  
	struct BBoxInfo 
	{ 
		BBoxInfo () { nPrimType=0; }
		Vec3 vMins, vMaxs; float fColor[4]; int nPrimType; 
	};
	std::vector<BBoxInfo> m_arrBoxesToDraw;

	virtual	void	SetCamera(const CCamera &cam)=0;
  virtual	void	SetViewport(int x, int y, int width, int height)=0;
  virtual	void	SetScissor(int x=0, int y=0, int width=0, int height=0)=0;
  virtual void  GetViewport(int *x, int *y, int *width, int *height)=0;

  virtual void	SetState(int State, int AlphaRef=-1) { EF_SetState(State, AlphaRef); }
	virtual void	SetCullMode	(int mode=R_CULL_BACK)=0;
	virtual bool	EnableFog	(bool enable)=0;
	virtual void	SetFog		(float density,float fogstart,float fogend,const float *color,int fogmode)=0;
  virtual void  SetFogColor(float * color)=0;
	virtual	void	EnableVSync(bool enable)=0;

  virtual void  DrawTriStrip(CVertexBuffer *src, int vert_num=4)=0;
	
	virtual void	PushMatrix()=0;
	virtual void	PopMatrix()=0;

	virtual	void	EnableTMU(bool enable)=0;
	virtual void	SelectTMU(int tnum)=0;
	
	virtual bool	ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp)=0;
  virtual void  ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height)=0;

  virtual	bool	SaveTga(unsigned char *sourcedata,int sourceformat,int w,int h,const char *filename,bool flip);

	//download an image to video memory. 0 in case of failure
	virtual	unsigned int DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat=true, int filter=FILTER_BILINEAR, int Id=0, char *szCacheName=NULL, int flags=0)=0;
  virtual	void UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF=eTF_X8R8G8B8)=0;
	
  virtual bool DXTCompress( byte *raw_data,int nWidth,int nHeight,ETEX_Format eTF, bool bUseHW, bool bGenMips, int nSrcBytesPerPix, const Vec3 vLumWeight, MIPDXTcallback callback );
  virtual bool DXTDecompress(byte *srcData,const size_t srcFileSize,byte *dstData,int nWidth,int nHeight,int nMips,ETEX_Format eSrcTF, bool bUseHW, int nDstBytesPerPix);

	virtual	bool	SetGammaDelta(const float fGamma)=0;

	virtual	void	RemoveTexture(unsigned int TextureId)=0;

  virtual	void	SetTexture(int tnum);
  virtual	void	SetWhiteTexture();

  virtual void  PrintToScreen(float x, float y, float size, const char *buf);
	virtual void	WriteXY		(int x,int y, float xscale,float yscale,float r,float g,float b,float a,const char *message, ...);	
	// Default implementation.
	// To be overrided in derived classes.
	virtual void	Draw2dText( float posX,float posY,const char *szText,SDrawTextInfo &info );
  virtual void TextToScreen(float x, float y, const char * format, ...);
  virtual void TextToScreenColor(int x, int y, float r, float g, float b, float a, const char * format, ...);

	/*!	Draw an image on the screen as a label text
			@param vPos:	3d position
			@param fSize: size of the image
			@nTextureId:	Texture Id dell'immagine
	*/
	virtual void DrawLabelImage(const Vec3 &vPos,float fSize,int nTextureId);

  virtual void DrawLabel(Vec3 pos, float font_size, const char * label_text, ...);
  virtual void DrawLabelEx(Vec3 pos, float font_size, float * pfColor, bool bFixedSize, bool bCenter, const char * label_text, ...);
	virtual void Draw2dLabel( float x,float y, float font_size, float * pfColor, bool bCenter, const char * label_text, ...);
	
  virtual void	Draw2dImage	(float xpos,float ypos,float w,float h,int texture_id,float s0=0,float t0=0,float s1=1,float t1=1,float angle=0,float r=1,float g=1,float b=1,float a=1,float z=1)=0;

	virtual int	SetPolygonMode(int mode)=0;

  virtual void ResetToDefault()=0;

	inline float ScaleCoordX(float value)
	{
//		value=(int)((float)(value)*(float)(m_width)/800.0f);	
		value*=float(m_width)/800.0f;
		return (value);
	}
	
	inline float ScaleCoordY(float value)
	{
//		value=(int)((float)(value)*(float)(m_height)/600.0f);	
		value*=float(m_height)/600.0f;
		return (value);
	}

  void SetWidth(int nW) { m_width=nW; }
  void SetHeight(int nH) { m_height=nH; }
	virtual	int		GetWidth()	{ return (m_width); }
	virtual	int		GetHeight() { return (m_height); }

	bool DoCompressedNormalmapEmulation() const;

	virtual int		GetPolygonMode() { return(m_polygon_mode); }

	const CCamera& GetCamera(void) { return(m_cam);  }
  const CRenderCamera& GetRCamera(void) { return(m_rcam);  }

	void		GetPolyCount(int &nPolygons,int &nShadowVolPolys) 
	{
    nPolygons = GetPolyCount();
		nShadowVolPolys = 0;
	}
	
	int GetPolyCount()
  {
    int nPolys = 0;
    for (int i=0; i<EFSLIST_NUM; i++)
    {
      nPolys += m_RP.m_PS.m_nPolygons[i];
    }
    return nPolys;
  }
	
	virtual void SetMaterialColor(float r, float g, float b, float a)=0;
	
	virtual bool WriteDDS(byte *dat, int wdt, int hgt, int Size, const char *name, ETEX_Format eF, int NumMips);
	virtual bool WriteTGA(byte *dat, int wdt, int hgt, const char *name, int src_bits_per_pixel, int dest_bits_per_pixel );
	virtual bool WriteJPG(byte *dat, int wdt, int hgt, char *name, int src_bits_per_pixel);

	virtual char * GetStatusText(ERendStats type)=0;
	virtual void GetMemoryUsage(ICrySizer* Sizer)=0;
	virtual void GetLogVBuffers() = 0;

  virtual int GetFrameID(bool bIncludeRecursiveCalls=true)
  {
    if (bIncludeRecursiveCalls)
      return m_nFrameID;
    return m_nFrameUpdateID;
  }

  // Project/UnProject
  virtual void ProjectToScreen( float ptx, float pty, float ptz, 
                                float *sx, float *sy, float *sz )=0;
  virtual int UnProject(float sx, float sy, float sz, 
                float *px, float *py, float *pz,
                const float modelMatrix[16], 
                const float projMatrix[16], 
                const int    viewport[4])=0;
  virtual int UnProjectFromScreen( float  sx, float  sy, float  sz, 
                           float *px, float *py, float *pz)=0;

  virtual void FlushTextMessages();

  // Shadow Mapping
  virtual void PrepareDepthMap(ShadowMapFrustum* SMSource, CDLight* pLight=NULL)=0;
  virtual void SetupShadowOnlyPass(int Num, ShadowMapFrustum* pShadowInst, Matrix34 * pObjMat)=0;
	virtual void DrawAllShadowsOnTheScreen()=0;
	virtual void OnEntityDeleted(IRenderNode * pRenderNode)=0;

  virtual void SetClipPlane( int id, float * params )=0;
	virtual void EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract)=0;
	
	virtual void SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa)=0;

	//for editor
  virtual void  GetModelViewMatrix(float *mat)=0;
  virtual void  GetProjectionMatrix(float *mat)=0;
  virtual Vec3 GetUnProject(const Vec3 &WindowCoords,const CCamera &cam) { return(Vec3(1,1,1)); };

  virtual void DrawObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)=0;
  virtual void GenerateObjSprites(PodArray<struct SVegetationSpriteInfo> *pList, float fMaxViewDist, CObjManager *pObjMan, float fZoomFactor)=0;
  virtual void DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode=0)=0;
  virtual void DrawQuad(float dy,float dx, float dz, float x, float y, float z)=0;

  virtual void ClearBuffer(uint nFlags, ColorF *vColor, float depth = 1.0f)=0;
  virtual void ReadFrameBuffer(unsigned char * pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX=-1, int nScaledY=-1)=0;
  
  //misc 
  virtual bool ScreenShot(const char *filename=NULL, int width=0)=0;

  virtual int	GetColorBpp()		{ return (m_cbpp); }
  virtual int	GetDepthBpp()		{ return (m_zbpp); }
  virtual int	GetStencilBpp()		{ return (m_sbpp); }
  virtual char*	GetVertexProfile(bool bSupportedProfile)=0;
  virtual char*	GetPixelProfile(bool bSupportedProfile)=0;

  virtual void Set2DMode(bool enable, int ortox, int ortoy,float znear=-1e10f,float zfar=1e10f)=0;

  virtual int ScreenToTexture()=0;

  // Shaders/Shaders support
  // RE - RenderElement
  bool m_bTimeProfileUpdated;
  int m_PrevProfiler;
  int m_nCurSlotProfiler;

  FILE *m_LogFile;
  FILE *m_LogFileStr;
  FILE *m_LogFileSh;
  _inline void Logv(int RecLevel, char *format, ...)
  {
    va_list argptr;
    
    if (m_LogFile)
    {
      for (int i=0; i<RecLevel; i++)
      {
        fprintf(m_LogFile, "  ");
      }
      va_start (argptr, format);
#ifndef PS2
      vfprintf (m_LogFile, format, argptr);
#endif      
      va_end (argptr);
    }
  }
  _inline void LogStrv(int RecLevel, char *format, ...)
  {
    va_list argptr;

    if (m_LogFileStr)
    {
      for (int i=0; i<RecLevel; i++)
      {
        fprintf(m_LogFileStr, "  ");
      }
      va_start (argptr, format);
#ifndef PS2
      vfprintf (m_LogFileStr, format, argptr);
#endif      
      va_end (argptr);
    }
  }
  _inline void LogShv(char *format, ...)
  {
    va_list argptr;
    int RecLevel = SRendItem::m_RecurseLevel;

    if (m_LogFileSh)
    {
      for (int i=0; i<RecLevel; i++)
      {
        fprintf(m_LogFileSh, "  ");
      }
      va_start (argptr, format);
#ifndef PS2
      vfprintf (m_LogFileSh, format, argptr);
#endif      
      va_end (argptr);
      fflush(m_LogFileSh);
    }
  }
  _inline void Log(char *str)
  {
    if (m_LogFile)
    {
      fprintf (m_LogFile, str);
    }
  }
  void EF_AddClientPolys2D();  
  void EF_AddClientPolys();  
  void EF_RemovePolysFromScene();
  
  bool EF_TryToMerge(CRenderObject *pNewObject, CRenderObject *pOldObject, CRendElement *pRE);

  void EF_TransformDLights();
  void EF_IdentityDLights();

  _inline void *EF_GetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags)
  {
    void *p;
    
    if (m_RP.m_pRE)
      p = m_RP.m_pRE->mfGetPointer(ePT, Stride, Type, Dst, Flags);
    else
      p = SRendItem::mfGetPointerCommon(ePT, Stride, Type, Dst, Flags);
    
    return p;
  }
  inline void EF_StartMerging()
  {
    if (m_RP.m_FrameMerge != m_RP.m_Frame)
    {
      m_RP.m_FrameMerge = m_RP.m_Frame;
      SBufInfoTable *pOffs = &gBufInfoTable[m_RP.m_CurVFormat];
      int Size = m_VertexSize[m_RP.m_CurVFormat];    
      m_RP.m_Stride = Size;
      m_RP.m_OffsD  = pOffs->OffsColor;
      m_RP.m_OffsT  = pOffs->OffsTC;
      m_RP.m_NextPtr = m_RP.m_Ptr;
      m_RP.m_NextPtrTang = m_RP.m_PtrTang;
    }
  }
  struct SSavedDLight
  {
    CDLight *m_pLight;
    CDLight m_Contents;
  };
  float EF_LightRadius(CDLight *dl)
  {
    float fRadius;
    if (m_RP.m_ObjFlags & FOB_TRANS_MASK)
    {
      float fLen = m_RP.m_pCurObject->m_II.m_Matrix(0,0)*m_RP.m_pCurObject->m_II.m_Matrix(0,0) + m_RP.m_pCurObject->m_II.m_Matrix(0,1)*m_RP.m_pCurObject->m_II.m_Matrix(0,1) + m_RP.m_pCurObject->m_II.m_Matrix(0,2)*m_RP.m_pCurObject->m_II.m_Matrix(0,2);
      unsigned int *n1 = (unsigned int *)&fLen;
      unsigned int n = 0x5f3759df - (*n1 >> 1);
      float *n2 = (float *)&n;
      float fISqrt = (1.5f - (fLen * 0.5f) * *n2 * *n2) * *n2;
      fRadius = dl->m_fRadius * fISqrt;
    }
    else
      fRadius = dl->m_fRadius;

    return fRadius;
  }
  _inline void EF_PushObjectsList()
  {
    m_ObjsStack[SRendItem::m_RecurseLevel].nNumVisObjects = m_RP.m_NumVisObjects;
    m_ObjsStack[SRendItem::m_RecurseLevel].nNumTempObjects = m_RP.m_TempObjects.Num();
  }
  _inline void EF_PopObjectsList()
  {
    m_RP.m_NumVisObjects = m_ObjsStack[SRendItem::m_RecurseLevel].nNumVisObjects;
    m_RP.m_TempObjects.SetUse(m_ObjsStack[SRendItem::m_RecurseLevel].nNumTempObjects);
  }

  _inline void EF_PushFog()
  {
    int nLevel = m_nCurFSStackLevel;
    if (nLevel >= 8)
      return;
    memcpy(&m_FSStack[nLevel], &m_FS, sizeof(SFogState));
    m_nCurFSStackLevel++;
  }
  _inline void EF_PopFog()
  {
    int nLevel = m_nCurFSStackLevel;
    if (nLevel <= 0)
      return;
    nLevel--;
    bool bFog = m_FS.m_bEnable;
    if (m_FS != m_FSStack[nLevel])
    {
      memcpy(&m_FS, &m_FSStack[nLevel], sizeof(SFogState));
      SetFog(m_FS.m_FogDensity, m_FS.m_FogStart, m_FS.m_FogEnd, &m_FS.m_FogColor[0], m_FS.m_nFogMode);
    }
    else
      m_FS.m_bEnable = m_FSStack[nLevel].m_bEnable;
    bool bNewFog = m_FS.m_bEnable;
    m_FS.m_bEnable = bFog;
    EnableFog(bNewFog);
    m_nCurFSStackLevel--;
  }
  void EF_AddRTStat(CTexture *pTex, int nFlags = 0, int nW=-1, int nH=-1);
  void EF_PrintRTStats(const char *szName);

  void EF_ApplyShadowQuality();

  void EF_ApplyShaderQuality(EShaderType eST);
  virtual EShaderQuality EF_GetShaderQuality(EShaderType eST);
  virtual ERenderQuality EF_GetRenderQuality() const;

  CREMesh *EF_GetTempMeshRE();
  uint EF_BatchFlags(SShaderItem& SH, CRenderObject *pObj);

  virtual float EF_GetWaterZElevation(float fX, float fY);
  virtual void EF_PipelineShutdown() = 0;

  virtual bool EF_PrecacheResource(IShader *pSH, float fDist, float fTimeToReady, int Flags);
  virtual bool EF_PrecacheResource(ITexture *pTP, float fDist, float fTimeToReady, int Flags);
  virtual bool EF_PrecacheResource(IRenderMesh *pPB, float fDist, float fTimeToReady, int Flags);
  virtual bool EF_PrecacheResource(CDLight *pLS, float fDist, float fTimeToReady, int Flags);
  
  // Create splash
  virtual void	EF_AddSplash (Vec3 Pos, eSplashType eST, float fForce, int Id=-1);
  void EF_UpdateSplashes(float fTime);

  virtual void EF_AddPolyToScene2D(int Ef, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts);
  virtual void EF_AddPolyToScene2D(const SShaderItem& si, int nTempl, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts);  

	virtual CRenderObject* EF_AddPolygonToScene(const SShaderItem& si, int numPts, const struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *verts, const SPipTangents *tangs, CRenderObject *obj, uint16 *inds, int ninds, int nAW, bool bMerge = true);
	virtual CRenderObject* EF_AddPolygonToScene(const SShaderItem& si, CRenderObject* obj, int numPts, int ninds, struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F*& verts, SPipTangents*& tangs, uint16*& inds, int nAW, bool bMerge = true);
	
  void EF_CheckOverflow(int nVerts, int nInds, CRendElement *re, int* nNewVerts=NULL, int* nNewInds=NULL);
  void EF_Start(CShader *ef, int nTech, SRenderShaderResources *Res, CRendElement *re);
  
  //==========================================================
  // external interface for shaders
  //==========================================================

	virtual CRenderObject* EF_AddParticlesToScene(const SShaderItem& si, CRenderObject* pRO, IParticleVertexCreator* pPVC, SParticleRenderInfo const& RenInfo, int nAW, bool& canUseGS);

  // Shaders management
  virtual string *EF_GetShaderNames(int& nNumShaders);
  virtual IShader *EF_LoadShader (const char *name, int flags=0, uint nMaskGen=0);
  virtual SShaderItem EF_LoadShaderItem (const char *name, bool bShare, int flags=0, SInputShaderResources *Res=NULL, uint nMaskGen=0);
	// reload file
  virtual bool EF_ReloadFile (const char *szFileName);
  virtual void EF_ReloadShaderFiles (int nCategory);
  virtual void EF_ReloadTextures ();
  virtual int EF_LoadLightmap(const char* nameTex);
  virtual bool	EF_ScanEnvironmentCM (const char *name, int size, Vec3& Pos);
  virtual ITexture *EF_GetTextureByID(int Id);
  virtual ITexture *EF_LoadTexture(const char* nameTex, uint flags, byte eTT, float fAmount1=-1.0f, float fAmount2=-1.0f);
  virtual const SShaderProfile &GetShaderProfile(EShaderType eST) const;
  virtual void SetShaderQuality(EShaderType eST, EShaderQuality eSQ);

  // Create new RE of type (edt)
  virtual CRendElement *EF_CreateRE (EDataType edt);

  // Begin using shaders
  virtual void EF_StartEf ();

  // Get Object for RE transformation
  virtual CRenderObject *EF_GetObject (bool bTemp=false, int num=-1);

  // Add shader to the list (virtual)
  virtual void EF_AddEf (CRendElement *pRE, SShaderItem& pSH,  CRenderObject *pObj, int nList=EFSLIST_GENERAL, int nAW=1);

	// Add shader to the list 
	void EF_AddEf_NotVirtual (CRendElement *pRE, SShaderItem& pSH, CRenderObject *pObj, int nList=EFSLIST_GENERAL, int nAW=1);
  
  // Hide shader template (exclude from list)
  virtual bool					EF_HideTemplate(const char *name);
  // UnHide shader template (include in list)
  virtual bool					EF_UnhideTemplate(const char *name);
  // UnHide all shader templates (include in list)
  virtual bool					EF_UnhideAllTemplates();

  // Draw all shaded REs in the list
  virtual void EF_EndEf3D (int nFlags)=0;

  // 2d interface for shaders
  virtual void EF_EndEf2D(bool bSort)=0;

  // Dynamic lights
  virtual bool EF_IsFakeDLight (CDLight *Source);
  virtual void EF_ADDDlight(CDLight *Source);
  virtual void EF_ADDFillLight(const SFillLight & rLight);
  virtual void EF_ClearLightsList();
  virtual bool EF_UpdateDLight(CDLight *pDL);

  virtual void *EF_Query(int Query, INT_PTR Param=0);

  virtual void EF_SetState(int st, int AlphaRef=-1) = 0;
  void EF_SetStencilState(int st, uint nStencRef, uint nStencMask, uint nStencWriteMask);

	// create/delete RenderMesh object
  virtual IRenderMesh * CreateRenderMesh(bool bDynamic, const char *szSourceType,const char *szSourceName);
  virtual IRenderMesh * CreateRenderMeshInitialized(
    void * pVertBuffer, int nVertCount, int nVertFormat, 
    ushort* pIndices, int nIndices,
    int nPrimetiveType, const char *szType,const char *szSourceName, EBufferType eBufType = eBT_Static,
    int nMatInfoCount=1, int nClientTextureBindID=0,    
    bool (*PrepareBufferCallback)(IRenderMesh *, bool)=NULL,
    void *CustomData = NULL,
    bool bOnlyVideoBuffer=false, bool bPrecache=true, SPipTangents * pTangents=NULL);

  virtual void DeleteRenderMesh(IRenderMesh * pLBuffer);

  virtual int GetMaxActiveTexturesARB() { return 0;}

  //////////////////////////////////////////////////////////////////////
	// Replacement functions for the Font engine ( vlad: for font can be used old functions )
	virtual	bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF=eTF_A8R8G8B8)=0;
	virtual	int  FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF=eTF_A8R8G8B8, bool genMips=false)=0;
  virtual	bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData)=0;
	virtual	void FontReleaseTexture(class CFBitmap *pBmp)=0;
	virtual void FontSetTexture(class CFBitmap*, int nFilterMode)=0;
  virtual void FontSetTexture(int nTexId, int nFilterMode)=0;
	virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight)=0;
	virtual void FontSetBlending(int src, int dst)=0;
	virtual void FontRestoreRenderingState()=0;

	virtual void Hint_DontSync( CTexture &rTex ){}

  //////////////////////////////////////////////////////////////////////
  // Used for pausing timer related stuff (eg: for texture animations, and shader 'time' parameter)
  void PauseTimer(bool bPause) {  m_bPauseTimer=bPause; }
	virtual IShaderPublicParams* CreateShaderPublicParams();

#ifndef EXCLUDE_SCALEFORM_SDK
	virtual void SF_ConfigMask( ESFMaskOp maskOp, unsigned int stencilCount );
	const SSF_GlobalDrawParams* SF_GetGlobalDrawParams() const { return m_pSFDrawParams; }
	virtual int SF_CreateTexture(int width, int height, int numMips, unsigned char* pData, ETEX_Format eTF);
	virtual void SF_GetMeshMaxSize(int& numVertices, int& numIndices) const;
#endif // #ifndef EXCLUDE_SCALEFORM_SDK

	virtual IVideoPlayer* CreateVideoPlayerInstance() const;

	virtual void SetCloudShadowTextureId( int id, const Vec3 & vSpeed ) { m_nCloudShadowTexId = id; m_vCloudShadowSpeed = vSpeed;}
	virtual void SetSkyLightRenderParams( const SSkyLightRenderParams* pSkyLightRenderParams ) { m_pSkyLightRenderParams = pSkyLightRenderParams; }
	const SSkyLightRenderParams* GetSkyLightRenderParams() const { return m_pSkyLightRenderParams; }
	virtual uint16 PushFogVolumeContribution( const ColorF& fogVolumeContrib );
	const ColorF& GetFogVolumeContribution( uint16 idx ) const;

	virtual int GetMaxTextureSize() { return m_MaxTextureSize; }

	int GetCloudShadowTextureId() const { return m_nCloudShadowTexId; }  
  Vec3 GetCloudShadowSpeed() const
  {
    return m_vCloudShadowSpeed;
  }

  bool IsHDRModeEnabled()
  {
    bool bHDR = true;
    if (!CV_r_hdrrendering)
      bHDR = false;
    if (CV_r_measureoverdraw)
      bHDR = false;
    if (!CV_r_hdrallownonfp && m_nHDRType > 1)
      bHDR = false;
    return bHDR;
  }
  alloc_info_struct *GetFreeChunk(int bytes_count, int nBufSize, PodArray<alloc_info_struct>& alloc_info, const char *szSource);
  bool ReleaseChunk(int p, PodArray<alloc_info_struct>& alloc_info);

	virtual const char * GetTextureFormatName(ETEX_Format eTF);
	virtual int GetTextureFormatDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF);
	virtual void SetDefaultMaterials(IMaterial * pDefMat, IMaterial * pTerrainDefMat) { m_pDefaultMaterial = pDefMat; m_pTerrainDefaultMaterial = pTerrainDefMat; }

protected:
	void EF_AddParticle( CREParticle* pParticle, const SShaderItem& shaderItem, CRenderObject* pRO, int nAW );
	void EF_RemoveParticlesFromScene();
	void SafeReleaseParticleREs();
	void GetMemoryUsageParticleREs( ICrySizer * pSizer );

	CRenderObject* MergeParticleRO( CRenderObject* pRO );
	CRenderObject* MergePolygonRO( CRenderObject* pRO );

protected:	  
	int				m_width, m_height, m_cbpp, m_zbpp, m_sbpp, m_FullScreen;	
	int				m_polygon_mode;

	CCamera			m_cam;					// current camera
  CRenderCamera	m_rcam;				// current camera

  int m_nFrameID;							// with recursive calls, access through GetFrameID(true)

  TArray<CCryName> m_HidedShaderTemplates;

  struct text_info_struct 
	{ 
		char	mess[256];
		Vec3 pos; 
		float font_size; 
		float fColor[4]; 
		bool	bFixedSize,bCenter,b2D; 
		int		nTextureId;
	};

  PodArray<text_info_struct> m_listMessages;

	int m_nCloudShadowTexId;
  Vec3 m_vCloudShadowSpeed;
	const SSkyLightRenderParams* m_pSkyLightRenderParams;
	std::vector<ColorF> m_fogVolumeContibutions;

#ifndef EXCLUDE_SCALEFORM_SDK
	const SSF_GlobalDrawParams* m_pSFDrawParams;
#endif

public:  

#ifdef WIN32
	static IDirectBee *m_pDirectBee;		// connection to D3D9 wrapper DLL, 0 if not established
#endif
  bool m_bNVLibInitialized;
  bool m_bDriverHasActiveMultiGPU;		// SLI or crossfire (not detected currently), affects r_multigpu behavior
  bool IsMultiGPUModeActive() const;
  uint m_nGPUs;

  CCamera	m_prevCamera;								// camera from previous frame
  uint m_nFrameUpdateID;							// without recursive calls, access through GetFrameID(false)
  uint m_nFrameLoad;
  uint m_nFrameReset;
  uint m_nFrameSwapID;							// without recursive calls, access through GetFrameID(false)
  bool m_bTemporaryDisabledSFX;

  ColorF m_cClearColor;
  int	 m_numtmus;

  CREPostProcess *m_pREPostProcess;

  // Used for pausing timer related stuff (eg: for texture animations, and shader 'time' parameter)
  bool m_bPauseTimer;
  float m_fPrevTime;
  bool m_bUseZpass;
  bool m_bWireframeMode, m_bWireframeModePrev;

  // HDR rendering stuff
  int m_dwHDRCropWidth;
  int m_dwHDRCropHeight;

  //=====================================================================
  // Shaders interface
  CShaderMan m_cEF;
  _smart_ptr<IMaterial> m_pDefaultMaterial;
  _smart_ptr<IMaterial> m_pTerrainDefaultMaterial;

  int m_TexGenID;

	IFFont *m_pDefaultFont;
  
  virtual void SetClearColor(const Vec3 & vColor) { m_cClearColor.r=vColor[0];m_cClearColor.g=vColor[1];m_cClearColor.b=vColor[2]; }

  //////////////////////////////////////////////////////////////////////
  // console variables
  //////////////////////////////////////////////////////////////////////

  static int CV_r_vsync;
  static int CV_r_stats;
  static int CV_r_log;
  static int CV_r_logTexStreaming;
  static int CV_r_logShaders;
  static int CV_r_logVBuffers;
  static int CV_r_flush;
  

  static int CV_r_hdrrendering;
  static int CV_r_hdrdebug;
  static int CV_r_hdrtype;
  static int CV_r_hdrallownonfp;
  static int CV_r_hdrhistogram;
  static float CV_r_hdrlevel;
	static float CV_r_hdrbrightoffset;
  static float CV_r_hdrbrightthreshold;

	// eye adaption
	static float CV_r_eyeadaptationfactor;
	static float CV_r_eyeadaptationbase;
	static float CV_r_eyeadaptionmin;
	static float CV_r_eyeadaptionmax;
	static float CV_r_eyeadaptionscale;
	static float CV_r_eyeadaptionbase;
	static float CV_r_eyeadaptionspeed;
	static float CV_r_eyeadaptionclamp;		// todo:remove

  static int CV_r_geominstancing;
  static int CV_r_geominstancingthreshold;
	static int m_iGeomInstancingThreshold;		// internal value, auto mapped depending on GPU hardware, 0 means not set yet
	static void ChangeGeomInstancingThreshold( ICVar *pVar=0 );

  static int CV_r_LightVolumesDebug;

  static int CV_r_UseShadowsPool;
  static float CV_r_ShadowsSlopeScaleBias;
  static float CV_r_ShadowsBias;
  static int CV_r_shadowtexformat;
  static int CV_r_ShadowsMaskResolution;
  static int CV_r_ShadowsMaskDownScale;
  static int CV_r_ShadowsDeferredMode;
  static int CV_r_ShadowsStencilPrePass;
  static int CV_r_ShadowsDepthBoundNV;
  static int CV_r_ShadowsForwardPass;
  static float CV_r_shadowbluriness;
	static float CV_r_shadow_jittering;
	static float CV_r_VarianceShadowMapBlurAmount;
  static int CV_r_ShadowsGridAligned;
	static int CV_r_ShadowGenGS;
	static int CV_r_ShadowPass;
  static int CV_r_ShadowGen;
  static int CV_r_shadowblur;

  static int CV_r_ShadowGenMode;
	static int CV_capture_misc_render_buffers;
	
	static int CV_r_SSAO;
	static int CV_r_SSAO_blur;
  static int CV_r_SSAO_downscale_ztarget;
  static int CV_r_SSAO_downscale_result_mask;
	static float CV_r_SSAO_amount;
  static float CV_r_SSAO_darkening;
	static float CV_r_SSAO_radius;
  static float CV_r_SSAO_blurriness;
  static int CV_r_SSAO_quality;
  static float CV_r_SSAO_depth_range;
  static int CV_r_TerrainAO;
  static int CV_r_TerrainAO_FadeDist;
  static int CV_r_FillLights;
  static int CV_r_FillLightsMode;
  static int CV_r_FillLightsDebug;

  static int CV_r_debuglights;
  static int CV_r_cullgeometryforlights;
  static ICVar *CV_r_showlight;

  static int    CV_r_debugscreenfx;    

	static int		CV_r_rc_autoinvoke;
      
  static int    CV_r_glow;  
  static float    CV_r_glow_fullscreen_threshold;  
  static float    CV_r_glow_fullscreen_multiplier;  
  static int    CV_r_postblend;  
	static int    CV_r_refraction;   
  static int    CV_r_sunshafts; 
  static int    CV_r_distant_rain;  
  static int    CV_r_nightvision;  

  static float  CV_r_glitterSize;  
  static float  CV_r_glitterVariation;  
  static float  CV_r_glitterSpecularPow;
  static int    CV_r_glitterAmount;  
  
  static int    CV_r_postprocess_effects;
  static int    CV_r_postprocess_effects_params_blending;
  static int    CV_r_postprocess_effects_reset;
  static int    CV_r_postprocess_effects_filters;
  static int    CV_r_postprocess_effects_gamefx;
  static int    CV_r_postprocess_profile_fillrate;

  static int    CV_r_colorgrading;
  static int    CV_r_colorgrading_selectivecolor;
  static int    CV_r_colorgrading_levels;
  static int    CV_r_colorgrading_filters;
  static int    CV_r_colorgrading_dof;
     
  static int CV_r_cloudsupdatealways;
  static int CV_r_cloudsdebug;

  static int CV_r_showdyntextures;
  static ICVar *CV_r_showdyntexturefilter;
  static int CV_r_shownormals;
  static int CV_r_showlines;
  static float CV_r_normalslength;
  static int CV_r_showtangents;
  static int CV_r_showtimegraph;
  static int CV_r_showtextimegraph;
  static int CV_r_showlumhistogram;
  static int CV_r_graphstyle;

  static ICVar *CV_r_excludeshader;
  static ICVar *CV_r_showonlyshader;
  static int CV_r_profileshaders;
  static int CV_r_profileshaderssmooth;

  static int CV_r_dyntexmaxsize;
  static int CV_r_dyntexatlascloudsmaxsize;
  static int CV_r_dyntexatlasspritesmaxsize;

  static int CV_r_texpostponeloading;
  static int CV_r_texatlassize;
  static int CV_r_texmaxanisotropy;
  static int CV_r_texmaxsize;
  static int CV_r_texminsize;
  static int CV_r_texresolution;
  static int CV_r_texlmresolution;
  static int CV_r_texbumpresolution;
  static int CV_r_texbumpheightmap;
  static int CV_r_texskyquality;
  static int CV_r_texskyresolution;
  static int CV_r_texnormalmaptype;
  static int CV_r_texhwmipsgeneration;
//	static int CV_r_texhwdxtcompression;
  static int CV_r_texgrid;
  static int CV_r_texlog;
  static int CV_r_texnoload;
  
  static int CV_r_texturesstreampoolsize;
  static int CV_r_texturesstreaming;
  static float CV_r_texturesstreamingmaxasync;
  static int CV_r_texturesstreamingnoupload;
  static int CV_r_texturesstreamingonlyvideo;
  static int CV_r_texturesstreamingsync;
	static int CV_r_texturesstreamingignore;
  static int CV_r_texturesmipbiasing;
  static int CV_r_texturesfilteringquality;

	static float CV_r_TextureLodDistanceRatio;
	static int CV_r_TextureLodMaxLod;

  static int CV_r_lightssinglepass;

  static int CV_r_envlightcmdebug;
  static int CV_r_envcmwrite;
  static int CV_r_envcmresolution;
  static int CV_r_envtexresolution;
  static float CV_r_envlcmupdateinterval;
  static float CV_r_envcmupdateinterval;
  static float CV_r_envtexupdateinterval;

//  static float CV_r_waterupdateFactor;
//  static float CV_r_waterupdateDistance;
//  static float CV_r_waterupdateRotation;
  static int CV_r_waterreflections_mgpu;	
  static int CV_r_waterreflections_use_min_offset;	
  static float CV_r_waterreflections_min_visible_pixels_update;	
  static float CV_r_waterreflections_minvis_updatefactormul;	
  static float CV_r_waterreflections_minvis_updatedistancemul;	
  static float CV_r_waterUpdateChange;
  static float CV_r_waterUpdateTimeMin;
  static float CV_r_waterUpdateTimeMax;
  static int CV_r_waterreflections;	
  static int CV_r_waterrefractions;
  static float CV_r_waterreflections_offset;	
  
  static int CV_r_watercaustics;
  static int CV_r_waterreflections_quality;  
  static int CV_r_water_godrays;  

  static int CV_r_reflections;	
  static int CV_r_reflections_quality;	
  
  static int CV_r_texture_anisotropic_level;
  static int CV_r_texnoaniso;
  static int CV_r_oceanrendtype;
  static int CV_r_oceansectorsize;
  static int CV_r_oceanheightscale;
  static int CV_r_oceanloddist;
  static int CV_r_oceantexupdate;
  static int CV_r_oceanmaxsplashes;
  static float CV_r_oceansplashscale;

  static int CV_r_reloadshaders;

  static int CV_r_detailtextures;
  static float CV_r_detailscale;
  static float CV_r_detaildistance;
  static int CV_r_detailnumlayers;

  static int CV_r_nopreprocess;
  static int CV_r_noloadtextures;
  static int CV_r_texbindmode;
  static int CV_r_nodrawshaders;
  static int CV_r_nodrawnear;
	static int CV_r_drawnearfov;

  static int CV_r_optimisedlightsetup;

  static int CV_r_shadersalwaysusecolors;
  static int CV_r_shadersdebug;
  static int CV_r_shadersuserfolder;
  static int CV_r_shadersignoreincludeschanging;
  static int CV_r_shaderspreactivate;
  static int CV_r_shadersintcompiler;
  static int CV_r_shadersasynccompiling;
  static int CV_r_shadersasyncmaxthreads;
  static int CV_r_shaderscacheoptimiselog;
  static int CV_r_shadersprecachealllights;
  static int CV_r_shadersstaticbranching;
  static int CV_r_shadersdynamicbranching;

  static int CV_r_debugrendermode;
  static int CV_r_debugrefraction;

  static int CV_r_meshprecache;
  static int CV_r_meshshort;

  static int CV_r_multigpu;
  static int CV_r_validateDraw;
  static int CV_r_profileDIPs;
  static int CV_r_occlusionqueries_mgpu;

  static int CV_r_fsaa;
  static int CV_r_fsaa_samples;
  static int CV_r_fsaa_quality;

  static int CV_r_atoc;

  static int CV_r_impostersdraw;
	static float CV_r_imposterratio;
  static int CV_r_impostersupdateperframe;

  static int CV_r_flares;

  static int CV_r_beams;
  static int CV_r_beamssoftclip;
  static int CV_r_beamshelpers;
  static int CV_r_beamsmaxslices;
  static float CV_r_beamsdistfactor;
  
  static int CV_r_usezpass;
	static int CV_r_usealphablend;
	static int CV_r_useedgeaa;
  static int CV_r_usehwskinning;
  static int CV_r_usemateriallayers;
  static int CV_r_usesoftparticles;
  static int CV_r_useparticles_refraction;
  static int CV_r_useparticles_glow;
	static int CV_r_usepom;
  static int CV_r_rain;  
  static float CV_r_rain_maxviewdist;  

  static int CV_r_hair_sorting_quality;
  
  static int CV_r_motionblur;  
	static int s_MotionBlurMode;				// potentially adjusted internal state of r_motionblur
  static float CV_r_motionblur_shutterspeed;  
  static int CV_r_motionblurframetimescale;  
  static int CV_r_motionblurdynamicquality;  
  static float CV_r_motionblurdynamicquality_rotationacc_stiffness;  
  static float CV_r_motionblurdynamicquality_rotationthreshold;  
  static float CV_r_motionblurdynamicquality_translationthreshold;  
  static int CV_r_debug_motionblur;

  static int CV_r_debug_extra_scenetarget_fsaa; 

  static int CV_r_customvisions;  



  static int CV_r_dof;

  static float CV_r_gamma;
  static float CV_r_contrast;
  static float CV_r_brightness;
  static int CV_r_nohwgamma;
  
  static int CV_r_coronas;
  static int CV_r_scissor;
  static float CV_r_coronafade;
  static float CV_r_coronacolorscale;
  static float CV_r_coronasizescale;

  static int CV_r_cullbyclipplanes;
	
	static int CV_r_PolygonMode;		
	static int CV_r_GetScreenShot;	

  static int CV_r_printmemoryleaks;
  static int CV_r_releaseallresourcesonexit;
  static int CV_r_character_nodeform;
  static int CV_r_VegetationSpritesAlphaBlend;
  static int CV_r_VegetationSpritesNoBend;
  static int CV_r_ZPassOnly;
  static int CV_r_VegetationSpritesNoGen;
  static int CV_r_VegetationSpritesTexRes;
  static int CV_r_VegetationSpritesGenAlways;

  static int CV_r_measureoverdraw;
  static float CV_r_measureoverdrawscale;

  static int CV_r_ShowVideoMemoryStats;
	static int CV_r_ShowRenderTarget;
	static int CV_r_ShowRenderTarget_FullScreen;
  static int CV_r_ShowLightBounds;
	static int CV_r_MergeRenderChunksForDepth;
	static int CV_r_TextureCompressor;
  
	static int CV_r_RAM;
	static int CV_r_UseGSParticles;
	static int CV_r_Force3DcEmulation;
	
	static float CV_r_ZFightingDepthScale;
	static float CV_r_ZFightingExtrude;

  virtual void MakeMatrix(const Vec3 & pos, const Vec3 & angles,const Vec3 & scale, Matrix34 * mat){assert(0);};

	// Sergiy: it's enough to allocate 16 bytes more, even on 64-bit machine
	// - and we need to store only the offset, not the actual pointer
  void* operator new( size_t Size )
  {
    byte *ptr = (byte *)malloc(Size+16+4);
    memset(ptr, 0, Size+16+4);
    byte *bPtrRes = (byte *)((INT_PTR)(ptr+4+16) & ~0xf);
    ((byte**)bPtrRes)[-1] = ptr;

    return bPtrRes;
  }
  void operator delete( void *Ptr )
  {
    free(((byte**)Ptr)[-1]);
  }

  virtual WIN_HWND GetHWND() = 0;
	virtual WIN_HWND GetCurrentContextHWND() {	return GetHWND();	};

  void SetTextureAlphaChannelFromRGB(byte * pMemBuffer, int nTexSize);

	void EnableSwapBuffers(bool bEnable) { m_bSwapBuffers = bEnable; }
	bool m_bSwapBuffers;

public:
	virtual void MarkForDepthMapCapture()
	{
		assert(!m_bMarkForDepthMapCapture);
		m_bMarkForDepthMapCapture = true;
	}
	virtual bool IsMarkedForDepthMapCapture() const { return m_bMarkForDepthMapCapture; }
	virtual void UnmarkForDepthMapCapture()
	{
		assert(m_bMarkForDepthMapCapture);
		m_bMarkForDepthMapCapture = false;
	}

	virtual void SetDataForVertexScatterGeneration(struct SScatteringObjectData* const *objects_for_vertexscattergeneration, int numobjects)
	{
		m_ObjectsForVertexScatterGeneration = objects_for_vertexscattergeneration;
		m_nNumObjectsForVertexScatterGeneration = numobjects;
	}

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS
public:
	virtual void  EF_AddGPUParticlesToScene( int32 nGPUParticleIdx, AABB const& bb, const SShaderItem& shaderItem, CRenderObject* pRO, bool nAW, bool canUseGS );

protected:
	void  EF_RemoveGPUParticlesFromScene();
	void  SafeReleaseParticleGPUREs();
	void	GetMemoryUsageParticleGPUREs( ICrySizer *pSizer );
#endif

public:
	virtual void BeginPerfEvent(uint32 color, const wchar_t* pstrMessage) const {}
	virtual void EndPerfEvent() const {}
};

//struct CVars;
class CryCharManager;
void *gGet_D3DDevice();
void *gGet_glReadPixels();
extern Matrix44 sIdentityMatrix;
#include "CommonRender.h"

#define SKY_BOX_SIZE 32.f

#endif //renderer
