#ifndef __PerforceSourceControl_h__
#define __PerforceSourceControl_h__

#pragma once


#include "clientapi.h"


class CMyClientUser : public ClientUser
{
public:
	CMyClientUser()
	{
		m_initDesc = "Automatic.";
		strcpy(m_desc, m_initDesc);
		Init();
	}
	void HandleError(Error *e);
	void OutputStat( StrDict *varList );
	void Edit( FileSys *f1, Error *e );
	void	OutputInfo( char level, const char *data );
	
	void Init();
	void PreCreateFileName(const char * file);

	Error m_e;
	char m_action[64];
	char m_headAction[64];
	char m_desc[1024];
	char m_file[MAX_PATH];
	char m_findedFile[MAX_PATH];

	const char * m_initDesc;
	bool m_bIsSetup;
	bool m_bIsPreCreateFile;
};




class CPerforceSourceControl : public ISourceControl, public IClassDesc
{
public:

	// constructor
	CPerforceSourceControl();
	void FreeData();


	// from ISourceControl
	uint32 GetFileAttributes( const char *filename );

	bool Add( const char *filename, const char * desc, int nFlags);
	bool CheckIn( const char *filename, const char * desc, int nFlags);
	bool CheckOut( const char *filename, int nFlags);
	bool UndoCheckOut( const char *filename, int nFlags);
	bool Rename( const char *filename, const char *newfilename, const char * desc, int nFlags);
	bool Delete( const char *filename, const char * desc, int nFlags);
	bool GetLatestVersion( const char *filename, int nFlags);
	


	uint32 GetFileAttributesAndFileName( const char *filename, char * FullFileName );
	bool IsFolder(const char * filename, char * FullFileName);

	// from IClassDesc
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_SCM_PROVIDER; };
	REFGUID ClassID()
	{
		// {3c209e66-0728-4d43-897d-168962d5c8b5}
		static const GUID guid = { 0x3c209e66, 0x0728, 0x4d43, { 0x89, 0x7d, 0x16, 0x89, 0x62, 0xd5, 0xc8, 0xb5 } };
		return guid;
	}
	virtual const char* ClassName() { return "Perforce"; };
	virtual const char* Category() { return "SourceControl"; };
	virtual CRuntimeClass* GetRuntimeClass(){return 0;};
	virtual void ShowAbout() {};

	// from IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface( const IID &riid, void **ppvObj ) 
	{ 
		if(riid == __uuidof(ISourceControl)/* && m_pIntegrator*/)
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

protected:
	bool IsFileManageable(const char *sFilename);
	bool IsFileExistsInDatabase(const char *sFilename);
	bool IsFileCheckedOutByUser(const char *sFilename);
	void ConvertFileNameCS(char *sDst, const char *sSrcFilename);
	bool FindFile(char * clientFile, const char * folder, const char * file);
	bool FindDir(char * clientFile, const char * folder, const char * dir);

private:
	//CNxNSmartIntegrator* m_pIntegrator;
	CMyClientUser m_ui;
	ClientApi m_client;
	Error m_e;

	ULONG m_ref;
};




#endif //__PerforceSourceControl_h__