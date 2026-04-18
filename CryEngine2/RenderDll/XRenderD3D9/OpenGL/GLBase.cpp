/*=============================================================================
GLBase.cpp : OpenGL Render interface implementation.
Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "../DriverD3D.h"

#include <ctype.h>

#ifdef OPENGL

#ifdef PS3
#include <GLES/glext.h>
#elif defined(LINUX)
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <SDL/SDL.h>
#endif

#if defined(LINUX) || defined(PS3)
int gScreenWidth = 0, gScreenHeight = 0;
#endif

#if defined(LINUX)
static SDL_Surface *gContext = NULL;
#endif

#ifdef PS3
#include <sdk_version.h>
#include <sys/paths.h>
#include <PSGL/report.h>

using namespace std;

// Debug code: if PSGL_DEBUG_SWAP is defined, then setting the global flag
// psglSwap_EnableDebug will cause psglSwap() to be called twice after every
// draw call (glDrawArrays() or glDrawElements()).  Between the two swap
// calls, psglSwap_Break() is called (intended for setting a breakpoint
// there).
#define PSGL_DEBUG_SWAP 1

#ifdef PSGL_DEBUG_SWAP
void psglSwap_Break() { }
bool psglSwap_EnableDebug = false;
#endif

#define REPORT_IGNORE_MAXLEN (1024)
size_t ps3_report_ignore_maxsize = REPORT_IGNORE_MAXLEN;
char ps3_report_ignore_buf[REPORT_IGNORE_MAXLEN] = "";
char *ps3_report_ignore = ps3_report_ignore_buf;
#endif

#if !defined(PS3) && !defined(LINUX)
// GL functions implementations.
#define GL_EXT(name) byte SUPPORTS##name;
#define GL_PROC(ext,ret,func,parms) ret (__stdcall *func)parms;
#include "GLFuncs.h"
#undef GL_EXT
#undef GL_PROC
#define CLEAR_PROC(func) do (func) = NULL; while (false)
#else
#define CLEAR_PROC(func) ((void)0)
#endif

#if defined(LINUX)
#define GL_EXT(name) byte SUPPORTS ## name;
#define GL_PROC(ext,ret,func,parms)
#include "GLFuncs.h"
#undef GL_EXT
#undef GL_PROC
#endif

#if defined(PS3)
#include <sys/process.h>
#include <sys/spu_initialize.h>
#endif

#if defined(PS3)
#define GL_FRAMEBUFFER_COMPLETE_EXT \
	GL_FRAMEBUFFER_COMPLETE_OES
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT \
	GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT \
	GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT \
	GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT \
	GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT \
	GL_FRAMEBUFFER_UNSUPPORTED_OES

#define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER_OES
#define GL_RENDERBUFFER_EXT GL_RENDERBUFFER_OES
#define GL_DEPTH_ATTACHMENT_EXT GL_DEPTH_ATTACHMENT_OES

#define glGenFramebuffersEXT glGenFramebuffersOES
#define glDeleteFramebuffersEXT glDeleteFramebuffersOES
#define glBindFramebufferEXT glBindFramebufferOES
#define glFramebufferRenderbufferEXT glFramebufferRenderbufferOES
#define glFramebufferTexture2DEXT glFramebufferTexture2DOES
#define glCheckFramebufferStatusEXT glCheckFramebufferStatusOES
#define glGenRenderbuffersEXT glGenRenderbuffersOES
#define glDeleteRenderbuffersEXT glDeleteRenderbuffersOES
#define glBindRenderbufferEXT glBindRenderbufferOES
#define glRenderbufferStorageEXT glRenderbufferStorageOES
#endif

static int CV_gl_3dfx_gamma_control;
static int CV_gl_arb_texture_compression;
static int CV_gl_arb_multitexture;
static int CV_gl_arb_pbuffer;
static int CV_gl_arb_pixel_format;
static int CV_gl_arb_buffer_region;
static int CV_gl_arb_render_texture;
static int CV_gl_arb_multisample;
static int CV_gl_arb_vertex_program;
static int CV_gl_arb_vertex_buffer_object;
static int CV_gl_arb_fragment_program;
static int CV_gl_arb_texture_env_combine;
static int CV_gl_ext_swapcontrol;
static int CV_gl_ext_bgra;
static int CV_gl_ext_depth_bounds_test;
static int CV_gl_ext_multi_draw_arrays;
static int CV_gl_ext_compiled_vertex_array;
static int CV_gl_ext_texture_cube_map;
static int CV_gl_ext_separate_specular_color;
static int CV_gl_ext_secondary_color;
static int CV_gl_ext_texture_env_combine;
static int CV_gl_ext_paletted_texture;
static int CV_gl_ext_stencil_two_side;
static int CV_gl_ext_stencil_wrap;
static int CV_gl_ext_texture_filter_anisotropic;
static int CV_gl_ext_texture_env_add;
static int CV_gl_ext_texture_rectangle;
static int CV_gl_hp_occlusion_test;
static int CV_gl_nv_fog_distance;
static int CV_gl_nv_texture_env_combine4;
static int CV_gl_nv_point_sprite;
static int CV_gl_nv_vertex_array_range;
static int CV_gl_nv_register_combiners;
static int CV_gl_nv_register_combiners2;
static int CV_gl_nv_texgen_reflection;
static int CV_gl_nv_texgen_emboss;
static int CV_gl_nv_vertex_program;
static int CV_gl_nv_vertex_program3;
static int CV_gl_nv_texture_rectangle;
static int CV_gl_nv_texture_shader;
static int CV_gl_nv_texture_shader2;
static int CV_gl_nv_texture_shader3;
static int CV_gl_nv_fragment_program;
static int CV_gl_nv_multisample_filter_hint;
static int CV_gl_sgix_depth_texture;
static int CV_gl_sgix_shadow;
static int CV_gl_sgis_generate_mipmap;
static int CV_gl_sgis_texture_lod;
static int CV_gl_ati_separate_stencil;
static int CV_gl_ati_fragment_shader;

static IDirect3DDevice9 *g_pDev;
#pragma comment (lib, "OpenGL/CG/cg.lib")
#pragma comment (lib, "OpenGL/CG/cgGL.lib")

CGcontext scgContext;

#if defined(PS3)
// The name of the current shader (for debugging).
static char ps3ShaderName_buf[1024] = "";
char *ps3ShaderName = ps3ShaderName_buf;
size_t ps3ShaderName_size = sizeof ps3ShaderName_buf;
#endif

// Map a D3DFORMAT value to a GL format
// Parameters:
// - glFormat: if not NULL, then the associated GL format is assigned to
//   *glFormat. This is one of GL_ARGB_SCE, GL_BGRA, GL_ABGR, GL_ALPHA,
//   GL_LUMINANCE, GL_LUMINANCE_ALPHA, ...
// - glType: if not NULL, then the GL data type is assigned to *glType. This is
//   one of GL_UNSIGNED_BYTE, GL_HALF_FLOAT_ARB, GL_FLOAT,
//   GL_UNSIGNED_SHORT_4_4_4_4, ...
// - glInternalFormat: if not NULL, then a suggestion for an internal format
//   suitable for the specified format is assigned to *glInternalFormat, e.g.
//   GL_RGBA8. In case of a compressed format, the corresponding compressed GL
//   format (GL_COMPRESSED_*).
// - glCompressed: if not NULL, then a flag indicating a compressed format is
//   assigned to *glCompressed. If this is NULL, then compressed formats are
//   not recognized (i.e. the function returns false for compressed formats).
// Return:
// The function returns true if the specified format is recognized and false
// otherwise.
static bool D3DFORMAT2GL(
		D3DFORMAT Format,
		GLenum *glFormat,
		GLenum *glType,
		GLint *glInternalFormat,
		bool *glCompressed
		)
{
	GLenum format = (GLenum)0;
	GLenum type = (GLenum)0;
	GLint internalFormat = 0;
	bool compressed = false;

	switch (Format)
	{
  case D3DFMT_DXT1:
		internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		compressed = true;
		break;
	case D3DFMT_DXT3:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		compressed = true;
		break;
	case D3DFMT_DXT5:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		compressed = true;
		break;
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8R8G8B8:
#if defined(PS3)
		format = GL_ARGB_SCE;
		internalFormat = GL_ARGB_SCE;
#else
		format = GL_BGRA;
		internalFormat = GL_RGBA8;
#endif
		//type = GL_UNSIGNED_BYTE;
		type = GL_UNSIGNED_INT_8_8_8_8;
		break;
	case D3DFMT_A8:
		format = GL_ALPHA;
		type = GL_UNSIGNED_BYTE;
    internalFormat = GL_ALPHA8;
		break;
  case D3DFMT_L8:
		format = GL_LUMINANCE;
		type = GL_UNSIGNED_BYTE;
    internalFormat	= GL_LUMINANCE8;
		break;
  case D3DFMT_A4R4G4B4:
		//format = GL_ARGB_SCE;
		format = GL_RGBA;
		type = GL_UNSIGNED_SHORT_4_4_4_4;
    internalFormat = GL_RGBA4;
		break;
	case D3DFMT_A16B16G16R16F:
		//format = GL_ABGR;
		format = GL_RGBA;
		type = GL_HALF_FLOAT_ARB;
		internalFormat = GL_RGBA16F_ARB;
		break;
	case D3DFMT_G16R16F:
		format = GL_LUMINANCE_ALPHA;
		type = GL_HALF_FLOAT_ARB;
		internalFormat = GL_LUMINANCE_ALPHA16F_ARB;
		break;
	case D3DFMT_R16F:
		format = GL_RED;
		type = GL_HALF_FLOAT_ARB;
		internalFormat = GL_LUMINANCE16F_ARB;
		break;
	case D3DFMT_A32B32G32R32F:
		//format = GL_ABGR;
		format = GL_RGBA;
		type = GL_FLOAT;
		internalFormat = GL_RGBA32F_ARB;
		break;
	default:
		return false;
	}
	if (compressed && glCompressed == NULL) return false;
	if (glCompressed) *glCompressed = compressed;
	if (glFormat) *glFormat = format;
	if (glType) *glType = type;
	if (glInternalFormat) *glInternalFormat = internalFormat;
	return true;
}

static void FillAlpha(int w, int h, uint8 *data)
{
	for (int i = w * h * 4 - 4; i >= 0; i -= 4)
		data[i] = 255;
}

static int GetVSParamRegisterIndex(CGparameter Param)
{
#if defined(PS3) && defined(CELL_ARCH_CEB)
	// The PSGL implementation of cgGetParameterResourceIndex() in SDK 0.5.x
	// returns the attribute number (i.e. register number) of the parameter.
	// E.g.  TEXCOORD0 has index 8.  This is in conflict with the Cg API
	// specification, but it happens to be exactly what we want here.
	int Index = cgGetParameterResourceIndex(Param);
#else
	CGresource Resource = cgGetParameterResource(Param);
	int Index = -1;
	switch (Resource)
	{
		case CG_ATTR0: case CG_POSITION0: Index = 0; break;
		case CG_ATTR1: case CG_BLENDWEIGHT0: Index = 1; break;
		case CG_ATTR2: case CG_NORMAL0: Index = 2; break;
		case CG_ATTR3: case CG_COLOR0: case CG_DIFFUSE0: Index = 3; break;
		case CG_ATTR4: case CG_COLOR1: case CG_SPECULAR0: Index = 4; break;
		case CG_ATTR5: case CG_FOGCOORD: case CG_TESSFACTOR: Index = 5; break;
		case CG_ATTR6: case CG_PSIZE0: Index = 6; break;
		case CG_ATTR7: case CG_BLENDINDICES0: Index = 7; break;
		case CG_ATTR8: case CG_TEXCOORD0: Index = 8; break;
		case CG_ATTR9: case CG_TEXCOORD1: Index = 9; break;
		case CG_ATTR10: case CG_TEXCOORD2: Index = 10; break;
		case CG_ATTR11: case CG_TEXCOORD3: Index = 11; break;
		case CG_ATTR12: case CG_TEXCOORD4: Index = 12; break;
		case CG_ATTR13: case CG_TEXCOORD5: Index = 13; break;
		case CG_ATTR14: case CG_TEXCOORD6: case CG_TANGENT0: Index = 14; break;
		case CG_ATTR15: case CG_TEXCOORD7: case CG_BINORMAL0: Index = 15; break;
		default: break;
	}
#endif
	return Index;
}

const char *GetFBStatusString(GLenum fbStatus)
{
	switch (fbStatus)
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		return "GL_FRAMEBUFFER_COMPLETE";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
		return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
	  return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
		return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
		return "GL_FRAMEBUFFER_INCOMPLETE_FORMATS";
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		return "GL_FRAMEBUFFER_UNSUPPORTED";
	default:
	  break;
	}
	return NULL;
}

#if defined(_DEBUG)
static std::map<CGprogram, string> shaderNameMap;
#if defined(PS3)
const char *GetMappedSemantic(CGparameter, char *, size_t);
#endif

// Debug function for creating a description string of a program parameter.
static const char *DescribeProgramParameter(
		CGparameter Param,
		char *Buffer,
		size_t Size,
		const char *ExtraInfo,
		int Indent = 0)
{
	char *BufferP = Buffer, *const BufferEnd = Buffer + Size;
	const char *Name = cgGetParameterName(Param);
	CGtype Type = cgGetParameterType(Param);
	CGprogram Program = cgGetParameterProgram(Param);
	CGprofile Profile = cgGetProgramProfile(Program);
	bool IsReferenced = cgIsParameterReferenced(Param);
	CGparameter FirstElementParam = Param;
	while (cgGetParameterType(FirstElementParam) == CG_ARRAY)
		FirstElementParam = cgGetArrayParameter(FirstElementParam, 0);
	char TypeName[256];
	char IndentPrefix[256];
	Indent = min(Indent, (int)sizeof IndentPrefix - 1);
	memset(IndentPrefix, ' ', Indent);
	IndentPrefix[Indent] = 0;
#if 0
	// The current SDK (version 0.8.1) does not support the retrieval of type
	// names for user defined types.
	if (Type == CG_STRUCT)
	{
		CGtype UserType = cgGetParameterNamedType(Param);
		const char *UserTypeName = cgGetTypeString(UserType);
		snprintf(TypeName, sizeof TypeName - 1, "%s(%s)",
				cgGetTypeString(Type), UserTypeName);
	}
	else
#endif
	{
		strncpy(TypeName, cgGetTypeString(Type), sizeof TypeName - 1);
		TypeName[sizeof TypeName - 1] = 0;
	}
	BufferP += snprintf(BufferP, BufferEnd - BufferP,
			"[CGparameter Name=%s Type=%s%s",
			Name,
			TypeName,
			IsReferenced ? "" : " (unused)");
	if (ExtraInfo != NULL)
	{
		BufferP += snprintf(BufferP, BufferEnd - BufferP,
				"\n%s  %s Profile=%s",
				IndentPrefix,
				ExtraInfo,
				cgGetProfileString(Profile));
	}
	if (Type == CG_ARRAY)
	{
		int ArrayDimension = cgGetArrayDimension(Param);
		CGtype ArrayType = cgGetArrayType(Param);
		char ArraySize[256] = "";
		for (int i = 0; i < ArrayDimension; ++i)
		{
			snprintf(
					ArraySize + strlen(ArraySize),
					sizeof ArraySize - strlen(ArraySize) - 1,
					"[%i]",
					(int)cgGetArraySize(Param, i));
			ArraySize[sizeof ArraySize - 1] = 0;
		}
		BufferP += snprintf(BufferP, BufferEnd - BufferP,
				"\n%s  ArrayDimension=%i ArraySize=%s ArrayType=%s",
				IndentPrefix, ArrayDimension, ArraySize,
				cgGetTypeString(ArrayType));
	}
	if (Type != CG_STRUCT && IsReferenced)
	{
		CGresource BaseResource = cgGetParameterBaseResource(FirstElementParam);
		CGresource Resource = cgGetParameterResource(FirstElementParam);
		int ResourceIndex = cgGetParameterResourceIndex(FirstElementParam);
		BufferP += snprintf(BufferP, BufferEnd - BufferP,
				"\n%s  BaseResource=%s Resource=%s ResourceIndex=%i",
				IndentPrefix,
				cgGetResourceString(BaseResource),
				cgGetResourceString(Resource),
				ResourceIndex);
	}
	CGenum Direction = cgGetParameterDirection(Param);
	CGenum Variablility = cgGetParameterVariability(Param);
	BufferP += snprintf(BufferP, BufferEnd - BufferP,
			"\n%s  Direction=%s Variablility=%s",
			IndentPrefix,
			cgGetEnumString(Direction),
			cgGetEnumString(Variablility));
	if (Type != CG_STRUCT)
	{
		const char *Semantic = cgGetParameterSemantic(FirstElementParam);
		if (Semantic != NULL && Semantic[0])
		{
			BufferP += snprintf(BufferP, BufferEnd - BufferP,
					"\n%s  Semantic=%s",
					IndentPrefix,
					Semantic);
#if defined(PS3)
			CGprofile VertexProgramProfile = cgGLGetLatestProfile(CG_GL_VERTEX);
			bool IsVertexProgram = VertexProgramProfile == Profile;
			if (IsVertexProgram)
			{
				char MappedSemantic[256];
				GetMappedSemantic(
						FirstElementParam,
						MappedSemantic,
						sizeof MappedSemantic);
				MappedSemantic[sizeof MappedSemantic - 1] = 0;
				BufferP += snprintf(BufferP, BufferEnd - BufferP,
						" MappedSemantic=%s", MappedSemantic);
			}
#endif
		}
	}
	if (BufferP < BufferEnd)
		*BufferP++ = ']';
	if (BufferP < BufferEnd)
		*BufferP = 0;
	else
		BufferEnd[-1] = 0;
	return Buffer;
}

static char catchShaderName[128] = "Terrain@TerrainLayerPS(RT4002000001)(LT1)(ps_2_0)";

static void CatchShaderNameBreak(const char *shaderName)
{
	printf("CatchShaderNameBreak(shaderName = \"%s\")\n", shaderName);
}

// Debug function dumping all shader parameters of a shader program.
static uint32 DumpShaderProgram(
		CGprogram Prog,
		CGparameter Param = NULL,
		int Indent = 0,
		uint32 Flags = 0)
{
	char ParamDescription[1024];
	CGprofile Profile = cgGetProgramProfile(Prog);
	CGprofile VertexProgramProfile = cgGLGetLatestProfile(CG_GL_VERTEX);
	bool IsVertexProgram = VertexProgramProfile == Profile;
	const char *shaderName = "<unknown>";
	std::map<CGprogram, string>::const_iterator element
		= shaderNameMap.find(Prog);
	bool top = Param == NULL;
	char IndentPrefix[256];
	Indent = min(Indent, (int)sizeof IndentPrefix - 1);
	memset(IndentPrefix, ' ', Indent);
	IndentPrefix[Indent] = 0;

	if (element != shaderNameMap.end())
		shaderName = element->second.c_str();

  if (top && shaderName && !strcmp(shaderName, catchShaderName))
		CatchShaderNameBreak(shaderName);

	if (top)
	{
		printf("[CGprogram 0x%lx/%s %s\n",
				(unsigned long)(UINT_PTR)Prog,
				cgGetProfileString(Profile),
				shaderName);
		assert(Param == NULL);
		Param = cgGetFirstParameter(Prog, CG_PROGRAM);
	}
	// Dump all parameters.
	while (Param != NULL)
	{
		CGtype Type = cgGetParameterType(Param);
		if (Type == CG_STRUCT || cgIsParameterReferenced(Param))
		{
			printf("%s  %s\n",
					IndentPrefix, 
					DescribeProgramParameter(
						Param,
						ParamDescription,
						sizeof ParamDescription,
						NULL,
						Indent + 2));
		}
		if (Type == CG_STRUCT)
		{
			CGparameter FirstStructParam = cgGetFirstStructParameter(Param);
			if (FirstStructParam != NULL)
				Flags |= DumpShaderProgram(Prog, FirstStructParam, Indent + 2);
		}

		// Referenced uniform parameters with an explicit register binding are of
		// special interest, so we'll set flags 0x1 (VP) and 0x2 (FP) to indicate
		// their presence.
		if (cgIsParameterReferenced(Param)
				&& Type != CG_STRUCT && Type != CG_ARRAY
				&& cgGetParameterVariability(Param) == CG_UNIFORM
				&& cgGetParameterSemantic(Param)[0] == 'C')
		{
			if (IsVertexProgram)
				Flags |= 1;
			else
				Flags |= 2;
		}

		Param = cgGetNextParameter(Param);
	}
	if (top)
	{
		printf("%s  Flags=0x%x]\n", IndentPrefix, (unsigned)Flags);
	}
	return Flags;
}
#endif

static CGparameter GetNamedParameterNocase_Search(
		CGprogram pr,
		const char *name,
		CGparameter param
		)
{
	CGparameter p = NULL;

	do
	{
		if (!stricmp(name, cgGetParameterName(param)))
			return param;
		if (cgGetParameterType(param) == CG_STRUCT)
		{
			p = GetNamedParameterNocase_Search(pr, name,
					cgGetFirstStructParameter(param));
			if (p) return p;
		}
	} while ((param = cgGetNextParameter(param)));
	return NULL;
}

// Get a program parameter by name (case insensitive).
static CGparameter GetNamedParameterNocase(CGprogram pr, const char *name)
{
	CGparameter param = cgGetNamedParameter(pr, name);

	if (param == NULL)
	{
		// We'll flip the case of the first character of the parameter name and
		// try again.
		char tmp[strlen(name) + 1];
		strcmp(tmp, name);
		if (isupper(tmp[0]))
			tmp[0] = tolower(tmp[0]);
		else if (islower(tmp[0]))
			tmp[0] = toupper(tmp[0]);
		param = cgGetNamedParameter(pr, tmp);
	}
	if (param == NULL)
	{
		// We'll do a case-insensitive search for the specified parameter.
		param = GetNamedParameterNocase_Search(pr, name,
				cgGetFirstParameter(pr, CG_PROGRAM));
	}
	return param;
}

#if defined(PS3)
// FIXME:
// The following is a global cache of shader programs. This will be
// removed as soon as SONY fixes the broken cgGetProgramString()
// implementation.
struct ShaderProgram
{
	byte *data;
	size_t length;
	ShaderProgram(void) { data = NULL; length = 0; }
	ShaderProgram(const ShaderProgram& s) { init(s.data, s.length); }
	ShaderProgram(void *data, size_t length) { init(data, length); }
	~ShaderProgram() { if (data) delete[] data; }
	ShaderProgram& operator=(const ShaderProgram& s)
	{
		if (data) delete[] data;
		init(s.data, s.length);
		return *this;
	}
private:
	void init(void *data, size_t length)
	{
		if (length)
		{
			this->data = new byte[length];
			memcpy(this->data, data, length);
		} else
		{
			this->data = NULL;
		}
		this->length = length;
	}
};
static std::map<CGprogram, ShaderProgram> shaderMap;
static std::map<CGprogram, string> semanticMap;
static void StoreShaderProgram(
		CGprogram prog,
		void *data,
		size_t length,
		const char *shaderName
		)
{
	const uint8 *data8 = reinterpret_cast<const uint8 *>(data);
	assert((data8[0] & 0x7f) == 0);
	size_t progSize = (data8[1] << 16) | (data8[2] << 8) | data8[3];
	bool hasSemanticMap = (data8[0] & 0x80) == 0x80;

	shaderMap[prog] = ShaderProgram(data, length);
#if defined(_DEBUG)
	shaderNameMap[prog] = shaderName ? shaderName : "<unknown>";
#endif
	if (hasSemanticMap)
	{
		assert(length >= progSize + 8);
		data8 += 4 + progSize;
		assert(data8[0] == 1);
		size_t mapSize = (data8[1] << 16) | (data8[2] << 8) | data8[3];
		char map[mapSize + 1];
		assert(length == progSize + mapSize + 8);
		memcpy(map, data8 + 4, mapSize);
		map[mapSize] = 0;
		semanticMap[prog] = map;
	}
	else
	{
		assert(length == progSize + 4);
	}
}
void *GetShaderProgram(CGprogram prog, size_t &length)
{
	ShaderProgram &s = shaderMap[prog];
	length = s.length;
	return s.data;
}
void *GetShaderProgram(CGprogram prog)
{
	return shaderMap[prog].data;
}
void ClearShaderProgram(CGprogram prog)
{
	shaderMap.erase(prog);
}
void StoreMappedSemantics(
		CGprogram prog,
		const void *data,
		const char *shaderName
		)
{
	const uint8 *data8 = reinterpret_cast<const uint8 *>(data);
	assert((data8[0] & 0x7f) == 0);
	size_t progSize = (data8[1] << 16) | (data8[2] << 8) | data8[3];
	bool hasSemanticMap = (data8[0] & 0x80) == 0x80;

#if defined(_DEBUG)
	shaderNameMap[prog] = shaderName ? shaderName : "<unknown>";
#endif
	if (hasSemanticMap)
	{
		data8 += 4 + progSize;
		assert(data8[0] == 1);
		size_t mapSize = (data8[1] << 16) | (data8[2] << 8) | data8[3];
		char map[mapSize + 1];
		memcpy(map, data8 + 4, mapSize);
		map[mapSize] = 0;
		semanticMap[prog] = map;
	}
}
const char *GetMappedSemantic(
		CGparameter param,
		char *buffer,
		size_t bufferSize
		)
{
	CGprogram prog = cgGetParameterProgram(param);
	std::map<CGprogram, string>::const_iterator element
		= semanticMap.find(prog);
	if (element == semanticMap.end())
	{
		const char *semantic = cgGetParameterSemantic(param);
		strncpy(buffer, semantic, bufferSize);
		buffer[bufferSize - 1] = 0;
	}
	else
	{
		const char *map = element->second.c_str();
		int index = GetVSParamRegisterIndex(param);
		assert(index >= 0 && index < 16);
		const char *p = map, *q;
		for (int i = 0; i < index; ++i)
		{
			p = strchr(p, ';');
			assert(p != NULL);
			++p;
		}
		q = strchr(p, ';');
		assert(q != NULL);
		if (p == q)
		{
			const char *semantic = cgGetParameterSemantic(param);
			strncpy(buffer, semantic, bufferSize);
			buffer[bufferSize - 1] = 0;
		}
		else
		{
			memcpy(buffer, p, min(bufferSize - 1, q - p));
			buffer[min(bufferSize - 1, q - p)] = 0;
		}
	}
	return buffer;
}
#endif

IDirect3D9::~IDirect3D9()
{
}

int IDirect3D9::Release()
{
  return 0;
}

#if defined(PS3) || defined(LINUX)
static void LoadingScreen();
#endif

#if !defined(PS3) && !defined(LINUX)
bool IDirect3D9::LoadOpenGLLibrary()
{
  m_hLibHandle = ::LoadLibrary("OpenGL32.dll");
  if (!m_hLibHandle)
  {
    iLog->Log("Warning: Couldn't load library <%s>\n", "OpenGL32.dll");
    return false;
  }
  m_hLibHandleGDI = ::LoadLibrary("GDI32.dll");
  assert(m_hLibHandleGDI);
  return true;
}

bool IDirect3D9::FreeOpenGLLibrary()
{
  if (m_hLibHandle)
  {
    ::FreeLibrary((HINSTANCE)m_hLibHandle);
    m_hLibHandle = NULL;
  }
  if (m_hLibHandleGDI)
  {
    ::FreeLibrary((HINSTANCE)m_hLibHandleGDI);
    m_hLibHandleGDI = NULL;
  }

  // Free OpenGL function.
#define GL_EXT(name) SUPPORTS##name=0;
#define GL_PROC(ext,ret,func,parms) func = NULL;
#include "GLFuncs.h"
#undef GL_EXT
#undef GL_PROC

  return true;
}

//////////////////////////////////////////////////////////////////////
bool IDirect3D9::SetupPixelFormat(D3DFORMAT fmtBack, D3DFORMAT fmtZ, SRendContext *rc)
{
  iLog->LogToFile("SetupPixelFormat ...");

  PIXELFORMATDESCRIPTOR pfd;
  int pixelFormat;

  memset(&pfd, 0, sizeof(pfd));

  pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
  pfd.dwLayerMask = PFD_MAIN_PLANE;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cAlphaBits = 8;
  pfd.cDepthBits = 24;
  pfd.cAccumBits = 0;
  pfd.cStencilBits = 8;

  BOOL status = FALSE;

  if (m_RContexts.Num() && rc != m_RContexts[0])
    pixelFormat = m_RContexts[0]->m_PixFormat;
  else
    pixelFormat = ChoosePixelFormat(m_pCurrContext->m_hDC, &pfd);
  m_pCurrContext->m_PixFormat = pixelFormat;

  if (!pixelFormat)
  {
    iLog->LogToFile("Cannot ChoosePixelFormat \n");
    return (false);
  }

  iLog->LogToFile("Selected cColorBits = %d, cDepthBits = %d, Stencilbits=%d\n", pfd.cColorBits, pfd.cDepthBits,pfd.cStencilBits);

  iLog->LogToFile("GL PIXEL FORMAT:\n");

  memset(&pfd, 0, sizeof(pfd));
  if (DescribePixelFormat(m_pCurrContext->m_hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd))
  {
    if (pfd.dwFlags & PFD_DRAW_TO_WINDOW)   iLog->LogToFile("PFD_DRAW_TO_WINDOW\n");
    if (pfd.dwFlags & PFD_DRAW_TO_BITMAP)   iLog->LogToFile("PFD_DRAW_TO_BITMAP\n");

    if (pfd.dwFlags & PFD_SUPPORT_GDI)      iLog->LogToFile("PFD_SUPPORT_GDI\n");

    if (pfd.dwFlags & PFD_SUPPORT_OPENGL)   iLog->LogToFile("PFD_SUPPORT_OPENGL\n");
    if (pfd.dwFlags & PFD_GENERIC_ACCELERATED)  iLog->LogToFile("PFD_GENERIC_ACCELERATED\n");

    if (pfd.dwFlags & PFD_NEED_PALETTE)     iLog->LogToFile("PFD_NEED_PALETTE\n");
    if (pfd.dwFlags & PFD_NEED_SYSTEM_PALETTE)  iLog->LogToFile("PFD_NEED_SYSTEM_PALETTE\n");
    if (pfd.dwFlags & PFD_DOUBLEBUFFER)     iLog->LogToFile("PFD_DOUBLEBUFFER\n");
    if (pfd.dwFlags & PFD_STEREO)       iLog->LogToFile("PFD_STEREO\n");
    if (pfd.dwFlags & PFD_SWAP_LAYER_BUFFERS) iLog->LogToFile("PFD_SWAP_LAYER_BUFFERS\n");
    iLog->LogToFile("Pixel format %d:\n",pixelFormat);
    iLog->LogToFile("Pixel Type: %d\n",pfd.iPixelType);
    iLog->LogToFile("Bits: Color=%d R=%d G=%d B=%d A=%d\n", pfd.cColorBits, pfd.cRedBits, pfd.cGreenBits, pfd.cBlueBits, pfd.cAlphaBits);
    iLog->LogToFile("Bits: Accum=%d Depth=%d Stencil=%d\n", pfd.cAccumBits, pfd.cDepthBits, pfd.cStencilBits);

    iLog->LogToFile("Using cColorBits = %d, cDepthBits = %d, Stencilbits=%d\n", pfd.cColorBits, pfd.cDepthBits,pfd.cStencilBits);

    if (pfd.dwFlags & PFD_GENERIC_FORMAT)
    {
      iLog->LogToFile("PFD_GENERIC_FORMAT\n");
      return (false);
    }

    gRenDev->m_abpp = pfd.cAlphaBits;
    gRenDev->m_sbpp = pfd.cStencilBits;
    gRenDev->m_cbpp = pfd.cColorBits;
    gRenDev->m_zbpp = pfd.cDepthBits;
  }
  else
  {
    iLog->LogToFile("Warning: Cannot DescribePixelFormat \n");
    return (false);
  }

  // set the format to closest match
  SetPixelFormat(m_pCurrContext->m_hDC, pixelFormat, &pfd);

  return true;
}

#elif defined(PS3)

bool IDirect3D9::PS3_ResetContext(int nFSAA)
{
  psglResetCurrentContext();

  int nWidth = gRenDev->GetWidth();
  int nHeight = gRenDev->GetHeight();

  glViewport(0, 0, nWidth, nHeight);
	m_CurScissorRect.left		= 0;
	m_CurScissorRect.top		= 0;
	m_CurScissorRect.right	= nWidth;
	m_CurScissorRect.bottom = nHeight;
  glScissor(0, 0, nWidth, nHeight);
	glEnable(GL_SCISSOR_TEST);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glEnable(GL_DEPTH_TEST);
  glFlush();
	return true;
}

bool IDirect3D9::PS3_MakeNewContext(int nFSAA)
{
  if (m_pCurrContext->m_cgContext)
    cgDestroyContext(m_pCurrContext->m_cgContext);

  PSGLcontext* oldContext = psglGetCurrentContext();
  if (oldContext)
  {
    psglDestroyContext(oldContext);
  }

  m_pCurrContext->m_psglContext = psglCreateContext();
  if (!m_pCurrContext->m_psglContext)
    return false;
  psglMakeCurrent(m_pCurrContext->m_psglContext, m_pCurrContext->m_psglDevice);

  //psglLoadShaderLibrary(SYS_APP_HOME "/shaders.bin");

  bool bRes = PS3_ResetContext(nFSAA);

  return bRes;
}

bool IDirect3D9::PS3_SetupWindow(int nFSAA)
{
	int width = gRenDev->GetWidth(), height = gRenDev->GetHeight();
	bool interlaced = false;
#if defined(CELL_ARCH_DEH)
	bool vgaOutput = true;
#endif

#if CELL_SDK_VERSION < 0x082000
  // Setup window
  PSGLbufferParameters params=
  {
		width, height,
		colorBits:24,
		alphaBits:8,
		depthBits:24,
		stencilBits:8,
    deviceType:PSGL_DEVICE_TYPE_AUTO,
    TVStandard:PSGL_TV_STANDARD_NONE,
    TVFormat:PSGL_TV_FORMAT_AUTO,
		bufferingMode: PSGL_BUFFERING_MODE_DOUBLE,
    nFSAA
  };
	if ((width == 640 && height == 480)
			|| (width == 800 && height == 600)
			|| (width == 1024 && height == 768)
			|| (width == 1280 && height == 1024)
			|| (width == 1600 && height == 1200)
			|| (width == 1920 && height == 1200))
	{
#if !defined(CELL_ARCH_DEH)
		params.deviceType = PSGL_DEVICE_TYPE_VGA;
#else
		fprintf(stderr,
				"VGA screen resolution %ix%i not supported by DEH\n",
				width, height);
		iLog->LogError(
				"VGA screen resolution %ix%i not supported",
				width, height);
		return false;
#endif
	} else if (width == 720 && height == 480)
	{
		params.deviceType = PSGL_DEVICE_TYPE_TV;
#if defined(CELL_ARCH_DEH)
		if (vgaOutput)
		{
			iLog->LogWarning(
					"VGA/VESA output not supported for screen resolution %ix%i",
					width, height);
		}
#endif
		if (interlaced)
			params.TVStandard = PSGL_TV_STANDARD_HD480I;
		else
			params.TVStandard = PSGL_TV_STANDARD_HD480P;
	} else if (width == 720 && height == 576)
	{
		params.deviceType = PSGL_DEVICE_TYPE_TV;
#if defined(CELL_ARCH_DEH)
		if (vgaOutput)
		{
			iLog->LogWarning(
					"VGA/VESA output not supported for screen resolution %ix%i",
					width, height);
		}
#endif
		if (interlaced)
			params.TVStandard = PSGL_TV_STANDARD_HD576I;
		else
			params.TVStandard = PSGL_TV_STANDARD_HD576P;
	} else if (width == 1280 && height == 720)
	{
		params.deviceType = PSGL_DEVICE_TYPE_TV;
#if defined(CELL_ARCH_DEH)
		if (vgaOutput)
		{
			params.TVStandard = PSGL_TV_STANDARD_1280x720_ON_VESA_1280x768;
			//params.TVStandard = PSGL_TV_STANDARD_1280x720_ON_VESA_1280x1024;
		}
		else
#endif
		params.TVStandard = PSGL_TV_STANDARD_HD720P;
	} else if (width == 1920 && height == 1080)
	{
		params.deviceType = PSGL_DEVICE_TYPE_TV;
#if defined(CELL_ARCH_DEH)
		if (vgaOutput)
		{
			params.TVStandard = PSGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200;
		}
		else
#endif
		if (interlaced)
			params.TVStandard = PSGL_TV_STANDARD_HD1080I;
		else
			params.TVStandard = PSGL_TV_STANDARD_HD1080P;
	}
  m_pCurrContext->m_psglDevice = psglCreateDevice(&params);
#else
	// Starting with SDK 0.8.3, we'll use the new device initialization call.
	// The old call is deprecated now.
	PSGLdeviceParameters params;
	memset(&params, 0, sizeof params);
	params.enable =
		PSGL_DEVICE_PARAMETERS_COLOR_FORMAT
			| PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT
			| PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE
			| PSGL_DEVICE_PARAMETERS_TV_STANDARD
			| PSGL_DEVICE_PARAMETERS_CONNECTOR
			| PSGL_DEVICE_PARAMETERS_BUFFERING_MODE
			| PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
	params.colorFormat = GL_ARGB_SCE;
	params.depthFormat = GL_DEPTH_COMPONENT24;
	params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;
	if (width == 720 && height == 480)
	{
		params.TVStandard = PSGL_TV_STANDARD_HD480I;
		params.connector = PSGL_DEVICE_CONNECTOR_COMPONENT;
	}
	else if (width == 720 && height == 576)
	{
		params.TVStandard = PSGL_TV_STANDARD_HD576I;
		params.connector = PSGL_DEVICE_CONNECTOR_COMPONENT;
	}
	else if (width == 1280 && height == 720)
	{
		params.TVStandard = PSGL_TV_STANDARD_1280x720_ON_VESA_1280x768;
		params.connector = PSGL_DEVICE_CONNECTOR_VGA;
	}
	else if (width == 1920 && height == 1080)
	{
		params.TVStandard = PSGL_TV_STANDARD_1920x1080_ON_VESA_1920x1200;
		params.connector = PSGL_DEVICE_CONNECTOR_VGA;
	}
	else
	{
		iLog->LogWarning(
				"screen resolution %ix%i not supported",
				width, height);
		params.TVStandard = PSGL_TV_STANDARD_NONE;
		params.connector = PSGL_DEVICE_CONNECTOR_NONE;
	}
	params.width = width;
	params.height = height;
	params.bufferingMode = PSGL_BUFFERING_MODE_DOUBLE;
  m_pCurrContext->m_psglDevice = psglCreateDeviceExtended(&params);
#endif
	gScreenWidth = width;
	gScreenHeight = height;

  bool rv = PS3_MakeNewContext(nFSAA);
	if (rv)
	{
		LoadingScreen();
		psglSwap();
	}
	return rv;
}
#endif

#if defined(LINUX)
bool IDirect3D9::LINUX_SetupWindow()
{
	int width = gRenDev->GetWidth(), height = gRenDev->GetHeight();
	bool fullscreen = gRenDev->m_CVFullScreen->GetIVal() != 0;

	gScreenWidth = width;
	gScreenHeight = height;

	// Init GL context.
	glViewport(0, 0, width, height);
	m_CurScissorRect.left = 0;
	m_CurScissorRect.top = 0;
	m_CurScissorRect.right = width;
	m_CurScissorRect.bottom = height;
  glScissor(0, 0, width, height);
	glEnable(GL_SCISSOR_TEST);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glEnable(GL_DEPTH_TEST);
	if (fullscreen) {
		LoadingScreen();
		SDL_GL_SwapBuffers();
	}
	return true;
}
#endif

#ifdef PS3
static void PS3_ErrorCallback(void)
{
	CGerror error = cgGetError();
	const char *errorString = cgGetErrorString(error);

	fprintf(stderr, "PSGL Cg Error: %s [%i]\n", errorString, (int)error);
	iLog->LogError("PSGL Cg Error: %s", errorString);
}

static void (*PS3_ReportCallback_Chain)(GLenum, GLuint, const char *) = NULL;

static void PS3_Report(const char *reportId, const char *message)
{
	if (iLog)
		iLog->LogWarning("PSGL: %s: %s", reportId, message);
}

static void PS3_ReportCallback(
		GLenum report,
		GLuint reportClassMask,
		const char *message
		)
{
	char reportIdBuffer[100];
	const char *reportId = NULL;
	const static struct {
		GLenum report;
		const char *id;
	} reportMap[] = {
		{ PSGL_REPORT_UNKNOWN, "UNKNOWN" },
		{ PSGL_REPORT_VERSION, "VERSION" },
		{ PSGL_REPORT_DEBUG, "DEBUG" },
		{ PSGL_REPORT_ASSERT, "ASSERT" },
		{ PSGL_REPORT_GL_ERROR, "GL_ERROR" },
		{ PSGL_REPORT_MISSING_STATE, "MISSING_STATE" },
		{ PSGL_REPORT_VERTEX_SLOW_PATH, "VERTEX_SLOW_PATH" },
		{ PSGL_REPORT_VERTEX_DATA_WARNING, "VERTEX_DATA_WARNING" },
		{ PSGL_REPORT_COPY_TEXTURE_SLOW_PATH, "COPY_TEXTURE_SLOW_PATH" },
		{ PSGL_REPORT_COPY_TEXTURE_WARNING, "COPY_TEXTURE_WARNING" },
		{ PSGL_REPORT_TEXTURE_COPY_BACK, "TEXTURE_COPY_BACK" },
		{ PSGL_REPORT_TEXTURE_REALLOC, "TEXTURE_REALLOC" },
		{ PSGL_REPORT_FP32_FILTERING, "FP32_FILTERING" },
		{ PSGL_REPORT_TEXTURE_INCOMPLETE, "TEXTURE_INCOMPLETE" },
		{ PSGL_REPORT_FRAMEBUFFER_INCOMPLETE, "FRAMEBUFFER_INCOMPLETE" },
		{ PSGL_REPORT_FRAMEBUFFER_UNSUPPORTED, "FRAMEBUFFER_UNSUPPORTED" },
		{ PSGL_REPORT_CG_ERROR, "CG_ERROR" },
		{ 0, NULL }
	};

	for (int i = 0; reportMap[i].id; ++i)
	{
		if (report == reportMap[i].report)
		{
			reportId = reportMap[i].id;
			break;
		}
	}
	if (!reportId)
	{
		snprintf(reportIdBuffer, sizeof reportIdBuffer,
				"report%d", (int)report);
		reportId = reportIdBuffer;
	}
	{ const char *p, *q;
		for (p = ps3_report_ignore;;)
		{
			while (*p && (isspace(*p) || *p == ',')) ++p;
			if (!*p) break;
			for (q = p; *p && *p != ',' && !isspace(*p); ++p);
			if (!strncasecmp(reportId, q, p - q) || !strncasecmp("all", q, p - q))
			{
				// Ignore.
				return;
			}
		}
	}

	if (PS3_ReportCallback_Chain)
		PS3_ReportCallback_Chain(report, reportClassMask, message);
	printf("PS3_ReportCallback(report = %s, reportClassMask = 0x%x, \"%s\")\n",
			reportId, (unsigned)reportClassMask, message);
	PS3_Report(reportId, message);
}
#endif

bool IDirect3D9::CreateRContext(SRendContext *rc, D3DFORMAT fmtBack, D3DFORMAT fmtZ, bool bAllowFSAA)
{
  bool statusFSAA = false;
  int pixelFormat = 0;

  m_pCurrContext = rc;

  int nFSAA = bAllowFSAA ? CRenderer::CV_r_fsaa : 0;
  gRenDev->m_Features &= ~RFT_SUPPORTFSAA;
#if defined(PS3)
	PS3_SetupWindow(nFSAA);
	cgSetErrorCallback(PS3_ErrorCallback);
#elif defined(LINUX)
	LINUX_SetupWindow();
#else
  if (bAllowFSAA && m_RContexts.Num() && rc == m_RContexts[0])
  {
  }

  if (statusFSAA)
  {
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    DescribePixelFormat(m_pCurrContext->m_hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
    SetPixelFormat(m_pCurrContext->m_hDC, pixelFormat, &pfd);
    rc->m_PixFormat = pixelFormat;

    m_pCurrContext->m_hRC = pwglCreateContext(m_pCurrContext->m_hDC);
    m_pCurrContext->m_bFSAAWasSet = true;

    pwglMakeCurrent(m_pCurrContext->m_hDC, m_pCurrContext->m_hRC);

    glEnable(GL_MULTISAMPLE_ARB);
  }
  else
  {
    m_pCurrContext->m_bFSAAWasSet = false;
    if (!SetupPixelFormat(fmtBack, fmtZ, rc))
      return false;

    rc->m_hRC = pwglCreateContext(rc->m_hDC);
  }
  if (!rc->m_hRC)
  {
    iLog->Log("Warning: wglCreateContext failed\n");
    return false;
  }

  pwglMakeCurrent(rc->m_hDC, rc->m_hRC);
  if (m_RContexts[0] != rc)
  {
    pwglShareLists(m_RContexts[0]->m_hRC, rc->m_hRC);
    CRenderer::CV_r_vsync = 1;
  }

#endif

  m_pCurrContext->m_cgContext = cgCreateContext();
  scgContext = m_pCurrContext->m_cgContext;

  return true;
}

#if !defined(PS3) && !defined(LINUX)

// D3D system functions
//====================================================================

typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC) ();
static PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
static PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT;

#define GET_GL_PROC(functype,funcname) \
  static functype funcname = (functype)pwglGetProcAddress(#funcname);\
  assert(funcname);\

#endif

bool IDirect3D9::FindExt(const char* Name)
{
  char *str = (char*)glGetString(GL_EXTENSIONS);
  if (strstr(str, Name))
    return true;

#if !defined(LINUX)
  GET_GL_PROC(PFNWGLGETEXTENSIONSSTRINGARBPROC,wglGetExtensionsStringARB);
  if(wglGetExtensionsStringARB)
  {
    const char * wglExt = wglGetExtensionsStringARB(m_RContexts[0]->m_hDC);
    if (wglExt)
      return (strstr(wglExt, Name) != NULL);
  }
#endif

  return false;
}

#if !defined(LINUX)
void IDirect3D9::FindProc(void*& ProcAddress, char* Name, char* SupportName, byte& Supports, bool bAllowExt)
{
  if (Name[0] == 'p')
    Name = &Name[1];
  if(!ProcAddress)
    ProcAddress = GetProcAddress((HINSTANCE)m_hLibHandle, Name);
  if(!ProcAddress)
    ProcAddress = GetProcAddress((HINSTANCE)m_hLibHandleGDI, Name);
  if(!ProcAddress && Supports && bAllowExt)
    ProcAddress = pwglGetProcAddress(Name);
  if( !ProcAddress)
  {
    if(Supports)
      iLog->Log("Warning:   Missing function '%s' for '%s' support\n", Name, SupportName);
    Supports = 0;
  }
}
#endif

void IDirect3D9::FindProcs(bool bAllowExt)
{
#define GL_EXT(name) if(bAllowExt) SUPPORTS##name = FindExt(TEXT(#name)+1);
#if defined(LINUX)
#define GL_PROC(ext,ret,func,parms)
#else
#define GL_PROC(ext,ret,func,parms) FindProc(*(void**)&func, #func, #ext, SUPPORTS##ext, bAllowExt);
#endif
#include "GLFuncs.h"
#undef GL_EXT
#undef GL_PROC
}

IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
  IDirect3D9 *pD3D = new IDirect3D9;

  iConsole->Register("GL_3DFX_gamma_control", &CV_gl_3dfx_gamma_control, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_texture_compression", &CV_gl_arb_texture_compression, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_multitexture", &CV_gl_arb_multitexture, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_pbuffer", &CV_gl_arb_pbuffer, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_pixel_format", &CV_gl_arb_pixel_format, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_buffer_region", &CV_gl_arb_buffer_region, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_render_texture", &CV_gl_arb_render_texture, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_multisample", &CV_gl_arb_multisample, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_vertex_program", &CV_gl_arb_vertex_program, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_vertex_buffer_object", &CV_gl_arb_vertex_buffer_object, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_fragment_program", &CV_gl_arb_fragment_program, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ARB_texture_env_combine", &CV_gl_arb_texture_env_combine, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_swapcontrol", &CV_gl_ext_swapcontrol, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_bgra", &CV_gl_ext_bgra, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_depth_bounds_test", &CV_gl_ext_depth_bounds_test, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_multi_draw_arrays", &CV_gl_ext_multi_draw_arrays, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_Compiled_Vertex_Array", &CV_gl_ext_compiled_vertex_array, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_texture_cube_map", &CV_gl_ext_texture_cube_map, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_separate_specular_color", &CV_gl_ext_separate_specular_color, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_secondary_color", &CV_gl_ext_secondary_color, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_paletted_texture", &CV_gl_ext_paletted_texture, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_stencil_two_side", &CV_gl_ext_stencil_two_side, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_stencil_wrap", &CV_gl_ext_stencil_wrap, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_texture_filter_anisotropic", &CV_gl_ext_texture_filter_anisotropic, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_texture_env_add", &CV_gl_ext_texture_env_add, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_texture_env_combine", &CV_gl_ext_texture_env_combine, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_EXT_texture_rectangle", &CV_gl_ext_texture_rectangle, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_HP_occlusion_test", &CV_gl_hp_occlusion_test, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_fog_distance", &CV_gl_nv_fog_distance, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texture_env_combine4", &CV_gl_nv_texture_env_combine4, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_point_sprite", &CV_gl_nv_point_sprite, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_vertex_array_range", &CV_gl_nv_vertex_array_range, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_register_combiners", &CV_gl_nv_register_combiners, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_register_combiners2", &CV_gl_nv_register_combiners2, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texgen_reflection", &CV_gl_nv_texgen_reflection, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texgen_emboss", &CV_gl_nv_texgen_emboss, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_vertex_program", &CV_gl_nv_vertex_program, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_vertex_program3", &CV_gl_nv_vertex_program3, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texture_rectangle", &CV_gl_nv_texture_rectangle, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texture_shader", &CV_gl_nv_texture_shader, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texture_shader2", &CV_gl_nv_texture_shader2, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_texture_shader3", &CV_gl_nv_texture_shader3, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_fragment_program", &CV_gl_nv_fragment_program, 0, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_NV_multisample_filter_hint", &CV_gl_nv_multisample_filter_hint, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_SGIX_depth_texture", &CV_gl_sgix_depth_texture, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_SGIX_shadow", &CV_gl_sgix_shadow, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_SGIS_generate_mipmap", &CV_gl_sgis_generate_mipmap, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_SGIS_texture_lod", &CV_gl_sgis_texture_lod, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ATI_separate_stencil", &CV_gl_ati_separate_stencil, 1, VF_REQUIRE_APP_RESTART);
  iConsole->Register("GL_ATI_fragment_shader", &CV_gl_ati_fragment_shader, 1, VF_REQUIRE_APP_RESTART);

  if (!pD3D->m_RContexts.Num())
  {
    SRendContext *rc = new SRendContext;
    pD3D->m_RContexts.AddElem(rc);
  }
  SRendContext *rc = pD3D->m_RContexts[0];

#if !defined(PS3) && !defined(LINUX)
  if (!pD3D->LoadOpenGLLibrary())
  {
    iLog->Log("Error: Could not open OpenGL library\n");
exr:
    pD3D->FreeOpenGLLibrary();
    return NULL;
  }
  rc->m_Glhwnd = (HWND)gcpRendD3D->GetHWND();
  rc->m_hDC = GetDC(rc->m_Glhwnd);

  // Find functions.
  SUPPORTS_GL = 1;
  pD3D->FindProcs(false);
  if(!SUPPORTS_GL)
  {
    iLog->Log("Error: Library <%s> isn't OpenGL library\n", "OpenGL32.dll");
    goto exr;
  }
#elif defined(PS3)
  // psgl setup
	PS3_ReportCallback_Chain = psglGetReportFunction();
	psglSetReportFunction(PS3_ReportCallback);

  PSGLinitOptions options;
	memset(&options, 0, sizeof options);
  options.enable = PSGL_INIT_MAX_SPUS
		| PSGL_INIT_INITIALIZE_SPUS
		| PSGL_INIT_PERSISTENT_MEMORY_SIZE
#if defined(CELL_ARCH_DEH)
		| PSGL_INIT_HOST_MEMORY_SIZE
		//| PSGL_INIT_FIFO_SIZE
#endif
		;
  options.maxSPUs = 1;
  options.initializeSPUs = GL_FALSE;
  options.persistentMemorySize = 96 << 20;
#if defined(CELL_ARCH_DEH)
	options.hostMemorySize = 0;
	//options.fifoSize = 1 << 20;
#endif
  psglInit(&options);
#else
	// Linux initialization
	// Init SDL video system.
	// Note: The SDL API must be initialized before SDL_InitSubSystem() may be
	// called. A good place to call SDL_Init() is the launcher.
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		iLog->LogError("SDL init failed: %s", SDL_GetError());
		return false;
	}
	int width = gRenDev->GetWidth(), height = gRenDev->GetHeight();
	unsigned flags = SDL_OPENGL;
	bool fullscreen = gRenDev->m_CVFullScreen->GetIVal() != 0;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;
	gContext = SDL_SetVideoMode(width, height, 0, flags);
	if (gContext == NULL)
	{
		iLog->LogError("SDL_SetVideoMode failed: %s", SDL_GetError());
		return false;
	}
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif
  pD3D->m_pCurrContext = rc;

  pD3D->m_nNumAdapters = 1;
  ZeroStruct(pD3D->m_AdapterIdent);
  strcpy(pD3D->m_AdapterIdent.Driver, "Crytek OpenGL driver");
  strcpy(pD3D->m_AdapterIdent.Description, "Crytek OpenGL driver");
#if defined(PS3)
  strcpy(pD3D->m_AdapterIdent.DeviceName, "PS3_DEVICE");
#elif defined(LINUX)
  strcpy(pD3D->m_AdapterIdent.DeviceName, "LINUX_DEVICE");
#else
  strcpy(pD3D->m_AdapterIdent.DeviceName, "OPENGL_DEVICE");
#endif
  pD3D->m_AdapterIdent.VendorId = 0xffff;
  pD3D->m_AdapterIdent.DeviceId = 0xffff;

#if defined(PS3)
	DEVMODE ps3DevModes[] = {
#if CELL_SDK_VERSION < 0x080000
		// VGA Modes
		{ sizeof(DEVMODE), 32, 640, 480, 60 },
		{ sizeof(DEVMODE), 32, 800, 600, 60 },
		{ sizeof(DEVMODE), 32, 1024, 768, 60 },
		{ sizeof(DEVMODE), 32, 1280, 1024, 60 },
		{ sizeof(DEVMODE), 32, 1600, 1200, 60 },
#endif
		// HDTV Modes
		{ sizeof(DEVMODE), 32, 720, 480, 60 },
		{ sizeof(DEVMODE), 32, 720, 576, 60 },
		{ sizeof(DEVMODE), 32, 1280, 720, 60 },
		{ sizeof(DEVMODE), 32, 1920, 1080, 60 },
	};
	for (int i = 0; i < sizeof ps3DevModes / sizeof(DEVMODE); ++i)
	{
		pD3D->m_VidModes.AddElem(ps3DevModes[i]);
	}
#elif defined(LINUX)
	// FIXME to be done
	DEVMODE devMode = { sizeof(DEVMODE), 32, 1024, 768, 60 };
	pD3D->m_VidModes.AddElem(devMode);
#else
  int nMode;
  for (nMode=0;;nMode++)
  {
    DEVMODE Tmp;
    ZeroMemory(&Tmp,sizeof(Tmp));
    Tmp.dmSize = sizeof(Tmp);

    if (!EnumDisplaySettings(NULL, nMode, &Tmp))
      break;

    pD3D->m_VidModes.AddElem(Tmp);
  }
#endif

  if (!pD3D->m_VidModes.Num())
  {
    iLog->Log("Warning: Cannot find at least one video mode available\n");
    return NULL;
  }
  ZeroMemory(&pD3D->m_DeviceCaps,sizeof(pD3D->m_DeviceCaps));

  pD3D->OpenGLExtensions2D3D9Caps();

  return pD3D;
}


HRESULT IDirect3D9::GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
{
  DEVMODE Tmp;
  ZeroMemory(&Tmp,sizeof(Tmp));
  Tmp.dmSize = sizeof(Tmp);

#if defined(PS3)
  Tmp.dmPelsWidth = gRenDev->GetWidth();
  Tmp.dmPelsHeight = gRenDev->GetHeight();
  Tmp.dmDisplayFrequency = 60;
#elif defined(LINUX)
	// XXX to be done
#else
  if (!EnumDisplaySettings(NULL, -1, &Tmp))
    return D3DERR_NOTAVAILABLE;
#endif
  pMode->Format = D3DFMT_X8R8G8B8;
  pMode->Width = Tmp.dmPelsWidth;
  pMode->Height = Tmp.dmPelsHeight;
  pMode->RefreshRate = Tmp.dmDisplayFrequency;

  return S_OK;
}

bool IDirect3D9::OpenGLExtensions2D3D9Caps()
{
  D3DCAPS9 *pCaps = &m_DeviceCaps;

  pCaps->DeviceType = D3DDEVTYPE_HAL;
  pCaps->DevCaps |= D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_PUREDEVICE;

#if !defined(PS3) && !defined(LINUX)
  if (SUPPORTS_GL_ARB_fragment_program)
    pCaps->PixelShaderVersion = D3DPS_VERSION(2,0);
  else
    pCaps->PixelShaderVersion = D3DPS_VERSION(1,1);

  if (SUPPORTS_GL_ARB_vertex_program)
    pCaps->VertexShaderVersion = D3DVS_VERSION(2,0);
  else
    pCaps->VertexShaderVersion = D3DVS_VERSION(1,1);
#else
  pCaps->PixelShaderVersion = D3DPS_VERSION(2,0);
  pCaps->VertexShaderVersion = D3DPS_VERSION(2,0);
#endif

#ifndef PS3
  int nClipPlanes;
  glGetIntegerv(GL_MAX_CLIP_PLANES, &nClipPlanes);
  pCaps->MaxUserClipPlanes = nClipPlanes;

  int nMaxtexSize;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &nMaxtexSize);
  iLog->Log(" OGL Max Texture size=%dx%d",nMaxtexSize, nMaxtexSize);
  pCaps->MaxTextureWidth = nMaxtexSize;
  pCaps->MaxTextureHeight = nMaxtexSize;
  pCaps->MaxTextureAspectRatio = nMaxtexSize;

  int nMaxAnisotropy;
  glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &nMaxAnisotropy);
  pCaps->MaxAnisotropy = nMaxAnisotropy;

  int nMaxTUnits; 
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &nMaxTUnits);
  pCaps->MaxSimultaneousTextures = nMaxTUnits;
  pCaps->MaxTextureBlendStages = nMaxTUnits;
#else
  pCaps->MaxUserClipPlanes = 6;
  pCaps->MaxTextureWidth = 2048;
  pCaps->MaxTextureHeight = 2048;
  pCaps->MaxTextureAspectRatio = 2048;
  pCaps->MaxAnisotropy = 16;
  pCaps->MaxSimultaneousTextures = 16;
  pCaps->MaxTextureBlendStages = 16;
#endif

  pCaps->MaxStreams = 16;
  pCaps->MaxStreamStride = 255;

  pCaps->NumSimultaneousRTs = 1;

  pCaps->TextureCaps |= D3DPTEXTURECAPS_MIPMAP |
                        D3DPTEXTURECAPS_PROJECTED;
#ifndef PS3
  if (SUPPORTS_GL_ARB_texture_cube_map)
    pCaps->TextureCaps |= D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP;
  if (SUPPORTS_GL_EXT_texture3D)
    pCaps->TextureCaps |= D3DPTEXTURECAPS_VOLUMEMAP;
  if (!SUPPORTS_GL_NV_texture_rectangle)
    pCaps->TextureCaps |= D3DPTEXTURECAPS_POW2;
#else
  pCaps->TextureCaps |= D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP;
#endif

  pCaps->TextureFilterCaps |= D3DPTFILTERCAPS_MINFPOINT |
                              D3DPTFILTERCAPS_MINFLINEAR |
                              D3DPTFILTERCAPS_MINFANISOTROPIC |
                              D3DPTFILTERCAPS_MAGFPOINT |
                              D3DPTFILTERCAPS_MAGFLINEAR |
                              D3DPTFILTERCAPS_MAGFANISOTROPIC;
                              D3DPTFILTERCAPS_MIPFPOINT |
                              D3DPTFILTERCAPS_MIPFLINEAR;

  pCaps->TextureOpCaps |= -1;

  pCaps->StencilCaps |= D3DSTENCILCAPS_KEEP |
                        D3DSTENCILCAPS_INCR |
                        D3DSTENCILCAPS_DECR;
#ifndef PS3
  if (SUPPORTS_GL_EXT_stencil_two_side)
    pCaps->StencilCaps |= D3DSTENCILCAPS_TWOSIDED;
#else
  pCaps->StencilCaps |= D3DSTENCILCAPS_TWOSIDED;
#endif

  pCaps->RasterCaps |= D3DPRASTERCAPS_FOGRANGE |
                       D3DPRASTERCAPS_ZTEST |
                       D3DPRASTERCAPS_FOGTABLE |
                       D3DPRASTERCAPS_MIPMAPLODBIAS |
                       D3DPRASTERCAPS_SCISSORTEST |
                       D3DPRASTERCAPS_DEPTHBIAS |
                       D3DPRASTERCAPS_ANISOTROPY |
                       D3DPRASTERCAPS_ZBUFFERLESSHSR;

  pCaps->DevCaps2 |= D3DDEVCAPS2_STREAMOFFSET;

  return true;
}

#ifndef PS3
bool IDirect3D9::CheckOGLExtensions()
{
  iLog->Log("\n...Check OpenGL extensions\n");

  FindProcs( true );

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_texture_compression)
    iLog->Log("  ...GL_ARB_texture_compression not found\n");
  else
  if (CV_gl_arb_texture_compression)
  {
    iLog->Log("  ...using GL_ARB_texture_compression\n");
  }
  else
  {
    SUPPORTS_GL_ARB_texture_compression = 0;
    iLog->Log("  ...ignoring GL_ARB_texture_compression\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_EXT_swap_control)
    iLog->Log("  ...WGL_EXT_swap_control not found\n");
  else
  if (CV_gl_ext_swapcontrol)
    iLog->Log("  ...using WGL_EXT_swap_control\n");
  else
  {
    SUPPORTS_WGL_EXT_swap_control = 0;
    CLEAR_PROC(wglSwapIntervalEXT);
    iLog->Log("  ...ignoring WGL_EXT_swap_control\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_ARB_render_texture)
    iLog->Log("  ...WGL_ARB_render_texture not found\n");
  else
  if (CV_gl_arb_render_texture)
    iLog->Log("  ...using WGL_ARB_render_texture\n");
  else
  {
    SUPPORTS_WGL_ARB_render_texture = 0;
    iLog->Log("  ...ignoring WGL_ARB_render_texture\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_3DFX_gamma_control)
    iLog->Log("  ...WGL_3DFX_gamma_control not found\n");
  else
  if (CV_gl_3dfx_gamma_control)
    iLog->Log("  ...using WGL_3DFX_gamma_control\n");
  else
  {
    SUPPORTS_WGL_3DFX_gamma_control = 0;
    CLEAR_PROC(wglGetDeviceGammaRamp3DFX);
    CLEAR_PROC(wglSetDeviceGammaRamp3DFX);
    iLog->Log("  ...ignoring WGL_3DFX_gamma_control\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_multisample)
    iLog->Log("  ...GL_ARB_multisample not found\n");
  else
  if (CV_gl_arb_multisample)
    iLog->Log("  ...using GL_ARB_multisample\n");
  else
  {
    SUPPORTS_GL_ARB_multisample = 0;
    iLog->Log("  ...ignoring GL_ARB_multisample\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_ARB_pbuffer)
    iLog->Log("  ...WGL_ARB_pbuffer not found\n");
  else
  if (CV_gl_arb_pbuffer)
    iLog->Log("  ...using WGL_ARB_pbuffer\n");
  else
  {
    SUPPORTS_WGL_ARB_pbuffer = 0;
    iLog->Log("  ...ignoring WGL_ARB_pbuffer\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_ARB_pixel_format)
    iLog->Log("  ...WGL_ARB_pixel_format not found\n");
  else
  if (CV_gl_arb_pixel_format)
    iLog->Log("  ...using WGL_ARB_pixel_format\n");
  else
  {
    SUPPORTS_WGL_ARB_pixel_format = 0;
    iLog->Log("  ...ignoring WGL_ARB_pixel_format\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_WGL_ARB_buffer_region)
    iLog->Log("  ...WGL_ARB_buffer_region not found\n");
  else
  if (CV_gl_arb_buffer_region)
    iLog->Log("  ...using WGL_ARB_buffer_region\n");
  else
  {
    SUPPORTS_WGL_ARB_buffer_region = 0;
    iLog->Log("  ...ignoring WGL_ARB_buffer_region\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_texture_env_combine)
    iLog->Log("  ...GL_ARB_texture_env_combine not found\n");
  else
  if (CV_gl_arb_texture_env_combine)
    iLog->Log("  ...using GL_ARB_texture_env_combine\n");
  else
  {
    SUPPORTS_GL_ARB_texture_env_combine = 0;
    iLog->Log("  ...ignoring GL_ARB_texture_env_combine\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_SGIX_depth_texture)
  {
    iLog->Log("  ...GL_SGIX_depth_texture not found\n");
  }
  else
  if (CV_gl_sgix_depth_texture)
  {
    iLog->Log("  ...using GL_SGIX_depth_texture\n");
  }
  else
  {
    SUPPORTS_GL_SGIX_depth_texture = 0;
    iLog->Log("  ...ignoring GL_SGIX_depth_texture\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_SGIX_shadow)
    iLog->Log("  ...GL_SGIX_shadow not found\n");
  else
  if (CV_gl_sgix_shadow)
    iLog->Log("  ...using GL_SGIX_shadow\n");
  else
  {
    SUPPORTS_GL_SGIX_shadow = 0;
    iLog->Log("  ...ignoring GL_SGIX_shadow\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_SGIS_generate_mipmap)
    iLog->Log("  ...GL_SGIS_generate_mipmap not found\n");
  else
  if (CV_gl_sgis_generate_mipmap)
    iLog->Log("  ...using GL_SGIS_generate_mipmap\n");
  else
  {
    SUPPORTS_GL_SGIS_generate_mipmap = 0;
    iLog->Log("  ...ignoring GL_SGIS_generate_mipmap\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_SGIS_texture_lod)
    iLog->Log("  ...GL_SGIS_texture_lod not found\n");
  else
  if (CV_gl_sgis_texture_lod)
    iLog->Log("  ...using GL_SGIS_texture_lod\n");
  else
  {
    SUPPORTS_GL_SGIS_texture_lod = 0;
    iLog->Log("  ...ignoring GL_SGIS_texture_lod\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_multitexture)
    iLog->Log("  ...GL_ARB_multitexture not found\n");
  else
  if (CV_gl_arb_multitexture)
  {
    iLog->Log("  ...using GL_ARB_multitexture\n");
  }
  else
  {
    SUPPORTS_GL_ARB_multitexture = 0;
    iLog->Log("  ...ignoring GL_ARB_multitexture\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texgen_reflection)
    iLog->Log("  ...GL_NV_texgen_reflection not found\n");
  else
  if (CV_gl_nv_texgen_reflection)
  {
    iLog->Log("  ...using GL_NV_texgen_reflection\n");
  }
  else
  {
    SUPPORTS_GL_NV_texgen_reflection = 0;
    iLog->Log("  ...ignoring GL_NV_texgen_reflection\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_HP_occlusion_test)
    iLog->Log("  ...GL_HP_occlusion_test not found\n");
  else
  if (CV_gl_hp_occlusion_test)
  {
    iLog->Log("  ...using GL_HP_occlusion_test\n");
  }
  else
  {
    SUPPORTS_GL_HP_occlusion_test = 0;
    iLog->Log("  ...ignoring GL_HP_occlusion_test\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_occlusion_query)
    iLog->Log("  ...GL_NV_occlusion_query not found\n");
  else
  if (CV_gl_hp_occlusion_test)
  {
    iLog->Log("  ...using GL_NV_occlusion_query\n");
  }
  else
  {
    SUPPORTS_GL_NV_occlusion_query = 0;
    iLog->Log("  ...ignoring GL_NV_occlusion_query\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_multisample_filter_hint)
    iLog->Log("  ...GL_NV_multisample_filter_hint not found\n");
  else
  if (CV_gl_nv_multisample_filter_hint)
    iLog->Log("  ...using GL_NV_multisample_filter_hint\n");
  else
  {
    SUPPORTS_GL_NV_multisample_filter_hint = 0;
    iLog->Log("  ...ignoring GL_NV_multisample_filter_hint\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_fog_distance)
    iLog->Log("  ...GL_NV_fog_distance not found\n");
  else
  if (CV_gl_nv_fog_distance)
  {
    iLog->Log("  ...using GL_NV_fog_distance\n");
  }
  else
  {
    SUPPORTS_GL_NV_fog_distance = 0;
    iLog->Log("  ...ignoring GL_NV_fog_distance\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texgen_emboss)
    iLog->Log("  ...GL_NV_texgen_emboss not found\n");
  else
  if (CV_gl_nv_texgen_emboss)
  {
    iLog->Log("  ...using GL_NV_texgen_emboss\n");
  }
  else
  {
    SUPPORTS_GL_NV_texgen_emboss = 0;
    iLog->Log("  ...ignoring GL_NV_texgen_emboss\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_separate_specular_color)
    iLog->Log("  ...GL_EXT_separate_specular_color not found\n");
  else
  if (CV_gl_ext_separate_specular_color)
  {
    iLog->Log("  ...using GL_EXT_separate_specular_color\n");
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT, GL_SEPARATE_SPECULAR_COLOR_EXT);
  }
  else
  {
    SUPPORTS_GL_EXT_separate_specular_color = 0;
    iLog->Log("  ...ignoring GL_EXT_separate_specular_color\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_texture_env_combine)
    iLog->Log("  ...GL_EXT_texture_env_combine not found\n");
  else
  if (CV_gl_ext_texture_env_combine)
    iLog->Log("  ...using GL_EXT_texture_env_combine\n");
  else
  {
    SUPPORTS_GL_EXT_texture_env_combine = 0;
    iLog->Log("  ...ignoring GL_EXT_texture_env_combine\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_multi_draw_arrays)
    iLog->Log("  ...GL_EXT_multi_draw_arrays not found\n");
  else
  if (CV_gl_ext_multi_draw_arrays)
  {
    iLog->Log("  ...using GL_EXT_multi_draw_arrays\n");
  }
  else
  {
    SUPPORTS_GL_EXT_multi_draw_arrays = 0;
    iLog->Log("  ...ignoring GL_EXT_multi_draw_arrays\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texture_env_combine4)
    iLog->Log("  ...GL_NV_texture_env_combine4 not found\n");
  else
  if (CV_gl_nv_texture_env_combine4)
  {
    iLog->Log("  ...using GL_NV_texture_env_combine4\n");
  }
  else
  {
    SUPPORTS_GL_NV_texture_env_combine4 = 0;
    iLog->Log("  ...ignoring GL_NV_texture_env_combine4\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_point_sprite)
    iLog->Log("  ...GL_NV_point_sprite not found\n");
  else
  if (CV_gl_nv_point_sprite)
  {
    iLog->Log("  ...using GL_NV_point_sprite\n");
  }
  else
  {
    SUPPORTS_GL_NV_point_sprite = 0;
    iLog->Log("  ...ignoring GL_NV_point_sprite\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_vertex_array_range)
    iLog->Log("  ...GL_NV_vertex_array_range not found\n");
  else
  if (CV_gl_nv_vertex_array_range)
    iLog->Log("  ...using GL_NV_vertex_array_range\n");
  else
  {
    SUPPORTS_GL_NV_vertex_array_range = 0;
    iLog->Log("  ...ignoring GL_NV_vertex_array_range\n");
    CLEAR_PROC(wglAllocateMemoryNV);
    CLEAR_PROC(glVertexArrayRangeNV);
    CLEAR_PROC(wglFreeMemoryNV);
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_compiled_vertex_array)
    iLog->Log("  ...GL_EXT_compiled_vertex_array not found.\n");
  else
  if ( CV_gl_ext_compiled_vertex_array)
    iLog->Log("  ...using GL_EXT_compiled_vertex_array.\n");
  else
  {
    SUPPORTS_GL_EXT_compiled_vertex_array = 0;
    CLEAR_PROC(glLockArraysEXT);
    CLEAR_PROC(glUnlockArraysEXT);
    iLog->Log("  ...ignoring GL_EXT_compiled_vertex_array.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_texture_env_add)
    iLog->Log("  ...GL_EXT_texture_env_add not found.\n");
  else
  if ( CV_gl_ext_texture_env_add)
    iLog->Log("  ...using GL_EXT_texture_env_add.\n");
  else
  {
    SUPPORTS_GL_EXT_texture_env_add = 0;
    iLog->Log("  ...ignoring GL_EXT_texture_env_add.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_texture_filter_anisotropic)
    iLog->Log("  ...GL_EXT_texture_filter_anisotropic not found.\n");
  else
  if (CV_gl_ext_texture_filter_anisotropic)
  {
    iLog->Log("  ...using GL_EXT_texture_filter_anisotropic.\n");
    if (!CRenderer::CV_r_texture_anisotropic_level)
      CRenderer::CV_r_texture_anisotropic_level = 1;
  }
  else
  {
    SUPPORTS_GL_EXT_texture_filter_anisotropic = 0;
    iLog->Log("  ...ignoring GL_EXT_texture_filter_anisotropic.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_bgra)
    iLog->Log("  ...GL_EXT_bgra not found.\n");
  else
  if ( CV_gl_ext_bgra)
    iLog->Log("  ...using GL_EXT_bgra.\n");
  else
  {
    SUPPORTS_GL_EXT_bgra = 0;
    iLog->Log("  ...ignoring GL_EXT_bgra.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_depth_bounds_test)
    iLog->Log("  ...GL_EXT_depth_bounds_test not found.\n");
  else
  if (CV_gl_ext_depth_bounds_test)
    iLog->Log("  ...using GL_EXT_depth_bounds_test.\n");
  else
  {
    SUPPORTS_GL_EXT_depth_bounds_test = 0;
    iLog->Log("  ...ignoring GL_EXT_depth_bounds_test.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_secondary_color)
    iLog->Log("  ...GL_EXT_secondary_color not found.\n");
  else
  if (CV_gl_ext_secondary_color)
  {
    iLog->Log("  ...using GL_EXT_secondary_color.\n");
  }
  else
  {
    SUPPORTS_GL_EXT_secondary_color = 0;
    iLog->Log("  ...ignoring GL_EXT_secondary_color.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_paletted_texture)
    iLog->Log("  ...GL_EXT_paletted_texture not found.\n");
  else
  if (CV_gl_ext_paletted_texture)
  {
    iLog->Log("  ...using GL_EXT_paletted_texture.\n");
  }
  else
  {
    SUPPORTS_GL_EXT_paletted_texture = 0;
    iLog->Log("  ...ignoring GL_EXT_paletted_texture.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_stencil_two_side)
    iLog->Log("  ...GL_EXT_stencil_two_side not found.\n");
  else
  if (CV_gl_ext_stencil_two_side)
    iLog->Log("  ...using GL_EXT_stencil_two_side.\n");
  else
  {
    SUPPORTS_GL_EXT_stencil_two_side = 0;
    iLog->Log("  ...ignoring GL_EXT_stencil_two_side.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_stencil_wrap)
    iLog->Log("  ...GL_EXT_stencil_wrap not found.\n");
  else
  if (CV_gl_ext_stencil_wrap)
    iLog->Log("  ...using GL_EXT_stencil_wrap.\n");
  else
  {
    SUPPORTS_GL_EXT_stencil_wrap = 0;
    iLog->Log("  ...ignoring GL_EXT_stencil_wrap.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_register_combiners)
    iLog->Log("  ...GL_NV_register_combiners not found.\n");
  else
  if (CV_gl_nv_register_combiners)
  {
    iLog->Log("  ...using GL_NV_register_combiners.\n");
  }
  else
  {
    SUPPORTS_GL_NV_register_combiners = 0;
    iLog->Log("  ...ignoring GL_NV_register_combiners.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_register_combiners2)
    iLog->Log("  ...GL_NV_register_combiners2 not found.\n");
  else
  if (CV_gl_nv_register_combiners2)
  {
    iLog->Log("  ...using GL_NV_register_combiners2.\n");
    glEnable(GL_PER_STAGE_CONSTANTS_NV);
  }
  else
  {
    SUPPORTS_GL_NV_register_combiners2 = 0;
    iLog->Log("  ...ignoring GL_NV_register_combiners2.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_vertex_program)
    iLog->Log("  ...GL_NV_vertex_program not found.\n");
  else
  if (CV_gl_nv_vertex_program)
  {
    iLog->Log("  ...using GL_NV_vertex_program.\n");
  }
  else
  {
    SUPPORTS_GL_NV_vertex_program = 0;
    iLog->Log("  ...ignoring GL_NV_vertex_program3.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_vertex_program)
    iLog->Log("  ...GL_ARB_vertex_program not found.\n");
  else
  if (CV_gl_arb_vertex_program)
  {
    iLog->Log("  ...using GL_ARB_vertex_program.\n");
  }
  else
  {
    SUPPORTS_GL_ARB_vertex_program = 0;
    iLog->Log("  ...ignoring GL_ARB_vertex_program.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texture_shader)
    iLog->Log("  ...GL_NV_texture_shader not found.\n");
  else
  if (CV_gl_nv_texture_shader)
  {
    iLog->Log("  ...using GL_NV_texture_shader.\n");
  }
  else
  {
    SUPPORTS_GL_NV_texture_shader = 0;
    iLog->Log("  ...ignoring GL_NV_texture_shader.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texture_shader2)
    iLog->Log("  ...GL_NV_texture_shader2 not found.\n");
  else
  if (CV_gl_nv_texture_shader2)
  {
    iLog->Log("  ...using GL_NV_texture_shader2.\n");
  }
  else
  {
    SUPPORTS_GL_NV_texture_shader2 = 0;
    iLog->Log("  ...ignoring GL_NV_texture_shader2.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texture_shader3)
    iLog->Log("  ...GL_NV_texture_shader3 not found.\n");
  else
  if (CV_gl_nv_texture_shader3)
  {
    iLog->Log("  ...using GL_NV_texture_shader3.\n");
  }
  else
  {
    SUPPORTS_GL_NV_texture_shader3 = 0;
    iLog->Log("  ...ignoring GL_NV_texture_shader3.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_texture_rectangle)
    iLog->Log("  ...GL_NV_texture_rectangle not found.\n");
  else
  if (CV_gl_nv_texture_rectangle)
  {
    iLog->Log("  ...using GL_NV_texture_rectangle.\n");
  }
  else
  {
    SUPPORTS_GL_NV_texture_rectangle = 0;
    iLog->Log("  ...ignoring GL_NV_texture_rectangle.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_texture_rectangle)
    iLog->Log("  ...GL_EXT_texture_rectangle not found.\n");
  else
  if (CV_gl_ext_texture_rectangle && !SUPPORTS_GL_NV_texture_rectangle)
  {
    iLog->Log("  ...using GL_EXT_texture_rectangle.\n");
  }
  else
  {
    SUPPORTS_GL_EXT_texture_rectangle = 0;
    iLog->Log("  ...ignoring GL_EXT_texture_rectangle.\n");
  }

/////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_EXT_texture_cube_map)
    iLog->Log("  ...GL_EXT_texture_cube_map not found.\n");
  else
  if (CV_gl_ext_texture_cube_map)
  {
    iLog->Log("  ...using GL_EXT_texture_cube_map.\n");
  }
  else
  {
    SUPPORTS_GL_EXT_texture_cube_map = 0;
    iLog->Log("  ...ignoring GL_EXT_texture_cube_map.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ATI_separate_stencil)
    iLog->Log("  ...GL_ATI_separate_stencil not found.\n");
  else
  if (CV_gl_ati_separate_stencil)
  {
    iLog->Log("  ...using GL_ATI_separate_stencil.\n");
  }
  else
  {
    SUPPORTS_GL_ATI_separate_stencil = 0;
    iLog->Log("  ...ignoring GL_ATI_separate_stencil.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ATI_fragment_shader)
    iLog->Log("  ...GL_ATI_fragment_shader not found.\n");
  else
  if (CV_gl_ati_fragment_shader)
  {
    iLog->Log("  ...using GL_ATI_fragment_shader.\n");
  }
  else
  {
    SUPPORTS_GL_ATI_fragment_shader = 0;
    iLog->Log("  ...ignoring GL_ATI_fragment_shader.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_vertex_buffer_object)
    iLog->Log("  ...GL_ARB_vertex_buffer_object not found.\n");
  else
  if (CV_gl_arb_vertex_buffer_object)
  {
    SUPPORTS_GL_ATI_fragment_shader = 0;
    SUPPORTS_GL_NV_fence = 0;
    SUPPORTS_GL_NV_vertex_array_range = 0;
    iLog->Log("  ...using GL_ARB_vertex_buffer_object.\n");
  }
  else
  {
    SUPPORTS_GL_ARB_vertex_buffer_object = 0;
    iLog->Log("  ...ignoring GL_ARB_vertex_buffer_object.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_NV_fragment_program)
    iLog->Log("  ...GL_NV_fragment_program not found.\n");
  else
  if (CV_gl_nv_fragment_program)
  {
    iLog->Log("  ...using GL_NV_fragment_program.\n");
    SUPPORTS_GL_ATI_fragment_shader = 0;
    SUPPORTS_GL_ARB_fragment_program = 0;
  }
  else
  {
    SUPPORTS_GL_NV_fragment_program = 0;
    iLog->Log("  ...ignoring GL_NV_fragment_program.\n");
  }


  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_fragment_program)
    iLog->Log("  ...GL_ARB_fragment_program not found.\n");
  else
  if (CV_gl_arb_fragment_program && !SUPPORTS_GL_NV_fragment_program)
  {
    iLog->Log("  ...using GL_ARB_fragment_program.\n");
    SUPPORTS_GL_ATI_fragment_shader = 0;
  }
  else
  {
    SUPPORTS_GL_ARB_fragment_program = 0;
    iLog->Log("  ...ignoring GL_ARB_fragment_program.\n");
  }

  if (!SUPPORTS_GL_NV_vertex_program3)
    iLog->Log("  ...GL_NV_vertex_program3 not found.\n");
  else
  if (CV_gl_nv_vertex_program3)
  {
    iLog->Log("  ...using GL_NV_vertex_program3.\n");
    if (SUPPORTS_GL_ARB_vertex_program)
      SUPPORTS_GL_NV_vertex_program = 0;
  }
  else
  {
    SUPPORTS_GL_NV_vertex_program3 = 0;
    iLog->Log("  ...ignoring GL_NV_vertex_program3.\n");
  }

  /////////////////////////////////////////////////////////////////////////////////////

  if (!SUPPORTS_GL_ARB_texture_env_combine)
    iLog->Log("  ...GL_ARB_texture_env_combine not found.\n");
  else
  if (CV_gl_arb_fragment_program)
  {
    iLog->Log("  ...using GL_ARB_texture_env_combine.\n");
  }
  else
  {
    SUPPORTS_GL_ARB_texture_env_combine = 0;
    iLog->Log("  ...ignoring GL_ARB_texture_env_combine.\n");
  }

  iLog->Log("\n");

  return true;
}

bool IDirect3D9::CheckGammaSupport()
{
  return true;
}
#endif

HRESULT IDirect3D9::CreateDevice(
		UINT Adapter,
		D3DDEVTYPE DeviceType,
		HWND hFocusWindow,
		DWORD BehaviorFlags,
		D3DPRESENT_PARAMETERS* pPresentationParameters,
		struct IDirect3DDevice9** ppReturnedDeviceInterface
		)
{
	if (DeviceType != D3DDEVTYPE_HAL)
	{
		return D3DERR_NOTAVAILABLE;
	}

  m_pCurrContext->m_bFSAAWasSet = false;
  IDirect3DDevice9 *pDev = new IDirect3DDevice9;
  bool bRes = CreateRContext(m_RContexts[0], pPresentationParameters->AutoDepthStencilFormat, pPresentationParameters->BackBufferFormat, false);
  if (!bRes)
    return D3DERR_INVALIDDEVICE;
  pDev->m_pD3D = this;
  pDev->m_PresentParams = *pPresentationParameters;
  g_pDev = pDev;

  iLog->Log("****** OpenGL Driver Stats ******\n");
#if !defined(PS3) && !defined(LINUX)
  iLog->Log("Driver: %s\n", "OpenGL32.dll");
#endif
  m_szVendorName = (const char *)glGetString(GL_VENDOR);
  iLog->Log("GL_VENDOR: %s\n", m_szVendorName);
  m_szRendererName = (const char *)glGetString(GL_RENDERER);
  iLog->Log("GL_RENDERER: %s\n", m_szRendererName);
  m_szVersionName = (const char *)glGetString(GL_VERSION);
  iLog->Log("GL_VERSION: %s\n", m_szVersionName);
  m_szExtensionsName = (const char *)glGetString(GL_EXTENSIONS);
  iLog->LogToFile("GL_EXTENSIONS:\n");
  char ext[8192];
  char *token;
  strcpy(ext, (char *)m_szExtensionsName);
  token = strtok(ext, " ");
  while (token)
  {
    iLog->LogToFile("  %s\n", token);
    token = strtok(NULL, " ");
  }
  iLog->LogToFile("\n");

#ifndef PS3

#if !defined(LINUX)
  GET_GL_PROC(PFNWGLGETEXTENSIONSSTRINGARBPROC,wglGetExtensionsStringARB);
  if(wglGetExtensionsStringARB)
  {
    const char * wglExt = wglGetExtensionsStringARB(m_pCurrContext->m_hDC);
    if (wglExt)
    {
      iLog->LogToFile("WGL_EXTENSIONS:\n");

      strcpy(ext, (char *)wglExt);
      token = strtok(ext, " ");
      while (token)
      {
        iLog->LogToFile("  %s\n", token);
        token = strtok(NULL, " ");
      }
      iLog->LogToFile("\n");
    }
  }
#endif

  CheckOGLExtensions();
  CheckGammaSupport();
#endif

  OpenGLExtensions2D3D9Caps();

  glDisable(GL_POLYGON_OFFSET_FILL);
#ifndef PS3
  glEnable(GL_FRAGMENT_PROGRAM_ARB);
  glEnable(GL_VERTEX_PROGRAM_ARB);
  glDrawBuffer  (GL_BACK);
#else
  //cgGLEnableProfile(CG_PROFILE_SCE_VP10);
  //cgGLEnableProfile(CG_PROFILE_SCE_FP10);
	//cgGLEnableProfile(CG_PROFILE_SCE_VP_TYPEC);
	//cgGLEnableProfile(CG_PROFILE_SCE_FP_TYPEC);
	cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_FRAGMENT));
	cgGLEnableProfile(cgGLGetLatestProfile(CG_GL_VERTEX));
	pDev->m_CurScissorRect = m_CurScissorRect;
#endif
  glShadeModel(GL_SMOOTH);    
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);    
  glEnable (GL_TEXTURE_2D);
  glPixelStorei (GL_PACK_ALIGNMENT, 1);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);  
  glMatrixMode  (GL_MODELVIEW);
  glLoadIdentity();

	// Setup back buffer and zstencil surfaces.
	pDev->m_pBackBuffer = new IDirect3DSurface9;
	pDev->m_pBackBuffer->m_nType = D3DRTYPE_SURFACE;
	pDev->m_pBackBuffer->m_Format = pDev->m_PresentParams.BackBufferFormat;
	pDev->m_pBackBuffer->m_Width = pDev->m_PresentParams.BackBufferWidth;
	pDev->m_pBackBuffer->m_Height = pDev->m_PresentParams.BackBufferHeight;
	pDev->m_pBackBuffer->m_Pool = D3DPOOL_DEFAULT;
	pDev->m_pBackBuffer->m_Usage = D3DUSAGE_RENDERTARGET;
	pDev->m_pBackBuffer->m_nID = 0;
	pDev->m_pCurTarget[0] = pDev->m_pBackBuffer;
	pDev->m_pBackBuffer->AddRef();
	pDev->m_pCurZStencil = new IDirect3DSurface9;
	pDev->m_pCurZStencil->m_nType = D3DRTYPE_SURFACE;
	pDev->m_pCurZStencil->m_Format = D3DFMT_D24S8;
	pDev->m_pCurZStencil->m_Width = pDev->m_PresentParams.BackBufferWidth;
	pDev->m_pCurZStencil->m_Height = pDev->m_PresentParams.BackBufferHeight;
	pDev->m_pCurZStencil->m_Pool = D3DPOOL_DEFAULT;
	pDev->m_pCurZStencil->m_Usage = D3DUSAGE_DEPTHSTENCIL;
	pDev->m_pCurZStencil->m_nID = 0;

  *ppReturnedDeviceInterface = pDev;

  return S_OK;
}

UINT IDirect3D9::GetAdapterCount()
{
  return m_nNumAdapters;
}

HRESULT IDirect3D9::CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels)
{
  // Don't support FSAA for now
  if (DeviceType != D3DDEVTYPE_HAL)
    return D3DERR_NOTAVAILABLE;

	if (pQualityLevels != NULL)
	{
		// Avoid an uninitialized memory read error in the caller.
		*pQualityLevels = 1;
	}
  return S_OK;
}

HRESULT IDirect3D9::GetAdapterIdentifier(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER9* pIdentifier)
{
  *pIdentifier = m_AdapterIdent;
  return S_OK;
}

UINT IDirect3D9::GetAdapterModeCount(UINT Adapter,D3DFORMAT Format)
{
  return m_VidModes.Num();
}

HRESULT IDirect3D9::EnumAdapterModes(UINT Adapter,D3DFORMAT Format,UINT Mode,D3DDISPLAYMODE* pMode)
{
  assert(Mode < m_VidModes.Num());
  DEVMODE *pDM = &m_VidModes[Mode];
  pMode->Width = pDM->dmPelsWidth;
  pMode->Height = pDM->dmPelsHeight;
  pMode->RefreshRate = pDM->dmDisplayFrequency;
  switch (pDM->dmBitsPerPel)
  {
    case 16:
    case 15:
      pMode->Format = D3DFMT_R5G6B5;
      break;

    case 24:
    case 32:
      pMode->Format = D3DFMT_X8R8G8B8;
      break;

    default:
      pMode->Width = 0;
      pMode->Height = 0;
      return D3DERR_NOTAVAILABLE;
  }

  return S_OK;
}

HRESULT IDirect3D9::GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS9* pCaps)
{
  *pCaps = m_DeviceCaps;
  return S_OK;
}

HRESULT IDirect3D9::CheckDeviceType(UINT Adapter,D3DDEVTYPE DevType,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
  // In OpenGL we use only 32bit render
  if (DevType != D3DDEVTYPE_HAL)
    return D3DERR_NOTAVAILABLE;
  if (AdapterFormat != D3DFMT_X8R8G8B8)
    return D3DERR_NOTAVAILABLE;
  if (BackBufferFormat != D3DFMT_A8R8G8B8)
    return D3DERR_NOTAVAILABLE;
  return S_OK;
}

HRESULT IDirect3D9::CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
{
  if (DeviceType != D3DDEVTYPE_HAL)
    return D3DERR_NOTAVAILABLE;
  if (AdapterFormat != D3DFMT_X8R8G8B8)
    return D3DERR_NOTAVAILABLE;
  if (Usage & D3DUSAGE_DEPTHSTENCIL)
  {
    if (RType != D3DRTYPE_SURFACE)
      return D3DERR_INVALIDCALL;
    if (CheckFormat != D3DFMT_D24S8)
      return D3DERR_NOTAVAILABLE;
    return S_OK;
  }
  if (RType == D3DRTYPE_TEXTURE)
  {
		if (CheckFormat == D3DFMT_3DC)
			return D3DERR_NOTAVAILABLE;
    switch (CheckFormat)
    {
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A8R8G8B8:
      case D3DFMT_R5G6B5:
      case D3DFMT_R8G8B8:
      case D3DFMT_A1R5G5B5:
      case D3DFMT_A4R4G4B4:
      case D3DFMT_X1R5G5B5:
      case D3DFMT_A8:
      case D3DFMT_A8L8:
      case D3DFMT_L8:
      case D3DFMT_A16B16G16R16F:
      case D3DFMT_G16R16F:
      case D3DFMT_R16F:
      case D3DFMT_A32B32G32R32F:
        return S_OK;

      case D3DFMT_V8U8:
        return S_OK;

      case D3DFMT_DXT1:
      case D3DFMT_DXT3:
      case D3DFMT_DXT5:
#ifndef PS3
        if (SUPPORTS_GL_ARB_texture_compression)
#endif
          return S_OK;
        return D3DERR_NOTAVAILABLE;
      case D3DFMT_CxV8U8:
      case D3DFMT_X8L8V8U8:
      case D3DFMT_L6V5U5:
      case D3DFMT_Q8W8V8U8:
      case D3DFMT_V16U16:
      case D3DFMT_A16B16G16R16:
      case D3DFMT_R32F:
      case D3DFMT_D24S8:
      case D3DFMT_D16:
        return D3DERR_NOTAVAILABLE;

      default:
        assert(0);
    }
  }
  if (RType == D3DRTYPE_SURFACE)
  {
		switch (CheckFormat)
		{
		case D3DFMT_X8R8G8B8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_R8G8B8:
		case D3DFMT_A8:
		case D3DFMT_A16B16G16R16F:
		case D3DFMT_G16R16F:
		case D3DFMT_R16F:
		case D3DFMT_A32B32G32R32F:
		case D3DFMT_R32F:
		case MAKEFOURCC('I', 'N', 'S', 'T'):
			return S_OK;
		default:
			assert(0);
			return D3DERR_NOTAVAILABLE;
		}
  }
  return S_OK;
}


HRESULT IDirect3D9::CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
{
  if (DeviceType != D3DDEVTYPE_HAL)
    return D3DERR_NOTAVAILABLE;
  if (AdapterFormat != D3DFMT_X8R8G8B8)
    return D3DERR_NOTAVAILABLE;
  if (RenderTargetFormat != D3DFMT_A8R8G8B8)
    return D3DERR_NOTAVAILABLE;
  if (DepthStencilFormat != D3DFMT_D24S8)
    return D3DERR_NOTAVAILABLE;

  return S_OK;
}



//====================================================================
// D3D device functions

//====================================================================

IDirect3DDevice9::~IDirect3DDevice9()
{
	SAFE_RELEASE(m_pCurZStencil);
	SAFE_RELEASE(m_pCurTarget[0]);
	SAFE_RELEASE(m_pCurTarget[1]);
	SAFE_RELEASE(m_pCurTarget[2]);
	SAFE_RELEASE(m_pCurTarget[3]);
	SAFE_RELEASE(m_pBackBuffer);
	if (m_nCurFBO || m_nCurFBOZ)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		if (m_nCurFBO)
		{
			glDeleteFramebuffersEXT(1, &m_nCurFBO);
			m_nCurFBO = 0;
		}
		if (m_nCurFBOZ)
		{
			glDeleteRenderbuffersEXT(1, &m_nCurFBOZ);
			m_nCurFBOZ = 0;
		}
	}
	SAFE_RELEASE(m_pCurVShader);
  for (int i = 0; i < MAX_TMU; ++i)
	{
    SAFE_RELEASE(m_ReqTS[i].m_pTex);
	}
}

IDirect3DDevice9::IDirect3DDevice9()
{
  int i;

  m_nCurTMU = 0;
  m_fAT_ReqVal = 0;
  m_nAT_ReqOp = GL_GREATER;
  m_bAT_Dirty = true;
  m_bAB_Dirty = true;
  m_nAB_Src = GL_ONE;
  m_nAB_Dst = GL_ZERO;
  m_bAB_Enabled = false;
  m_bAT_Enabled = false;
	m_bDT_Enabled = true;

  m_bSTENC_Dirty = false;
  m_nSTENC_ref = 0;
  m_nSTENC_mask = -1;
  m_nSTENC_func = GL_ALWAYS;
  m_nSTENC_OpFail = GL_KEEP;
  m_nSTENC_OpZFail = GL_KEEP;
  m_nSTENC_OpPass = GL_KEEP;

  m_pCurVDeclaration = NULL;
  m_pCurIB = NULL;

  m_nCurStreamMask = 0;
  m_nCurrentVB = -1;
  m_nCurrentIB = -1;

	m_pCurVShader = NULL;

  m_nCP_Mask = 0;

	memset(m_ReqTS, 0, sizeof m_ReqTS);
  for (i=0; i<MAX_TMU; i++)
  {
    m_ReqTS[i].nAddressU = GL_REPEAT;
    m_ReqTS[i].nAddressV = GL_REPEAT;
    m_ReqTS[i].nAddressW = GL_REPEAT;

    m_ReqTS[i].nD3DMagFilter = D3DTEXF_LINEAR;
    m_ReqTS[i].nD3DMinFilter = D3DTEXF_LINEAR;
    m_ReqTS[i].nD3DMipFilter = D3DTEXF_NONE;
  }

	m_nCurFBO = 0;
	m_nCurFBOZ = 0;
	m_pCurTarget[0] = NULL;
	m_pCurTarget[1] = NULL;
	m_pCurTarget[2] = NULL;
	m_pCurTarget[3] = NULL;
	m_pCurZStencil = NULL;
}

void IDirect3DDevice9::SelectTMU(int nTMU)
{
  if (nTMU != m_nCurTMU)
  {
#ifndef PS3
    glActiveTextureARB(GL_TEXTURE0_ARB+nTMU);
    glClientActiveTextureARB(GL_TEXTURE0_ARB+nTMU);
#else
    glActiveTexture(GL_TEXTURE0+nTMU);
    glClientActiveTexture(GL_TEXTURE0+nTMU);
#endif
    m_nCurTMU = nTMU;
  }
}


HRESULT IDirect3DDevice9::TestCooperativeLevel()
{
  return S_OK;
}
UINT IDirect3DDevice9::GetAvailableTextureMem()
{
  return 0;
}
HRESULT IDirect3DDevice9::EvictManagedResources()
{
  assert(0);
  return S_OK;
}
HRESULT IDirect3DDevice9::GetDirect3D(IDirect3D9** ppD3D9)
{
  *ppD3D9 = m_pD3D;

  return S_OK;
}

HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps)
{
  *pCaps = m_pD3D->m_DeviceCaps;
  return S_OK;
}

HRESULT IDirect3DDevice9::GetDisplayMode(UINT iSwapChain,D3DDISPLAYMODE* pMode)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetCursorProperties(UINT XHotSpot,UINT YHotSpot,IDirect3DSurface9* pCursorBitmap)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
  if (!m_pD3D)
    return D3DERR_INVALIDCALL;

#if defined(PS3)
#ifdef PSGL_DEBUG_SWAP
	if (psglSwap_EnableDebug)
	{
		psglSwap();
		glClearColor(0.05f, 0.15f, 0.1f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glFinish();
		psglSwap_Break();
		psglSwap();
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
#endif
	psglSwap();
  m_pD3D->m_pCurrContext->m_nCurSurface = 1-m_pD3D->m_pCurrContext->m_nCurSurface;
#elif defined(LINUX)
	SDL_GL_SwapBuffers();
#else
  SwapBuffers(m_pD3D->m_pCurrContext->m_hDC);
#endif

  return S_OK;
}

HRESULT IDirect3DDevice9::GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer)
{
  if (!ppBackBuffer)
    return D3DERR_INVALIDCALL;

	/*
  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_nType = D3DRTYPE_SURFACE;
  pSurf->m_Format = m_PresentParams.BackBufferFormat;
  pSurf->m_Width = m_PresentParams.BackBufferWidth;
  pSurf->m_Height = m_PresentParams.BackBufferHeight;
  pSurf->m_Pool = D3DPOOL_DEFAULT;
  pSurf->m_Usage = 0;
  *ppBackBuffer = pSurf;
	*/

	*ppBackBuffer = m_pBackBuffer;
	m_pBackBuffer->AddRef();

  return S_OK;
}

