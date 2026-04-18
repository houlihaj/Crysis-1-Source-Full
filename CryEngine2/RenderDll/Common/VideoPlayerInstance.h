#ifndef _C_VIDEO_PLAYER_INSTANCE_H_
#define _C_VIDEO_PLAYER_INSTANCE_H_

#pragma once

#include <IVideoPlayer.h>


#ifndef EXCLUDE_CRI_SDK


#include "cri_mw.h"


class CCriMwLibInit;
class CTexture;
class CShader;


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


class CVideoPlayer : public IVideoPlayer
{
public:
	// IVideoPlayer interface
	virtual void Release();

	virtual bool Load(const char* pFilePath, unsigned int options, int audioCh = 0, int voiceCh = -1, bool useSubtitles = false);

	virtual EPlaybackStatus GetStatus() const;
	virtual bool Start();
	virtual bool Stop();
	virtual bool Pause(bool pause);
	virtual bool SetViewport(int x0, int y0, int width, int height);
	virtual bool Render();
	virtual void GetSubtitle(int subtitleCh, char* pStBuf, size_t pStBufLen);
	
	virtual void EnablePerFrameUpdate(bool enable);
	virtual bool IsPerFrameUpdateEnabled() const;
	
	virtual int GetWidth() const;
	virtual int GetHeight() const;

public:
	CVideoPlayer();
	virtual ~CVideoPlayer();

public:
	static void ProcessPerFrameUpdates();
	static void OnAppFocusChanged(bool appHasFocus);

private:
	void Destruct(bool callerIsDtor = false);

	bool CreateTextures();
	void DestroyTextures();
	void UpdateTextures(const MwsfdFrmObj& frm);

	bool CreateTexture(CTexture*& pTexture, const char* pTextureName, int width, int height, unsigned char clearValue);
	void RecreateDeviceTexture(CTexture*& pTexture, unsigned char clearValue);
	void ClearTexture(CTexture* pTexture, unsigned char clearValue);
	void UploadTextureData(CTexture* pTexture, void* pSrcData, int srcWidth);

	void DecoderUpdate(bool checkDeviceLost);
	void Display();

	bool DiscardFrame(const MwsfdFrmObj& frm) const;
	bool IsFullAlphaMovie() const;
	bool DynTexDataLost() const;

	void StartPlayback();

	void Link();
	void Unlink();

	bool PauseInt(bool pause);
	void OnFocusChanged();

private:
	static CCriMwLibInit* ms_pCriMwInit;
	static SLinkNode<CVideoPlayer> ms_rootNode;
	static bool ms_appHasFocus;

private:
	bool m_playbackStarted;
	bool m_loopPlayback;
	int m_viewportX0;
	int m_viewportY0;
	int m_viewportWidth;
	int m_viewportHeight;
	MwsfdCrePrm m_sfdCreationParams;
	MWPLY m_sfdHandle;
	CTexture* m_pCurFrameY;
	CTexture* m_pCurFrameCb;
	CTexture* m_pCurFrameCr;
	CTexture* m_pCurFrameA;
	CShader* m_pCriMwShader;
	char m_fullFilePath[256];
	const char* m_pFilePath;
	MwsfdAudioCodec m_audioCodec;
	char* m_pAudioWB;
	char* m_pVoiceWB;
	SLinkNode<CVideoPlayer> m_node;
	int m_lastDecSvrUpdate;
	int m_frameReset;
	bool m_perFrameUpdate;
	bool m_pauseRequestLostFocus;
	float m_curVolume;
};


#endif // #ifndef EXCLUDE_CRI_SDK


#endif // #ifndef _C_VIDEO_PLAYER_INSTANCE_H_