////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   UIEnumsDatabase.h
//  Version:     v1.00
//  Created:     16/2/2006 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __UIEnumsDatabase_h__
#define __UIEnumsDatabase_h__
#pragma once

struct CUIEnumsDatabase_SEnum 
{
	CString name;
	std::vector<CString> strings; // Display Strings.
	std::vector<CString> values;  // Corresponding Values.

	CString NameToValue( const CString &name );
	CString ValueToName( const CString &value );
};

//////////////////////////////////////////////////////////////////////////
// Stores string associates to the enumeration collections for UI.
//////////////////////////////////////////////////////////////////////////
class CUIEnumsDatabase
{
public:
	CUIEnumsDatabase();
	~CUIEnumsDatabase();

	void SetEnumStrings( const CString &enumName,const char **sStringsArray,int nStringCount );
	CUIEnumsDatabase_SEnum* FindEnum( const CString &enumName );

private:
	typedef std::map<CString,CUIEnumsDatabase_SEnum*> Enums;
	Enums m_enums;
};

#endif // __UIEnumsDatabase_h__