HRESULT IDirect3DDevice9::GetRasterStatus(UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus)
{
  assert(0);
  return S_OK;
}

void IDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
  assert(0);
}

void IDirect3DDevice9::GetGammaRamp(UINT iSwapChain,D3DGAMMARAMP* pRamp)
{
  assert(0);
}

HRESULT IDirect3DDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle)
{
	GLuint texId = 0;
	GLenum format, type;
	GLint internalFormat;
	bool skipAlloc = false;

  if (!ppTexture)
    return D3DERR_INVALIDCALL;

  if (Levels == 0)
    Levels = 1;

	// Allocate the texture storage for the texture.
	if (!D3DFORMAT2GL(Format, &format, &type, &internalFormat, NULL))
		skipAlloc = true;

  glGenTextures(1, &texId);
	if (!skipAlloc)
	{
		glBindTexture(GL_TEXTURE_2D, texId);
		if (Levels > 1)
		{
			// FIXME Should be configurable.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					GL_NEAREST_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
		} else
		{
			// Disable mip-mapping.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, Levels - 1);
#if defined(PS3)
		if ((Usage & D3DUSAGE_RENDERTARGET)
				&& (!(Width & (Width - 1)) && !(Height & (Height - 1))
					|| type == GL_HALF_FLOAT_ARB || type == GL_FLOAT))
		{
			// Texels for these textures can not be swizzled automatically, so
			// they must be allocated in a format suitable for rendering.
			glTexParameteri(
					GL_TEXTURE_2D,
					GL_TEXTURE_ALLOCATION_HINT_SCE,
					GL_TEXTURE_TILED_GPU_SCE);
		}
#endif
		for (int l = 0; l < Levels; ++l)
		{
			int w = Width >> l, h = Height >> l;
			if (w == 0) w = 1;
			if (h == 0) h = 1;
			glTexImage2D(GL_TEXTURE_2D, l, internalFormat,
					w, h, 0, format, type, NULL);
		}
	}

  IDirect3DTexture9 *pTex = new IDirect3DTexture9;
  pTex->m_nWidth = Width;
  pTex->m_nHeight = Height;
  pTex->m_nLevelCount = Levels;
  pTex->m_Format = Format;
  pTex->m_Usage = Usage;
  pTex->m_Pool = Pool;
  pTex->m_nType = D3DRTYPE_TEXTURE;
  pTex->m_eTT = eTT_2D;
  pTex->m_nGLtarget = GL_TEXTURE_2D;
	pTex->m_nID = (int)texId;
  pTex->m_pLockedData = new byte *[Levels];
  ZeroMemory(pTex->m_pLockedData, sizeof(byte *)*Levels);

  *ppTexture = pTex;

  return S_OK;
}

HRESULT IDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
{
  assert (Levels > 0 && Levels < 20);
  if (!ppVolumeTexture || Levels <= 0 || Levels > 20)
    return D3DERR_INVALIDCALL;

  IDirect3DVolumeTexture9 *pTex = new IDirect3DVolumeTexture9;
  pTex->m_nWidth = Width;
  pTex->m_nHeight = Height;
  pTex->m_nDepth = Depth;
  pTex->m_nLevelCount = Levels;
  pTex->m_Format = Format;
  pTex->m_Usage = Usage;
  pTex->m_Pool = Pool;
  pTex->m_nType = D3DRTYPE_TEXTURE;
  pTex->m_eTT = eTT_Cube;
  pTex->m_nGLtarget = GL_TEXTURE_3D;
  glGenTextures(1, &pTex->m_nID);
  pTex->m_pLockedData = new byte *[Levels];
  ZeroMemory(pTex->m_pLockedData, sizeof(byte *)*Levels);

  *ppVolumeTexture = pTex;

  return S_OK;
}

HRESULT IDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
{
  assert (Levels > 0 && Levels < 20);
  if (!ppCubeTexture || Levels <= 0 || Levels > 20)
    return D3DERR_INVALIDCALL;

  IDirect3DCubeTexture9 *pTex = new IDirect3DCubeTexture9;
  pTex->m_nWidth = EdgeLength;
  pTex->m_nHeight = EdgeLength;
  pTex->m_nLevelCount = Levels;
  pTex->m_Format = Format;
  pTex->m_Usage = Usage;
  pTex->m_Pool = Pool;
  pTex->m_nType = D3DRTYPE_TEXTURE;
  pTex->m_eTT = eTT_Cube;
#ifndef PS3
  pTex->m_nGLtarget = GL_TEXTURE_CUBE_MAP_ARB;
#else
  pTex->m_nGLtarget = GL_TEXTURE_CUBE_MAP;
#endif
  glGenTextures(1, &pTex->m_nID);
  pTex->m_pLockedData = new byte *[Levels*6];
  ZeroMemory(pTex->m_pLockedData, sizeof(byte *)*Levels*6);

  *ppCubeTexture = pTex;

  return S_OK;
}

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT nLength,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle)
{
#ifndef PS3
  if (!SUPPORTS_GL_ARB_vertex_buffer_object)
    return D3DERR_NOTAVAILABLE;
#endif
  if (!ppVertexBuffer)
    return D3DERR_NOTAVAILABLE;

	bool dynamic = false;
	if (Usage & D3DUSAGE_DYNAMIC || Pool == D3DPOOL_MANAGED)
		dynamic = true;

  IDirect3DVertexBuffer9 *pBuf = new IDirect3DVertexBuffer9;
  pBuf->m_nLength = nLength;
  pBuf->m_Usage = Usage;
  pBuf->m_Pool = Pool;
#ifndef PS3
  glGenBuffersARB(1, &pBuf->m_nID);
  glBindBufferARB(GL_ARRAY_BUFFER_ARB, pBuf->m_nID);
  glBufferDataARB(GL_ARRAY_BUFFER_ARB, nLength, NULL, dynamic ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB);
#else
  glGenBuffers(1, &pBuf->m_nID);
  glBindBuffer(GL_ARRAY_BUFFER, pBuf->m_nID);
  glBufferData(GL_ARRAY_BUFFER, nLength, NULL, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
#endif
  m_nCurrentVB = pBuf->m_nID;

  *ppVertexBuffer = pBuf;
  return S_OK;
}

HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT nLength,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle)
{
#ifndef PS3
  if (!SUPPORTS_GL_ARB_vertex_buffer_object)
    return D3DERR_NOTAVAILABLE;
#endif
  if (!ppIndexBuffer)
    return D3DERR_NOTAVAILABLE;

  IDirect3DIndexBuffer9 *pBuf = new IDirect3DIndexBuffer9;
  pBuf->m_nLength = nLength;
  pBuf->m_Usage = Usage;
#ifndef PS3
  glGenBuffersARB(1, &pBuf->m_nID);
  glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, pBuf->m_nID);
  glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, nLength, NULL, (Usage & D3DUSAGE_DYNAMIC) ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB);
