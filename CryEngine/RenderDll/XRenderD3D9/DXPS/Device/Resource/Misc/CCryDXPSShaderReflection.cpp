#include "StdAfx.h"
#include "../../../Layer0/CCryDXPS.hpp"
#include "CCryDXPSShaderReflection.hpp"
#include "CCryDXPSShaderDesc.hpp"
//#include <assert.h>
#include <algorithm>

//////////////////////////////////////////////////////////////
void CCryShaderReflectionType::GetDesc(D3D10_SHADER_TYPE_DESC *pDesc)
{
	pDesc->Class	=	m_TypeClass;
}

ID3D10ShaderReflectionType* CCryShaderReflectionType::GetMemberTypeByIndex(uint32 Index)
{
	CRY_ASSERT_MESSAGE(0,"CCryShaderReflectionType::GetMemberTypeByIndex not implemented yet");
	return 0;
}

ID3D10ShaderReflectionType* CCryShaderReflectionType::GetMemberTypeByName(char* Name)
{
	CRY_ASSERT_MESSAGE(0,"CCryShaderReflectionType::GetMemberTypeByName not implemented yet");
	return 0;
}

const char* CCryShaderReflectionType::GetMemberTypeName(uint32 Index)
{
	CRY_ASSERT_MESSAGE(0,"CCryShaderReflectionType::GetMemberTypeName not implemented yet");
	return "";
}

//////////////////////////////////////////////////////////////
void CCryShaderReflectionVariable::GetDesc(D3D10_SHADER_VARIABLE_DESC *pDesc)
{
	*pDesc	=	m_VarDesc;
//	memcpy(pDsec,&m_VarDesc,sizeof(D3D10_SHADER_VARIABLE_DESC));
}

ID3D10ShaderReflectionType* CCryShaderReflectionVariable::GetType()
{
	return &m_Type;
}

//////////////////////////////////////////////////////////////
void CCryDXPSShaderReflectionConstBuffer::GetDesc(D3D10_SHADER_BUFFER_DESC *pDesc)
{
//	pDesc->Variables	=	cellGcmCgGetCountParameter(m_Program);
	pDesc->Variables	=	m_pShaderDesc->ConstantCount();
}


