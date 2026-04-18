#ifndef __VariableTypeInfo_h__
#define __VariableTypeInfo_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Variable.h"
#include "UIEnumsDatabase.h"
#include <CryTypeInfo.h>

//////////////////////////////////////////////////////////////////////////
// Adaptors for TypeInfo-defined variables to IVariable

inline CString SpacedName(const char* sName)
{
	// Split name with spaces.
	CString sSpacedName = sName;
	for (int i = 1; i < sSpacedName.GetLength(); i++)
	{
		if (isupper(sSpacedName.GetAt(i)) && islower(sSpacedName.GetAt(i-1)))
		{
			sSpacedName.Insert(i, ' ');
			i++;
		}
	}
	return sSpacedName;
}

//////////////////////////////////////////////////////////////////////////
// Scalar variable
//////////////////////////////////////////////////////////////////////////

class	CVariableTypeInfo : public CVariableBase
{
public:

	// Dynamic constructor function
	static IVariable* Create(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress);

	static IVariable* Create( CTypeInfo::CVarInfo const& VarInfo, void* pBaseAddress )
	{
		return Create(VarInfo, VarInfo.Type, VarInfo.GetAddress(pBaseAddress));
	}

	// Constructor.
	CVariableTypeInfo(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress)
	: m_pData(pAddress)
	{
		SetName( SpacedName(VarInfo.GetDisplayName()) );
		SetTypes(VarInfo, TypeInfo);
	}

	// Translation to editor type values is currently done with some clunky type and name testing.
	void SetTypes(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo)
	{
		// Translation to editor type values is currently done with some clunky type and name testing.
		m_pTypeInfo = &TypeInfo;
		m_pVarInfo = &VarInfo;
		m_eType = UNKNOWN;
		SetDataType(DT_SIMPLE);

		if (TypeInfo.IsType<bool>())
			m_eType = BOOL;
		else if (TypeInfo.IsType<int>() || TypeInfo.IsType<uint>())
			m_eType = INT;
		else if (TypeInfo.IsType<float>())
			m_eType = FLOAT;
		else if (TypeInfo.IsType<Vec3>())
			m_eType = VECTOR;
		else if (TypeInfo.IsType<ColorF>())
		{
			m_eType = VECTOR;
			SetDataType(DT_COLOR);
		}
#ifdef CURVE_TYPE
		else if (TypeInfo.IsType< TCurve<float> >())
		{
			SetDataType(DT_CURVE|DT_PERCENT);
		}
		else if (TypeInfo.IsType< TCurve<ColorF> >())
		{
			SetDataType(DT_CURVE|DT_COLOR);
		}
#endif
		else
		{
			// Edit everything else as a string.
			m_eType = STRING;
			CString sVarName = VarInfo.GetDisplayName();
			if (sVarName == "Texture")
				SetDataType(DT_TEXTURE);
			else if (sVarName == "Material")
				SetDataType(DT_MATERIAL);
			else if (sVarName == "Geometry")
				SetDataType(DT_OBJECT);
			else if (sVarName == "Sound")
				SetDataType(DT_SOUND);
		}
	}

	// IVariable implementation.
	virtual	EType	GetType() const								{ return m_eType; }
	virtual	int		GetSize() const								{ return m_pTypeInfo->Size; }