#else
  glGenBuffers(1, &pBuf->m_nID);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBuf->m_nID);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, nLength, NULL, (Usage & D3DUSAGE_DYNAMIC) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
#endif
  m_nCurrentIB = pBuf->m_nID;

  *ppIndexBuffer = pBuf;

  return S_OK;
}

HRESULT IDirect3DDevice9::CreateRenderTarget(
		UINT Width,
		UINT Height,
		D3DFORMAT Format,
		D3DMULTISAMPLE_TYPE MultiSample,
		DWORD MultisampleQuality,
		BOOL Lockable,
		IDirect3DSurface9** ppSurface,
		D3DSURFACE_PARAMETERS* pSharedHandle
		)
{
  if (!ppSurface)
    return D3DERR_INVALIDCALL;

	GLuint format = 0, rid = 0;
	switch (Format)
	{
	case D3DFMT_X8R8G8B8:
  case D3DFMT_A8R8G8B8:
	case D3DFMT_R8G8B8:
	case D3DFMT_A8:
	  //format = GL_ARGB_SCE;
	  format = GL_RGBA8;
		break;
	case D3DFMT_A16B16G16R16F:
	case D3DFMT_G16R16F:
	case D3DFMT_R16F:
		format = GL_RGBA16F_ARB;
		break;
	case D3DFMT_A32B32G32R32F:
	case D3DFMT_R32F:
		format = GL_RGBA32F_ARB;
		break;
	default:
	  assert(0);
	}
	glGenRenderbuffersEXT(1, &rid);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rid);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, format, Width, Height);

  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_Width = Width;
  pSurf->m_Height = Height;
  pSurf->m_Format = Format;
  pSurf->m_Usage = D3DUSAGE_RENDERTARGET;
	pSurf->m_nID = (int)rid;
  *ppSurface = pSurf;

  return S_OK;
}

