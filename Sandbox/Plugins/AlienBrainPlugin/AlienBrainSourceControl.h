#ifndef __AlienBrainSourceControl_h__
#define __AlienBrainSourceControl_h__

#pragma once

class CNxNIntegrator;
class CNxNSmartIntegrator;

class CAlienBrainSourceControl : public ISourceControl, public IClassDesc
{
public:

	// constructor
	CAlienBrainSourceControl();
	void FreeData();


	// from ISourceControl
	uint32 GetFileAttributes( const char *filename );

	bool Add( const char *filename, int nFlags);
	bool CheckIn( const char *filename, int nFlags);
	bool CheckOut( const char *filename, int nFlags);
	bool UndoCheckOut( const char *filename, int nFlags);
	bool Rename( const char *filename, const char *newfilename, int nFlags);
	bool Delete( const char *filename, int nFlags);
	bool GetLatestVersion( const char *filename, int nFlags);
	


	uint32 GetFileAttributesAndFileName( const char *filename, char * FullFileName );
	bool IsFolder(const char * filename, char * FullFileName);

	// from IClassDesc
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_SCM_PROVIDER; };
	REFGUID ClassID()
	{
		// {8a5f2577-5e89-46a2-971f-eb6765796828}
		static const GUID guid = { 0x8a5f2577, 0x5e89, 0x46a2, { 0x97, 0x1f, 0xeb, 0x67, 0x65, 0x79, 0x68, 0x28 } };
		return guid;
	}
	virtual const char* ClassName() { return "AlienBrain source control"; };
	virtual const char* Category() { return "SourceControl"; };
	virtual CRuntimeClass* GetRuntimeClass(){return 0;};
	virtual void ShowAbout() {};

	// from IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface( const IID &riid, void **ppvObj ) 
	{ 
		if(riid == __uuidof(ISourceControl) && m_pIntegrator)
		{
			*ppvObj = this;
			return S_OK;
		}
		return E_NOINTERFACE ; 
	}
	ULONG STDMETHODCALLTYPE AddRef() { return ++m_ref; };
	ULONG STDMETHODCALLTYPE Release() 
	{ 
		if((--m_ref) == 0)
		{
			FreeData();
			delete this;
			return 0; 
		}
		else
			return m_ref;
	}
private:
	CNxNSmartIntegrator* m_pIntegrator;

	ULONG m_ref;
};




#endif //__AlienBrainSourceControl_h__