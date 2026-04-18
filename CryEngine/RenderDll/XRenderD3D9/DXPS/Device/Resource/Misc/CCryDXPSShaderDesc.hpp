#ifndef __CRYDXPSSHADERDESC__
#define __CRYDXPSSHADERDESC__

#include "../../../Tools/DXPSShaderCompiler/DXPSShaderCompiler/Core/Reflect/DXPSReflectionShared.hpp"

class CDXPSShaderDesc	:	protected SShaderDesc
{
	inline uint8*			Data()const{return (uint8*)this;}
public:

	inline	uint32											ValidateVersion()const{return m_Version==DXPS_REF_SHADER_VERSION;}
	inline	uint32											PixelShader()const{return m_PixelShader;}

	inline	const SRefVertexAttribute*	VertexAttribute()const{return reinterpret_cast<const SRefVertexAttribute*>(&Data()[m_VertexAttributeOffset]);}
	inline	uint32											VertexAttributeCount()const{return m_VertexAttributeCount;}

	inline	const SRefSampler*					Sampler()const{return reinterpret_cast<const SRefSampler*>(&Data()[m_SamplerOffset]);}
	inline	uint32											SamplerCount()const{return m_SamplerCount;}

	inline	const SRefConstant*					Constant()const{return reinterpret_cast<const SRefConstant*>(&Data()[m_ConstOffset]);}
	inline	uint32											ConstantCount()const{return m_ConstCount;}

	inline	const SVSConst*							VSConstant()const{return reinterpret_cast<const SVSConst*>(&Data()[m_VSConstOffset]);}
	inline	uint32											VSConstantCount()const{return m_VSConstCount;}

	inline	const	SRefPatch*						Patch()const{return reinterpret_cast<const SRefPatch*>(&Data()[m_PatchOffset]);}
	inline	uint32											PatchCount()const{return m_PatchCount;}

	inline	CGprogram										Program()const{return reinterpret_cast<CGprogram>(&Data()[m_ShaderBinaryOffset]);}
	inline	uint32											ProgramSize()const{return m_ShaderBinarySize;}

	inline	const char*									NameTable()const{return reinterpret_cast<const char*>(&Data()[m_NameTableOffset]);}
	inline	uint32											NameTableSize()const{return m_NameTableSize;}
};

#endif