HRESULT IDirect3DDevice9::CreateDepthStencilSurface(
		UINT Width,
		UINT Height,
		D3DFORMAT Format,
		D3DMULTISAMPLE_TYPE MultiSample,
		DWORD MultisampleQuality,
		BOOL Discard,
		IDirect3DSurface9** ppSurface,
		D3DSURFACE_PARAMETERS* pSharedHandle
		)
{
  if (!ppSurface)
    return D3DERR_INVALIDCALL;

  GLuint format = 0, id = 0;
  switch (Format)
  {
	case D3DFMT_D24FS8:
	case D3DFMT_D24S8:
		format = GL_DEPTH_COMPONENT24;
		break;
	default:
		assert(0);
  }
	glGenRenderbuffersEXT(1, &id);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, id);
	glRenderbufferStorageEXT(
			GL_RENDERBUFFER_EXT,
			format,
			Width,
			Height);

  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_Width = Width;
  pSurf->m_Height = Height;
  pSurf->m_Format = Format;
  pSurf->m_Usage = D3DUSAGE_DEPTHSTENCIL;
	pSurf->m_nID = (int)id;
  *ppSurface = pSurf;

  return S_OK;
}

HRESULT IDirect3DDevice9::UpdateSurface(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture)
{
  assert(0);
  return S_OK;
}


HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::StretchRect(
		IDirect3DSurface9* pSourceSurface,
		CONST RECT* pSourceRect,
		IDirect3DSurface9* pDestSurface,
		CONST RECT* pDestRect,
		D3DTEXTUREFILTERTYPE Filter
		)
{
	GLuint fid = 0;
	RECT sourceRectAll, destRectAll;
	if (pSourceRect == NULL)
	{
		sourceRectAll.left = 0;
		sourceRectAll.top = 0;
		sourceRectAll.right = pSourceSurface->m_Width;
		sourceRectAll.bottom = pSourceSurface->m_Height;
		pSourceRect = &sourceRectAll;
	}
	if (pDestRect == NULL)
	{
		destRectAll.left = 0;
		destRectAll.top = 0;
		destRectAll.right = pDestSurface->m_Width;
		destRectAll.bottom = pDestSurface->m_Height;
		pDestRect = &destRectAll;
	}
	const int srcWidth = pSourceRect->right - pSourceRect->left;
	const int srcHeight = pSourceRect->bottom - pSourceRect->top;
	const int dstWidth = pDestRect->right - pDestRect->left;
	const int dstHeight = pDestRect->bottom - pDestRect->top;
	bool srcCurrent = false;
	bool dstCurrent = false;
	IDirect3DBaseTexture9 *pSourceTexture = NULL;
	CD3D9Renderer *const renderer = reinterpret_cast<CD3D9Renderer *>(gRenDev);

	// Check if the src surface or the dst surface are the currently active
	// render target at index 0.
	/*
	if (pSourceSurface == m_pCurTarget[0])
		srcCurrent = true;
	if (pDestSurface == m_pCurTarget[0])
		dstCurrent = true;
	*/

	if (pDestSurface->m_pTex
			&& srcWidth == dstWidth
			&& srcHeight == dstHeight
			&& false)
	{
		// Special case: Copy to texture with an an exact size match. This can be
		// done via glCopyTexSubImage2D().

		if (!srcCurrent)
		{
			// Bind the src surface to the current framebuffer.
			if (pSourceSurface->m_nID != 0)
			{
				glGenFramebuffersEXT(1, &fid);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fid);
				glFramebufferRenderbufferEXT(
						GL_FRAMEBUFFER_EXT,
						GL_COLOR_ATTACHMENT0_EXT,
						GL_RENDERBUFFER_EXT,
						(GLuint)pSourceSurface->m_nID);
			}
			else
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		}

		// Copy the texture.
		glBindTexture(GL_TEXTURE_2D, pDestSurface->m_pTex->m_nID);
		glCopyTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				pDestRect->left, pDestRect->top,
				pSourceRect->left, pSourceRect->top,
				dstWidth, dstHeight);
		if (!srcCurrent)
		{
			// Restore the framebuffer.
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
		}
	} else
	{
		// General case: StretchRect() is implemented by rendering a textured quad
		// to the dst surface. If the src surface is not a texture, then we'll
		// copy the source rect to a temporary texture.

		renderer->EF_SelectTMU(0);

		HRESULT hr = S_OK;

		// Get the src texture. Copy to tmp if necessary.
		if (pSourceSurface->m_pTex == NULL)
		{
			if (!srcCurrent)
			{
				// Bind the src surface to the current framebuffer.
				if (pSourceSurface->m_nID != 0)
				{
					glGenFramebuffersEXT(1, &fid);
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fid);
					glFramebufferRenderbufferEXT(
							GL_FRAMEBUFFER_EXT,
							GL_COLOR_ATTACHMENT0_EXT,
							GL_RENDERBUFFER_EXT,
							(GLuint)pSourceSurface->m_nID);
				}
				else
				{
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

#if defined(PS3)
static int debug1 = 0;
if (debug1)
{
psglSwap();
puts("break here");
psglSwap();
debug1 = 2;
}
#endif
				}
			}

			// Copy the specified rect from the src surface to a temp texture.
			IDirect3DTexture9 *pTex = NULL;
			hr = CreateTexture(
					srcWidth,
					srcHeight,
					1, // Levels
					D3DUSAGE_RENDERTARGET,
					pSourceSurface->m_Format,
					D3DPOOL_DEFAULT,
					&pTex,
					NULL);
			pSourceTexture = pTex;
			GLenum format, type;
			if (!D3DFORMAT2GL(pSourceSurface->m_Format, &format, &type, NULL, NULL))
			{
				// Surface format not supported.
				assert(0);
			}
			glClientActiveTexture(GL_TEXTURE0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, pSourceTexture->m_nID);
			if (srcWidth == dstWidth && srcHeight == dstHeight)
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			else
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

#if !defined(NDEBUG)
			// Check the framebuffer status.
			GLenum fbStatus = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if (fbStatus != GL_FRAMEBUFFER_COMPLETE_EXT)
			{
				const char *fbStatusString = GetFBStatusString(fbStatus);
				assert(fbStatusString != NULL);
				fprintf(stderr,
						"Incomplete framebuffer in IDirect3DDevice9::StretchRect: %s\n",
						fbStatusString);
			}
#endif

			glCopyTexImage2D(
					GL_TEXTURE_2D,
					0,
					format,
					pSourceRect->left, pSourceRect->top,
					srcWidth, srcHeight,
					0);

			if (!srcCurrent)
			{
				// Restore the framebuffer.
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
			}
		} else
		{
			pSourceTexture = pSourceSurface->m_pTex;
			pSourceTexture->AddRef();
			glBindTexture(GL_TEXTURE_2D, pSourceTexture->m_nID);
		}

		int viewportX, viewportY, viewportWidth, viewportHeight;
		renderer->GetViewport(
				&viewportX,
				&viewportY,
				&viewportWidth,
				&viewportHeight);
		renderer->FX_PushRenderTarget(
				0,
				pDestSurface,
				renderer->FX_GetScreenDepthSurface(),
				false
				);

		bool resample = false;
		if (srcWidth != dstHeight || srcHeight != dstHeight)
			resample = true;
		uint nPasses;
		if (resample)
			CShaderMan::m_shPostEffects->FXSetTechnique("TextureToTextureResampled");
		else
			CShaderMan::m_shPostEffects->FXSetTechnique("TextureToTexture");
		CShaderMan::m_shPostEffects->FXBegin(
				&nPasses,
				FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		CShaderMan::m_shPostEffects->FXBeginPass(0);

		renderer->EF_SetState(GS_NODEPTHTEST);

    // Set samples position, use rotated grid.
    float s1 = 2.0f / (float)srcWidth;
    float t1 = 2.0f / (float)srcHeight;
    Vec4 pParams0 = Vec4(s1 * 0.95f, t1 * 0.25f, -s1 * 0.25f, t1 * 0.96f); 
    Vec4 pParams1 = Vec4(-s1 * 0.96f, -t1 * 0.25f, s1 * 0.25f, -t1 * 0.96f);  
    CShaderMan::m_shPostEffects->FXSetVSFloat(
				"texToTexParams0",
				&pParams0,
				1);
    CShaderMan::m_shPostEffects->FXSetVSFloat(
				"texToTexParams1",
				&pParams1,
				1);

    struct_VERTEX_FORMAT_P3F_TEX2F quad[4];
		static float st00 = 0, st01 = 0, st10 = 0, st11 = 1;
		static float st20 = 1, st21 = 0, st30 = 1, st31 = 1;
		float offsetU = 0.5f / (float)srcWidth, offsetV = 0.5f / (float)srcHeight;     
		quad[0].xyz = Vec3(-offsetU, -offsetV, 0);
		quad[0].st[0] = st00;
		quad[0].st[1] = st01;
		quad[1].xyz = Vec3(-offsetU, 1 - offsetV, 0);
		quad[1].st[0] = st10;
		quad[1].st[1] = st11;
		quad[2].xyz = Vec3(1 - offsetU, -offsetV, 0);
		quad[2].st[0] = st20;
		quad[2].st[1] = st21;
		quad[3].xyz = Vec3(1 - offsetU, 1 - offsetV, 0);
		quad[3].st[0] = st30;
		quad[3].st[1] = st31;

renderer->EF_Commit();
static float color_r = 0, color_g = 0, color_b = 0, color_a = 1;
glClearColor(color_r, color_g, color_b, color_a);
#if defined(PS3)
glClearDepthf(1.0f);
#else
glClearDepth(1.0f);
#endif
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glDisable(GL_DEPTH_TEST);
glDisable(GL_ALPHA_TEST);
glDisable(GL_BLEND);

		static int vertexCount = 4;
    CVertexBuffer strip(quad, VERTEX_FORMAT_P3F_TEX2F, vertexCount);
		renderer->DrawTriStrip(&strip, vertexCount);     

		CShaderMan::m_shPostEffects->FXEndPass();
		CShaderMan::m_shPostEffects->FXEnd();

		renderer->FX_PopRenderTarget(0);
		renderer->SetViewport(
				viewportX,
				viewportY,
				viewportWidth,
				viewportHeight);

		if (!dstCurrent)
		{
			// Restore the framebuffer.
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
		}
	}

	// Restore the currently bound texture.
	int currentTMU = m_nCurTMU;
	if (m_ReqTS[currentTMU].m_pTex)
	{
		SetTexture(0, m_ReqTS[currentTMU].m_pTex);
	}
	SelectTMU(currentTMU);

	// Clean up.
	SAFE_RELEASE(pSourceTexture);
	if (fid)
		glDeleteFramebuffersEXT(1, &fid);
  return S_OK;
}

static void IDirect3DDevice9_ColorFill_invalidRT(const char *message)
{
	printf(
			"ERROR: "
			"IDirect3DDevice9::ColorFill: "
			"%s\n",
			message);
}

