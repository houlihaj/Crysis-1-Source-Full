////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   errorreport.h
//  Version:     v1.00
//  Created:     30/5/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Class that collects error reports to present them later.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __errorreport_h__
#define __errorreport_h__
#pragma once

// forward declarations.
class CMaterial;
class CParticleItem;

#include "BaseLibraryItem.h"
#include "Objects/BaseObject.h"

/*! Single error entry in error report.
 */
class CErrorRecord
{
public:
	enum ESeverity
	{
		ESEVERITY_ERROR,
		ESEVERITY_WARNING,
		ESEVERITY_COMMENT
	};
	enum EFlags
	{
		FLAG_NOFILE			= 0x0001,	// Indicate that required file was not found.
		FLAG_SCRIPT			= 0x0002,	// Error with scripts.
		FLAG_TEXTURE		= 0x0004,	// Error with scripts.
		FLAG_OBJECTID		= 0x0008,	// Error with object Ids, Unresolved/Duplicate etc...
		FLAG_AI					= 0x0010,	// Error with AI.
	};
	//! Severety of this error.
	ESeverity severity;
	//! Module of error.
	EValidatorModule module;
	//! Error Text.
	CString error;
	//! File which is missing or causing problem.
	CString file;
	//! More detailed description for this error.
	CString description;
	int count;
	//! Object that caused this error.
	_smart_ptr<CBaseObject> pObject;
	//! Library Item that caused this error.
	_smart_ptr<CBaseLibraryItem> pItem;
	int flags;

	CErrorRecord()
	{
		severity = ESEVERITY_WARNING;
		module = VALIDATOR_MODULE_EDITOR;
		pObject = 0;
		pItem = 0;
		flags = 0;
		count = 0;
	}
	CString GetErrorText();
};

/*! Error report manages collection of errors occured duruing map analyzes or level load.
 */
class CErrorReport : public IValidator
{
public:
	CErrorReport();

	//! If enabled errors are reported immidiatly and not stored.
	void SetImmidiateMode( bool bEnable );
	bool IsImmidiateMode() const { return m_bImmidiateMode; };

	//! Adds new error to report.
	void ReportError( CErrorRecord &err );

	//! Check if error report have any errors.
	bool IsEmpty() const;

	//! Get number of contained error records.
	int GetErrorCount() const { return m_errors.size(); };
	//! Get access to indexed error record.
	CErrorRecord& GetError( int i );
	//! Clear all error records.
	void Clear();

	//! Display dialog with all errors.
	void Display();

	//! Assign current Object to which new reported warnings are assigned.
	void SetCurrentValidatorObject( CBaseObject *pObject );
	//! Assign current Item to which new reported warnings are assigned.
	void SetCurrentValidatorItem( CBaseLibraryItem *pItem );
	//! Assign current filename.
	void SetCurrentFile( const CString &file );

	//////////////////////////////////////////////////////////////////////////
	// Implement IValidator interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void Report( SValidatorRecord &record );

private:
	//! Array of all error records added to report.
	std::vector<CErrorRecord> m_errors;
	bool m_bImmidiateMode;
	_smart_ptr<CBaseObject> m_pObject;
	_smart_ptr<CBaseLibraryItem> m_pItem;
	CParticleItem *m_pParticle;
	CString m_currentFilename;
};

//////////////////////////////////////////////////////////////////////////
//! Automatic class to record and display error.
class CErrorsRecorder
{
public:
	CErrorsRecorder();
	~CErrorsRecorder();
};

#endif // __errorreport_h__
