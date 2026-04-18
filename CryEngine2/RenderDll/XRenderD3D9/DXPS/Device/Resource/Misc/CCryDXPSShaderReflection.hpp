#ifndef __CRYDXPSSHADERREFLECTION__
#define __CRYDXPSSHADERREFLECTION__

#include "../CCryDXPSResource.hpp"
#include "../../../CCryDXPSMisc.hpp"
#include "../../../Layer0/CCryDXPS.hpp"


class CCryShaderReflectionType;
class CCryShaderReflectionType
{
private:
	friend class CCryDXPSShaderReflectionConstBuffer;
	D3D10_SHADER_VARIABLE_CLASS	m_TypeClass;
public:
    void GetDesc(D3D10_SHADER_TYPE_DESC *pDesc);
    CCryShaderReflectionType* GetMemberTypeByIndex(uint32 Index);
    CCryShaderReflectionType* GetMemberTypeByName(char* Name);
    const char* GetMemberTypeName(uint32 Index);
};

typedef CCryShaderReflectionType ID3D10ShaderReflectionType;

class CCryShaderReflectionVariable
{
private:
	friend class CCryDXPSShaderReflectionConstBuffer;
	CCryShaderReflectionType		m_Type;
	D3D10_SHADER_VARIABLE_DESC	m_VarDesc;
public:
    void GetDesc(D3D10_SHADER_VARIABLE_DESC *pDesc);
    CCryShaderReflectionType* GetType();
};

typedef CCryShaderReflectionVariable ID3D10ShaderReflectionVariable;

class CCryDXPSShaderReflectionConstBuffer
{
private:
	friend class CCryDXPSShaderReflection;
	CCryShaderReflectionVariable				m_Var;
//	CGprogram														m_Program;
	const CDXPSShaderDesc*							m_pShaderDesc;

	//CCryShaderReflectionVariable* GetVariableByParam(CGparameter Param);
public:
    void GetDesc(D3D10_SHADER_BUFFER_DESC *pDesc);
    CCryShaderReflectionVariable* GetVariableByIndex(uint32 Index);
    CCryShaderReflectionVariable* GetVariableByName(const char* Name);
};

typedef CCryDXPSShaderReflectionConstBuffer ID3D10ShaderReflectionConstantBuffer;


class CCryDXPSShaderReflection;
class CCryDXPSShaderReflection	:	public CCryDXPSResource	,	public CCryRefAndWeak<CCryDXPSShaderReflection>
{
private:
	CCryDXPSShaderReflectionConstBuffer m_ConstBuffer;

public:
	CCryDXPSShaderReflection(const char* pProgram):
		CCryDXPSResource(EDXPS_RT_SHADERREFLECTION),
		CCryRefAndWeak<CCryDXPSShaderReflection>(this)
	{
		const uint32 ByteSize	=	reinterpret_cast<const uint32*>(pProgram)[0];
		uint8* pData	=	(uint8*)CRY_DXPS_NEWARRAY(uint8,ByteSize);
		for(uint32 a=0;a<ByteSize;a++)
			pData[a]	=	reinterpret_cast<const uint8*>(pProgram)[a+4];

//		m_ConstBuffer.m_Program	=	(CGprogram)pData;
		m_ConstBuffer.m_pShaderDesc	=	(const CDXPSShaderDesc*)pData;
	}
	inline	void		ReleaseResources()
									{
										CRY_DXPS_DELETEARRAY(reinterpret_cast<const uint8*>(m_ConstBuffer.m_pShaderDesc));
										m_ConstBuffer.m_pShaderDesc	=	0;
									}
	
	void GetDesc(D3D10_SHADER_DESC *pDesc);
    
	CCryDXPSShaderReflectionConstBuffer* GetConstantBufferByIndex(uint32 Index);
	CCryDXPSShaderReflectionConstBuffer* GetConstantBufferByName(char* Name);
    
  void GetResourceBindingDesc(uint32 ResourceIndex, D3D10_SHADER_INPUT_BIND_DESC *pDesc);
    
  void GetInputParameterDesc(uint32 ParameterIndex, D3D10_SIGNATURE_PARAMETER_DESC *pDesc);
  void GetOutputParameterDesc(uint32 ParameterIndex, D3D10_SIGNATURE_PARAMETER_DESC *pDesc);

	unsigned long		AddRef(){return IncRef();}
	unsigned long		Release(){return DecRef();}

	//unified const reflection
/*	static void			CopyWithoutBrackets(char* pDst,const char* pSrc);
	static uint32		ParseIndex(const char* pName);
	static bool			FirstOccurence(CGprogram Program,const CGparameter SeekParam);
	static uint32		IndexOf(CGprogram Program,const CGparameter SeekParam,bool ReflectIndexed);
	static uint32		SizeOf(CGprogram Program,const CGparameter SeekParam);
	*/
};

typedef CCryDXPSShaderReflection ID3D10ShaderReflection;

#endif