HRESULT IDirect3DDevice9::ColorFill(IDirect3DSurface9* pSurface,CONST RECT* pRect,D3DCOLOR color)
{
	bool isCurrent = false;
	uint8 r8 = (color >> 16) & 0xff;
	uint8 g8 = (color >> 8) & 0xff;
	uint8 b8 = color & 0xff;
	uint8 a8 = (color >> 24) & 0xff;
	GLuint fbo = 0;
	int screenHeight = pSurface->m_Height;

	if (pSurface == m_pCurTarget[0])
		isCurrent = true;

	assert(pSurface->m_pTex != NULL);
	if (!isCurrent)
	{
		glGenFramebuffersEXT(1, &fbo);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
		if ((pSurface->m_Usage * D3DUSAGE_RENDERTARGET) != D3DUSAGE_RENDERTARGET)
		{
			puts("WARNING: ColorFill: surface not a suitable render target");
		}
		assert(pSurface->m_pTex->m_nID != 0);
		glFramebufferTexture2DEXT(
				GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D,
				(GLuint)pSurface->m_pTex->m_nID,
				0);
	}
	if (pRect)
		glViewport(
				pRect->left,
				screenHeight - pRect->bottom,
				pRect->right - pRect->left,
				pRect->bottom - pRect->top);
	else
		glViewport(0, 0, pSurface->m_Width, pSurface->m_Height);
#if defined(PS3)
	glDepthRangef(0, 1);
#else
	glDepthRange(0, 1);
#endif
	glClearColor(r8 / 255.0f, g8 / 255.0f, b8 / 255.0f, a8 / 255.0f);

	switch (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
	  break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
	  IDirect3DDevice9_ColorFill_invalidRT(
				"incomplete framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
	  IDirect3DDevice9_ColorFill_invalidRT(
				"missing framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
	  IDirect3DDevice9_ColorFill_invalidRT(
				"framebuffer attachments have different dimensions");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
	  IDirect3DDevice9_ColorFill_invalidRT(
				"framebuffer color attachments have different internal formats");
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		IDirect3DDevice9_ColorFill_invalidRT(
				"framebuffer configuration unsupported");
		break;
	default:
	  assert(0);
	}

	glClear(GL_COLOR_BUFFER_BIT);

	if (!isCurrent)
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
		glDeleteFramebuffersEXT(1, &fbo);
		fbo = 0;
	}

	glViewport(
			m_CurViewport.X,
			screenHeight - m_CurViewport.Y - m_CurViewport.Height,
			m_CurViewport.Width,
			m_CurViewport.Height);
#if defined(PS3)
	glDepthRangef(m_CurViewport.MinZ, m_CurViewport.MaxZ);
#else
	glDepthRange(m_CurViewport.MinZ, m_CurViewport.MaxZ);
#endif
	
  return S_OK;
}

HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(
		UINT Width,
		UINT Height,
		D3DFORMAT Format,
		D3DPOOL Pool,
		IDirect3DSurface9** ppSurface,
		HANDLE* pSharedHandle
		)
{
  if (!ppSurface)
		return D3DERR_INVALIDCALL;
	if (Pool != D3DPOOL_MANAGED && Pool != D3DPOOL_SCRATCH)
		return D3DERR_INVALIDCALL;

#if 0
	GLenum format = 0;
	GLuint rId = 0;
	if (!D3DFORMAT2GL(Format, &format, NULL, NULL, NULL))
		return D3DERR_INVALIDCALL;

	glGenRenderbuffersEXT(1, &rId);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rId);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, format, Width, Height);

  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_Width = Width;
  pSurf->m_Height = Height;
  pSurf->m_Format = Format;
  pSurf->m_Usage = 0;
	pSurf->m_nID = (int)rId;
  *ppSurface = pSurf;
#else

	assert(0);

#endif

  return S_OK;
}

static void IDirect3DDevice9_SetRenderTarget_invalidRT(const char *message)
{
	printf(
			"ERROR: "
			"IDirect3DDevice9::SetRenderTarget: "
			"Invalid render target: %s\n",
			message);
}

HRESULT IDirect3DDevice9::SetRenderTarget(
		DWORD RenderTargetIndex,
		IDirect3DSurface9* pRenderTarget
		)
{
	GLenum err = GL_NO_ERROR;

  if (!pRenderTarget)
    return D3DERR_INVALIDCALL;
	if (pRenderTarget->m_Usage & D3DUSAGE_RENDERTARGET != D3DUSAGE_RENDERTARGET)
		return D3DERR_INVALIDCALL;

	if (pRenderTarget->m_nID == 0 && pRenderTarget->m_pTex == NULL)
	{
		// Back buffer.
		if (m_nCurFBO)
		{
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			glDeleteFramebuffersEXT(1, &m_nCurFBO);
			m_nCurFBO = 0;
		}
		if (m_nCurFBOZ)
		{
			glDeleteRenderbuffersEXT(1, &m_nCurFBOZ);
			m_nCurFBOZ = 0;
		}
		// The back buffer can be bound only to index 0.
		assert(RenderTargetIndex == 0);
	} else
	{
		// If the render target is not the back buffer, then we'll create and
		// bind a new framebuffer object. Unfortunately, we can not reuse the
		// z/stencil buffer of the back buffer, so we'll allocate one here.
		assert(RenderTargetIndex < 4);
		if (m_nCurFBO == 0)
		{
			glGenFramebuffersEXT(1, &m_nCurFBO);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
		}
		if (m_nCurFBOZ)
		{
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
					GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
			glDeleteRenderbuffersEXT(1, &m_nCurFBOZ);
			m_nCurFBOZ = 0;
		}
		glGenRenderbuffersEXT(1, &m_nCurFBOZ);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_nCurFBOZ);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
				pRenderTarget->m_Width, pRenderTarget->m_Height);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
				GL_RENDERBUFFER_EXT, m_nCurFBOZ);
		if (pRenderTarget->m_pTex)
		{
			IDirect3DBaseTexture9 *pTex = pRenderTarget->m_pTex;
			GLuint texId = (GLuint)pTex->m_nID;
#if defined(PS3)
			// Check if the texture format is supported as a render target.
			switch (pTex->m_Format)
			{
			case D3DFMT_X8R8G8B8:
			case D3DFMT_A8R8G8B8:
			case D3DFMT_A16B16G16R16F:
			case D3DFMT_A32B32G32R32F:
				break;
			default:
				IDirect3DDevice9_SetRenderTarget_invalidRT(
						"texture format not supported");
				break;
			}
#endif
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glFramebufferTexture2DEXT(
					GL_FRAMEBUFFER_EXT,
					GL_COLOR_ATTACHMENT0_EXT + RenderTargetIndex,
					GL_TEXTURE_2D,
					texId,
					0);
			if (m_ReqTS[m_nCurTMU].m_pTex)
			{
				SetTexture(m_nCurTMU, m_ReqTS[m_nCurTMU].m_pTex);
			}
		} else
			glFramebufferRenderbufferEXT(
					GL_FRAMEBUFFER_EXT,
					GL_COLOR_ATTACHMENT0_EXT + RenderTargetIndex,
					GL_RENDERBUFFER_EXT,
					(GLuint)pRenderTarget->m_nID);
	}

	switch (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
	  break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
	  IDirect3DDevice9_SetRenderTarget_invalidRT(
				"incomplete framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
	  IDirect3DDevice9_SetRenderTarget_invalidRT(
				"missing framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
	  IDirect3DDevice9_SetRenderTarget_invalidRT(
				"framebuffer attachments have different dimensions");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
	  IDirect3DDevice9_SetRenderTarget_invalidRT(
				"framebuffer color attachments have different internal formats");
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		IDirect3DDevice9_SetRenderTarget_invalidRT(
				"framebuffer configuration unsupported");
		break;
	default:
	  assert(0);
	}

	pRenderTarget->AddRef();
	SAFE_RELEASE(m_pCurTarget[RenderTargetIndex]);
	m_pCurTarget[RenderTargetIndex] = pRenderTarget;

	// Set the default viewport.
	D3DVIEWPORT9 viewport;
	viewport.X = 0;
	viewport.Y = 0;
	viewport.Width = pRenderTarget->m_Width;
	viewport.Height = pRenderTarget->m_Height;
	viewport.MinZ = 0.f;
	viewport.MaxZ = 1.f;
	SetViewport(&viewport);

  return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderTarget(
		DWORD RenderTargetIndex,
		IDirect3DSurface9** ppRenderTarget
		)
{
  if (!ppRenderTarget)
    return D3DERR_INVALIDCALL;

	assert(RenderTargetIndex < 4);
	if (!m_pCurTarget[RenderTargetIndex])
		return D3DERR_NOTFOUND;

	*ppRenderTarget = m_pCurTarget[RenderTargetIndex];
	(*ppRenderTarget)->AddRef();

  return S_OK;
}

static void IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
		const char *message
		)
{
	printf(
			"ERROR: "
			"IDirect3DDevice9::SetDepthStencilSurface: "
			"Invalid framebuffer: %s\n",
			message);
}

HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
  if (!pNewZStencil)
    return D3DERR_INVALIDCALL;

	assert(pNewZStencil->m_pTex == NULL);
	if (pNewZStencil->m_nID == 0)
	{
		// This surface represents the depth/stencil buffer of the back buffer.
		// This is basically a NO-OP.
	} else
	{
		if (m_nCurFBO == 0)
		{
			glGenFramebuffersEXT(1, &m_nCurFBO);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_nCurFBO);
		}
		// In DX we can set a depth/stencil buffer that is larger than the color
		// attachment(s).  This is not possible with PSGL, so we must check if the
		// size of the depth/stencil surface matches the size of the color
		// attachment(s) (we'll use the first color attachment for refernece).  In
		// case of a mismatch, we'll create a temporary depth/stencil buffer
		// instead.
		GLuint zStencilID = 0;
		if (m_pCurTarget[0] != NULL
				&& (m_pCurTarget[0]->m_Width != pNewZStencil->m_Width
					|| m_pCurTarget[0]->m_Height != pNewZStencil->m_Height))
		{
			if (m_nCurFBOZ != 0)
				glDeleteRenderbuffersEXT(1, &m_nCurFBOZ);
			glGenRenderbuffersEXT(1, &m_nCurFBOZ);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_nCurFBOZ);
			glRenderbufferStorageEXT(
					GL_RENDERBUFFER_EXT,
					GL_DEPTH_COMPONENT24,
					m_pCurTarget[0]->m_Width,
					m_pCurTarget[0]->m_Height);
			zStencilID = m_nCurFBOZ;
		} else
			zStencilID = (GLuint)pNewZStencil->m_nID;
		glFramebufferRenderbufferEXT(
				GL_FRAMEBUFFER_EXT,
				GL_DEPTH_ATTACHMENT_EXT,
				GL_RENDERBUFFER_EXT,
				zStencilID);
	}

	switch (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT))
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
	  break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
	  IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
				"incomplete framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
	  IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
				"missing framebuffer attachment");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
	  IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
				"framebuffer attachments have different dimensions");
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
	  IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
				"framebuffer color attachments have different internal formats");
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		IDirect3DDevice9_SetDepthStencilBuffer_invalidFB(
				"framebuffer configuration unsupported");
		break;
	default:
	  assert(0);
	}

	pNewZStencil->AddRef();
	SAFE_RELEASE(m_pCurZStencil);
	m_pCurZStencil = pNewZStencil;

  return S_OK;
}

HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
  if (!ppZStencilSurface)
    return D3DERR_INVALIDCALL;

	if (!m_pCurZStencil)
		return D3DERR_NOTFOUND;

	/*
  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_nType = D3DRTYPE_SURFACE;
  pSurf->m_Format = m_PresentParams.AutoDepthStencilFormat;
  pSurf->m_Width = m_PresentParams.BackBufferWidth;
  pSurf->m_Height = m_PresentParams.BackBufferHeight;
  pSurf->m_Pool = D3DPOOL_DEFAULT;
  pSurf->m_Usage = 0;
	*ppZStencilSurface = pSurf;
	*/

  *ppZStencilSurface = m_pCurZStencil;
	m_pCurZStencil->AddRef();

  return S_OK;
}

HRESULT IDirect3DDevice9::BeginScene()
{
  glFinish();

  return S_OK;
}

const char* ErrorString( GLenum errorCode )
{
   static char *tess_error[] = {
      "missing gluEndPolygon",
      "missing gluBeginPolygon",
      "misoriented contour",
      "vertex/edge intersection",
      "misoriented or self-intersecting loops",
      "coincident vertices",
      "colinear vertices",
      "intersecting edges",
      "not coplanar contours"
   };
   static char *nurbs_error[] = {
      "spline order un-supported",
      "too few knots",
      "valid knot range is empty",
      "decreasing knot sequence knot",
      "knot multiplicity greater than order of spline",
      "endcurve() must follow bgncurve()",
      "bgncurve() must precede endcurve()",
      "missing or extra geometric data",
      "can't draw pwlcurves",
      "missing bgncurve()",
      "missing bgnsurface()",
      "endtrim() must precede endsurface()",
      "bgnsurface() must precede endsurface()",
      "curve of improper type passed as trim curve",
      "bgnsurface() must precede bgntrim()",
      "endtrim() must follow bgntrim()",
      "bgntrim() must precede endtrim()",
      "invalid or missing trim curve",
      "bgntrim() must precede pwlcurve()",
      "pwlcurve referenced twice",
      "pwlcurve and nurbscurve mixed",
      "improper usage of trim data type",
      "nurbscurve referenced twice",
      "nurbscurve and pwlcurve mixed",
      "nurbssurface referenced twice",
      "invalid property",
      "endsurface() must follow bgnsurface()",
      "misoriented trim curves",
      "intersecting trim curves",
      "UNUSED",
      "unconnected trim curves",
      "unknown knot error",
      "negative vertex count encountered",
      "negative byte-stride encounteed",
      "unknown type descriptor",
      "null control array or knot vector",
      "duplicate point on pwlcurve"
   };

   /* GL Errors */
   if (errorCode==GL_NO_ERROR) {
      return "no error";
   }
   else if (errorCode==GL_INVALID_VALUE) {
      return "invalid value";
   }
   else if (errorCode==GL_INVALID_ENUM) {
      return "invalid enum";
   }
   else if (errorCode==GL_INVALID_OPERATION) {
      return "invalid operation";
   }
   else if (errorCode==GL_STACK_OVERFLOW) {
      return "stack overflow";
   }
   else if (errorCode==GL_STACK_UNDERFLOW) {
      return "stack underflow";
   }
   else if (errorCode==GL_OUT_OF_MEMORY) {
      return "out of memory";
   }
   return NULL;
}

HRESULT IDirect3DDevice9::EndScene()
{
  GLenum err = glGetError();
  int errnum=0;
  while (err != GL_NO_ERROR)
  {
    char * pErrText = (char*)ErrorString(err);
#if defined(PS3)
		if (err == GL_INVALID_FRAMEBUFFER_OPERATION_EXT)
			pErrText = "Invalid framebffer operation";
#endif
    iLog->Log( "glGetError: %s:",  pErrText ? pErrText : "-");

    err = glGetError();
    if (++errnum>10)
      break;
  }

  return S_OK;
}

HRESULT IDirect3DDevice9::Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
{
  int nGLFlags = 0;
  if (Flags & D3DCLEAR_TARGET)
  {
    ColorF cl = ColorF((uint)Color);
    glClearColor(cl.r, cl.g, cl.b, cl.a);
    nGLFlags |= GL_COLOR_BUFFER_BIT;
  }
  if (Flags & D3DCLEAR_ZBUFFER)
  {
    nGLFlags |= GL_DEPTH_BUFFER_BIT;
#ifndef PS3
    glClearDepth(Z);
#else
    glClearDepthf(Z);
#endif
  }
  if (Flags & D3DCLEAR_STENCIL)
    nGLFlags |= GL_STENCIL_BUFFER_BIT;

  glClear(nGLFlags);         

  return S_OK;
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
	int screenHeight = 768; // Guess, in case no target surface is set.

	if (m_pCurTarget[0] != NULL)
		screenHeight = m_pCurTarget[0]->m_Height;
  glViewport(
			pViewport->X,
			screenHeight - pViewport->Y - pViewport->Height,
			pViewport->Width,
			pViewport->Height);
#ifndef PS3
  glDepthRange(pViewport->MinZ, pViewport->MaxZ);
#else
  glDepthRangef(pViewport->MinZ, pViewport->MaxZ);
#endif
	m_CurViewport = *pViewport;

  return S_OK;
}

HRESULT IDirect3DDevice9::GetViewport(D3DVIEWPORT9* pViewport)
{
  *pViewport = m_CurViewport;
  return S_OK;
}

HRESULT IDirect3DDevice9::SetClipPlane(DWORD Index,CONST float* pPlane)
{
#if defined(_DEBUG)
	GLint maxPlanes;
	glGetIntegerv(GL_MAX_CLIP_PLANES, &maxPlanes);
	assert(Index < maxPlanes);
#endif
	GLenum nPlane = 0;
	switch(Index)
	{
	case 0:
		nPlane = GL_CLIP_PLANE0;
		break;
	case 1:
		nPlane = GL_CLIP_PLANE1;
		break;
	case 2:
		nPlane = GL_CLIP_PLANE2;
		break;
	case 3:
		nPlane = GL_CLIP_PLANE3;
		break;
	case 4:
		nPlane = GL_CLIP_PLANE4;
		break;
	case 5:
		nPlane = GL_CLIP_PLANE5;
		break;
	default:
		assert(0);
		return S_FALSE;
	}
	glEnable(nPlane);
#if defined(PS3)
	glClipPlanef(nPlane, pPlane);
#else
	GLdouble planeEq[4] = { pPlane[0], pPlane[1], pPlane[2], pPlane[3] };
	glClipPlane(nPlane, planeEq);
#endif
  return S_OK;
}

HRESULT IDirect3DDevice9::GetClipPlane(DWORD Index,float* pPlane)
{
  assert(0);
  return S_OK;
}

static int sAB_D3D92OGL(int nValue)
{
  switch (nValue)
  {
    case D3DBLEND_ZERO:
      return GL_ZERO;
    case D3DBLEND_ONE:
      return GL_ONE;
    case D3DBLEND_SRCCOLOR:
      return GL_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:
      return GL_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:
      return GL_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:
      return GL_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:
      return GL_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:
      return GL_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:
      return GL_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:
      return GL_ONE_MINUS_DST_COLOR;
    default:
      assert(0);
  }
  return -1;
}

HRESULT IDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
{
  switch (State)
  {
    case D3DRS_CULLMODE:
      {
        switch (Value)
        {
          case D3DCULL_NONE:
            glDisable(GL_CULL_FACE);
            break;
          case D3DCULL_CW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
          case D3DCULL_CCW:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
          default:
            assert(0);
        }
      }
      break;

      case D3DRS_ALPHATESTENABLE:
        if (Value)
          glEnable(GL_ALPHA_TEST);
        else
          glDisable(GL_ALPHA_TEST);
        m_bAT_Enabled = Value;
        break;

      case D3DRS_ALPHAREF:
        m_fAT_ReqVal = (float)Value / 255.0f;
        m_bAT_Dirty = true;
        break;

      case D3DRS_ALPHAFUNC:
        {
          switch (Value)
          {
            case D3DCMP_GREATER:
              m_nAT_ReqOp = GL_GREATER;
              break;
            case D3DCMP_EQUAL:
              m_nAT_ReqOp = GL_EQUAL;
              break;
            case D3DCMP_LESS:
              m_nAT_ReqOp = GL_EQUAL;
              break;
            case D3DCMP_LESSEQUAL:
              m_nAT_ReqOp = GL_LEQUAL;
              break;
            case D3DCMP_GREATEREQUAL:
              m_nAT_ReqOp = GL_GEQUAL;
              break;
            default:
              assert(0);
          }
        }
        m_bAT_Dirty = true;
        break;

      case D3DRS_FILLMODE:
        {
          switch (Value)
          {
            case D3DFILL_SOLID:
              glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
              break;
            case D3DFILL_WIREFRAME:
              glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
              break;
            case D3DFILL_POINT:
              glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
              break;
            default:
              assert(0);
          }
        }
        break;

      case D3DRS_ZFUNC:
        {
          switch (Value)
          {
            case D3DCMP_LESSEQUAL:
              glDepthFunc(GL_LEQUAL);
              break;
            case D3DCMP_EQUAL:
              glDepthFunc(GL_EQUAL);
              break;
            case D3DCMP_GREATER:
              glDepthFunc(GL_GREATER);
              break;
            case D3DCMP_LESS:
              glDepthFunc(GL_LESS);
              break;
            case D3DCMP_GREATEREQUAL:
              glDepthFunc(GL_GEQUAL);
              break;
            default:
              assert(0);
          }
        }
        break;

      case D3DRS_ZWRITEENABLE:
        glDepthMask(Value);
        break;

      case D3DRS_ZENABLE:
        if (!Value)
          glDisable(GL_DEPTH_TEST);
        else
          glEnable(GL_DEPTH_TEST);
				m_bDT_Enabled = Value;
        break;

      case D3DRS_DITHERENABLE:
        //if (!Value)
          glDisable(GL_DITHER);
        //else
        //  glEnable(GL_DITHER);
        break;

      case D3DRS_CLIPPLANEENABLE:
        {
          int i;
          for (i=0; i<m_pD3D->m_DeviceCaps.MaxUserClipPlanes; i++)
          {
            int nBit = 1<<i;
            if (Value ^ m_nCP_Mask & nBit)
            {
              if (!(Value & nBit))
              {
                m_nCP_Mask &= ~nBit;
#ifndef PS3
                glDisable(GL_CLIP_PLANE0 + i);
#endif
              }
              else
              {
                m_nCP_Mask |= nBit;
#ifndef PS3
                glEnable(GL_CLIP_PLANE0 + i);
#endif
              }
            }
          }
        }
        break;

      case D3DRS_STENCILENABLE:
        if (!Value)
          glDisable(GL_STENCIL_TEST);
        else
          glEnable(GL_STENCIL_TEST);
        break;

      case D3DRS_TWOSIDEDSTENCILMODE:
        {
#ifndef PS3
          if (SUPPORTS_GL_EXT_stencil_two_side)
          {
            if (Value)
              glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            else
              glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
          }
          else
            assert(0);
#else
          assert(0);
#endif
        }
        break;
      case D3DRS_CCW_STENCILFAIL:
        break;
      case D3DRS_CCW_STENCILZFAIL:
        break;
      case D3DRS_CCW_STENCILPASS:
        break;
      case D3DRS_CCW_STENCILFUNC:
        break;

      case D3DRS_STENCILMASK:
        m_nSTENC_mask = Value;
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_STENCILREF:
        m_nSTENC_ref = Value;
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_STENCILFUNC:
        {
          switch (Value)
          {
            case D3DCMP_ALWAYS:
              m_nSTENC_func = GL_ALWAYS;
              break;
            case D3DCMP_NEVER:
              m_nSTENC_func = GL_NEVER;
              break;
            case D3DCMP_EQUAL:
              m_nSTENC_func = GL_EQUAL;
              break;
            case D3DCMP_NOTEQUAL:
              m_nSTENC_func = GL_NOTEQUAL;
              break;
            case D3DCMP_LESS:
              m_nSTENC_func = GL_LESS;
              break;
            case D3DCMP_GREATER:
              m_nSTENC_func = GL_GREATER;
              break;
            case D3DCMP_GREATEREQUAL:
              m_nSTENC_func = GL_GEQUAL;
              break;
            case D3DCMP_LESSEQUAL:
              m_nSTENC_func = GL_LEQUAL;
              break;
          }
        }
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_STENCILFAIL:
        {
          switch (Value)
          {
            case D3DSTENCILOP_KEEP:
              m_nSTENC_OpFail = GL_KEEP;
              break;
            case D3DSTENCILOP_REPLACE:
              m_nSTENC_OpFail = GL_REPLACE;
              break;
            case D3DSTENCILOP_INCR:
#ifndef PS3
              m_nSTENC_OpFail = GL_INCR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_DECR:
#ifndef PS3
              m_nSTENC_OpFail = GL_DECR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_INCRSAT:
              m_nSTENC_OpFail = GL_INCR;
              break;
            case D3DSTENCILOP_DECRSAT:
              m_nSTENC_OpFail = GL_DECR;
              break;
          }
        }
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_STENCILZFAIL:
        {
          switch (Value)
          {
            case D3DSTENCILOP_KEEP:
              m_nSTENC_OpZFail = GL_KEEP;
              break;
            case D3DSTENCILOP_REPLACE:
              m_nSTENC_OpZFail = GL_REPLACE;
              break;
            case D3DSTENCILOP_INCR:
#ifndef PS3
              m_nSTENC_OpZFail = GL_INCR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_DECR:
#ifndef PS3
              m_nSTENC_OpZFail = GL_DECR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_INCRSAT:
              m_nSTENC_OpZFail = GL_INCR;
              break;
            case D3DSTENCILOP_DECRSAT:
              m_nSTENC_OpZFail = GL_DECR;
              break;
          }
        }
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_STENCILPASS:
        {
          switch (Value)
          {
            case D3DSTENCILOP_KEEP:
              m_nSTENC_OpPass = GL_KEEP;
              break;
            case D3DSTENCILOP_REPLACE:
              m_nSTENC_OpPass = GL_REPLACE;
              break;
            case D3DSTENCILOP_INCR:
#ifndef PS3
              m_nSTENC_OpPass = GL_INCR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_DECR:
#ifndef PS3
              m_nSTENC_OpPass = GL_DECR_WRAP_EXT;
#else
              assert(0);
#endif
              break;
            case D3DSTENCILOP_INCRSAT:
              m_nSTENC_OpPass = GL_INCR;
              break;
            case D3DSTENCILOP_DECRSAT:
              m_nSTENC_OpPass = GL_DECR;
              break;
          }
        }
        m_bSTENC_Dirty = true;
        break;

      case D3DRS_SRCBLEND:
        m_nAB_Src = sAB_D3D92OGL(Value);
        m_bAB_Dirty = true;
        break;

      case D3DRS_DESTBLEND:
        m_nAB_Dst = sAB_D3D92OGL(Value);
        m_bAB_Dirty = true;
        break;

      case D3DRS_ALPHABLENDENABLE:
				m_bAB_Enabled = Value;
        if (m_bAB_Enabled)
          glEnable(GL_BLEND);
        else
          glDisable(GL_BLEND);
        break;

			case D3DRS_SRCBLENDALPHA:
				// FIXME: implement!
				break;

			case D3DRS_DESTBLENDALPHA:
				// FIXME: implement!
				break;

			case D3DRS_SEPARATEALPHABLENDENABLE:
				// FIXME: implement!
				break;

			case D3DRS_SRGBWRITEENABLE:
				// FIXME: implement!
				break;

      case D3DRS_DEPTHBIAS:
        if (Value)
        {
          glPolygonOffset(-1.0f, (float)Value);
          glEnable(GL_POLYGON_OFFSET_FILL);
        }
        else
        {
          glDisable(GL_POLYGON_OFFSET_FILL);
        }
        break;

      case D3DRS_COLORWRITEENABLE:
        {
          GLboolean r = (Value & 1) != 0;
          GLboolean g = (Value & 2) != 0;
          GLboolean b = (Value & 4) != 0;
          GLboolean a = (Value & 8) != 0;
          glColorMask(r, g, b, a);
        }
        break;
			case D3DRS_SCISSORTESTENABLE:
        if(Value)
					glEnable(GL_SCISSOR_TEST);
				else
					glDisable(GL_SCISSOR_TEST);
				break;
			case D3DRS_POINTSIZE:
				if(Value == MAKEFOURCC('I', 'N', 'S', 'T'))
					return S_OK;
				glPointSize(*(float*)&Value);
				break;
    default:
      assert(0);
  }
  return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderState(D3DRENDERSTATETYPE State,DWORD* pValue)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture)
{
	IDirect3DBaseTexture9 *pPrevTexture = m_ReqTS[Stage].m_pTex;
  m_ReqTS[Stage].m_pTex = pTexture;
  if (pTexture == NULL)
	{
		if (pPrevTexture != NULL)
			pPrevTexture->Release();
    return S_OK;
	}
	if (pTexture != pPrevTexture)
	{
		if (pTexture != NULL)
			pTexture->AddRef();
		if (pPrevTexture != NULL)
			pPrevTexture->Release();
	}
  assert (pTexture->m_nID);
  int target = pTexture->m_nGLtarget;

  SelectTMU(Stage);
  glEnable(target);
  glBindTexture(target, pTexture->m_nID);

  return S_OK;
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue)
{
  assert(0);
  return S_OK;
}

static int sD3D9TexAddress2GL(DWORD Value)
{
  switch (Value)
  {
    case D3DTADDRESS_WRAP:
      return GL_REPEAT;
    case D3DTADDRESS_CLAMP:
      return GL_CLAMP;
    case D3DTADDRESS_BORDER:
      return GL_CLAMP_TO_EDGE;
    default:
      assert(0);
  }
  return 0;
}

HRESULT IDirect3DDevice9::SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
{
  switch (Type)
  {
    case D3DSAMP_ADDRESSU:
      m_ReqTS[Sampler].nAddressU = sD3D9TexAddress2GL(Value);
      break;
    case D3DSAMP_ADDRESSV:
      m_ReqTS[Sampler].nAddressV = sD3D9TexAddress2GL(Value);
      break;
    case D3DSAMP_ADDRESSW:
      m_ReqTS[Sampler].nAddressW = sD3D9TexAddress2GL(Value);
      break;
    case D3DSAMP_MINFILTER:
      m_ReqTS[Sampler].nD3DMinFilter = Value;
      break;
    case D3DSAMP_MAGFILTER:
      m_ReqTS[Sampler].nD3DMagFilter = Value;
      break;
    case D3DSAMP_MIPFILTER:
      m_ReqTS[Sampler].nD3DMipFilter = Value;
      break;

    case D3DSAMP_MAXANISOTROPY:
      m_ReqTS[Sampler].nAnisotropy = Value;
      break;

    default:
      assert(0);
  }
  return S_OK;
}

