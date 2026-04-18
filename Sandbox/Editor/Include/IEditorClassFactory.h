////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ISourceControl.h
//  Version:     v1.00
//  Created:     22/9/2004 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IEditorClassFactory_h__
#define __IEditorClassFactory_h__

#pragma once


/** System class Id.
*/
enum ESystemClassID
{
	ESYSTEM_CLASS_OBJECT		= 0x0001,
	ESYSTEM_CLASS_EDITTOOL	= 0x0002,

	ESYSTEM_CLASS_PREFERENCE_PAGE	= 0x0020, // Preference Page.
	ESYSTEM_CLASS_VIEWPANE	      = 0x0021, // View pane.
	ESYSTEM_CLASS_SCM_PROVIDER    = 0x0022, // Source/Asset Control Managment Provider.

	ESYSTEM_CLASS_USER			= 0x1000,
};


/** Event that can be sent to classes.
*/
enum EClassEvent
{

};


/** This interface describes Class created by plugin.
*/
struct IClassDesc : public IUnknown
{
	//////////////////////////////////////////////////////////////////////////
	// IUnknown implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual HRESULT STDMETHODCALLTYPE QueryInterface( const IID &riid, void **ppvObj ) { return E_NOINTERFACE ; }
	virtual ULONG STDMETHODCALLTYPE AddRef() { return 0; }
	virtual ULONG STDMETHODCALLTYPE Release() { return 0; }

	//////////////////////////////////////////////////////////////////////////
	// Class description.
	//////////////////////////////////////////////////////////////////////////
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() = 0;

	//! Return the GUID of the class created by plugin.
	virtual REFGUID ClassID() = 0;

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() = 0;

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char* Category() = 0;

	//! Get MFC runtime class of the object, created by this ClassDesc.
	virtual CRuntimeClass* GetRuntimeClass() = 0;
	//////////////////////////////////////////////////////////////////////////
};


struct CRYEDIT_API IEditorClassFactory
{
public:
	//! Access class factory singleton.
	//static CClassFactory* Instance();

	//! Register new class to the factory.
	virtual void RegisterClass( IClassDesc *cls ) = 0;

	//! Find class in the factory by class name.
	virtual IClassDesc* FindClass( const char *className ) const = 0;
	//! Find class in the factory by classid.
	virtual IClassDesc* FindClass( const GUID& clsid ) const = 0;

	//! Get classes that matching specific requirements.
	virtual void GetClassesBySystemID( ESystemClassID systemCLSID,std::vector<IClassDesc*> &classes ) = 0;
	virtual void GetClassesByCategory( const char* category,std::vector<IClassDesc*> &classes ) = 0;
};


#endif //__IEditorClassFactory_h__