/*bool CCryDXPSShaderReflection::FirstOccurence(CGprogram Program,const CGparameter SeekParam)
{
	const char* pSeekName	=	cellGcmCgGetParameterName(Program,SeekParam);
	int32 SeekL		=	strlen(pSeekName);
	if(pSeekName[SeekL-1]==']')
	{
		while(pSeekName[--SeekL]!='[');
	}
	for(uint32 a=0,Idx=0,LastIdx=0,Size=cellGcmCgGetCountParameter(Program);a<Size;a++)
	{
		CGparameter Param	=	cellGcmCgGetIndexParameter(Program,a);
		if(Param==SeekParam)
			return true;
		if(	CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
				CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
				CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
		{
			const char* pName	=	cellGcmCgGetParameterName(Program,Param);
			if(strncmp(pName,pSeekName,SeekL)==0 && (pName[SeekL]=='[' || pName[SeekL]==0))
				return false;
		}
	}
	CRY_ASSERT_MESSAGE(0,"ERROR: FirstOccurence: param not found at all!!!");
	return true;//error
}

uint32 CCryDXPSShaderReflection::ParseIndex(const char* pName)
{
	const int32 L		=	strlen(pName);
	if(L==0 || ']'!=pName[L-1])
		return 0;

	uint32	Idx=0;
	for(int a=2,Base=1;a<L && '['!=pName[L-a];a++)
	{
		Idx+=(pName[L-a]-'0')*Base;
		Base*=10;
	}
	return Idx;
}

uint32 CCryDXPSShaderReflection::SizeOf(CGprogram Program,const CGparameter SeekParam)
{
	const char* pSeekName	=	cellGcmCgGetParameterName(Program,SeekParam);
	int32 SeekL		=	strlen(pSeekName);
	if(pSeekName[SeekL-1]==']')
	{
		while(pSeekName[--SeekL]!='[');
	}

	uint32 ConstCount=0;
	for(uint32 a=0,Idx=0,LastIdx=0,Size=cellGcmCgGetCountParameter(Program);a<Size;a++)
	{
		CGparameter Param	=	cellGcmCgGetIndexParameter(Program,a);
		if(	CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
				CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
				CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
		{
			const char* pName	=	cellGcmCgGetParameterName(Program,Param);
			if(strncmp(pName,pSeekName,SeekL)==0 && pName[SeekL]=='[')
				ConstCount	=	max(ConstCount,CCryDXPSShaderReflection::ParseIndex(pName));
		}
	}
	return ConstCount+1;



/*	for(uint32 a=0,Size=cellGcmCgGetCountParameter(Program);a<Size;a++)
	{
		CGparameter Param	=	cellGcmCgGetIndexParameter(Program,a);
		if(	CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
				CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
				CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
		{
			if(Param==SeekParam)
			{
				uint32 Size=0;

#ifdef _DEBUG
	{
		const CGbool Referenced	=	cellGcmCgGetParameterReferenced(Program,Param);
		const CGenum InOut			=	cellGcmCgGetParameterDirection(Program,Param);
		const CGenum Varia			=	cellGcmCgGetParameterVariability(Program,Param);
		if(!strncmp("cBitmapColorTransform",cellGcmCgGetParameterName(Program,Param),sizeof("cBitmapColorTransform")-1))
		{
			int a=0;
		}
		const char* pName	=	cellGcmCgGetParameterName(Program,Param);
		char Semantic[1024];
		strcpy(Semantic,cellGcmCgGetParameterSemantic(Program,Param));
	}
#endif
				a++;
				Param	=	cellGcmCgGetIndexParameter(Program,a);
				const char* pName	=	cellGcmCgGetParameterName(Program,Param);
				const uint32 L		=	strlen(pName);
				while(']'==cellGcmCgGetParameterName(Program,Param)[L-1])
				{

#ifdef _DEBUG
	const CGbool Referenced	=	cellGcmCgGetParameterReferenced(Program,Param);
	const CGenum InOut			=	cellGcmCgGetParameterDirection(Program,Param);
	const CGenum Varia			=	cellGcmCgGetParameterVariability(Program,Param);
	const char* pName	=	cellGcmCgGetParameterName(Program,Param);
#endif
					Size++;
					a++;
					Param	=	cellGcmCgGetIndexParameter(Program,a);
				}

				return Size>0?Size:1;//if >0 then the indices are enumerated, else the size is 1
			}
		}
	}
	CRY_ASSERT_MESSAGE(0,"ERROR: SizeOf could not be determined !!!");
	return 0;
	*/
//}
/*
uint32 CCryDXPSShaderReflection::IndexOf(CGprogram Program,const CGparameter SeekParam,bool ReflectIndexed)
{
	//if we don't wish to index the indexed consts and it's not the first const with that name, return invalid
	if(!ReflectIndexed && !FirstOccurence(Program,SeekParam))
		return ~0;

	for(uint32 a=0,Idx=0,LastIdx=0,Size=cellGcmCgGetCountParameter(Program);a<Size;a++)
	{
		CGparameter Param	=	cellGcmCgGetIndexParameter(Program,a);
		if(	CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
				CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
				CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
		{
			if(Param==SeekParam)
			{
				const char* pName	=	cellGcmCgGetParameterName(Program,Param);
				const int32 L		=	strlen(pName);
				if(']'!=pName[L-1])
						return Idx;
				else
				{
					Idx=ParseIndex(pName);
					return LastIdx+Idx;//add the array index to the base index(lastidx)
				}
			}
			if(FirstOccurence(Program,Param))
			{
				LastIdx=Idx;
				Idx+=CCryDXPSShaderReflection::SizeOf(Program,Param);
			}
		}
	}
	CRY_ASSERT_MESSAGE(0,"ERROR: ParamIndex not found !!!");
	return ~0;
}

void CCryDXPSShaderReflection::CopyWithoutBrackets(char* pDst,const char* pSrc)
{
	while(*pSrc && *pSrc!='[')
	{
		*pDst++	=	*pSrc++;
	}
	*pDst=0;
}
*/