HRESULT IDirect3DDevice9::SetScissorRect(CONST RECT* pRect)
{
	assert(pRect);
	m_CurScissorRect = *pRect;
  glScissor
	(
		m_CurScissorRect.left, 
		m_CurScissorRect.top, 
		m_CurScissorRect.right - m_CurScissorRect.left, 
		m_CurScissorRect.bottom - m_CurScissorRect.top
	);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetScissorRect(RECT* pRect)
{
	assert(pRect);
	*pRect = m_CurScissorRect;
  return S_OK;
}

HRESULT IDirect3DDevice9::SetNPatchMode(float nSegments)
{
  assert(0);
  return S_OK;
}

float IDirect3DDevice9::GetNPatchMode()
{
  assert(0);
  return S_OK;
}

int sSizeFromDeclType(byte nType)
{
  switch (nType)
  {
    case D3DDECLTYPE_FLOAT1: 
      return 1;
    case D3DDECLTYPE_FLOAT2: 
      return 2;
    case D3DDECLTYPE_FLOAT3: 
      return 3;
    case D3DDECLTYPE_FLOAT4: 
    case D3DDECLTYPE_D3DCOLOR: 
    case D3DDECLTYPE_UBYTE4: 
    case D3DDECLTYPE_UBYTE4N: 
      return 4;
    case D3DDECLTYPE_SHORT2: 
    case D3DDECLTYPE_SHORT2N: 
    case D3DDECLTYPE_USHORT2N: 
      return 2;
    case D3DDECLTYPE_SHORT4: 
    case D3DDECLTYPE_SHORT4N: 
    case D3DDECLTYPE_USHORT4N: 
      return 4;
    default:
      assert(0);
      break;
  }
  return 0;
}

int sGLTypeFromDeclType(byte nType)
{
  switch (nType)
  {
    case D3DDECLTYPE_FLOAT1: 
    case D3DDECLTYPE_FLOAT2: 
    case D3DDECLTYPE_FLOAT3: 
    case D3DDECLTYPE_FLOAT4: 
      return GL_FLOAT;
    case D3DDECLTYPE_D3DCOLOR: 
    case D3DDECLTYPE_UBYTE4: 
    case D3DDECLTYPE_UBYTE4N: 
      return GL_UNSIGNED_BYTE;
    case D3DDECLTYPE_SHORT2: 
    case D3DDECLTYPE_SHORT2N: 
    case D3DDECLTYPE_SHORT4: 
    case D3DDECLTYPE_SHORT4N: 
      return GL_SHORT;
    case D3DDECLTYPE_USHORT2N: 
    case D3DDECLTYPE_USHORT4N: 
      return GL_UNSIGNED_SHORT;
    default:
      assert(0);
      break;
  }
  return 0;
}

int sNormalizedFromDeclType(byte nType)
{
  switch (nType)
  {
    case D3DDECLTYPE_FLOAT1: 
    case D3DDECLTYPE_FLOAT2: 
    case D3DDECLTYPE_FLOAT3: 
    case D3DDECLTYPE_FLOAT4: 
    case D3DDECLTYPE_UBYTE4: 
    case D3DDECLTYPE_SHORT2: 
    case D3DDECLTYPE_SHORT4: 
      return GL_FALSE;
    case D3DDECLTYPE_D3DCOLOR: 
    case D3DDECLTYPE_UBYTE4N: 
    case D3DDECLTYPE_SHORT2N: 
    case D3DDECLTYPE_SHORT4N: 
    case D3DDECLTYPE_USHORT2N: 
    case D3DDECLTYPE_USHORT4N: 
      return GL_TRUE;
    default:
      assert(0);
      break;
  }
  return GL_FALSE;
}

bool Commit_Dump = false;
bool Commit_SetAllStreams = false;

HRESULT IDirect3DDevice9::Commit(int nFirstVertex)
{
  // Commit streams
  int i;
  IDirect3DVertexDeclaration9 *pDecl = m_pCurVDeclaration;

  int nID = m_nCurrentVB;
  for (i=0; i<pDecl->m_Declaration.Num(); i++)
  {
    D3DVERTEXELEMENT9 *pElem = &pDecl->m_Declaration[i];
    SGLStreamData *pStream = &m_CurStream[pElem->Stream];

// XXX Debug code.
if (Commit_Dump)
{
printf(
"Commit: i=%i last_vertex_index=%i first_index=%i prev_vb_id(nID)=%i\n"
"  element = { size=%i stream=%i offset=%i type=%i method=%i usage=%i usage_index=%i }\n"
"  stream = { buf=0x%x offset=%i stride=%i dirty=%s touched=%s }\n",
i, m_nLastVertexIndex, nFirstVertex, nID,
sSizeFromDeclType(pElem->Type),
(int)pElem->Stream, (int)pElem->Offset, (int)pElem->Type, (int)pElem->Method,
(int)pElem->Usage, (int)pElem->UsageIndex,
(unsigned)(UINT_PTR)pStream->m_pBuf, pStream->m_nOffset, pStream->m_nStride,
pStream->m_bDirty ? "true" : "false", pStream->m_bTouched ? "true" : "false");
}

    if (pStream->m_bDirty || m_nLastVertexIndex != nFirstVertex
				|| Commit_SetAllStreams
				)
    {
      pStream->m_bTouched = true;
      IDirect3DVertexBuffer9 *pBuf = pStream->m_pBuf;
      if (!pBuf)
        continue;
      if (nID != pBuf->m_nID)
      {
        nID = pBuf->m_nID;
#ifndef PS3
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, nID);
#else
        glBindBuffer(GL_ARRAY_BUFFER, nID);
#endif
      }
			char *BufferOffset;
			BufferOffset = BUFFER_OFFSET(
					pStream->m_nOffset
					+ pElem->Offset
					+ nFirstVertex * pStream->m_nStride);
      switch (pElem->UsageIndex)
      {
        case FSM_POSITION:
          glVertexPointer(
							sSizeFromDeclType(pElem->Type),
							GL_FLOAT,
							pStream->m_nStride,
							BufferOffset);
          break;
        case FSM_COLOR0:
          glColorPointer(
							sSizeFromDeclType(pElem->Type),
							GL_UNSIGNED_BYTE,
							pStream->m_nStride,
							BufferOffset);
          break;
        case FSM_COLOR1:
#ifndef PS3
          glSecondaryColorPointerEXT(
							sSizeFromDeclType(pElem->Type),
							GL_UNSIGNED_BYTE,
							pStream->m_nStride,
							BufferOffset);
#else
          cgGLAttribPointer(
							FSM_COLOR1,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							sNormalizedFromDeclType(pElem->Type),
							0,
							BufferOffset);
#endif
          break;
        case FSM_TANGENT:
#ifndef PS3
					glVertexAttribPointerARB(
							FSM_TANGENT_USAGE_INDEX,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							GL_FALSE,
							pStream->m_nStride,
							BufferOffset);
#else
					cgGLAttribPointer(
							FSM_TANGENT_USAGE_INDEX,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							sNormalizedFromDeclType(pElem->Type),
							pStream->m_nStride,
							BufferOffset);
#endif
					break;
        case FSM_BINORMAL:
#ifndef PS3
					glVertexAttribPointerARB(
							FSM_BINORMAL_USAGE_INDEX,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							GL_FALSE,
							pStream->m_nStride,
							BufferOffset);
#else
					cgGLAttribPointer(
							FSM_BINORMAL_USAGE_INDEX,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							sNormalizedFromDeclType(pElem->Type),
							pStream->m_nStride,
							BufferOffset);
#endif
					break;
        case FSM_BLENDWEIGHT:
        case FSM_BLENDINDICES:
				case FSM_PSIZE:
				case FSM_TESSFACTOR:
#ifndef PS3
          glVertexAttribPointerARB(
							pElem->UsageIndex,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							GL_FALSE,
							pStream->m_nStride,
							BufferOffset);
#else
          cgGLAttribPointer(
							pElem->UsageIndex,
							sSizeFromDeclType(pElem->Type),
							sGLTypeFromDeclType(pElem->Type),
							sNormalizedFromDeclType(pElem->Type),
							pStream->m_nStride,
							BufferOffset);
#endif
          break;
        case FSM_NORMAL:
          glNormalPointer(GL_FLOAT, pStream->m_nStride, BufferOffset);
          break;
        case FSM_TEXCOORD0:
        case FSM_TEXCOORD1:
        case FSM_TEXCOORD2:
        case FSM_TEXCOORD3:
				case FSM_TEXCOORD4:
				case FSM_TEXCOORD5:
				case FSM_TEXCOORD6:
				case FSM_TEXCOORD7:
          SelectTMU(pElem->UsageIndex - FSM_TEXCOORD0);
          glTexCoordPointer(
							sSizeFromDeclType(pElem->Type),
							GL_FLOAT,
							pStream->m_nStride,
							BufferOffset);
          break;
        default:
          assert(0);
      }
    }
  }
  m_nCurrentVB = nID;
  m_nLastVertexIndex = nFirstVertex;
  for (i=0; i<MAX_STREAMS; i++)
  {
    if (m_CurStream[i].m_bTouched)
    {
      m_CurStream[i].m_bDirty = false;
      m_CurStream[i].m_bTouched = false;
    }
  }

  // Commit states
  if (m_bAB_Dirty && m_bAB_Enabled)
  {
    m_bAB_Dirty = false;
    glBlendFunc(m_nAB_Src, m_nAB_Dst);
  }
  if (m_bAT_Dirty && m_bAT_Enabled)
  {
    m_bAT_Dirty = false;
    glAlphaFunc(m_nAT_ReqOp, m_fAT_ReqVal);
  }

  // Commit texture states
  for (i=0; i<MAX_TMU; i++)
  {
    if (!m_ReqTS[i].m_pTex)
      continue;
    IDirect3DBaseTexture9 *pTex = m_ReqTS[i].m_pTex;
    int nGLTarget = pTex->m_nGLtarget;
		SetTexture(i, pTex);
    if (m_ReqTS[i].nAddressU != pTex->m_CurState.nAddressU)
    {
      pTex->m_CurState.nAddressU = m_ReqTS[i].nAddressU;
      glTexParameteri(nGLTarget, GL_TEXTURE_WRAP_S, m_ReqTS[i].nAddressU);
    }
    if (m_ReqTS[i].nAddressV != pTex->m_CurState.nAddressV)
    {
      pTex->m_CurState.nAddressV = m_ReqTS[i].nAddressV;
      glTexParameteri(nGLTarget, GL_TEXTURE_WRAP_T, m_ReqTS[i].nAddressV);
    }
    if (m_ReqTS[i].nAddressW != pTex->m_CurState.nAddressW)
    {
      pTex->m_CurState.nAddressW = m_ReqTS[i].nAddressW;
      glTexParameteri(nGLTarget, GL_TEXTURE_WRAP_R, m_ReqTS[i].nAddressW);
    }

    if (m_ReqTS[i].nD3DMinFilter != pTex->m_CurState.nD3DMinFilter
				|| m_ReqTS[i].nD3DMagFilter != pTex->m_CurState.nD3DMagFilter
				|| m_ReqTS[i].nD3DMipFilter != pTex->m_CurState.nD3DMipFilter)
    {
      pTex->m_CurState.nD3DMinFilter = m_ReqTS[i].nD3DMinFilter;
      pTex->m_CurState.nD3DMagFilter = m_ReqTS[i].nD3DMagFilter;
      pTex->m_CurState.nD3DMipFilter = m_ReqTS[i].nD3DMipFilter;

      if (m_ReqTS[i].nD3DMagFilter == D3DTEXF_LINEAR)
        glTexParameteri(nGLTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      else
      if (m_ReqTS[i].nD3DMagFilter == D3DTEXF_POINT)
        glTexParameteri(nGLTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      else
      if (m_ReqTS[i].nD3DMagFilter == D3DTEXF_ANISOTROPIC)
        glTexParameteri(nGLTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      else
        assert(0);
      if (m_ReqTS[i].nD3DMinFilter == D3DTEXF_LINEAR
					|| m_ReqTS[i].nD3DMinFilter == D3DTEXF_ANISOTROPIC)
      {
				if (pTex->m_nLevelCount > 1)
				{
					if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_LINEAR)
						glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					else
					if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_POINT)
						glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
					else
					if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_NONE)
						glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					else
						assert(0);
				}
				else
				{
					glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				}
      }
      else
      if (m_ReqTS[i].nD3DMinFilter == D3DTEXF_POINT)
      {
        if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_LINEAR)
          glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        else
        if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_POINT)
          glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        else
        if (m_ReqTS[i].nD3DMipFilter == D3DTEXF_NONE)
          glTexParameteri(nGLTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        else
          assert(0);
      }
      else
        assert(0);
    }

  }

  return S_OK;
}

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
  Commit(0);

  int nGLPrim = 0;
  int nGLVerts = 0;
  switch (PrimitiveType)
  {
    case D3DPT_TRIANGLELIST:
      nGLPrim = GL_TRIANGLES;
      nGLVerts = PrimitiveCount * 3;
      break;
    case D3DPT_TRIANGLESTRIP:
      nGLPrim = GL_TRIANGLE_STRIP;
      nGLVerts = PrimitiveCount + 2;
      break;
    case D3DPT_TRIANGLEFAN:
      nGLPrim = GL_TRIANGLE_FAN;
      nGLVerts = PrimitiveCount + 2;
      break;
    case D3DPT_LINELIST:
      nGLPrim = GL_LINES;
      nGLVerts = PrimitiveCount * 2;
      break;
    default:
      assert(0);
  }

  glDrawArrays(nGLPrim, StartVertex, nGLVerts);
#ifdef PSGL_DEBUG_SWAP
	if (psglSwap_EnableDebug)
	{
		psglSwap();
		glFinish();
		psglSwap_Break();
		psglSwap();
	}
#endif

  return S_OK;
}

HRESULT IDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE Prim,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
  if (!m_pCurIB)
    return D3DERR_INVALIDCALL;

  Commit(BaseVertexIndex);

  int nGLPrim = 0;
  int nGLInds = 0;
  switch (Prim)
  {
    case D3DPT_TRIANGLELIST:
      nGLPrim = GL_TRIANGLES;
      nGLInds = primCount * 3;
      break;
    case D3DPT_TRIANGLESTRIP:
      nGLPrim = GL_TRIANGLE_STRIP;
      nGLInds = primCount + 2;
      break;
    case D3DPT_TRIANGLEFAN:
      nGLPrim = GL_TRIANGLE_FAN;
      nGLInds = primCount + 2;
      break;
    case D3DPT_LINELIST:
      nGLPrim = GL_LINES;
      nGLInds = primCount * 2;
      break;
    default:
      assert(0);
  }

  glDrawElements(nGLPrim, nGLInds, GL_UNSIGNED_SHORT, BUFFER_OFFSET(startIndex*sizeof(short)));

#ifdef PSGL_DEBUG_SWAP
	if (psglSwap_EnableDebug)
	{
		psglSwap();
		glFinish();
		psglSwap_Break();
		psglSwap();
	}
#endif

  return S_OK;
}

HRESULT IDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
  if (!ppDecl)
    return D3DERR_INVALIDCALL;
  IDirect3DVertexDeclaration9 *pDecl = new IDirect3DVertexDeclaration9;

  while (pVertexElements->Stream != 0xff)
  {
    pDecl->m_Declaration.AddElem(*pVertexElements);
    pVertexElements++;
  }
  pDecl->m_Declaration.Shrink();
  int i;
  TArray<int> Conflicts;
  bool bUsed[16];
  memset(bUsed, 0, sizeof(bool) * 16);
  for (i=0; i<pDecl->m_Declaration.Num(); i++)
  {
    D3DVERTEXELEMENT9 *pElem = &pDecl->m_Declaration[i];
    switch (pElem->Usage)
    {
      case D3DDECLUSAGE_POSITION:
        if (pElem->UsageIndex == 0)
        {
          bUsed[FSM_POSITION] = true;
          pElem->UsageIndex = FSM_POSITION;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_POSITION with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_COLOR:
        if (pElem->UsageIndex <= 1)
        {
          pElem->UsageIndex = FSM_COLOR0+pElem->UsageIndex;
          bUsed[pElem->UsageIndex] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_COLOR with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_TEXCOORD:
				assert(pElem->UsageIndex <= 7);
        pElem->UsageIndex = FSM_TEXCOORD0+pElem->UsageIndex;
        bUsed[pElem->UsageIndex] = true;
        break;
      case D3DDECLUSAGE_TANGENT:
        if (pElem->UsageIndex == 0)
        {
          pElem->UsageIndex = FSM_TANGENT;
					if(bUsed[FSM_TANGENT_USAGE_INDEX] == true)
						iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_TANGENT collides with TEXCOORD6");
          bUsed[FSM_TANGENT_USAGE_INDEX] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_TANGENT with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_BINORMAL:
        if (pElem->UsageIndex == 0)
        {
          pElem->UsageIndex = FSM_BINORMAL;
					if(bUsed[FSM_BINORMAL_USAGE_INDEX] == true)
						iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_BINORMAL collides with TEXCOORD7");
          bUsed[FSM_BINORMAL_USAGE_INDEX] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_BINORMAL with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_BLENDWEIGHT:
        if (pElem->UsageIndex == 0)
        {
          pElem->UsageIndex = FSM_BLENDWEIGHT;
          bUsed[pElem->UsageIndex] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_BLENDWEIGHT with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_BLENDINDICES:
        if (pElem->UsageIndex == 0)
        {
          pElem->UsageIndex = FSM_BLENDINDICES;
          bUsed[pElem->UsageIndex] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_BLENDINDICES with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
      case D3DDECLUSAGE_NORMAL:
        if (pElem->UsageIndex == 0)
        {
          pElem->UsageIndex = FSM_NORMAL;
          bUsed[pElem->UsageIndex] = true;
          break;
        }
				iLog->Log("Warning: IDirect3DDevice9::CreateVertexDeclaration: D3DDECLUSAGE_NORMAL with usage index %d", pElem->UsageIndex);
        Conflicts.AddElem(i);
        break;
    }
  }
#if 1
  for (i=0; i<Conflicts.Num(); i++)
  {
    D3DVERTEXELEMENT9 *pElem = &pDecl->m_Declaration[Conflicts[i]];
    int j;
    for (j=0; j<16; j++)
    {
      if (!bUsed[j])
      {
        bUsed[j] = true;
printf("CreateVertexDeclaration: remapping Usage=%i, UsageIndex=%i -> ATTR%i\n",
(int)pElem->Usage, (int)pElem->UsageIndex, j);
        pElem->UsageIndex = j;
        break;
      }
    }
    if (j == 16)
    {
      assert(0);
      iLog->Log("Error: IDirect3DDevice9::CreateVertexDeclaration failed - not enough input attributes");
      return D3DERR_INVALIDCALL;
    }
  }
#endif
  pDecl->m_nStreamMask = 0;
  for (i=0; i<pDecl->m_Declaration.Num(); i++)
    pDecl->m_nStreamMask |= 1 << pDecl->m_Declaration[i].UsageIndex;

  *ppDecl = pDecl;

  return S_OK;
}

inline void IDirect3DDevice9::SetVertexAttrib(int i, int nMask)
{
  bool bEnable = (nMask & (1<<i)) ? true : false;

	if (i == FSM_TANGENT_USAGE_INDEX)
		bEnable = (nMask & ((1 << i) | (1 << FSM_TANGENT))) ? true : false;
	else if (i == FSM_BINORMAL_USAGE_INDEX)
		bEnable = (nMask & ((1 << i) | (1 << FSM_BINORMAL))) ? true : false;
	if (i >= 16)
		return;

	switch (i)
	{
	case FSM_POSITION:
		m_bVA_Enabled = bEnable;
		break;
	case FSM_TEXCOORD0:
	case FSM_TEXCOORD1:
	case FSM_TEXCOORD2:
	case FSM_TEXCOORD3:
	case FSM_TEXCOORD4:
	case FSM_TEXCOORD5:
	case FSM_TEXCOORD6:
	case FSM_TEXCOORD7:
		SelectTMU(i - FSM_TEXCOORD0);
		m_bTCA_Enabled = bEnable;
		break;
	case FSM_COLOR0:
		m_bCA_Enabled = bEnable;
		break;
	case FSM_NORMAL:
		m_bNA_Enabled = bEnable;
		break;
	default:
		break;
	}

#if 0
	// XXX Disabled temporarily
#if defined(PS3)
	if (m_pCurVShader != NULL)
	{
		int tempI = i;
		//tangent and binormal are mapped to index 16 and 17 which are outside the 0..15 range
		//so remap them
#ifdef FIXME__NO_TANGENT_BINORMAL
		assert(tempI <= 15);
#else
		if(tempI == FSM_TANGENT)
			tempI = FSM_TANGENT_USAGE_INDEX;
		else
		if(tempI == FSM_BINORMAL)
			tempI = FSM_BINORMAL_USAGE_INDEX;
#endif
		if(m_pCurVShader->m_streamId[tempI] != -1)
		{
#if CELL_SDK_VERSION > 0x080000
			// Starting with SDK 0.8.x, attributes may be enabled directly through
			// cgGLEnableAttrib/cgGLDisableAttrib.
			if (bEnable)
				cgGLEnableAttrib(tempI);
			else
				cgGLDisableAttrib(tempI);
#else
			CGparameter Param = (CGparameter)(INT_PTR)m_pCurVShader->m_streamId[tempI];
			if (bEnable)
				cgGLEnableClientState(Param);
			else
				cgGLDisableClientState(Param);
#endif
			return;
		}
	}
#endif
#endif

  switch (i)
  {
    case FSM_POSITION:
      if (bEnable)
        glEnableClientState(GL_VERTEX_ARRAY);
      else
        glDisableClientState(GL_VERTEX_ARRAY);
      break;
    case FSM_TEXCOORD0:
    case FSM_TEXCOORD1:
    case FSM_TEXCOORD2:
    case FSM_TEXCOORD3:
		case FSM_TEXCOORD4:
		case FSM_TEXCOORD5:
		case FSM_TEXCOORD6:
		case FSM_TEXCOORD7:
      if (bEnable)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      break;
    case FSM_COLOR0:
      if (bEnable)
        glEnableClientState(GL_COLOR_ARRAY);
      else
        glDisableClientState(GL_COLOR_ARRAY);
      break;
    case FSM_COLOR1:
#ifndef PS3
      if (bEnable)
        glEnableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);
      else
        glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);
#else
  #if CELL_SDK_VERSION > 0x080000
			if (bEnable)
				cgGLEnableAttrib(FSM_COLOR1);
			else
				cgGLDisableAttrib(FSM_COLOR1);
  #endif
#endif
			break;
    case FSM_NORMAL:
      if (bEnable)
        glEnableClientState(GL_NORMAL_ARRAY);
      else
        glDisableClientState(GL_NORMAL_ARRAY);
      break;
    default:
#ifndef PS3
      if (bEnable)
        glEnableVertexAttribArrayARB(i);
      else
        glDisableVertexAttribArrayARB(i);
#else
  #if CELL_SDK_VERSION > 0x080000
			if (bEnable)
				cgGLEnableAttrib(i);
			else
				cgGLDisableAttrib(i);
	#endif
#endif
      break;
  }
}

HRESULT IDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
  if (!pDecl)
    return D3DERR_INVALIDCALL;

  IDirect3DVertexDeclaration9 *pPrev = m_pCurVDeclaration;
  m_pCurVDeclaration = pDecl;
  int i;
  if (!pPrev)
  {
    int nCurMask = pDecl->m_nStreamMask;
    for (i=0; i<=FSM_MAX_STREAM_INDEX; i++)
      SetVertexAttrib(i, nCurMask);
    return S_OK;
  }
  if (pPrev->m_nStreamMask != pDecl->m_nStreamMask)
  {
    int nPrevMask = pPrev->m_nStreamMask;
    int nCurMask = pDecl->m_nStreamMask;
    for (i=0; i<=FSM_MAX_STREAM_INDEX; i++)
    {
      int nBit = 1<<i;
      if ((nPrevMask ^ nCurMask) & nBit)
        SetVertexAttrib(i, nCurMask);
    }
  }

  return S_OK;
}

HRESULT IDirect3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetFVF(DWORD* pFVF)
{
  assert(0);
  return S_OK;
}

void CGParam2D39ConstDesc(CGprogram Prog, D3DXCONSTANT_DESC& CDesc, CGparameter Param)
{
	static std::set<CCryName> nameSet;
  const char* Name = cgGetParameterName(Param);
  CGtype cgType = cgGetParameterType(Param);

	// RecurseParams may call this function for the first element
	// of an array parameter. In this case, the name returned by
	// cgGetParameterName() will end with "[0]".
	if (strchr(Name, '['))
	{
	  char NameStripped[strlen(Name) + 1];
		strcpy(NameStripped, Name);
		*strrchr(NameStripped, '[') = 0;
		CCryName cName = NameStripped;
		nameSet.insert(cName);
		Name = CDesc.Name = cName.c_str();
	} else
	{
		CCryName cName = Name;
		nameSet.insert(cName);
		Name = CDesc.Name = cName.c_str();
	}
  CDesc.RegisterIndex = (UINT)(INT_PTR)Param;
	assert(CDesc.RegisterIndex < SHADER_BIND_SAMPLER);
  CGparameter pr = GetNamedParameterNocase(Prog, Name);
  if (cgType == CG_FLOAT4 || cgType == CG_FLOAT3 || cgType == CG_FLOAT2)
  {
    CDesc.RegisterSet = D3DXRS_FLOAT4;
    CDesc.Class = D3DXPC_VECTOR;
  }
  else
  if (cgType == CG_FLOAT)
  {
    CDesc.RegisterSet = D3DXRS_FLOAT4;
    CDesc.Class = D3DXPC_SCALAR;
  }
  else
  if (cgType == CG_FLOAT2x4 || cgType == CG_FLOAT2x3)
  {
    CDesc.RegisterSet = D3DXRS_FLOAT4;
    CDesc.Class = D3DXPC_MATRIX_ROWS2;
  }
  else
  if (cgType == CG_FLOAT3x4 || cgType == CG_FLOAT3x3)
  {
    CDesc.RegisterSet = D3DXRS_FLOAT4;
    CDesc.Class = D3DXPC_MATRIX_ROWS3;
  }
  else
  if (cgType == CG_FLOAT4x4 || cgType == CG_FLOAT4x3)
  {
    CDesc.RegisterSet = D3DXRS_FLOAT4;
    CDesc.Class = D3DXPC_MATRIX_ROWS4;
  }
  else
  if (cgType == CG_SAMPLER2D)
  {
    CDesc.RegisterIndex = cgGetParameterResourceIndex(Param);
    CDesc.RegisterSet = D3DXRS_SAMPLER;
    CDesc.Class = D3DXPC_OBJECT;
  }
  else
  if (cgType == CG_SAMPLERCUBE)
  {
    CDesc.RegisterIndex = cgGetParameterResourceIndex(Param);
    CDesc.RegisterSet = D3DXRS_SAMPLER;
    CDesc.Class = D3DXPC_OBJECT;
  }
  else
  if (cgType == CG_SAMPLER3D)
  {
    CDesc.RegisterIndex = cgGetParameterResourceIndex(Param);
    CDesc.RegisterSet = D3DXRS_SAMPLER;
    CDesc.Class = D3DXPC_OBJECT;
  }
  else
  {
    assert(0);
  }
}


void RecurseParams(CGprogram Prog, CGparameter Param, ID3DXConstantTable *pTable)
{
  if(!Param)
    return;

  D3DXCONSTANT_DESC CDesc;

  do
  {
		const char *Name = cgGetParameterName(Param);
    CGtype cgType = cgGetParameterType(Param);
		CGenum var = cgGetParameterVariability(Param);
		switch (var)
		{
			case CG_VARYING:
				continue;
			case CG_UNIFORM:
				break;
			case CG_CONSTANT:
				continue;
			case CG_MIXED:
				break;
			default:
				assert(0);
		}
		if (Name[0] == '_' && (Name[1] == 'g' || Name[1] == 'G') && Name[2] == '_')
			continue;
    switch(cgType)
    {
      case CG_STRUCT :
        RecurseParams(Prog, cgGetFirstStructParameter(Param), pTable);
        break;

      case CG_ARRAY :
        {
          int ArraySize = cgGetArraySize(Param, 0);
          CGparameter arParam = cgGetArrayParameter(Param, 0);
          CGbool bReferenced = cgIsParameterReferenced(arParam);
          if (bReferenced)
          {
            CDesc.RegisterCount = ArraySize;
            CGParam2D39ConstDesc(Prog, CDesc, arParam);
						if (CDesc.RegisterSet != D3DXRS_SAMPLER)
						{
							CDesc.RegisterIndex = (UINT)(INT_PTR)Param;
							assert(CDesc.RegisterIndex < SHADER_BIND_SAMPLER);
						}
            pTable->m_Constants.AddElem(CDesc);
          }
        }
        break;

      default:
        {
          /* Do stuff to param */
          CGbool bReferenced = cgIsParameterReferenced(Param);
          if (bReferenced)
          {
            CGParam2D39ConstDesc(Prog, CDesc, Param);
#if 0
						// Old code kept for reference.
            int nValues;
            cgGetParameterValues(Param, CG_DEFAULT, &nValues);
            CDesc.RegisterCount = max(nValues / 4, 1);
#endif
						// This is ugly, but still better than adding 113 case labels...
						switch (cgType)
						{
#if defined(PS3)
#define CG_DATATYPE_MACRO(name, compilerName, enumName, baseName, nrows, ncols) \
						case enumName: \
							CDesc.RegisterCount = max(nrows, 1); \
							break;
#include <Cg/NV/cg_datatypes.h>
#endif
#if defined(LINUX)
#define CG_DATATYPE_MACRO(name, compilerName, enumName, baseName, nrows, ncols, prmClass) \
						case enumName: \
							CDesc.RegisterCount = max(nrows, 1); \
							break;
#include <Cg/cg_datatypes.h>
#endif
#undef CG_DATATYPE_MACRO
						default:
						  assert(!"invalid cgType");
						}
            pTable->m_Constants.AddElem(CDesc);
//printf("RecurseParams: TYPE<%u>: CDesc.Name = \"%s\"\n", (unsigned)cgType, CDesc.Name);
          }
        }
        break;
    }
  } while((Param = cgGetNextParameter(Param)) != 0);
}


HRESULT IDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader)
{
  if (!ppShader || !pFunction)
    return D3DERR_INVALIDCALL;

  IDirect3DVertexShader9 *pSH = new IDirect3DVertexShader9;
  pSH->m_nType = D3DRTYPE_VSHADER;
#if defined(PS3)
	CGprofile profile = cgGLGetLatestProfile(CG_GL_VERTEX);
#else
  CGprofile profile = CG_PROFILE_ARBVP1;
#endif

  CGprogram Prog = cgCreateProgram(m_pD3D->m_pCurrContext->m_cgContext,
#if defined(PS3)
			CG_BINARY,
			(const char *)pFunction + 4,
#else
			CG_OBJECT,
			(const char *)pFunction,
#endif
			profile, NULL, NULL);
#if defined(PS3)
	if (Prog)
		StoreMappedSemantics(Prog, pFunction,
				ps3ShaderName[0] ? ps3ShaderName : NULL);
#endif
  CGerror err = cgGetError();
  if (err != CG_NO_ERROR)
  {
    if (err == CG_COMPILER_ERROR)
    {
      const char *sError = cgGetErrorString(err);
      Warning("Couldn't create CG program (%s)", sError);
      assert("CG Error" == 0);
    }
    assert (err == CG_NO_ERROR);
  }

	// Extract the varying input parameters from the program.
	CGparameter Param = cgGetNamedParameter(Prog, "IN");
	assert(Param != NULL);
	assert(cgGetParameterType(Param) == CG_STRUCT);
	Param = cgGetFirstStructParameter(Param);
	do
	{
		CGenum Direction = cgGetParameterDirection(Param);
		if (Direction != CG_IN && Direction != CG_INOUT)
			continue;
		CGenum Variablility = cgGetParameterVariability(Param);
		if (Variablility != CG_VARYING)
			continue;
		int RegisterIndex = GetVSParamRegisterIndex(Param);
		assert(RegisterIndex >= 0 && RegisterIndex < 16);
		pSH->m_streamId[RegisterIndex] = (int)(INT_PTR)Param;
	}
	while ((Param = cgGetNextParameter(Param)));

	if (!Prog)
	{
		assert(Prog);
	}
  pSH->m_nID = (int)(UINT_PTR)Prog;
  if (Prog)
  {
    cgGLLoadProgram(Prog);
    cgGLBindProgram(Prog);
  }
  *ppShader = pSH;
  err = cgGetError();
  //assert (err == CG_NO_ERROR);

  return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9* pShader)
{
  if (pShader == NULL)
    return S_OK;
	const CGprogram Prog = (CGprogram)(UINT_PTR)pShader->m_nID;
  cgGLBindProgram(Prog);
  CGerror err = cgGetError();
  // stupid CG bug
  if (err != CG_INVALID_PARAM_HANDLE_ERROR)
  {
    assert (err == CG_NO_ERROR);
  }
	SAFE_RELEASE(m_pCurVShader);
	m_pCurVShader = pShader;
	m_pCurVShader->AddRef();
#if defined(PS3) && defined(_DEBUG)
	//puts("IDirect3DDevice9::SetVertexShader");
	//DumpShaderProgram(Prog);
#endif

  return S_OK;
}

HRESULT IDirect3DDevice9::GetVertexShader(IDirect3DVertexShader9** ppShader)
{
  if (ppShader == NULL)
		return D3DERR_INVALIDCALL;
	*ppShader = m_pCurVShader;
	if (m_pCurVShader != NULL)
		m_pCurVShader->AddRef();
  return S_OK;
}

static HRESULT SetShaderConstantF(CGparameter Param, CONST float *,
		UINT Vector4fCount, CONST UINT cOffset, UINT ParamFlags);

HRESULT IDirect3DDevice9::SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount,CONST UINT cOffset)
{
	const UINT cUnMaskedStartRegister = (StartRegister & ~scParamMask);
	const UINT ParamFlags = StartRegister & scParamMask;
#if 0
	const UINT cUnMaskedStartRegister = StartRegister;
	const UINT ParamFlags = 0;
#endif
	const CGparameter Param = (CGparameter)(UINT_PTR)cUnMaskedStartRegister;
#if defined(_DEBUG) && defined(PS3)
	const CGprofile VertexProgramProfile = cgGLGetLatestProfile(CG_GL_VERTEX);
	CGprofile Profile = cgGetProgramProfile(cgGetParameterProgram(Param));
	assert(Profile == VertexProgramProfile);
#endif
	return SetShaderConstantF(Param, pConstantData, Vector4fCount, cOffset,
			ParamFlags);
}

