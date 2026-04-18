/*=============================================================================
	CommonRender.h: Crytek Common render helper functions and structures declarations.
	Copyright (c) 2001 Crytek Studios. All Rights Reserved.

	Revision history:
		* Created by Honich Andrey

=============================================================================*/

#ifndef _COMMONRENDER_H_
#define _COMMONRENDER_H_

#include "Cry_Math.h"

#include "Defs.h"
#include "Cry_Color.h"



#if defined (DIRECT3D9) || defined (OPENGL) || defined(NULL_RENDERER)
#define VSCONST_INSTDATA   40
#define VSCONST_SKINMATRIX (40)
#define VSCONST_SHAPEDEFORMATION (VSCONST_SKINMATRIX + NUM_MAX_BONES_PER_GROUP_WITH_MB * 4)
#define VSCONST_NOISE_TABLE 64
#define NUM_MAX_BONES_PER_GROUP (100)
#define NUM_MAX_BONES_PER_GROUP_WITH_MB (50)

#elif defined (DIRECT3D10)
#define VSCONST_INSTDATA   0
#define VSCONST_SKINMATRIX 0
#define VSCONST_SHAPEDEFORMATION 0
#define VSCONST_NOISE_TABLE 0
#define NUM_MAX_BONES_PER_GROUP (150)
#define NUM_MAX_BONES_PER_GROUP_WITH_MB (75)

#else
#error Dont know what to define limiting constants to

#endif


//////////////////////////////////////////////////////////////////////
class CRenderer;
extern CRenderer *gRenDev;

class CBaseResource;

//====================================================================

#define CR_LITTLE_ENDIAN


extern TArray <ColorF> gCurLightStyles;

struct SWaveForm;

extern bool gbRgb;

_inline DWORD COLCONV (DWORD clr)
{
  return ((clr & 0xff00ff00) | ((clr & 0xff0000)>>16) | ((clr & 0xff)<<16));
}
_inline void COLCONV (ColorF& col)
{
  float v = col[0];
  col[0] = col[2];
  col[2] = v;
}

_inline void f2d(double *dst, float *src)
{
  for (int i=0; i<16; i++)
  {
    dst[i] = src[i];
  }
}

_inline void d2f(float *dst, double *src)
{
  for (int i=0; i<16; i++)
  {
    dst[i] = (float)src[i];
  }
}

const int HALF_RAND = (RAND_MAX / 2);
_inline float RandomNum()
{
  int rn;
  rn = rand();
  return ((float)(rn - HALF_RAND) / (float)HALF_RAND);
}
_inline float UnsRandomNum()
{
  int rn;
  rn = rand();
  return ((float)rn / (float)RAND_MAX);
}

//=================================================================

typedef std::map<CCryName,CBaseResource *> ResourcesMap;
typedef ResourcesMap::iterator ResourcesMapItor;

typedef std::vector<CBaseResource *> ResourcesList;
typedef std::vector<int> ResourceIds;

struct SResourceContainer
{
  ResourcesList m_RList;             // List of objects for acces by Id's
  ResourcesMap  m_RMap;              // Map of objects for fast searching
  ResourceIds   m_AvailableIDs;      // Available object Id's for efficient ID's assigning after deleting

  ~SResourceContainer();
};

typedef std::map<CCryName,SResourceContainer *> ResourceClassMap;
typedef ResourceClassMap::iterator ResourceClassMapItor;

class CBaseResource
{
private:
  // Per resource variables
  uint m_nRefCount;
  int m_nID;
  CCryName m_ClassName;
  CCryName m_Name;

  static ResourceClassMap m_sResources;

public:
  // CCryUnknown interface
  virtual int AddRef() { return m_nRefCount++; }
  virtual int Release()
  {
    if (--m_nRefCount <= 0)
    {
      delete this;
      return 0;
    }
    return m_nRefCount;
  }
  virtual int GetRefCounter() const { return m_nRefCount; }

  // Constructors.
  CBaseResource() : m_nRefCount(1) {}
  CBaseResource(const CBaseResource& Src);
  CBaseResource& operator=(const CBaseResource& Src);

  // Destructors.
  virtual ~CBaseResource();

  CCryName GetCCryName() { return m_Name; }
  inline const char *GetName() const { return m_Name.c_str(); }
  inline const char *GetClassName() const { return m_ClassName.c_str(); }
  inline int GetID() const { return m_nID; }
  inline void SetID(int nID) { m_nID = nID; }

  virtual bool  IsValid();

  static ResourceClassMap& GetMaps() { return m_sResources; }
  static CBaseResource *GetResource(const CCryName& className, int nID, bool bAddRef);
  static CBaseResource *GetResource(const CCryName& className, const CCryName& Name, bool bAddRef);
  static SResourceContainer* GetResourcesForClass(const CCryName& className);
  static void ShutDown();

  bool Register(const CCryName& resName, const CCryName& Name);
  bool UnRegister();
};

//=================================================================

#endif