CCryShaderReflectionVariable* CCryDXPSShaderReflectionConstBuffer::GetVariableByIndex(uint32 Index)
{
#ifdef _DEBUG
	if(Index>=m_pShaderDesc->ConstantCount())
	{
		CRY_ASSERT_MESSAGE(0,"ERROR: index out of bound ->GetVariableByIndex!");
		return 0;
	}
#endif
	const SRefConstant& rConst	=	m_pShaderDesc->Constant()[Index];

	m_Var.m_Type.m_TypeClass			=	D3D10_SVC_SCALAR;
	m_Var.m_VarDesc.uFlags				= D3D10_SVF_USED;
	m_Var.m_VarDesc.Size					= rConst.m_Size<<4;
	m_Var.m_VarDesc.StartOffset		=	rConst.m_ConstRegister<<4;
	m_Var.m_VarDesc.CBufferIndex	=	rConst.m_ConstSlot;
	m_Var.m_VarDesc.Name					=	&m_pShaderDesc->NameTable()[rConst.m_NameIndex];

				
/*	CGprogram Program	=	m_Program;
	CGparameter Param	=	cellGcmCgGetIndexParameter(m_Program,Index);
	m_Var.m_VarDesc.uFlags = 0;
#ifdef _DEBUG
	const CGbool Referenced	=	cellGcmCgGetParameterReferenced(Program,Param);
	const CGenum InOut			=	cellGcmCgGetParameterDirection(Program,Param);
	const bool In						=	(CG_OUT			!=	InOut);
	const CGenum Varia			=	cellGcmCgGetParameterVariability(Program,Param);
	const bool Variability	=	(CG_UNIFORM	==	Varia);

	if(!strncmp("cBitmapColorTransform",cellGcmCgGetParameterName(Program,Param),sizeof("cBitmapColorTransform")-1))
	{
		int a=0;
	}

	char Semantic[1024];
	strcpy(Semantic,cellGcmCgGetParameterSemantic(Program,Param));
#endif
	if(	//FirstOccurence(m_Program,Index) &&
			CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
			CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
			CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
	{
		const CGtype T=cellGcmCgGetParameterType(Program,Param);
		switch(T)
		{
			case CG_HALF:
			case CG_FLOAT:
			case CG_BOOL:
				m_Var.m_Type.m_TypeClass	=	D3D10_SVC_SCALAR;
				m_Var.m_VarDesc.uFlags = D3D10_SVF_USED;
				break;
			case CG_BOOL2:
			case CG_BOOL3:
			case CG_BOOL4:
			case CG_HALF2:
			case CG_HALF3:
			case CG_HALF4:
			case CG_FLOAT2:
			case CG_FLOAT3:
			case CG_FLOAT4:
				m_Var.m_Type.m_TypeClass	=	D3D10_SVC_VECTOR;
				m_Var.m_VarDesc.uFlags = D3D10_SVF_USED;
				break;
			case CG_BOOL2x2:
			case CG_BOOL3x2:
			case CG_BOOL4x2:
			case CG_BOOL2x3:
			case CG_BOOL3x3:
			case CG_BOOL4x3:
			case CG_BOOL2x4:
			case CG_BOOL3x4:
			case CG_BOOL4x4:
			case CG_HALF2x2:
			case CG_HALF3x2:
			case CG_HALF4x2:
			case CG_HALF2x3:
			case CG_HALF3x3:
			case CG_HALF4x3:
			case CG_HALF2x4:
			case CG_HALF3x4:
			case CG_HALF4x4:
			case CG_FLOAT2x2:
			case CG_FLOAT3x2:
			case CG_FLOAT4x2:
			case CG_FLOAT2x3:
			case CG_FLOAT3x3:
			case CG_FLOAT4x3:
			case CG_FLOAT2x4:
			case CG_FLOAT3x4:
			case CG_FLOAT4x4:
				m_Var.m_Type.m_TypeClass	=	D3D10_SVC_MATRIX_ROWS;
				m_Var.m_VarDesc.uFlags = D3D10_SVF_USED;
				break;
		}
		switch(T)
		{
			case CG_HALF:m_Var.m_VarDesc.Size = 1*sizeof(float);break;
			case CG_FLOAT:m_Var.m_VarDesc.Size = 1*sizeof(float);break;
			case CG_BOOL:m_Var.m_VarDesc.Size = 1*sizeof(float);break;
			case CG_BOOL2:m_Var.m_VarDesc.Size = 2*sizeof(float);break;
			case CG_BOOL3:m_Var.m_VarDesc.Size = 3*sizeof(float);break;
			case CG_BOOL4:m_Var.m_VarDesc.Size = 4*sizeof(float);break;
			case CG_HALF2:m_Var.m_VarDesc.Size = 2*sizeof(float);break;
			case CG_HALF3:m_Var.m_VarDesc.Size = 3*sizeof(float);break;
			case CG_HALF4:m_Var.m_VarDesc.Size = 4*sizeof(float);break;
			case CG_FLOAT2:m_Var.m_VarDesc.Size = 2*sizeof(float);break;
			case CG_FLOAT3:m_Var.m_VarDesc.Size = 3*sizeof(float);break;
			case CG_FLOAT4:m_Var.m_VarDesc.Size = 4*sizeof(float);break;
			case CG_BOOL2x2:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_BOOL3x2:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_BOOL4x2:m_Var.m_VarDesc.Size = 15*sizeof(float);break;
			case CG_BOOL2x3:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_BOOL3x3:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_BOOL4x3:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_BOOL2x4:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_BOOL3x4:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_BOOL4x4:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_HALF2x2:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_HALF3x2:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_HALF4x2:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_HALF2x3:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_HALF3x3:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_HALF4x3:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_HALF2x4:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_HALF3x4:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_HALF4x4:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_FLOAT2x2:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_FLOAT3x2:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_FLOAT4x2:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_FLOAT2x3:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_FLOAT3x3:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_FLOAT4x3:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
			case CG_FLOAT2x4:m_Var.m_VarDesc.Size = 8*sizeof(float);break;
			case CG_FLOAT3x4:m_Var.m_VarDesc.Size = 12*sizeof(float);break;
			case CG_FLOAT4x4:m_Var.m_VarDesc.Size = 16*sizeof(float);break;
		}
		m_Var.m_VarDesc.StartOffset	=	cellGcmCgGetParameterResourceIndex(Program,Param)<<4;//Index<<2;
		m_Var.m_VarDesc.Name				=	cellGcmCgGetParameterName(Program,Param);
		if(m_Var.m_VarDesc.uFlags == D3D10_SVF_USED && m_Var.m_VarDesc.StartOffset>=(468<<4))
		{
			const uint32 Reg	=	CCryDXPSShaderReflection::IndexOf(m_Program,Param,false);
			if(Reg==~0)
				m_Var.m_VarDesc.uFlags=0;
			else
			{
				const uint32 Size	=	CCryDXPSShaderReflection::SizeOf(m_Program,Param);
				if(Reg==~0)
					m_Var.m_VarDesc.uFlags=0;
				else
				{
					m_Var.m_VarDesc.StartOffset	=	Reg<<4;
					m_Var.m_VarDesc.Size = 4*sizeof(float)*Size;
				}
			}
		}
	}*/
	return &m_Var;
}