HRESULT IDirect3DDevice9::GetVertexShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetVertexShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetVertexShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
  m_CurStream[StreamNumber].m_pBuf = pStreamData;
  m_CurStream[StreamNumber].m_nOffset = OffsetInBytes;
  m_CurStream[StreamNumber].m_nStride = Stride;
  m_CurStream[StreamNumber].m_bDirty = true;

  return S_OK;
}

HRESULT IDirect3DDevice9::GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* OffsetInBytes,UINT* pStride)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetStreamSourceFreq(UINT StreamNumber,UINT Divider)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetStreamSourceFreq(UINT StreamNumber,UINT* Divider)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
  if (!pIndexData)
  {
    if (m_nCurrentIB)
    {
#ifndef PS3
      glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
#else
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
      m_nCurrentIB = 0;
    }
  }
  else
  {
    if (m_nCurrentIB != pIndexData->m_nID)
    {
#ifndef PS3
      glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, pIndexData->m_nID);
#else
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pIndexData->m_nID);
#endif
      m_nCurrentIB = pIndexData->m_nID;
    }
  }
  m_pCurIB = pIndexData;

  return S_OK;
}

HRESULT IDirect3DDevice9::GetIndices(IDirect3DIndexBuffer9** ppIndexData)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader)
{
  if (!ppShader || !pFunction)
    return D3DERR_INVALIDCALL;

  IDirect3DPixelShader9 *pSH = new IDirect3DPixelShader9;
  pSH->m_nType = D3DRTYPE_PSHADER;

#ifndef PS3
  CGprofile profile = CG_PROFILE_ARBFP1;
#else
  //CGprofile profile = CG_PROFILE_SCE_FP10;
	CGprofile profile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
#endif

  CGprogram Prog = cgCreateProgram(m_pD3D->m_pCurrContext->m_cgContext,
#if defined(PS3)
			CG_BINARY,
			(const char *)pFunction + 4,
#else
			CG_OBJECT,
			(const char *)pFunction,
#endif
			profile, NULL, NULL);
#if defined(PS3)
	if (Prog)
		StoreMappedSemantics(Prog, pFunction,
				ps3ShaderName[0] ? ps3ShaderName : NULL);
#endif
  CGerror err = cgGetError();
  if (err != CG_NO_ERROR)
  {
    if (err == CG_COMPILER_ERROR)
    {
      const char *sError = cgGetErrorString(err);
      Warning("Couldn't create CG program (%s)", sError);
      assert("CG Error" == 0);
    }
  }
  assert(Prog);
  pSH->m_nID = (int)(UINT_PTR)Prog;
  if (Prog)
  {
    cgGLLoadProgram(Prog);
    cgGLBindProgram(Prog);
  }

  *ppShader = pSH;

  return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9* pShader)
{
  if (pShader == NULL)
    return S_OK;
	const CGprogram Prog = (CGprogram)(UINT_PTR)pShader->m_nID;
  cgGLBindProgram(Prog);
  CGerror err = cgGetError();
  if (err != CG_INVALID_PARAM_HANDLE_ERROR)
  {
    assert (err == CG_NO_ERROR);
  }
#if defined(PS3) && defined(_DEBUG)
	//puts("IDirect3DDevice9::SetPixelShader");
	//DumpShaderProgram(Prog);
#endif

  return S_OK;
}

HRESULT IDirect3DDevice9::GetPixelShader(IDirect3DPixelShader9** ppShader)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount,CONST UINT cOffset)
{
	const UINT cUnMaskedStartRegister = (StartRegister & ~scParamMask);
	const UINT ParamFlags = StartRegister & scParamMask;
#if 0
	const UINT cUnMaskedStartRegister = StartRegister;
	const UINT ParamFlags = 0;
#endif
	const CGparameter Param = (CGparameter)(UINT_PTR)cUnMaskedStartRegister;
#if defined(_DEBUG) && defined(PS3)
	const CGprofile cFragmentProgramProfile
		= cgGLGetLatestProfile(CG_GL_FRAGMENT);
	CGprofile Profile = cgGetProgramProfile(cgGetParameterProgram(Param));
	assert(Profile == cFragmentProgramProfile);
#endif
	return SetShaderConstantF(Param, pConstantData, Vector4fCount, cOffset,
			ParamFlags);
}

static HRESULT SetShaderConstantF(
		CGparameter Param,
		CONST float* pConstantData,
		UINT Vector4fCount,
		CONST UINT cOffset,
		UINT ParamFlags)
{
// XXX debug code: dump all shader constants to "shader_constants.log".
#if 0
//if (!strncasecmp(cgGetParameterName((CGparameter)(UINT_PTR)cUnMaskedStartRegister), "_g_", 3))
{
FILE *constantLog = NULL; //fopen("/app_home/shader_constants.log", "a");
if (constantLog == NULL) constantLog = stdout;
char ParamInfo[1024];
snprintf(ParamInfo, sizeof ParamInfo - 1, "Offset=%u Vector4fCount=%u Value=[",
(unsigned)cOffset, (unsigned)Vector4fCount);
ParamInfo[sizeof ParamInfo - 1] = 0;
for (int i = 0; i < Vector4fCount * 4; ++i)
{
snprintf(ParamInfo + strlen(ParamInfo), sizeof ParamInfo - strlen(ParamInfo) - 1,
"%s%g", i ? "," : "", (double)pConstantData[i]);
}
if (strlen(ParamInfo) < sizeof ParamInfo - 2) strcat(ParamInfo, "]");
ParamInfo[sizeof ParamInfo - 1] = 0;
char ParamDescription[1024];
fprintf(
constantLog,
"ShaderConstantF: %s\n",
DescribeProgramParameter(Param, ParamDescription, sizeof ParamDescription, ParamInfo));
if (constantLog != stdout)
fclose(constantLog);
constantLog = NULL;
}
#endif
  const float *pData = pConstantData;
  if (Vector4fCount == 1 && !(ParamFlags & scParamArrayBit))
    cgSetParameter4fv(Param, pData);
  else
  if (Vector4fCount > 1 || (ParamFlags & scParamArrayBit))
  {
		if(!(ParamFlags & scParamIsMatrixMask))
			cgGLSetParameterArray4f(Param, cOffset, Vector4fCount, pData);
		else
		{
			uint32 matrixCount = 1;
			switch (ParamFlags & scParamIsMatrixMask)
			{
			case scParam2x4MatrixBit:
				matrixCount = (((Vector4fCount + 1) & ~1) >> 1);
				break;
			case scParam3x4MatrixBit:
				matrixCount = (((Vector4fCount + 2) & ~2) / 3);
				break;
			case scParam4x4MatrixBit:
				matrixCount = (((Vector4fCount + 3) & ~3) >> 2);
				break;
			}
			if(matrixCount > 1 || (ParamFlags & scParamArrayBit))
				cgGLSetMatrixParameterArrayfr(Param, cOffset, matrixCount, pData);
			else
				cgSetMatrixParameterfr(Param, pData);
		}
  }

  CGerror err = cgGetError();
  if (err != CG_INVALID_PARAM_HANDLE_ERROR)
    assert (err == CG_NO_ERROR);

  return S_OK;
}

HRESULT IDirect3DDevice9::GetPixelShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetPixelShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::GetPixelShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::DrawRectPatch(UINT Handle,CONST float* pNumSegs,CONST D3DRECTPATCH_INFO* pRectPatchInfo)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::DrawTriPatch(UINT Handle,CONST float* pNumSegs,CONST D3DTRIPATCH_INFO* pTriPatchInfo)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::DeletePatch(UINT Handle)
{
  assert(0);
  return S_OK;
}

HRESULT IDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
  if (Type == D3DQUERYTYPE_EVENT)
  {
    if (ppQuery)
    {
      IDirect3DQuery9 *pQ = new IDirect3DQuery9;
      *ppQuery = pQ;
    }
    return S_OK;
  }
  if (Type == D3DQUERYTYPE_OCCLUSION)
  {
#ifndef PS3
    if (!SUPPORTS_GL_NV_occlusion_query)
      return D3DERR_NOTAVAILABLE;
#endif
#if 0
		// FIXME Implement!
		// Occlusion queries currently not implemented.
    if (ppQuery)
    {
      IDirect3DQuery9 *pQ = new IDirect3DQuery9;
      *ppQuery = pQ;
    }
    return S_OK;
#endif
  }
  return D3DERR_NOTAVAILABLE;
}


// texture functions

HRESULT D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9        pDestSurface,
                                   CONST PALETTEENTRY*       pDestPalette,
                                   CONST RECT*               pDestRect,
                                   LPDIRECT3DSURFACE9        pSrcSurface,
                                   CONST PALETTEENTRY*       pSrcPalette,
                                   CONST RECT*               pSrcRect,
                                   DWORD                     Filter,
                                   D3DCOLOR                  ColorKey)
{
  assert(0);
  return S_OK;
}


HRESULT IDirect3DBaseTexture9::SetAutoGenFilterType (D3DTEXTUREFILTERTYPE FilterType)
{
  m_nFilterType = FilterType;
  return S_OK;
}

HRESULT D3DXLoadSurfaceFromMemory(LPDIRECT3DSURFACE9        pDestSurface,
                                  CONST PALETTEENTRY*       pDestPalette,
                                  CONST RECT*               pDestRect,
                                  LPVOID                   pSrcMemory,
                                  D3DFORMAT                 SrcFormat,
                                  UINT                      SrcPitch,
                                  CONST PALETTEENTRY*       pSrcPalette,
                                  CONST RECT*               pSrcRect,
                                  DWORD                     Filter,
                                  D3DCOLOR                  ColorKey)
{
  if (!pDestSurface || !pSrcMemory)
    return D3DERR_INVALIDCALL;

  int i, j;
  int nDstSize = CTexture::TextureDataSize(pDestSurface->m_Width, pDestSurface->m_Height, 1, 1, CTexture::TexFormatFromDeviceFormat(pDestSurface->m_Format));
  pDestSurface->m_pLockedData = new byte[nDstSize];
  byte *pSrc = (byte *)pSrcMemory;
  byte *pDst = (byte *)pDestSurface->m_pLockedData;
	bool unknownFormat = false;
  switch (SrcFormat)
  {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
      {
        switch (pDestSurface->m_Format)
        {
          case D3DFMT_L8:
            {
              for (i=0; i<pDestSurface->m_Height; i++)
              {
                for (j=0; j<pDestSurface->m_Width; j++)
                {
                  float fLum = (float)pSrc[j*4+0]*0.3f + (float)pSrc[j*4+1]*0.59f + (float)pSrc[j*4+2]*0.11f;
                  pDst[j] = (byte)(fLum);
                }
                pSrc += SrcPitch;
                pDst += pDestSurface->m_Width;
              }
            }
            break;
          case D3DFMT_X8R8G8B8:
          case D3DFMT_A8R8G8B8:
            {
              for (i=0; i<pDestSurface->m_Height; i++)
              {
                for (j=0; j<pDestSurface->m_Width; j++)
                {
                  *(DWORD *)(&pDst[j*4]) = *(DWORD *)(&pSrc[j*4]);
                }
                pSrc += SrcPitch;
                pDst += pDestSurface->m_Width * 4;
              }
            }
            break;
					case D3DFMT_DXT1:
						NDXTCompression::CompressToDXT(pSrc, pDst, D3DFMT_DXT1, pDestSurface->m_Width, pDestSurface->m_Height);
						break;
					case D3DFMT_DXT3:
						NDXTCompression::CompressToDXT(pSrc, pDst, D3DFMT_DXT3, pDestSurface->m_Width, pDestSurface->m_Height);
						break;
					case D3DFMT_DXT5:
						NDXTCompression::CompressToDXT(pSrc, pDst, D3DFMT_DXT5, pDestSurface->m_Width, pDestSurface->m_Height);
						break;
          default:
           unknownFormat = true;
        }
      }
      break;
    default:
			unknownFormat = true;
	}
	if(unknownFormat)
		iLog->Log("Error in D3DXLoadSurfaceFromMemory: src.format: %d -> dest format: %d",SrcFormat, pDestSurface->m_Format);
  return S_OK;
}

HRESULT D3DXCreateTexture(LPDIRECT3DDEVICE9         pDevice,
                          UINT                      Width,
                          UINT                      Height,
                          UINT                      MipLevels,
                          DWORD                     Usage,
                          D3DFORMAT                 Format,
                          D3DPOOL                   Pool,
                          LPDIRECT3DTEXTURE9*       ppTexture)
{
  if (MipLevels == D3DX_DEFAULT)
    MipLevels = CTexture::CalcNumMips(Width, Height);
  return pDevice->CreateTexture(Width, Height, MipLevels, Usage, Format, Pool, ppTexture, NULL);
}

HRESULT D3DXCreateCubeTexture(LPDIRECT3DDEVICE9         pDevice,
                              UINT                      Size,
                              UINT                      MipLevels,
                              DWORD                     Usage,
                              D3DFORMAT                 Format,
                              D3DPOOL                   Pool,
                              LPDIRECT3DCUBETEXTURE9*   ppCubeTexture)
{
  if (MipLevels == D3DX_DEFAULT)
    MipLevels = CTexture::CalcNumMips(Size, Size);
  return pDevice->CreateCubeTexture(Size, MipLevels, Usage, Format, Pool, ppCubeTexture, NULL);
}

HRESULT D3DXCreateVolumeTexture(LPDIRECT3DDEVICE9         pDevice,
                                UINT                      Width,
                                UINT                      Height,
                                UINT                      Depth,
                                UINT                      MipLevels,
                                DWORD                     Usage,
                                D3DFORMAT                 Format,
                                D3DPOOL                   Pool,
                                LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture)
{
  return pDevice->CreateVolumeTexture(Width, Height, Depth, MipLevels, Usage, Format, Pool, ppVolumeTexture, NULL);
}

HRESULT D3DXFillTexture(   LPDIRECT3DTEXTURE9        pTexture,
                        LPD3DXFILL2D              pFunction,
                        LPVOID                    pData)
{
  //assert(0);
  return S_OK;
}

HRESULT D3DXFillVolumeTexture(LPDIRECT3DVOLUMETEXTURE9 pTexture,
                        LPD3DXFILL3D              pFunction,
                        LPVOID                    pData)
{
  //assert(0);
  return S_OK;
}

HRESULT D3DXSaveSurfaceToFile(
                              LPCSTR                    pDestFile,
                              D3DXIMAGE_FILEFORMAT      DestFormat,
                              LPDIRECT3DSURFACE9        pSrcSurface,
                              CONST PALETTEENTRY*       pSrcPalette,
                              CONST RECT*               pSrcRect)
{
  assert(0);
  return S_OK;
}

HRESULT D3DXFilterTexture(LPDIRECT3DBASETEXTURE9    pBaseTexture,
                          CONST PALETTEENTRY*       pPalette,
                          UINT                      SrcLevel,
                          DWORD                     Filter)
{
  assert(0);
  return S_OK;
}

//===================================================================================

IDirect3DQuery9::~IDirect3DQuery9()
{
}

HRESULT IDirect3DQuery9::Issue(DWORD dwIssueFlags)
{
  return S_OK;
}

HRESULT IDirect3DQuery9::GetData(void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
  //assert(0);
  return S_OK;
}

//=====================================================================================

IDirect3DSurface9::IDirect3DSurface9()
{
  m_Format = D3DFMT_UNKNOWN;
  m_Width = 0;
  m_Height = 0;
  m_nType = D3DRTYPE_SURFACE;
  m_Pool = D3DPOOL_DEFAULT;
  m_Usage = 0;
  m_nLevel = 0;
  m_pTex = NULL;
  m_pLockedData = NULL;
  m_nID = 0;
}

IDirect3DSurface9::~IDirect3DSurface9()
{
  SAFE_DELETE_VOID_ARRAY(m_pLockedData);
	if (m_pTex == NULL)
	{
		GLuint id;
		if ((m_Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL))
				&& m_nID)
		{
			id = (GLuint)m_nID;
			glDeleteRenderbuffersEXT(1, &id);
		}
	}
}

HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
  if (!pDesc)
    return D3DERR_INVALIDCALL;
  ZeroStruct(*pDesc);
  pDesc->Format = m_Format;
  pDesc->Width = m_Width;
  pDesc->Height = m_Height;
  pDesc->Usage = m_Usage;
  pDesc->Type = m_nType;
  pDesc->Pool = m_Pool;

  return S_OK;
}

HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
  if (!pLockedRect)
    return D3DERR_INVALIDCALL;

  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat(m_Format);
  int w = m_Width << m_nLevel;
  if (!w)
    w = 1;
  int h = m_Height << m_nLevel;
  if (!h)
    h = 1;
  int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);
  if (!m_pLockedData)
    m_pLockedData = new byte [nSize];
  pLockedRect->pBits = m_pLockedData;
  pLockedRect->Pitch = CTexture::TextureDataSize(w, 1, 1, 1, eTF);

  return S_OK;
}

HRESULT IDirect3DSurface9::UnlockRect()
{
  if (!m_pLockedData || !m_pTex)
    return D3DERR_INVALIDCALL;

  IDirect3DBaseTexture9 *pTexture = m_pTex;

  int w = m_Width;
  int h = m_Height;
  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat(m_Format);
  int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);

	GLenum format, type;
	GLint internalFormat;
	bool compressed = false;
	bool fillAlpha = m_Format == D3DFMT_X8R8G8B8;
	if (!D3DFORMAT2GL(m_Format, &format, &type, &internalFormat, &compressed))
	{
		assert(0);
	}
  assert (pTexture->m_nID);
  int target = pTexture->m_nGLtarget;

  glBindTexture(target, pTexture->m_nID);

	if (compressed)
	{
#if defined(PS3)
    glCompressedTexSubImage2D(target, m_nLevel, 0, 0, w, h,
				internalFormat, nSize, m_pLockedData);
#else
    glCompressedTexSubImage2DARB(target, m_nLevel, 0, 0, w, h,
				internalFormat, nSize, m_pLockedData);
#endif
	}
	else
	{
		if (fillAlpha)
			FillAlpha(w, h, (uint8 *)m_pLockedData);
    glTexSubImage2D(target, m_nLevel, 0, 0, w, h,
				format, type, m_pLockedData);
	}
  SAFE_DELETE_VOID_ARRAY(m_pLockedData);

  return S_OK;
}

//====================================================================================================

IDirect3DIndexBuffer9::~IDirect3DIndexBuffer9()
{
  if (m_nID)
  {
#ifndef PS3
    glDeleteBuffersARB(1, &m_nID);
#else
    glDeleteBuffers(1, &m_nID);
#endif
    if (g_pDev->m_nCurrentIB == m_nID)
      g_pDev->m_nCurrentIB = -1;
    m_nID = 0;
  }
}

HRESULT IDirect3DIndexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
  if (m_bLocked || !ppbData)
    return D3DERR_INVALIDCALL;

  if (g_pDev->m_nCurrentIB != m_nID)
  {
    g_pDev->m_nCurrentIB = m_nID;
#ifndef PS3
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_nID);
#else
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_nID);
#endif
  }
#ifndef PS3
  byte *dst = (byte *)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#else
  byte *dst = (byte *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_WRITE);
#endif
  dst += OffsetToLock;
  *ppbData = dst;
  m_bLocked = true;
  return S_OK;
}

HRESULT IDirect3DIndexBuffer9::Unlock()
{
  if (!m_bLocked)
    return D3DERR_INVALIDCALL;
  if (g_pDev->m_nCurrentIB != m_nID)
  {
    g_pDev->m_nCurrentIB = m_nID;
#ifndef PS3
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m_nID);
#else
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_nID);
#endif
  }
#ifndef PS3
  glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
#else
  if (!glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER))
	{
		puts("IDirect3DIndexBuffer9::Unlock(): buffer corrupted!");
	}
#endif

  m_bLocked = false;

  return S_OK;
}

HRESULT IDirect3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
  assert(0);
  return S_OK;
}

//====================================================================================================

IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9()
{
  if (m_nID)
  {
#ifndef PS3
    glDeleteBuffersARB(1, &m_nID);
#else
    glDeleteBuffers(1, &m_nID);
#endif
    if (g_pDev->m_nCurrentVB == m_nID)
      g_pDev->m_nCurrentVB = -1;
    m_nID = 0;
  }
}

HRESULT IDirect3DVertexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
  if (m_bLocked || !ppbData)
    return D3DERR_INVALIDCALL;

  if (g_pDev->m_nCurrentVB != m_nID)
  {
    g_pDev->m_nCurrentVB = m_nID;
#ifndef PS3
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_nID);
#else
    glBindBuffer(GL_ARRAY_BUFFER, m_nID);
#endif
  }
#ifndef PS3
  byte *dst = (byte *)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
#else
  byte *dst = (byte *)glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
#endif
  dst += OffsetToLock;
  *ppbData = dst;
  m_bLocked = true;

  return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Unlock()
{
  if (!m_bLocked)
    return D3DERR_INVALIDCALL;
  if (g_pDev->m_nCurrentVB != m_nID)
  {
    g_pDev->m_nCurrentVB = m_nID;
#ifndef PS3
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, m_nID);
#else
    glBindBuffer(GL_ARRAY_BUFFER, m_nID);
#endif
  }
#ifndef PS3
  glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
#else
  if (!glUnmapBuffer(GL_ARRAY_BUFFER))
	{
		puts("IDirect3DVertexBuffer9::Unlock(): buffer corrupted!");
	}
#endif
  m_bLocked = false;

  return S_OK;
}

HRESULT IDirect3DVertexBuffer9::GetDesc(D3DVERTEXBUFFER_DESC *pDesc)
{
  assert(0);
  return S_OK;
}

//====================================================================================================

IDirect3DTexture9::~IDirect3DTexture9()
{
  if (m_nID)
  {
    glDeleteTextures(1, &m_nID);
    m_nID = 0;
  }

  for (int i=0; i<m_nLevelCount; i++)
  {
    SAFE_DELETE_ARRAY(m_pLockedData[i]);
  }
  SAFE_DELETE_ARRAY(m_pLockedData);
}

HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT Level,IDirect3DSurface9** ppSurfaceLevel)
{
  if (!ppSurfaceLevel)
    return D3DERR_INVALIDCALL;

  IDirect3DSurface9 *pSurf = new IDirect3DSurface9;
  pSurf->m_pTex = this;
  pSurf->m_nLevel = Level;
  pSurf->m_Pool = m_Pool;

  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;

  pSurf->m_Width = w;
  pSurf->m_Height = h;
  pSurf->m_Format = m_Format;
  pSurf->m_Usage = m_Usage;

  *ppSurfaceLevel = pSurf;

  return S_OK;
}

HRESULT IDirect3DTexture9::LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
  if (Level >= m_nLevelCount)
    return D3DERR_INVALIDCALL;

  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  if (!m_pLockedData[Level])
  {
    int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);
    m_pLockedData[Level] = new byte[nSize];
  }
  pLockedRect->Pitch = CTexture::TextureDataSize(w, 1, 1, 1, eTF);
  pLockedRect->pBits = m_pLockedData[Level];

  return S_OK;
}

HRESULT IDirect3DTexture9::UnlockRect(UINT Level)
{
  if (!m_pLockedData[Level])
    return D3DERR_INVALIDCALL;

  glBindTexture(m_nGLtarget, m_nID);
  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);
	GLint internalFormat;
	GLenum format, type;
	bool compressed = false;
	bool fillAlpha = m_Format == D3DFMT_X8R8G8B8;
	if (!D3DFORMAT2GL(m_Format, &format, &type, &internalFormat, &compressed))
	{
		assert(0);
	}

	if (compressed)
	{
#ifndef PS3
    glCompressedTexImage2DARB(m_nGLtarget, Level, internalFormat, w, h, 0,
				nSize, m_pLockedData[Level]);
#else
    glCompressedTexImage2D(m_nGLtarget, Level, internalFormat, w, h, 0,
				nSize, m_pLockedData[Level]);
#endif
  }
	else
	{
		if (fillAlpha)
			FillAlpha(w, h, m_pLockedData[Level]);
    glTexImage2D(m_nGLtarget, Level, internalFormat, w, h, 0, format,
				type, m_pLockedData[Level]);
	}

  SAFE_DELETE_ARRAY(m_pLockedData[Level]);

  return S_OK;
}

HRESULT IDirect3DTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
  if (!pDesc || Level >= m_nLevelCount)
    return D3DERR_INVALIDCALL;

  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;

  pDesc->Width = w;
  pDesc->Height = h;
  pDesc->Format = m_Format;
  pDesc->Usage = m_Usage;
  pDesc->Pool = m_Pool;
  pDesc->Type = m_nType;

  return S_OK;
}

//====================================================================================================

IDirect3DCubeTexture9::~IDirect3DCubeTexture9()
{
  if (m_nID)
    glDeleteTextures(1, &m_nID);
  m_nID = 0;

  for (int i=0; i<m_nLevelCount*6; i++)
  {
    SAFE_DELETE_ARRAY(m_pLockedData[i]);
  }
  SAFE_DELETE_ARRAY(m_pLockedData);
}

HRESULT IDirect3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface)
{
  if (!ppCubeMapSurface)
    return D3DERR_INVALIDCALL;
  *ppCubeMapSurface = NULL;
  return S_OK;
}

HRESULT IDirect3DCubeTexture9::LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags)
{
  if (Level >= m_nLevelCount || FaceType >= 6)
    return D3DERR_INVALIDCALL;

  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  int nID = Level + FaceType*m_nLevelCount;
  if (!m_pLockedData[nID])
  {
    int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);
    m_pLockedData[nID] = new byte[nSize];
  }
  int nPixelBytes = CTexture::ComponentsForFormat(eTF);
  pLockedRect->Pitch = w * nPixelBytes;
  pLockedRect->pBits = m_pLockedData[nID];

  return S_OK;
}

HRESULT IDirect3DCubeTexture9::UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level)
{
  int nID = Level + FaceType*m_nLevelCount;
  if (!m_pLockedData[nID])
    return D3DERR_INVALIDCALL;

#ifndef PS3
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, m_nID);
  const GLenum cubefaces[6] = 
  {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X_EXT,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y_EXT,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_EXT,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z_EXT,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EXT,
  };
#else
  glBindTexture(GL_TEXTURE_CUBE_MAP, m_nID);
  const GLenum cubefaces[6] = 
  {
    GL_TEXTURE_CUBE_MAP_POSITIVE_X,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
    GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
  };
#endif
  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  int nSize = CTexture::TextureDataSize(w, h, 1, 1, eTF);
	GLint internalFormat;
	GLenum format, type;
	bool compressed = false;
	bool fillAlpha = m_Format == D3DFMT_X8R8G8B8;
	if (!D3DFORMAT2GL(m_Format, &format, &type, &internalFormat, &compressed))
	{
		assert(0);
	}
	if (compressed)
	{
#ifndef PS3
    glCompressedTexImage2DARB(cubefaces[FaceType], Level, internalFormat,
				w, h, 0, nSize, m_pLockedData[nID]);
#else
    glCompressedTexImage2D(cubefaces[FaceType], Level, internalFormat,
				w, h, 0, nSize, m_pLockedData[nID]);
#endif
	}
	else
	{
		if (fillAlpha)
			FillAlpha(w, h, m_pLockedData[nID]);
    glTexImage2D(cubefaces[FaceType], Level, internalFormat,
				w, h, 0, format, type, m_pLockedData[nID]);
	}

  SAFE_DELETE_ARRAY(m_pLockedData[nID]);

  return S_OK;
}

HRESULT IDirect3DCubeTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
  if (!pDesc || Level >= m_nLevelCount)
    return D3DERR_INVALIDCALL;

  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;

  pDesc->Width = w;
  pDesc->Height = h;
  pDesc->Format = m_Format;
  pDesc->Usage = m_Usage;
  pDesc->Pool = m_Pool;
  pDesc->Type = m_nType;

  return S_OK;
}

