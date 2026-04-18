//////////////////////////////////////////////////////////////////////////////
// CryEngine specific customizations of GFxSharedStates
//////////////////////////////////////////////////////////////////////////////

#ifndef _SHARED_STATES_H_
#define _SHARED_STATES_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GFxLoader.h>
#include <GFxPlayer.h>
#include <GFxLog.h>


struct ILocalizationManager;
struct ILog;


//////////////////////////////////////////////////////////////////////////
// CryGFxFileOpener

class CryGFxFileOpener : public GFxFileOpener
{
public:
	// GFxFileOpener interface
	virtual GFile* OpenFile(const char* pFilePath);

public:
	static CryGFxFileOpener& GetAccess();
	~CryGFxFileOpener();

private:
	CryGFxFileOpener();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxURLBuilder

class CryGFxURLBuilder : public GFxURLBuilder
{
	// GFxURLBuilder interface
	virtual void BuildURL(GFxString* pPath, const LocationInfo& loc);

public:
	static CryGFxURLBuilder& GetAccess();
	~CryGFxURLBuilder();

private:
	CryGFxURLBuilder();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxTranslator

class CryGFxTranslator : public GFxTranslator
{
public:
	// GFxTranslator interface
	virtual UInt GetCaps() const;
	virtual	Bool Translate(GFxWStringBuffer* pBuffer, const wchar_t* pKey);

public:
	static CryGFxTranslator& GetAccess();
	~CryGFxTranslator();

private:
	CryGFxTranslator();

private:
	ILocalizationManager* m_pILocMan;
	wstring m_localizedString;
};


//////////////////////////////////////////////////////////////////////////
// CryGFxLog

class CryGFxLog : public GFxLog
{
public:
	// GFxLog interface
	virtual	void LogMessageVarg(LogMessageType messageType, const char* pFmt, va_list argList);

public:
	static CryGFxLog& GetAccess();
	~CryGFxLog();

	void SetContext(const char* pFlashContext);
	const char* GetContext() const;

	void Log(const char* pFmt, ...);

private:
	CryGFxLog();

#if defined(WIN32) || defined(WIN64)
private:
	static ICVar* CV_sys_flash_debuglog;
	static int ms_sys_flash_debuglog;
#endif

private:
	ILog* m_pLog;
	const char* m_pCurFlashContext;
};


//////////////////////////////////////////////////////////////////////////
// CryGFxFSCommandHandler

class CryGFxFSCommandHandler : public GFxFSCommandHandler
{
public:
	// GFxFSCommandHandler interface
	virtual void Callback(GFxMovieView* pMovieView, const char* pCommand, const char* pArgs);

public:
	static CryGFxFSCommandHandler& GetAccess();
	~CryGFxFSCommandHandler();

private:
	CryGFxFSCommandHandler();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxExternalInterface

class CryGFxExternalInterface : public GFxExternalInterface
{
public:
	// GFxExternalInterface interface
	virtual void Callback(GFxMovieView* pMovieView, const char* pMethodName, const GFxValue* pArgs, UInt numArgs);

public:
	static CryGFxExternalInterface& GetAccess();
	~CryGFxExternalInterface();

private:
	CryGFxExternalInterface();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxUserEventHandler

class CryGFxUserEventHandler : public GFxUserEventHandler
{
public:
	// GFxUserEventHandler interface
	virtual void HandleEvent(class GFxMovieView* pMovieView, const class GFxEvent& event);

public:
	static CryGFxUserEventHandler& GetAccess();
	~CryGFxUserEventHandler();

private:
	CryGFxUserEventHandler();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxImageCreator

class CryGFxImageCreator : public GFxImageCreator
{
public:
	// GFxImageCreator interface
	virtual GImageInfoBase* CreateImage(const GFxImageCreateInfo& info);

public:
	static CryGFxImageCreator& GetAccess();
	~CryGFxImageCreator();

private:
	CryGFxImageCreator();
};


//////////////////////////////////////////////////////////////////////////
// CryGFxImageLoader

class CryGFxImageLoader : public GFxImageLoader
{
public:
	// GFxImageLoader interface
	virtual GImageInfoBase* LoadImage(const char *pUrl);

public:
	static CryGFxImageLoader& GetAccess();
	~CryGFxImageLoader();

private:
	CryGFxImageLoader();
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _SHARED_STATES_H_