CCryShaderReflectionVariable* CCryDXPSShaderReflectionConstBuffer::GetVariableByName(const char* Name)
{
	return 0;//GetVariableByParam(cellGcmCgGetNamedParameter(m_Program,Name));
}


//////////////////////////////////////////////////////////////


void CCryDXPSShaderReflection::GetDesc(D3D10_SHADER_DESC *pDesc)
{
	pDesc->ConstantBuffers	=	1;
//	pDesc->BoundResources		=	cellGcmCgGetCountParameter(m_ConstBuffer.m_Program);
	pDesc->BoundResources		=	m_ConstBuffer.m_pShaderDesc->SamplerCount();
	pDesc->InputParameters  = 0;

	//count input parameters
/*	CGparameter Param=cellGcmCgGetFirstLeafParameter(m_ConstBuffer.m_Program,CG_PROGRAM);
	while(Param)
	{
		const	uint32 InputOffset	=	cellGcmCgGetParameterResource(m_ConstBuffer.m_Program,Param) - CG_ATTR0;
		if(InputOffset<16)
			pDesc->InputParameters++;
		Param	=	cellGcmCgGetNextLeafParameter(m_ConstBuffer.m_Program,Param);
	}*/
	pDesc->InputParameters	=	m_ConstBuffer.m_pShaderDesc->VertexAttributeCount();
}
 
