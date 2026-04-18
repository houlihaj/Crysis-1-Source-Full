#ifndef GLBASE_H
#define GLBASE_H

#if !defined(LINUX)
#include "Mygl.h"
#endif

#if !defined(PS3) && !defined(LINUX)
// GL functions declarations.
#define GL_EXT(name) extern byte SUPPORTS##name;
#define GL_PROC(ext,ret,func,parms) extern ret (__stdcall *func)parms;
#include "GLFuncs.h"
#undef GL_EXT
#undef GL_PROC

#endif

namespace NDXTCompression
{
	void CompressToDXT(uint8 const* cpSrc, void* pDest, const D3DFORMAT cFormat, const uint32 cWidth, const uint32 cHeight);
}

//=====================================================================================
// D3D9 -> OpenGL wrapper

// We will use D3DX9 math in OpenGL as well
#include "D3DX9/PS3_d3dx9.h"

//maps global constants to handles and sets their values if specified
//necessary since there is no register mapping in cg as for hlsl
struct SGlobalConstMap
{
	static const int scPSConst = 1; //is pixel shader constant
	static const int scVSConst = 2;	//is vertex shader constant

	float values[4];					//values to be set, not used in every case
	float *pToBeUpdatedValues;//values to be set for pending updates (only allocated in case we need it)

	const char *cpConstName;//name to map handle
	int handle;							//cg handle
	int shaderType;					//ps or vs shader constant
  short isMatrix;					//tracks if constant is matrix type, scIs2x4Matrix..scIs4x4Matrix, 0 if not
	int toBeUpdated;				//0 if constant has already been transferred into shader, 1 otherwise
	int toBeUpdatedCount;		//Vector4fCount in case of pending update (toBeUpdated = true)
	int toBeUpdatedOffset;	//cOffset in case of pending update (toBeUpdated = true)
	int toBeUpdatedAllocCount;//allcoated number for pToBeUpdatedValues

	SGlobalConstMap() : handle(-1), shaderType(0), isMatrix(0), toBeUpdated(0), cpConstName(NULL),
		toBeUpdatedCount(0), toBeUpdatedOffset(0), pToBeUpdatedValues(NULL), toBeUpdatedAllocCount(0)
	{
		values[0] = values[1] = values[2] = values[3] = 0.f;
	}

	void Init()//just in case ctor is not called
	{
		handle = -1; 
		shaderType = 0;
		isMatrix = 0; 
		toBeUpdated = 0;
		values[0] = values[1] = values[2] = values[3] = 0.f;
	}

	const bool IsHandleValid() const
	{
		return handle > 0;	//0 is also a bad handle
	}

	void InitHandle(void *pSH, IDirect3DDevice9 *pDev, const bool cKeepHandle, const bool cDetType = false);

	HRESULT SetConstantIntoShader
	(
		IDirect3DDevice9 *pDev, 
		CONST float* pConstantData,
		UINT Vector4fCount,
		const unsigned int cOffset = 0
	);

	~SGlobalConstMap()
	{
		if(pToBeUpdatedValues)
		{
			delete [] pToBeUpdatedValues;
			pToBeUpdatedValues = NULL;
		}
	}
};

#endif