	virtual void GetLimits( float& fMin, float& fMax, bool& bHardMin, bool& bHardMax )
	{
		if (m_pVarInfo->GetAttr("Min", fMin))
			bHardMin = true;
		else if (m_pVarInfo->GetAttr("SoftMin", fMin))
			bHardMin = false;
		if (m_pVarInfo->GetAttr("Max", fMax))
			bHardMax = true;
		else if (m_pVarInfo->GetAttr("SoftMax", fMax))
			bHardMax = false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Access operators.
	//////////////////////////////////////////////////////////////////////////

	virtual void Set( const char *value )				
	{
		m_pTypeInfo->FromString(m_pData, value); 
		if (m_pTypeInfo->IsType<ColorF>())
		{
			*(ColorF*)m_pData /= 255.f;
		}
		OnSetValue(false);
	}
	virtual void Set( const CString& value )		
	{ 
		Set((const char*)value); 
	}
	virtual void Set( float value )
	{ 
		assert(m_pTypeInfo->IsType<float>());
		*(float*)m_pData = value;
		OnSetValue(false);
	}
	virtual void Set( int value )
	{ 
		assert(m_pTypeInfo->IsType<int>() || m_pTypeInfo->IsType<uint>());
		*(float*)m_pData = value;
		OnSetValue(false);
	}
	virtual void Set( const Vec3& value )
	{ 
		assert(m_eType == VECTOR);
		*(Vec3*)m_pData = value;
		OnSetValue(false);
	}

	virtual void Get( CString& value ) const
	{
		if (m_pTypeInfo->IsType<ColorF>())
		{
			ColorF clr = *(ColorF*)m_pData;
			clr *= 255.f;
			value.Format( "%d,%d,%d", int_round(clr.r), int_round(clr.g), int_round(clr.b) );
		}
		else
			value = (const char*)m_pTypeInfo->ToString(m_pData); 
	}
	virtual void Get( float& value ) const
	{ 
		assert(m_pTypeInfo->IsType<float>());
		value = *(float*)m_pData;
	}
	virtual void Get( int& value ) const
	{ 
		assert(m_pTypeInfo->IsType<int>() || m_pTypeInfo->IsType<uint>());
		value = *(float*)m_pData;
	}
	virtual void Get( bool& value ) const
	{ 
		assert(m_pTypeInfo->IsType<bool>());
		value = *(bool*)m_pData;
	}
	virtual void Get( Vec3& value ) const
	{ 
		assert(m_eType == VECTOR);
		value = *(Vec3*)m_pData;
	}

	// To do: This doesn't work as expected for Undo; the cloned IVariable references the same memory.
	virtual IVariable* Clone( bool bRecursive ) const
	{
		return Create(*m_pVarInfo, *m_pTypeInfo, m_pData);
	}

	// To do: This could be more efficient ?
	virtual void CopyValue( IVariable *fromVar )
	{
		assert(fromVar);
		CString str;
		fromVar->Get(str);
		Set(str);
	}

#ifdef CURVE_TYPE
	virtual ISplineInterpolator* GetSpline(bool bCreate) const
	{
		if (m_pTypeInfo->IsType< TCurve<float> >())
		{
			return ((TCurve<float>*)m_pData)->GetSpline(bCreate);
		}
		else if (m_pTypeInfo->IsType< TCurve<ColorF> >())
		{
			return ((TCurve<ColorF>*)m_pData)->GetSpline(bCreate);
		}
		return 0;
	}
#endif

protected:
	CTypeInfo::CVarInfo const* m_pVarInfo;
	CTypeInfo const*	m_pTypeInfo;		// TypeInfo system structure for this var.
	void*							m_pData;				// Existing address in memory. Directly modified.
	EType							m_eType;				// Type info for editor.
};

//////////////////////////////////////////////////////////////////////////
// Enum variable
//////////////////////////////////////////////////////////////////////////

class	CVariableTypeInfoEnum : public CVariableTypeInfo
{
	struct CTypeInfoEnumList: IVarEnumList
	{
		CTypeInfo const& TypeInfo;

		CTypeInfoEnumList(CTypeInfo const& info)
			: TypeInfo(info)
		{
		}

		virtual int GetItemsCount()											{ return TypeInfo.EnumElemCount(); }
		virtual const char* GetItemName( int index )		{ return TypeInfo.EnumElem(index)->ShortName; }
	};

public:

	// Constructor.
	CVariableTypeInfoEnum(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress, IVarEnumList* pEnumList = 0)
	: CVariableTypeInfo(VarInfo, TypeInfo, pAddress)
	{
		// Use custom enum, or enum defined in TypeInfo.
		m_enumList = pEnumList ? pEnumList : new CTypeInfoEnumList(TypeInfo);
	}

	//////////////////////////////////////////////////////////////////////////
	// Additional IVariable implementation.
	//////////////////////////////////////////////////////////////////////////

	IVarEnumList* GetEnumList() const
	{
		return m_enumList;
	}

protected:

	TSmartPtr<IVarEnumList> m_enumList;
};

//////////////////////////////////////////////////////////////////////////
// Struct variable
// Inherits implementation from CVariableArray
//////////////////////////////////////////////////////////////////////////

class	CVariableTypeInfoStruct : public CVariableTypeInfo
{
public:

	// Constructor.
	CVariableTypeInfoStruct(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress)
	:	CVariableTypeInfo(VarInfo, TypeInfo, pAddress)
	{
		m_eType = ARRAY;

		ProcessSubStruct(VarInfo, TypeInfo, pAddress);
	}

	void ProcessSubStruct(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress)
	{
		for AllSubVars( pSubVar, TypeInfo )
		{
			if (pSubVar->Type.HasSubVars() && (!*pSubVar->Name || pSubVar->GetAttr("Inline")))
			{
				// Recursively process nameless or base struct.
				ProcessSubStruct(VarInfo, pSubVar->Type, pSubVar->GetAddress(pAddress));
			}
			else if (pSubVar == TypeInfo.NextSubVar(0) && pSubVar->GetDisplayName() == string("Base"))
			{
				// Inline edit first sub-var in main field.
				m_flags |= UI_SHOW_CHILDREN;
				SetTypes(VarInfo, pSubVar->Type);
			}
			else
			{
				IVariable* pVar = CVariableTypeInfo::Create(*pSubVar, pAddress);
				m_Vars.push_back(pVar); 
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// IVariable implementation.
	//////////////////////////////////////////////////////////////////////////

	virtual void OnSetValue(bool bRecursive)
	{
		CVariableBase::OnSetValue(bRecursive);
		if (bRecursive)
		{
			for (Vars::iterator it = m_Vars.begin(); it != m_Vars.end(); ++it)
				(*it)->OnSetValue(true);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CopyValue( IVariable *fromVar )
	{
		assert(fromVar);
		if (fromVar->GetType() != IVariable::ARRAY)
			CVariableTypeInfo::CopyValue( fromVar );

		int numSrc = fromVar->NumChildVars();
		int numTrg = m_Vars.size();
		for (int i = 0; i < numSrc && i < numTrg; i++)
		{
			// Copy Every child variable.
			m_Vars[i]->CopyValue( fromVar->GetChildVar(i) );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	int NumChildVars() const													{ return m_Vars.size(); }
	IVariable* GetChildVar( int index ) const					{	return m_Vars[index];	}

protected:
	typedef std::vector<IVariablePtr> Vars;
	Vars	m_Vars;
};

struct CUIEnumsDBList: IVarEnumList
{
	CUIEnumsDatabase_SEnum const* m_pEnumList;

	CUIEnumsDBList(CUIEnumsDatabase_SEnum const* pEnumList)
		: m_pEnumList(pEnumList)
	{
	}
	virtual int GetItemsCount()											{ return m_pEnumList->strings.size(); }
	virtual const char* GetItemName( int index )		{ return m_pEnumList->strings[index]; }
};

//////////////////////////////////////////////////////////////////////////
// CVariableTypeInfo dynamic constructor.
//////////////////////////////////////////////////////////////////////////

IVariable* CVariableTypeInfo::Create(CTypeInfo::CVarInfo const& VarInfo, CTypeInfo const& TypeInfo, void* pAddress)
{
	if (TypeInfo.EnumElemCount())
		return new CVariableTypeInfoEnum(VarInfo, TypeInfo, pAddress);
	else if (TypeInfo.HasSubVars() && !TypeInfo.IsType<ColorF>() && !TypeInfo.IsType<Vec3>())
		return new CVariableTypeInfoStruct(VarInfo, TypeInfo, pAddress);
	else
	{
		cstr sSelect;
		if (VarInfo.GetAttr("Select", sSelect))
		{
			CUIEnumsDatabase_SEnum const* pEnums = GetIEditor()->GetUIEnumsDatabase()->FindEnum(sSelect);
			if (pEnums)
				return new CVariableTypeInfoEnum(VarInfo, TypeInfo, pAddress, new CUIEnumsDBList(pEnums) );
		}
		return new CVariableTypeInfo(VarInfo, TypeInfo, pAddress);
	}
}

#endif // __VariableTypeInfo_h__