CCryDXPSShaderReflectionConstBuffer* CCryDXPSShaderReflection::GetConstantBufferByIndex(uint32 Index)
{
	return &m_ConstBuffer;
}

CCryDXPSShaderReflectionConstBuffer* CCryDXPSShaderReflection::GetConstantBufferByName(char* Name)
{
	return &m_ConstBuffer;
}

void CCryDXPSShaderReflection::GetResourceBindingDesc(uint32 ResourceIndex, D3D10_SHADER_INPUT_BIND_DESC *pDesc)
{
	const SRefSampler& rSampler	=	m_ConstBuffer.m_pShaderDesc->Sampler()[ResourceIndex];
	pDesc->Type				=	D3D10_SIT_TEXTURE;
	pDesc->BindCount	=	1;
	pDesc->BindPoint	=	rSampler.m_SamplerIndex;
	pDesc->Name				=	&m_ConstBuffer.m_pShaderDesc->NameTable()[rSampler.m_NameIndex];

/*
	CGprogram Program	=	m_ConstBuffer.m_Program;
	CGparameter Param	=	cellGcmCgGetIndexParameter(Program,ResourceIndex);

	pDesc->Type				=	D3D10_SIT_CBUFFER;//D3D10_SIT_CBUFFER==0
	pDesc->BindCount	=	1;
	pDesc->BindPoint	=	-1;//cellGcmCgGetParameterOrdinalNumber(Program,Param);

	if(	//FirstOccurence(m_Program,Index) &&
			CG_TRUE			==	cellGcmCgGetParameterReferenced(Program,Param) &&
			CG_OUT			!=	cellGcmCgGetParameterDirection(Program,Param) &&
			CG_UNIFORM	==	cellGcmCgGetParameterVariability(Program,Param))
	{
		const CGtype T		=	cellGcmCgGetParameterType(Program,Param);
		switch(T)
		{
#if defined(_DEGUG)
			case CG_HALF:
			case CG_FLOAT:
			case CG_BOOL:
			case CG_BOOL2:
			case CG_BOOL3:
			case CG_BOOL4:
			case CG_HALF2:	
			case CG_HALF3:
			case CG_HALF4:
			case CG_FLOAT2:
			case CG_FLOAT3:
			case CG_FLOAT4:
			case CG_BOOL2x2:
			case CG_BOOL3x2:
			case CG_BOOL4x2:
			case CG_BOOL2x3:
			case CG_BOOL3x3:
			case CG_BOOL4x3:
			case CG_BOOL2x4:
			case CG_BOOL3x4:
			case CG_BOOL4x4:
			case CG_HALF2x2:
			case CG_HALF3x2:
			case CG_HALF4x2:
			case CG_HALF2x3:
			case CG_HALF3x3:
			case CG_HALF4x3:
			case CG_HALF2x4:
			case CG_HALF3x4:
			case CG_HALF4x4:
			case CG_FLOAT2x2:
			case CG_FLOAT3x2:
			case CG_FLOAT4x2:
			case CG_FLOAT2x3:
			case CG_FLOAT3x3:
			case CG_FLOAT4x3:
			case CG_FLOAT2x4:
			case CG_FLOAT3x4:
			case CG_FLOAT4x4:
					int a =0;
				break;
#endif
			case CG_SAMPLER1D:
			case CG_SAMPLER2D:
			case CG_SAMPLER3D:
			case CG_SAMPLERRECT:
			case CG_SAMPLERCUBE:
			{
				CGresource	Res	=	cellGcmCgGetParameterResource(Program,Param);
				if(Res!=CG_UNDEFINED)
				{
					pDesc->Name	=	&pDesc->TmpName[0];
					CCryDXPSShaderReflection::CopyWithoutBrackets(&pDesc->TmpName[0],cellGcmCgGetParameterName(Program,Param));
					pDesc->BindPoint	=	 Res	- CG_TEXUNIT0;
					pDesc->Type				=	D3D10_SIT_TEXTURE;
				}
#if defined(_DEBUG)
				else
				{
					CGresource	Res	=	cellGcmCgGetParameterResource(Program,Param);
					int Tex	=	Res	- CG_TEX0;
					int Sam	=	Res	- CG_TEXUNIT0;
					pDesc->Name	=	&pDesc->TmpName[0];
					CCryDXPSShaderReflection::CopyWithoutBrackets(&pDesc->TmpName[0],cellGcmCgGetParameterName(Program,Param));
				}
#endif
			}
			break;
			default:
				{
					int a =0;
				}
		}
	}
	*/
}

