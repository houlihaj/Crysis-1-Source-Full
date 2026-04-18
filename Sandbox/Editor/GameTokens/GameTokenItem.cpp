////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   GameTokenItem.cpp
//  Version:     v1.00
//  Created:     10/11/2003 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GameTokenItem.h"
#include "GameEngine.h"
#include "GameTokenLibrary.h"
#include "GameTokenManager.h"
#include <IGameTokens.h>

#define FTN_INT "Int"
#define FTN_FLOAT "Float"
#define FTN_VEC3 "Vec3"
#define FTN_STRING "String"
#define FTN_BOOL "Bool"

//////////////////////////////////////////////////////////////////////////
inline EFlowDataTypes FlowNameToType( const char *typeName )
{
	EFlowDataTypes tokenType = eFDT_Any;
	if (0 == strcmp(typeName,FTN_INT))
		tokenType = eFDT_Int;
	else if (0 == strcmp(typeName,FTN_FLOAT))
		tokenType = eFDT_Float;
	else if (0 == strcmp(typeName,FTN_VEC3))
		tokenType = eFDT_Vec3;
	else if (0 == strcmp(typeName,FTN_STRING))
		tokenType = eFDT_String;
	else if (0 == strcmp(typeName,FTN_BOOL))
		tokenType = eFDT_Bool;

	return tokenType;
}

//////////////////////////////////////////////////////////////////////////
inline const char* FlowTypeToName( EFlowDataTypes tokenType )
{
	// Saving.
	switch (tokenType)
	{
	case eFDT_Int:
		return FTN_INT;
	case eFDT_Float:
		return FTN_FLOAT;
	case eFDT_Vec3:
		return FTN_VEC3;
	case eFDT_String:
		return FTN_STRING;
	case eFDT_Bool:
		return FTN_BOOL;
	}
	return "";
}

class CFlowDataReadVisitorEditor
{
public:
	CFlowDataReadVisitorEditor( const char * data ) : m_data(data), m_ok(false) {}

	void Visit( int& i )
	{
		m_ok = 1 == sscanf( m_data, "%i", &i );
	}

	void Visit( float& i )
	{
		m_ok = 1 == sscanf( m_data, "%f", &i );
	}

	void Visit( EntityId& i )
	{
		m_ok = 1 == sscanf( m_data, "%u", &i );
	}

	void Visit( Vec3& i )
	{
		m_ok = 3 == sscanf( m_data, "%g,%g,%g", &i.x, &i.y, &i.z );
	}

	void Visit( string& i )
	{
		i = m_data;
		m_ok = true;
	}

	void Visit( bool& b )
	{
		int i;
		m_ok = 1 == sscanf( m_data, "%i", &i );
		if (m_ok)
			b = (i != 0);
		else
		{
			if (stricmp(m_data, "true") == 0)
			{
				m_ok = b = true;
			}
			else if (stricmp(m_data, "false") == 0)
			{
				m_ok = true;
				b = false;
			}
		}
	}

	void Visit( SFlowSystemVoid& )
	{
	}

	bool operator!() const
	{
		return !m_ok;
	}

private:
	const char * m_data;
	bool m_ok;
};

//////////////////////////////////////////////////////////////////////////
class CFlowDataWriteVisitorEditor
{
public:
	CFlowDataWriteVisitorEditor( string& out ) : m_out(out) {}

	void Visit( int i )
	{
		m_out.Format( "%i", i );
	}

	void Visit( float i )
	{
		m_out.Format( "%f", i );
	}

	void Visit( EntityId i )
	{
		m_out.Format( "%u", i );
	}

	void Visit( Vec3 i )
	{
		m_out.Format( "%g,%g,%g", i.x, i.y, i.z );
	}

	void Visit( const string& i )
	{
		m_out = i;
	}

	void Visit( bool b )
	{
		m_out = (b) ? "true" : "false";
	}

	void Visit( SFlowSystemVoid )
	{
		m_out = "";
	}

private:
	string& m_out;
};

//////////////////////////////////////////////////////////////////////////
inline string ConvertFlowDataToString( const TFlowInputData& value )
{
	string out;
	CFlowDataWriteVisitorEditor writer(out);
	value.Visit( writer );
	return out;
}

//////////////////////////////////////////////////////////////////////////
inline bool SetFlowDataFromString( TFlowInputData& value, const char *str )
{
	CFlowDataReadVisitorEditor visitor(str);
	value.Visit( visitor );
	return !!visitor;
}



