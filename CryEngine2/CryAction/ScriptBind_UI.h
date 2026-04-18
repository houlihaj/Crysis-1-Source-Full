/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Exposes basic UI functionality to the Script System.
  
 -------------------------------------------------------------------------
  History:
  - 16:8:2004   17:28 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_UI_H__
#define __SCRIPTBIND_UI_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


struct ISystem;
class CUISystem;
struct IGameFramework;

// <title UI>
// Syntax: UI
class CScriptBind_UI :
	public CScriptableBase
{
public:
	CScriptBind_UI(ISystem *pSystem, CUISystem *pUISystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_UI();

	void Release() { delete this; };

	// Script Events
	enum EUIScriptEvent
	{
		eUISE_OnInit = 0,
		eUISE_OnRelease,
		eUISE_OnPreUpdate,
		eUISE_OnPostUpdate,
		eUISE_OnIdle,
		eUISE_OnKeyDown,
		eUISE_OnKeyUp,
		eUISE_OnMouseDown,
		eUISE_OnMouseUp,
		eUISE_OnMouseMove,
		eUISE_OnLevelNotFound,
		eUISE_OnLoadingStart,
		eUISE_OnLoadingComplete,
		eUISE_OnLoadingError,
		eUISE_OnLoadingProgress,
		eUISE_Last,
	};
	
	void CallScriptEvent(EUIScriptEvent event)
	{
		if (m_onEventPtr[event])
		{
			Script::CallMethod(m_scriptTable, m_onEventPtr[event]);
		}
	};

	template <typename T1>
	void CallScriptEvent(EUIScriptEvent event, T1 p1)
	{
		if (m_onEventPtr[event])
		{
			Script::CallMethod(m_scriptTable, m_onEventPtr[event], p1);
		}
	};

	template <typename T1, typename T2>
	void CallScriptEvent(EUIScriptEvent event, T1 p1, T2 p2)
	{
		if (m_onEventPtr[event])
		{
			Script::CallMethod(m_scriptTable, m_onEventPtr[event], p1, p2);
		}
	};

	template <typename T1, typename T2, typename T3>
	void CallScriptEvent(EUIScriptEvent event, T1 p1, T2 p2, T3 p3)
	{
		if (m_onEventPtr[event])
		{
			Script::CallMethod(m_scriptTable, m_onEventPtr[event], p1, p2, p3);
		}
	};

	template <typename T1, typename T2, typename T3, typename T4>
	void CallScriptEvent(EUIScriptEvent event, T1 p1, T2 p2, T3 p3, T4 p4)
	{
		if (m_onEventPtr[event])
		{
			Script::CallMethod(m_scriptTable, m_onEventPtr[event], p1, p2, p3, p4);
		}
	};

	// Internal Events
	void OnInit();
	void OnRelease();


	// Script Functions
	int Reset(IFunctionHandler *pH);
	int EnableInput(IFunctionHandler *pH, bool enable);

	int Begin2D(IFunctionHandler *pH, int width, int height);
	int Clear(IFunctionHandler *pH, SmartScriptTable color);
	int SetScissor(IFunctionHandler *pH);
	int SetBlend(IFunctionHandler *pH);
	int DrawLine(IFunctionHandler *pH);
	int DrawTri(IFunctionHandler *pH);
	int DrawQuad(IFunctionHandler *pH);
	int DrawCircle(IFunctionHandler *pH);
	int Flush2D(IFunctionHandler *pH);
	int End2D(IFunctionHandler *pH);

	int Render3DEntity(IFunctionHandler *pH, ScriptHandle entityId);

	int GetScreenWidth(IFunctionHandler *pH);
	int GetScreenHeight(IFunctionHandler *pH);

	int GetWidth(IFunctionHandler *pH);
	int GetHeight(IFunctionHandler *pH);

	int PauseGame(IFunctionHandler *pH, bool pause);
	int IsGamePaused(IFunctionHandler *pH);

	int Localize(IFunctionHandler *pH);
	int SetFont(IFunctionHandler *pH);
	int DrawText(IFunctionHandler *pH);
	int GetTextSize(IFunctionHandler *pH);

	int LoadImage(IFunctionHandler *pH);
	int FreeImage(IFunctionHandler *pH);
	int GetImageSize(IFunctionHandler *pH);

	int CreateRenderTarget(IFunctionHandler *pH);
	int DestroyRenderTarget(IFunctionHandler *pH);
	int SetRenderTarget(IFunctionHandler *pH);

	int GetCurrentLevelInfo(IFunctionHandler *pH);

private:
	
	void RegisterMethods();
	void RegisterGlobals();

	CUISystem					*m_pUISystem;
	IGameFramework		*m_pGameFramework;
	SmartScriptTable	m_scriptTable;
	SmartScriptTable	m_levelInfo;

	HSCRIPTFUNCTION		m_onEventPtr[eUISE_Last];
};

#endif //__SCRIPTBIND_UI_H__