void CCryDXPSShaderReflection::GetInputParameterDesc(uint32 ParameterIndex, D3D10_SIGNATURE_PARAMETER_DESC *pDesc)
{
	const SRefVertexAttribute& rAttribute	=	m_ConstBuffer.m_pShaderDesc->VertexAttribute()[ParameterIndex];
	pDesc->SemanticName		=	&m_ConstBuffer.m_pShaderDesc->NameTable()[rAttribute.m_SemanticNameIndex];
	pDesc->SemanticIndex	=	SEMANTICINDEX[rAttribute.m_InputOffset];//cellGcmCgGetParameterOrdinalNumber(m_ConstBuffer.m_Program,Param);
	pDesc->ReadWriteMask	=	1;
//	CRY_ASSERT_MESSAGE(0,"CCryShaderReflectionType::GetInputParameterDesc not implemented yet");
//	pDesc->SemanticName = 0;
	//count input parameters
/*	unsigned int Counter=0;
	CGparameter Param=cellGcmCgGetFirstLeafParameter(m_ConstBuffer.m_Program,CG_PROGRAM);
	while(Param)
	{
		const	uint32 InputOffset	=	cellGcmCgGetParameterResource(m_ConstBuffer.m_Program,Param) - CG_ATTR0;
		if(InputOffset<16)
		{
			if(ParameterIndex==Counter)
			{
				pDesc->SemanticName		=	(char*)cellGcmCgGetParameterSemantic(m_ConstBuffer.m_Program,Param);
				pDesc->SemanticIndex	=	cellGcmCgGetParameterOrdinalNumber(m_ConstBuffer.m_Program,Param);
				pDesc->ReadWriteMask	=	1;
				return;
			}
			Counter++;
		}
		Param	=	cellGcmCgGetNextLeafParameter(m_ConstBuffer.m_Program,Param);
	}
	*/
}

void CCryDXPSShaderReflection::GetOutputParameterDesc(uint32 ParameterIndex, D3D10_SIGNATURE_PARAMETER_DESC *pDesc)
{
	CRY_ASSERT_MESSAGE(0,"CCryShaderReflectionType::GetOutputParameterDesc not implemented yet");
	pDesc->SemanticName = 0;
}


