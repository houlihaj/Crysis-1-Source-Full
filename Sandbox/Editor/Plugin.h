////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   plugin.h
//  Version:     v1.00
//  Created:     15/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Plugin architecture supporting classes.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __plugin_h__
#define __plugin_h__
#pragma once


/** Derive from this class to decrease ammount of work fro creating new class description.
		Provides standart reference counter implementation for IUnknown
*/
class CRefCountClassDesc : public IClassDesc
{
public:
	//////////////////////////////////////////////////////////////////////////
	// IUnknown implementation.
	//////////////////////////////////////////////////////////////////////////
	HRESULT STDMETHODCALLTYPE QueryInterface( const IID &riid, void **ppvObj ) { return E_NOINTERFACE ; }
	ULONG STDMETHODCALLTYPE AddRef()
	{
		m_nRefCount++;
		return m_nRefCount;
	};
	ULONG STDMETHODCALLTYPE Release()
	{
		int refs = m_nRefCount;
		if (--m_nRefCount <= 0)
			delete this;
		return refs;
	};
private:
	//! Ref count itself, its zeroed on node creation.
	int m_nRefCount;
};

/*
//! Show a modal about dialog / message box for the plugin.
virtual void ShowAbout() = 0;

//! Asks if the plugin can exit now. This might involve asking the user if he wants to save
//! data. The plugin is only supposed to ask for unsaved data which is not serialize into
//! the editor project file. When data is modified which is saved into the project file, the
//! plugin should call IEditor::SetDataModified() to make the editor ask
virtual bool CanExitNow() = 0;

//! The plugin should write / read its data to the passed stream. The data is saved to or loaded
//! from the editor project file. This function is called during the usual save / load process of
//! the editor's project file
virtual void Serialize( CXmlArchive &ar ) = 0;

//! Editor can send to class various events.
virtual void Event( EClassEvent event ) = 0;
*/

/** Class factory is a common repository of all registered plugni classes,
		classes here can found by thier class id or all classes of givven system class retrieved.
*/
class CRYEDIT_API CClassFactory : public IEditorClassFactory
{
public:
	CClassFactory();
	~CClassFactory();

	//! Access class factory singleton.
	static CClassFactory* Instance();

	//! Register new class to the factory.
	void RegisterClass( IClassDesc *cls );

	//! Find class in the factory by class name.
	IClassDesc* FindClass( const char *className ) const;
	//! Find class in the factory by classid.
	IClassDesc* FindClass( const GUID& clsid ) const;

	//! Get classes that matching specific requirements ordered alphabetically by name.
	void GetClassesBySystemID( ESystemClassID systemCLSID,std::vector<IClassDesc*> &classes );
	void GetClassesByCategory( const char* category,std::vector<IClassDesc*> &classes );

private:
	void RegisterAutoTypes();

	// Collection of all class description in factory.
	std::vector<IClassDesc*> m_classes;

	// Map of class name to class description.
	typedef std::map<CString,IClassDesc*> NameMap;
	NameMap m_nameToClass;

	// Map of guid to class description.
	typedef std::map<GUID,IClassDesc*,guid_less_predicate> GuidMap;
	GuidMap m_guidToClass;

	static CClassFactory *m_instance;
};

//////////////////////////////////////////////////////////////////////////
// Auto registration for flow nodes.
//////////////////////////////////////////////////////////////////////////
class CAutoRegisterClassHelper
{
public:
	CAutoRegisterClassHelper( IClassDesc *pClassDesc )
	{
		m_pClassDesc = pClassDesc;
		m_pNext = 0;
		if (!m_pLast)
		{
			m_pFirst = this;
		}
		else
			m_pLast->m_pNext = this;
		m_pLast = this;
	}

	//////////////////////////////////////////////////////////////////////////
	IClassDesc *m_pClassDesc;
	CAutoRegisterClassHelper* m_pNext;
	static CAutoRegisterClassHelper *m_pFirst;
	static CAutoRegisterClassHelper *m_pLast;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Use this define to automatically register a new class description.
// Ex. REGISTER_FLOW_NODE( "Delay",CFlowDelayNode )
//////////////////////////////////////////////////////////////////////////
#define REGISTER_CLASS_DESC( ClassDesc ) \
	ClassDesc g_AutoReg##ClassDesc; \
	CAutoRegisterClassHelper g_AutoRegHelper##ClassDesc( &(g_AutoReg##ClassDesc) );

#endif // __plugin_h__
