#ifndef _FLASH_PLAYER_INSTANCE_H_
#define _FLASH_PLAYER_INSTANCE_H_

#pragma once


#include <IFlashPlayer.h>


#ifndef EXCLUDE_SCALEFORM_SDK


#include <GFxPlayer.h>
#include "GRendererXRender.h"
#include "SharedResources.h"


struct ICVar;
struct SFlashProfilerData;


template<typename T>
struct SLinkNode
{
	SLinkNode()
	{
		m_pHandle = 0;
		m_pPrev = this;
		m_pNext = this;
	};

	void Link(SLinkNode* pRoot)
	{
		m_pPrev = pRoot->m_pPrev;
		m_pNext = pRoot;
		pRoot->m_pPrev->m_pNext = this;
		pRoot->m_pPrev = this;
	}

	void Unlink()
	{
		m_pPrev->m_pNext = m_pNext;
		m_pNext->m_pPrev = m_pPrev;
		m_pNext = m_pPrev = 0;
	}

	SLinkNode* m_pPrev;
	SLinkNode* m_pNext;
	T* m_pHandle;
};


class CFlashPlayer : public IFlashPlayer
{
public:
	// IFlashPlayer interface
	virtual void Release();

	virtual bool Load(const char* pFilePath, unsigned int options = IFlashPlayer::RENDER_EDGE_AA);

	virtual void SetBackgroundColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha);
	virtual void SetBackgroundAlpha(float alpha);
	virtual void SetViewport(int x0, int y0, int width, int height, float aspectRatio = 1.0f);
	virtual void GetViewport(int& x0, int& y0, int& width, int& height, float& aspectRatio);
	virtual void SetScissorRect(int x0, int y0, int width, int height);
	virtual void GetScissorRect(int& x0, int& y0, int& width, int& height);
	virtual void Advance(float deltaTime);
	virtual void Render();
	virtual void SetCompositingDepth(float depth);

	virtual void SetFSCommandHandler(IFSCommandHandler* pHandler);
	virtual void SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler);
	virtual void SendCursorEvent(const SFlashCursorEvent& cursorEvent);
	virtual void SendKeyEvent(const SFlashKeyEvent& keyEvent);

	virtual void SetVisible(bool visible);
	virtual bool GetVisible() const;

	virtual bool SetVariable(const char* pPathToVar, const SFlashVarValue& value);
	virtual bool GetVariable(const char* pPathToVar, SFlashVarValue* pValue) const;
	virtual bool IsAvailable(const char* pPathToVar) const;
	virtual bool SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count) const;
	virtual unsigned int GetVariableArraySize(const char* pPathToVar) const;
	virtual bool GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count);
	virtual bool Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);

	virtual int GetWidth() const;
	virtual int GetHeight() const;
	virtual size_t GetMetadata(char* pBuff, unsigned int buffSize) const;
	virtual const char* GetFilePath() const;

	virtual void ScreenToClient(int& x, int& y) const;
	virtual void ClientToScreen(int& x, int& y) const;

public:
	CFlashPlayer();
	virtual ~CFlashPlayer();

	void DelegateFSCommandCallback(const char* pCommand, const char* pArgs);
	void DelegateExternalInterfaceCallback(const char* pMethodName, const GFxValue* pArgs, UInt numArgs);

public:
	static void RenderFlashInfo();
	static void SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler);
	static IFlashLoadMovieHandler* GetFlashLoadMovieHandler();
	static void InitCVars();
	static int GetWarningLevel();

	enum ELogOptions
	{
		LO_LOADING			= 0x01, // log flash loading
		LO_ACTIONSCRIPT = 0x02, // log flash action script execution
		LO_PEAKS				= 0x04, // log high-level flash function calls which cause peaks
	};
	static unsigned int GetLogOptions();

private:
	bool IsEdgeAaAllowed() const;
	void UpdateRenderFlags();
	void UpdateASVerbosity();
	void Link();
	void Unlink();
	void EnableReleaseGuard(bool enabled);
	
private:
	static bool IsFlashEnabled();

private:
	static ICVar* CV_sys_flash;
	static ICVar* CV_sys_flash_edgeaa;
	static ICVar* CV_sys_flash_info;
	static ICVar* CV_sys_flash_info_peak_tolerance;
	static ICVar* CV_sys_flash_info_peak_exclude;
	static ICVar* CV_sys_flash_info_histo_scale;
	static ICVar* CV_sys_flash_log_options;
	static ICVar* CV_sys_flash_curve_tess_error;
	static ICVar* CV_sys_flash_warning_level;

	static int ms_sys_flash;
	static int ms_sys_flash_edgeaa;
	static int ms_sys_flash_info;
	static float ms_sys_flash_info_peak_tolerance;
	static float ms_sys_flash_info_histo_scale;
	static int ms_sys_flash_log_options;
	static float ms_sys_flash_curve_tess_error;
	static int ms_sys_flash_warning_level;

	static SLinkNode<CFlashPlayer> ms_rootNode;
	static IFlashLoadMovieHandler* ms_pLoadMovieHandler;	

private:
	bool m_rgEnabled;
	bool m_allowEgdeAA;
	float m_depth;
	GFxRenderConfig m_renderConfig;
	GFxActionControl m_asVerbosity;
	IFSCommandHandler* m_pFSCmdHandler;
	IExternalInterfaceHandler* m_pEIHandler;
	GPtr<GFxMovieDef> m_pMovieDef;
	GPtr<GFxMovieView> m_pMovieView;
	GPtr<GFxLoader2> m_pLoader;
	GPtr<GRendererXRender> m_pRenderer;
	string m_filePath;
	SLinkNode<CFlashPlayer> m_node;
	mutable SFlashProfilerData* m_pProfilerData;
};


#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _FLASH_PLAYER_INSTANCE_H_