//====================================================================================================

IDirect3DBaseTexture9::IDirect3DBaseTexture9()
{
  m_CurState.m_pTex = NULL;

  m_CurState.nAddressU = -1;
  m_CurState.nAddressV = -1;
  m_CurState.nAddressW = -1;

  m_CurState.nD3DMagFilter = -1;
  m_CurState.nD3DMinFilter = -1;
  m_CurState.nD3DMipFilter = -1;

  m_nFilterType = D3DTEXF_NONE;
  m_nLod = 0;
}

IDirect3DVolumeTexture9::~IDirect3DVolumeTexture9()
{
  if (m_nID)
  {
    glDeleteTextures(1, &m_nID);
    m_nID = 0;
  }

  for (int i=0; i<m_nLevelCount; i++)
  {
    SAFE_DELETE_ARRAY(m_pLockedData[i]);
  }
  SAFE_DELETE_ARRAY(m_pLockedData);
}

HRESULT IDirect3DVolumeTexture9::LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
{
  if (Level >= m_nLevelCount)
    return D3DERR_INVALIDCALL;

  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  int d = m_nDepth >> Level;
  if (!d)
    d = 1;
  int nID = Level;
  if (!m_pLockedData[nID])
  {
    int nSize = CTexture::TextureDataSize(w, h, d, 1, eTF);
    m_pLockedData[nID] = new byte[nSize];
  }
  int nPixelBytes = CTexture::ComponentsForFormat(eTF);
  pLockedVolume->RowPitch = w * nPixelBytes;
  pLockedVolume->SlicePitch = d;
  pLockedVolume->pBits = m_pLockedData[nID];

  return S_OK;
}

HRESULT IDirect3DVolumeTexture9::UnlockBox(UINT Level)
{
  int nID = Level;
  if (!m_pLockedData[nID])
    return D3DERR_INVALIDCALL;

  glBindTexture(m_nGLtarget, m_nID);

  ETEX_Format eTF = CTexture::TexFormatFromDeviceFormat((int)m_Format);
  int w = m_nWidth >> Level;
  if (!w)
    w = 1;
  int h = m_nHeight >> Level;
  if (!h)
    h = 1;
  int d = m_nDepth >> Level;
  if (!d)
    d = 1;
	int nSize = CTexture::TextureDataSize(w, h, d, 1, eTF);

	int dstFormat = 0, dstPixelFormat = 0;
	bool isCompressed = false;
	switch (m_Format)
	{
	case D3DFMT_DXT1:
		dstFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		isCompressed = true;
		break;
	case D3DFMT_DXT3:
		dstFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		isCompressed = true;
		break;
	case D3DFMT_DXT5:
		dstFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		isCompressed = true;
		break;
	case D3DFMT_X8R8G8B8:
		dstFormat = GL_RGBA8;
		dstPixelFormat = GL_BGRA;
		break;
	case D3DFMT_A8R8G8B8:
		dstFormat = GL_RGBA8;
		dstPixelFormat = GL_BGRA;
		break;
	case D3DFMT_A8:
		dstFormat = GL_ALPHA8;
		dstPixelFormat = GL_ALPHA;
		break;
	case D3DFMT_L8:
		dstFormat = GL_LUMINANCE8;
		dstPixelFormat = GL_LUMINANCE;
		break;
	default:
		assert(!"format not supported");
		break;
  }
	if (isCompressed)
		glCompressedTexImage3D(
			m_nGLtarget,
			Level,
			dstFormat,
			w, h, d, 0,
			nSize, m_pLockedData[nID]);
	else
    glTexImage3D(
			m_nGLtarget,
			Level,
			dstFormat, w, h, d, 0,
			dstPixelFormat,
			GL_UNSIGNED_BYTE,
			m_pLockedData[nID]);

  SAFE_DELETE_ARRAY(m_pLockedData[nID]);

  return S_OK;
}

//==================================================================================================
// Shaders D3DX functions

IDirect3DVertexShader9::~IDirect3DVertexShader9()
{
  if (m_nID)
  {
    cgDestroyProgram((CGprogram)(UINT_PTR)m_nID);
    m_nID = 0;
  }
}

IDirect3DPixelShader9::~IDirect3DPixelShader9()
{
#if 0
	// The base class destructor takes care of destroying the Cg program,
	// so there's no need to do it here!
  if (m_nID)
  {
    cgDestroyProgram((CGprogram)(UINT_PTR)m_nID);
    m_nID = 0;
  }
#endif
}

//==============================================================================================
// Shader constants

ID3DXConstantTable::~ID3DXConstantTable()
{
}

// Descs
HRESULT ID3DXConstantTable::GetDesc(D3DXCONSTANTTABLE_DESC *pDesc)
{
  if (!pDesc)
    return D3DERR_INVALIDCALL;
  pDesc->Constants = m_Constants.Num();
  pDesc->Version = 0;
  pDesc->Creator = "OpenGL CG";

  return S_OK;
}

HRESULT ID3DXConstantTable::GetConstantDesc(D3DXHANDLE hConstant, D3DXCONSTANT_DESC *pConstantDesc, UINT *pCount)
{
  if (!pConstantDesc || !pCount)
    return D3DERR_INVALIDCALL;
  *pConstantDesc = m_Constants[(int)(UINT_PTR)hConstant];
  *pCount = 1;

  return S_OK;
}

UINT ID3DXConstantTable::GetSamplerIndex(D3DXHANDLE hConstant)
{
  return 0;
}

// Handle operations
D3DXHANDLE ID3DXConstantTable::GetConstant(D3DXHANDLE hConstant, UINT Index)
{
  return (D3DXHANDLE)(UINT_PTR)Index;
}

ID3DXBuffer::~ID3DXBuffer()
{
  if (m_eType == eD3DXBufType_CGProg)
  {
    CGprogram Prog = (CGprogram)m_pBuf;
    cgDestroyProgram(Prog);
    m_pBuf = NULL;
  }
  else
  if (m_eType == eD3DXBufType_Buf)
  {
    SAFE_DELETE_VOID_ARRAY(m_pBuf);
  }
  else
  {
    assert(0);
  }
}

#ifdef PS3
size_t cgSrvCompile(const char *profile,
										const char *program,
										const char *entry,
										const char *comment,
										const char **options,
										char *buffer,
										size_t bufferSize
										);
#endif

#if defined(DEBUG) && !defined(PS3)
// Check the output of the Cg compiler.
// Return true if there's something worth reporting and false if everything is
// fine.
static bool CheckCgCompilerOutput(const char *compilerOutput)
{
	int reportLines = 0;
	const char *p, *q;
	char line[1024];

	// Check if the output contains anything but whitespace and patterns to be
	// ignored.
	for (p = q = compilerOutput;; ++p)
	{
		if (!*p || *p == '\n')
		{
			bool ignore = false;
			if (p > q)
			{
				if (p - q > sizeof line)
				{
					memcpy(line, q, sizeof line);
					line[sizeof line - 1] = 0;
				}
				else
				{
					memcpy(line, q, p - q);
					line[p - q] = 0;
				}
				for (; q < p + 1; ++q)
					if (!isspace(*q))
						break;
				if (q == p + 1) {
					// Line is all whitespace, ignore.
					continue;
				}
				if (strstr(line, "Profile option 'NumTemps'"))
					ignore = true;
				else if (strstr(line, "too large for semantic"))
					ignore = true;
				else if (strstr(line, "warning C1058: too much data in initialization"))
					ignore = true;
			}
			else
				ignore = true;
			if (!ignore)
				reportLines += 1;
			q = p + 1;
		}
		if (!*p)
			break;
	}
	return reportLines > 0;
}
#endif

HRESULT D3DXCompileShader(
                  LPCSTR                          pSrcData,
                  UINT                            SrcDataLen,
                  CONST D3DXMACRO*                pDefines,
                  LPD3DXINCLUDE                   pInclude,
                  LPCSTR                          pFunctionName,
                  LPCSTR                          pProfile,
                  DWORD                           Flags,
                  LPD3DXBUFFER*                   ppShader,
                  LPD3DXBUFFER*                   ppErrorMsgs,
                  LPD3DXCONSTANTTABLE*            ppConstantTable)
{
	const char *profileOpts[] =
	{
		"-DCGC=1",
		NULL,
	};

#if defined(PS3)
	// The early versions of the Cell SDK for PS3 do not support runtime shader
	// compilation. Until this issue is fixed, we'll work around the problem by
	// compiling shader programs on the development PC workstation. The PC is
	// running a Cg compilation server and we'll use that server for compiling
	// shader programs.
	const size_t bufferSize = 0x40000;
	char *buffer = new char[bufferSize];
	size_t size = cgSrvCompile(
			pProfile,
			pSrcData,
			pFunctionName,
			ps3ShaderName[0] ? ps3ShaderName : NULL,
			profileOpts,
			buffer,
			bufferSize);
	if (size == (size_t)-1 || size == bufferSize)
	{
		if (ppErrorMsgs)
		{
			static const char errMsg[] = "Shader compilation failed!\n";
			LPD3DXBUFFER errBuffer = new ID3DXBuffer;
			errBuffer->m_pBuf = (void *)new char[sizeof errMsg];
			strcpy((char *)errBuffer->m_pBuf, errMsg);
			errBuffer->m_nSize = sizeof errMsg;
			errBuffer->m_eType = eD3DXBufType_Buf;
			*ppErrorMsgs = errBuffer;
		}
		delete[] buffer;
		return D3DXERR_INVALIDDATA;
	}
	buffer[size] = 0;
	CGprofile profile;
	if (!strcmp(pProfile, "vs_2_0") || !strcmp(pProfile, "vs_1_1"))
	{
		//profile = CG_PROFILE_ARBVP1;
		//profile = CG_PROFILE_SCE_VP_TYPEC;
		profile = cgGLGetLatestProfile(CG_GL_VERTEX);
	} else if (!strcmp(pProfile, "ps_2_0") || !strcmp(pProfile, "ps_1_1"))
	{
		//profile = CG_PROFILE_ARBFP1;
		//profile = CG_PROFILE_SCE_FP_TYPEC;
		profile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
	} else
		assert(!"unrecognized profile");

	// Workaround for the broken cgGetProgramString() function.
	CGprogram prog = cgCreateProgram(scgContext, CG_BINARY,
			buffer + 4, profile, NULL, NULL);
	if (!prog)
	{
		CGerror err = cgGetError();
		const char *errMsg = cgGetErrorString(err);
		if (!errMsg) errMsg = "<null>";
		Warning("Could not create CG program [%i]: %s", (int)err, errMsg);
		assert(!"cgCreateProgram() failed");
	}
	StoreShaderProgram(prog, buffer, size,
			ps3ShaderName[0] ? ps3ShaderName : NULL);

	LPD3DXBUFFER dxBuffer = new ID3DXBuffer;
	dxBuffer->m_pBuf = prog;
	dxBuffer->m_nSize = 1;
	dxBuffer->m_eType = eD3DXBufType_CGProg;
	if (ppShader)	*ppShader = dxBuffer;
	*ppConstantTable = NULL;
	delete[] buffer;
	return S_OK;
#else
  CGprofile profile;
  if (!strcmp(pProfile, "vs_2_0"))
    profile = CG_PROFILE_ARBVP1;
  else
  if (!strcmp(pProfile, "vs_1_1"))
    profile = CG_PROFILE_ARBVP1;
  else
  if (!strcmp(pProfile, "ps_2_0"))
    profile = CG_PROFILE_ARBFP1;
  else
  if (!strcmp(pProfile, "ps_1_1"))
    profile = CG_PROFILE_ARBFP1;
  else
    assert(0);

  cgGLSetOptimalOptions(profile);

#if !defined(DEBUG)
  CGprogram Prog = cgCreateProgram(
			scgContext,
			CG_SOURCE,
			pSrcData,
			profile,
			pFunctionName,
			profileOpts);
#else
	// For debugging, we'll dump the Cg program to a temorary file and then call
	// cgCreateProgramFromFile() (so we'll get line numbers in error and warning
	// messages).  If the compiler reports anything unusual (error or warning),
	// then we'll keep the temporary file and also log the filename of the
	// temporary file.
	static int fileCounter = 1;
	char tmpFilename[256];
	snprintf(tmpFilename, sizeof tmpFilename,
			"/tmp/cgprogram_%i.cg", fileCounter);
	tmpFilename[sizeof tmpFilename - 1] = 0;
	++fileCounter;
	remove(tmpFilename);
	FILE *tmpFile = fopen(tmpFilename, "w");
	if (tmpFile == NULL)
	{
		Warning("Error writing temporary Cg file '%s': %s\n",
				tmpFilename, strerror(errno));
		return D3DXERR_INVALIDDATA;
	}
	fputs(pSrcData, tmpFile);
	fclose(tmpFile);
	tmpFile = NULL;
	CGprogram Prog = cgCreateProgramFromFile(
			scgContext,
			CG_SOURCE,
			tmpFilename,
			profile,
			pFunctionName,
			profileOpts);
	const char *compilerOutput = cgGetLastListing(scgContext);
	if (compilerOutput != NULL && CheckCgCompilerOutput(compilerOutput))
		iLog->Log("Cg compiler output for '%s':\n%s", tmpFilename, compilerOutput);
	else
		remove(tmpFilename);
#endif
  CGerror err = cgGetError();
  if (err != CG_NO_ERROR)
  {
    if (err == CG_COMPILER_ERROR)
    {
      const char *sError = cgGetErrorString(err);
      Warning("Couldn't create CG program (%s)", sError);
#if !defined(DEBUG)
			// For DEBUG the compiler output has already been logged.
			const char *compilerOutput = cgGetLastListing(scgContext);
			if (compilerOutput != NULL)
			{
				iLog->Log("Cg compiler output:\n%s", compilerOutput);
			}
#endif
      return D3DXERR_INVALIDDATA;
    }
  }
  assert(Prog);

  LPD3DXBUFFER pCode = new ID3DXBuffer;
  pCode->m_pBuf = Prog;
  pCode->m_nSize = 1;
  pCode->m_eType = eD3DXBufType_CGProg;
  const char *szCode;
	szCode = cgGetProgramString(Prog, CG_COMPILED_PROGRAM);

  if (ppShader)
    *ppShader = pCode;
  *ppConstantTable = NULL;

  return S_OK;
#endif
}

HRESULT D3DXBuildConstantsTable(IDirect3DBaseShader9 *pSH, LPD3DXCONSTANTTABLE *ppConstantTable)
{
  if (!pSH)
    return D3DERR_INVALIDCALL;

  if (!scgContext)
    scgContext = cgCreateContext();

  CGprogram Prog = (CGprogram)(UINT_PTR)pSH->m_nID;
  assert(Prog);

  if (ppConstantTable)
  {
    ID3DXConstantTable *pTable = new ID3DXConstantTable;

    RecurseParams(Prog, cgGetFirstParameter(Prog, CG_PROGRAM), pTable);
    *ppConstantTable = pTable;
  }
  return S_OK;
}

HRESULT D3DXGetShaderConstantTable(CONST DWORD *pFunction,
																	 LPD3DXCONSTANTTABLE *ppConstantTable)
{
#if defined(OPENGL)
#if defined(PS3)
	uint32 profile = (uint32)pFunction[1];
	SwapEndian(profile);
#endif
#if defined(LINUX)
	uint32 profile;
	if (((const char *)pFunction)[5] == 'v')
		profile = GL_VERTEX_PROGRAM_ARB;
	else
		profile = GL_FRAGMENT_PROGRAM_ARB;
#endif
	CGprogram prog = cgCreateProgram(
			scgContext,
#if defined(PS3)
			CG_BINARY,
			(const char *)pFunction + 4,
#else
			CG_OBJECT,
			(const char *)pFunction,
#endif
		 (CGprofile)profile,
		 NULL,
		 NULL);
	ID3DXConstantTable *pTable = new ID3DXConstantTable;
	RecurseParams(prog, cgGetFirstParameter(prog, CG_PROGRAM), pTable);
	*ppConstantTable = pTable;
	cgDestroyProgram(prog);
#else
	assert(0);
#endif
	return S_OK;
}

HRESULT D3DXDisassembleShader(
                      CONST DWORD*                    pShader, 
                      BOOL                            EnableColorCode, 
                      LPCSTR                          pComments, 
                      LPD3DXBUFFER*                   ppDisassembly)
{
  if (!pShader)
    return D3DERR_INVALIDCALL;

  CGprogram Prog = (CGprogram)pShader;
  const char *code;
	size_t size;
#if defined(PS3)
	code = (const char*)GetShaderProgram(Prog, size);
#else
	code = cgGetProgramString(Prog, CG_COMPILED_PROGRAM);
	size = strlen(code)+1;
#endif
  char *str = new char[size];
  cryMemcpy(str, code, size);

  LPD3DXBUFFER pCode = new ID3DXBuffer;
  pCode->m_pBuf = str;
  pCode->m_nSize = size;
  if (ppDisassembly)
    *ppDisassembly = pCode;

  return S_OK;
}

HRESULT D3DXGetShaderInputSemantics(
                                    CONST DWORD*                    pFunction,
                                    D3DXSEMANTIC*                   pSemantics,
                                    UINT*                           pCount)
{
  if (!pSemantics || !pCount)
    return D3DERR_INVALIDCALL;

  CGprogram Prog = (CGprogram)pFunction;
  CGparameter Param = cgGetFirstParameter(Prog, CG_PROGRAM);
  int nSemantics = 0;
  while (Param != 0)
  {
    const char* Name = cgGetParameterName(Param);
    CGtype cgType = cgGetParameterType(Param);
    if (cgType == CG_STRUCT && !strcmp(Name, "IN"))
    {
      nSemantics = 0;
      for (
					CGparameter Param1 = cgGetFirstStructParameter(Param);
					Param1 != NULL;
					Param1 = cgGetNextParameter(Param1))
      {
#if defined(PS3)
				char NameBuffer[128];
				const char *Name = GetMappedSemantic(Param1, NameBuffer, sizeof NameBuffer);
#else
        const char *Name = cgGetParameterSemantic(Param1);
#endif
        CGtype cgType1 = cgGetParameterType(Param1);
				if (!cgIsParameterReferenced(Param1)
						|| cgGetParameterVariability(Param1) != CG_VARYING)
					continue;
        assert(cgType1 == CG_FLOAT4 || cgType1 == CG_FLOAT3 || cgType1 == CG_FLOAT2);
        if (!strncmp(Name, "POSITION", 8))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_POSITION;
				}
        else
        if (!strncmp(Name, "COLOR", 5))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_COLOR;
				}
        else
        if (!strncmp(Name, "TEXCOORD", 8))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_TEXCOORD;
				}
        else
        if (!strncmp(Name, "TANGENT", 7))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_TANGENT;
				}
        else
        if (!strncmp(Name, "BINORMAL", 8))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_BINORMAL;
				}
        else
        if (!strncmp(Name, "NORMAL", 6))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_NORMAL;
				}
        else
        if (!strncmp(Name, "PSIZE", 5))
				{
          pSemantics[nSemantics].Usage = D3DDECLUSAGE_PSIZE;
				}
				else
				if (!strncmp(Name, "BLENDWEIGHT", 11))
				{
					pSemantics[nSemantics].Usage = D3DDECLUSAGE_BLENDWEIGHT;
				}
				else
				if (!strncmp(Name, "BLENDINDICES", 12))
				{
					pSemantics[nSemantics].Usage = D3DDECLUSAGE_BLENDINDICES;
				}
        else
          assert(0);
#if 0
				unsigned long index = cgGetParameterResourceIndex(Param1);
				if (index == ~0)
				{
					CGerror err = cgGetError();
					CryError(
						"cgGetParameterResourceIndex(param = 0x%lx): error [%i]: %s",
						(unsigned long)Param1,
						(int)err,
						cgGetErrorString(err));
					assert(0);
				}
#else
				unsigned long index = 0;
				{
					const char *p = Name;
					while (*p && !isdigit(*p)) ++p;
					if (isdigit(*p))
						index = atoi(p);
					else
					{
						assert(
								pSemantics[nSemantics].Usage != D3DDECLUSAGE_COLOR
								&& pSemantics[nSemantics].Usage != D3DDECLUSAGE_TEXCOORD);
					}
				}
#endif
        pSemantics[nSemantics].UsageIndex = index;
				nSemantics++;
      }
      break;
    }

    Param = cgGetNextParameter(Param);
  }
  if (pCount)
    *pCount = nSemantics;

  return S_OK;
}

IDirect3DBaseShader9::~IDirect3DBaseShader9()
{
  CGprogram pr = (CGprogram)(UINT_PTR)m_nID;
	if (pr)
		cgDestroyProgram(pr);
  m_nID = 0;
}

static void D3DXGetSHParamHandle_nullParam(const char *pN)
{
	printf("D3DXGetSHParamHandle_nullParam(\"%s\")\n", pN);
}

int D3DXGetSHParamHandle(void *pSH, SCGBind *pParam)
{
  IDirect3DBaseShader9 *pShader = (IDirect3DBaseShader9 *)pSH;
  CGprogram pr = (CGprogram)(UINT_PTR)pShader->m_nID;
	const char *paramName = pParam->m_Name.c_str();
  CGparameter param = GetNamedParameterNocase(pr, paramName);

	if (!param)
		D3DXGetSHParamHandle_nullParam(paramName);

	int nSampler = pParam->m_dwBind & SHADER_BIND_SAMPLER;
	if (!nSampler)
		pParam->m_dwBind = (int)(UINT_PTR)param;
	if(!nSampler && param)
	{
		//determine if it is a matrix type
		CGtype cgType = cgGetParameterType(param);
		switch(cgType)
		{
		case CG_FLOAT2x4:
			pParam->m_isMatrix = scIs2x4Matrix;
			break;
		case CG_FLOAT3x4:
			pParam->m_isMatrix = scIs3x4Matrix;
			break;
		case CG_FLOAT4x4:
			pParam->m_isMatrix = scIs4x4Matrix;
			break;
		case CG_ARRAY:
			pParam->m_isMatrix = scIsArray;
			break;
		case CG_STRUCT:
		default:
			break;
		}
	}
  return pParam->m_dwBind;
}

void SGlobalConstMap::InitHandle(void *pSH, IDirect3DDevice9 *pDev, const bool cKeepHandle, const bool cDetType)
{
	assert(pSH);
	if(!cpConstName || !pSH)//not ready yet, probably not even initialized yet
		return;
	if(!IsHandleValid())
	{
		IDirect3DBaseShader9 *pShader = (IDirect3DBaseShader9 *)pSH;
		CGprogram pr = (CGprogram)(UINT_PTR)pShader->m_nID;
		CGparameter param = cgGetNamedParameter(pr, cpConstName);
		if(param > 0) 
		{
			handle = (int)(UINT_PTR)param;
			toBeUpdated = 1;
			if(cDetType)
			{
				//determine if it is a matrix type
				CGtype cgType = cgGetParameterType(param);
				switch(cgType)
				{
				case CG_FLOAT2x4:
					isMatrix = scIs2x4Matrix;
					break;
				case CG_FLOAT3x4:
					isMatrix = scIs3x4Matrix;
					break;
				case CG_FLOAT4x4:
					isMatrix = scIs4x4Matrix;
					break;
				case CG_ARRAY:
					isMatrix |= scIsArray;
					break;//must be set outside
				case CG_STRUCT:
				default:
					isMatrix = 0;
				}
			}
		}
	}
	if(toBeUpdated && IsHandleValid())
	{
		assert(pDev);
		if(toBeUpdatedCount > 1)
			SetConstantIntoShader(pDev, pToBeUpdatedValues, toBeUpdatedCount, toBeUpdatedOffset);
		else
			SetConstantIntoShader(pDev, &values[0], 1, toBeUpdatedOffset);
	}
	if(!cKeepHandle)
		handle = -1;
}

HRESULT SGlobalConstMap::SetConstantIntoShader
(
	IDirect3DDevice9 *pDev, 
	CONST float* pConstantData,
	UINT Vector4fCount,
	const unsigned int cOffset
)
{
	assert(pDev);
	toBeUpdated = 0;
	values[0] = pConstantData[0];	values[1] = pConstantData[1];
	values[2] = pConstantData[2];	values[3] = pConstantData[3];
	toBeUpdatedCount	= Vector4fCount;
	toBeUpdatedOffset = cOffset;
	if(!IsHandleValid())//handle not valid
	{
		if(Vector4fCount > 1 || cOffset != 0)
		{
			if(toBeUpdatedAllocCount < Vector4fCount * 4)
			{
				delete [] pToBeUpdatedValues;
				toBeUpdatedAllocCount = 4 * Vector4fCount;
				pToBeUpdatedValues = new float[toBeUpdatedAllocCount];
			}
			assert(pToBeUpdatedValues);
			memcpy(pToBeUpdatedValues, pConstantData, Vector4fCount * 4 * sizeof(float));
		}
		toBeUpdated = 1;
		return S_OK;
	}
	else
	{
		int nReg = handle | 
			((isMatrix & scIs2x4Matrix)?scParam2x4MatrixBit :
			(isMatrix & scIs3x4Matrix)?scParam3x4MatrixBit :
			(isMatrix & scIs4x4Matrix)?scParam4x4MatrixBit : 0);
		nReg = nReg | ((isMatrix & scIsArray)?scParamArrayBit : 0);
		return ((shaderType == scPSConst)?
			pDev->SetPixelShaderConstantF(nReg, pConstantData, Vector4fCount, cOffset) :
			pDev->SetVertexShaderConstantF(nReg, pConstantData, Vector4fCount, cOffset));
	}
}

// The Float16->Float32 convertion is needed only for debugging, so there's
// not need for an ultra-optimized inline version.
// BEWARE: This method is completely untested!
FLOAT *D3DXFloat16To32Array(FLOAT *pOut, const D3DXFLOAT16 *pIn, UINT n)
{
	for( int i (0); i < n; ++i )
	{
		uint16_t src = *(uint16_t *)&pIn[i];
		unsigned m = (src & 0x3FF) | 0x400;
		int e = (int)((src & 0x7C00) >> 10) - 11;
		unsigned s = (src & 0x8000) >> 15;
		FLOAT v;
		if (e > 0)
			v = (float)m / (1 << e);
		else if (e < 0)
			v = (float)m * (1 << -e);
		if (s)
			v = -v;
		pOut[i] = v;
	}
	return pOut;
}

#if defined(PS3) || defined(LINUX)

static void LoadingScreen()
{
	// Programmer art...
	static const char loading[][100] = {
		//|       |       |       |       |       |       |       |       |       |
		"...............................................................................",
		".##...........................##....##.........................................",
		".##...........................##...............................................",
		".##.......#####....####...######..####...##.###...######.......................",
		".##......##...##......##.##...##....##...###..##.##...##.......................",
		".##......##...##..######.##...##....##...##...##.##...##.......................",
		".##......##...##.##...##.##...##....##...##...##..######....##......##......##.",
		".#######..#####...######..######..######.##...##......##....##......##......##.",
		"..................................................#####........................",
		"...............................................................................",
		""
	};

	uint8_t fg[4] = { 0x08, 0x48, 0x10, 0xff };
	uint8_t bg[4] = { 0x0c, 0x26, 0x19, 0x00 };

	glClearColor(bg[0] / 255.0, bg[1] / 255.0, bg[2] / 255.0, bg[3] / 255.0);
#if defined(PS3)
	glClearDepthf(1.0f);
#else
	glClearDepth(1.0f);
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLuint tex_id;
	glGenTextures(1, &tex_id);
	int width = strlen(loading[0]);
	int height;
	for (height = 0; loading[height][0]; ++height);
	uint8_t *image = new uint8_t[width * height * 4];
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			char c = loading[y][x];
			uint8_t *color = c == '.' ? bg : fg;
			memcpy(image + (y * width + x) * 4, color, 4);
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glClientActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			width, height, 0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			image);

	delete[] image;
	image = NULL;

	GLuint vb_id;
	glGenBuffers(1, &vb_id);
	glBindBuffer(GL_ARRAY_BUFFER, vb_id);
	glBufferData(
			GL_ARRAY_BUFFER,
			4 * sizeof(struct_VERTEX_FORMAT_P3F_TEX2F),
			NULL,
			GL_DYNAMIC_DRAW);

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	static const float stretch[2] = {
		10.0 /* Stretch X */,
		10.0 /* Stretch Y */
	};
	float size[2] = {
		stretch[0] * width / gScreenWidth /* Width */,
		stretch[1] * height / gScreenHeight /* Height */
	};
	const float dy = 0.05; /* Delta Y, positive = Up */
	float quad[4] = {
		-size[0] / 2 /* Left */,
		size[1] / 2 + dy /* Top */,
		size[0] / 2 /* Right */,
		-size[1] / 2 + dy /* Bottom */
	};
	struct vertex { float xyz[3]; float st[2]; };
	static const vertex verts[4] =
	{
	  { { quad[0], quad[1], 0 }, { 0, 0 } } /* Top left */,
		{ { quad[2], quad[1], 0 }, { 1, 0 } } /* Top right */,
		{ { quad[0], quad[3], 0 }, { 0, 1 } } /* Bottom left */,
		{ { quad[2], quad[3], 0 }, { 1, 1 } } /* Bottom right */
	};
	vertex *vb_data;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	vb_data = (vertex *)glMapBuffer(
			GL_ARRAY_BUFFER,
			GL_WRITE_ONLY);
	memcpy(vb_data, verts, sizeof verts);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glVertexPointer(
			3, GL_FLOAT, sizeof(vertex), (uint8_t *)0);
	glTexCoordPointer(
			2, GL_FLOAT, sizeof(vertex), (uint8_t *)12);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vb_id);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &tex_id);

	glFlush();
}

#endif // PS3

#if 0
ULONG IDirect3DTexture9::AddRef()
{
	ULONG refCount = IPS3Unknown::AddRef();
	printf("IDirect3DTexture9::AddRef RefCount = %u\n", (unsigned)refCount);
	if (refCount > 3)
	{
		puts("break here");
	}
	return refCount;
}

ULONG IDirect3DTexture9::Release()
{
	ULONG refCount = IPS3Unknown::Release();
	printf("IDirect3DTexture9::Release RefCount = %u\n", (unsigned)refCount);
	if (refCount > 2 || refCount < 1)
	{
		puts("break here");
	}
	return refCount;
}
#endif

#endif //OPENGL

// vim:ts=2