//////////////////////////////////////////////////////////////////////////
CGameTokenItem::CGameTokenItem()
{
	m_pTokenSystem =  GetIEditor()->GetGameEngine()->GetIGameTokenSystem();
	m_value = TFlowInputData((bool)0,true);
}

//////////////////////////////////////////////////////////////////////////
CGameTokenItem::~CGameTokenItem()
{
	if (m_pTokenSystem)
	{
		IGameToken *pToken = m_pTokenSystem->FindToken( m_cachedFullName );
		if (pToken)
			m_pTokenSystem->DeleteToken(pToken);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetName( const CString &name )
{
	IGameToken *pToken = NULL;
	if (m_pTokenSystem)
		m_pTokenSystem->FindToken( GetFullName() );
	__super::SetName(name);
	if (m_pTokenSystem)
	{
		if (pToken)
			m_pTokenSystem->RenameToken( pToken,GetFullName() );
		else
			m_pTokenSystem->SetOrCreateToken( GetFullName(),m_value );
	}
	m_cachedFullName = GetFullName();
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::Serialize( SerializeContext &ctx )
{
	//__super::Serialize( ctx );
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		const char *sTypeName = node->getAttr( "Type" );
		const char *sDefaultValue = node->getAttr( "Value" );

		if (!SetTypeName(sTypeName))
			return;
		SetFlowDataFromString(m_value,sDefaultValue);

		CString name = m_name;
		// Loading
		node->getAttr( "Name",name );

		// SetName will also set new value 
		if (!ctx.bUniqName)
		{
			SetName( name );
		}
		else
		{
			SetName( GetLibrary()->GetManager()->MakeUniqItemName(name) );
		}
		node->getAttr( "Description",m_description );
	}
	else
	{
		node->setAttr( "Name",m_name );
		// Saving.
		const char *sTypeName = FlowTypeToName((EFlowDataTypes)m_value.GetType());
		if (*sTypeName != 0)
			node->setAttr( "Type",sTypeName );
		string sValue = ConvertFlowDataToString( m_value );
		node->setAttr( "Value",sValue );
		if (!m_description.IsEmpty())
			node->setAttr( "Description",m_description );
	}
	m_bModified = false;
}

//////////////////////////////////////////////////////////////////////////
CString CGameTokenItem::GetTypeName() const
{
	return FlowTypeToName((EFlowDataTypes)m_value.GetType());
}

//////////////////////////////////////////////////////////////////////////
CString CGameTokenItem::GetValueString() const
{
	return (const char*)ConvertFlowDataToString( m_value );
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetValueString( const char* sValue )
{
	// Skip if the same type already.
	if (0 == strcmp(GetValueString(),sValue))
		return;

	SetFlowDataFromString(m_value,sValue);
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenItem::GetValue(TFlowInputData& data) const
{
	data = m_value;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::SetValue(const TFlowInputData& data, bool bUpdateGTS)
{
	m_value = data;
	if (bUpdateGTS && m_pTokenSystem)
	{
		IGameToken *pToken = m_pTokenSystem->FindToken( m_cachedFullName );
		if (pToken)
			pToken->SetValue(m_value);
	}
}
//////////////////////////////////////////////////////////////////////////
bool CGameTokenItem::SetTypeName( const char* typeName )
{
	EFlowDataTypes tokenType = FlowNameToType(typeName);
	
	// Skip if the same type already.
	if (tokenType == m_value.GetType())
		return true;

	CString prevVal = GetValueString();
	switch (tokenType)
	{
	case eFDT_Int:
		m_value = TFlowInputData((int)0,true);
		break;
	case eFDT_Float:
		m_value = TFlowInputData((float)0,true);
		break;
	case eFDT_Vec3:
		m_value = TFlowInputData(Vec3(0,0,0),true);
		break;
	case eFDT_String:
		m_value = TFlowInputData(string(""),true);
		break;
	case eFDT_Bool:
		m_value = TFlowInputData((bool)false,true);
		break;
	default:
		// Unknown type.
		return false;
	}
	SetValueString(prevVal);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenItem::Update()
{
	// Mark library as modified.
	SetModified();

	if (m_pTokenSystem)
	{
		// Recreate the game token with new default value.
		m_pTokenSystem->SetOrCreateToken( GetFullName(),m_value );
	}
}
