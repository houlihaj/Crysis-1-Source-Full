/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Input implementation for Linux using SDL
-------------------------------------------------------------------------
History:
- Jun 09,2006:	Created by Sascha Demetrio

*************************************************************************/
#include "StdAfx.h"

#ifdef USE_LINUXINPUT

#include "LinuxInput.h"

#include MATH_H

#include <IConsole.h>
#include <ILog.h>
#include <ISystem.h>
#include <IRenderer.h>

#include <SDL/SDL.h>

#define MOUSE_SYM_BASE (1024)
#define MOUSE_SYM(X) (MOUSE_SYM_BASE + (X))
#define MOUSE_AXIS_X (MOUSE_SYM(10))
#define MOUSE_AXIS_Y (MOUSE_SYM(11))
#define MOUSE_AXIS_Z (MOUSE_SYM(12))

// Define to enable automatic input grabbing support.  We don't want this
// during development.  If automatic input grabbing support is off, then input
// grabbing can be enforced by typing CTRL+ALT+G.
// #define LINUXINPUT_AUTOGRAB 1
#undef LINUXINPUT_AUTOGRAB

CLinuxInput::CLinuxInput(ISystem *pSystem) : CBaseInput()
{ 
	m_pSystem = pSystem;
	m_pLog = pSystem->GetILog();
};

bool CLinuxInput::Init()
{
	m_pLog->Log("Initializing LinuxInput/SDL");
	if (!CBaseInput::Init())
	{
		m_pLog->Log("Error: CBaseInput::Init failed");
		return false;
	}

	if (!AddInputDevice(new CLinuxKeyboardMouse(*this)))
	{
		m_pLog->Log("Error: Initializing Linux/SDL keyboard/mouse");
		return false;
	}
	return true;	
}

void CLinuxInput::ShutDown()
{
	m_pLog->Log("LinuxInput/SDL Shutdown");
	CBaseInput::ShutDown();
	delete this;
}

CLinuxInputDevice::CLinuxInputDevice(
		CLinuxInput &input,
		const char *deviceName)
	: CInputDevice((IInput&)input, deviceName),
		m_linuxInput(input)
{ }

CLinuxInputDevice::~CLinuxInputDevice()
{ }

CLinuxInput& CLinuxInputDevice::GetLinuxInput() const
{
	return m_linuxInput;
}

CLinuxKeyboardMouse::CLinuxKeyboardMouse(CLinuxInput& input)
	: CLinuxInputDevice(input, "keyboard_mouse"),
		m_posX(0), m_posY(0), m_bGrabInput(false)
{
	m_pSystem = GetISystem();
	m_pLog = m_pSystem->GetILog();
	m_pRenderer = m_pSystem->GetIRenderer();
}

CLinuxKeyboardMouse::~CLinuxKeyboardMouse()
{
}

bool CLinuxKeyboardMouse::Init()
{
	// Keyboard initialization.
	SetupKeyNames();
	SDL_EnableKeyRepeat(0, 0);
	SDL_EnableUNICODE(SDL_ENABLE);

	// Mouse initialization.
	MapSymbol(MOUSE_SYM(SDL_BUTTON_LEFT), eKI_Mouse1, "mouse1");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_MIDDLE), eKI_Mouse2, "mouse2");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_RIGHT), eKI_Mouse3, "mouse3");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_WHEELUP), eKI_MouseWheelUp, "mwheel_up");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_WHEELDOWN), eKI_MouseWheelDown, "mwheel_down");
	MapSymbol(MOUSE_AXIS_X, eKI_MouseX, "maxis_x", SInputSymbol::RawAxis);
	MapSymbol(MOUSE_AXIS_Y, eKI_MouseY, "maxis_y", SInputSymbol::RawAxis);
	MapSymbol(MOUSE_AXIS_Z, eKI_MouseZ, "maxis_z", SInputSymbol::RawAxis);

	SDL_ShowCursor(SDL_DISABLE);
#if defined(LINUXINPUT_AUTOGRAB)
	SDL_WM_GrabInput(SDL_GRAB_ON);
	m_bGrabInput = true;
#endif

	return true;
}

void CLinuxKeyboardMouse::Update()
{
	static const int maxPeep = 64;
	SDL_Event eventList[maxPeep];
	int nEvents = 0;
	unsigned type = 0;
	unsigned newX = m_posX, newY = m_posY;
	static const unsigned eventMask =
			SDL_EVENTMASK(SDL_KEYDOWN) | SDL_EVENTMASK(SDL_KEYUP)
			| SDL_EVENTMASK(SDL_MOUSEBUTTONDOWN) | SDL_EVENTMASK(SDL_MOUSEBUTTONUP)
			| SDL_EVENTMASK(SDL_MOUSEMOTION);

	PostHoldEvents();

	SInputSymbol *pSymbol = NULL;
	EInputState newState;

	SDL_PumpEvents();
	nEvents = SDL_PeepEvents(eventList, maxPeep, SDL_GETEVENT, eventMask);
	if (nEvents == -1)
	{
		GetISystem()->GetILog()->LogError(
				"SDL_GETEVENT error: %s", SDL_GetError());
		return;
	}
	for (int i = 0; i < nEvents; ++i)
	{
		type = eventList[i].type;
		if (type == SDL_KEYDOWN || type == SDL_KEYUP)
		{
			SDL_KeyboardEvent *const keyEvent = &eventList[i].key;
			m_lastKeySym = keyEvent->keysym.sym;
			m_lastMod = ConvertModifiers(keyEvent->keysym.mod);
			m_lastUNICODE = keyEvent->keysym.unicode;
		}
		if (type == SDL_KEYDOWN)
		{
			SDL_KeyboardEvent *const keyEvent = &eventList[i].key;
			SDLKey keySym = keyEvent->keysym.sym;
			pSymbol = DevSpecIdToSymbol((uint32)keySym);
			if (pSymbol == NULL)
				continue;
			pSymbol->state = eIS_Pressed;
			pSymbol->value = 1.f;
			// Check for CRTL+ALT+TAB or CTRL+ALT+ESC (in case the window manager
			// won't let ALT+TAB combintaions through).
			if ((keyEvent->keysym.mod & (KMOD_LALT | KMOD_RALT)) != 0
					&& (keyEvent->keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0)
			{
				if (keySym == SDLK_TAB || keySym == SDLK_ESCAPE)
					UngrabInput();
				else if (keySym == SDLK_g)
					GrabInput();
			}
			PostEvent(pSymbol, keyEvent->keysym.mod);
		}
		else if (type == SDL_KEYUP)
		{
			SDL_KeyboardEvent *const keyEvent = &eventList[i].key;
			SDLKey keySym = keyEvent->keysym.sym;
			pSymbol = DevSpecIdToSymbol((uint32)keySym);
			if (pSymbol == NULL)
				continue;
			pSymbol->state = eIS_Released;
			pSymbol->value = 0.f;
			PostEvent(pSymbol, keyEvent->keysym.mod);
		}
		else if (type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP)
		{
			SDL_MouseButtonEvent *const buttonEvent = &eventList[i].button;
			pSymbol = DevSpecIdToSymbol((uint32)MOUSE_SYM(buttonEvent->button));
			if (pSymbol == NULL)
				continue;
			if (type == SDL_MOUSEBUTTONDOWN)
			{
				pSymbol->state = eIS_Pressed;
				pSymbol->value = 1.f;
				PostEvent(pSymbol);
#if defined(LINUXINPUT_AUTOGRAB)
				GrabInput();
#endif
			}
			else
			{
				pSymbol->state = eIS_Released;
				pSymbol->value = 0.f;
				PostEvent(pSymbol);
			}
		}
		else if (type == SDL_MOUSEMOTION)
		{
			SDL_MouseMotionEvent *const motionEvent = &eventList[i].motion;
			// If we have grabbed the input exclusively, then we'll pass on the
			// delta values from the events as is.  Without exclusive input, we'll
			// compute the deltas ourselves from the stored position.
			if (m_bGrabInput)
			{
				if (motionEvent->xrel != 0 || motionEvent->yrel != 0)
				{
					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = motionEvent->xrel;
					PostEvent(pSymbol);
					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = motionEvent->yrel;
					PostEvent(pSymbol);
					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = 0.f;
					PostEvent(pSymbol);
				}
			}
			newX = (int)rintf(motionEvent->x * 800.f / m_pRenderer->GetWidth());
			newY = (int)rintf(motionEvent->y * 600.f / m_pRenderer->GetHeight());
		}
		else
		{
			// Unexpected event type.
			abort();
		}
	}

	// Generate mouse motion events when running without exclusive input.
	if (!m_bGrabInput)
	{
		if (newX != m_posX || newY != m_posY)
		{
			pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
			assert(pSymbol);
			pSymbol->state = eIS_Changed;
			pSymbol->value = (int)newX - (int)m_posX;
			PostEvent(pSymbol);
			pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
			assert(pSymbol);
			pSymbol->state = eIS_Changed;
			pSymbol->value = (int)newY - (int)m_posY;
			PostEvent(pSymbol);
			pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
			assert(pSymbol);
			pSymbol->state = eIS_Changed;
			pSymbol->value = 0.f;
			PostEvent(pSymbol);
		}
	}

	m_posX = newX;
	m_posY = newY;
}

const char *CLinuxKeyboardMouse::GetKeyName(const SInputEvent& event, bool bGUI)
{
	if (bGUI)
	{
		static __thread char keyName[2];
		SDLKey keySym = (SDLKey)NameToId(event.keyName);
		if (keySym == m_lastKeySym && event.modifiers == m_lastMod)
		{
			unsigned code = m_lastUNICODE;
			if (code > 0 && code < 255)
			{
				keyName[0] = (char)code;
				keyName[1] = 0;
				return keyName;
			}
		}
		keyName[0] = Event2ASCII(event);
		keyName[1] = 0;
		return keyName;
	}
	else
		return event.keyName.c_str();
}

unsigned char CLinuxKeyboardMouse::Event2ASCII(const SInputEvent& event)
{
	SDLKey keySym = (SDLKey)NameToId(event.keyName);
	char ascii = 0;
	bool alpha = false;

	switch (keySym)
	{
	case SDLK_BACKSPACE: ascii = '\b'; break;
	case SDLK_TAB: ascii = '\t'; break;
	case SDLK_RETURN: ascii = '\r'; break;
	case SDLK_ESCAPE: ascii = '\x1b'; break;
	case SDLK_SPACE: ascii = ' '; break;
	case SDLK_EXCLAIM: ascii = '!'; break;
	case SDLK_QUOTEDBL: ascii = '"'; break;
	case SDLK_HASH: ascii = '#'; break;
	case SDLK_DOLLAR: ascii = '$'; break;
	case SDLK_AMPERSAND: ascii = '&'; break;
	case SDLK_QUOTE: ascii = '\''; break;
	case SDLK_LEFTPAREN: ascii = '('; break;
	case SDLK_RIGHTPAREN: ascii = ')'; break;
	case SDLK_ASTERISK: ascii = '*'; break;
	case SDLK_PLUS: ascii = '+'; break;
	case SDLK_COMMA: ascii = ','; break;
	case SDLK_MINUS: ascii = '-'; break;
	case SDLK_PERIOD: ascii = '.'; break;
	case SDLK_SLASH: ascii = '/'; break;
	case SDLK_0: ascii = '0'; break;
	case SDLK_1: ascii = '1'; break;
	case SDLK_2: ascii = '2'; break;
	case SDLK_3: ascii = '3'; break;
	case SDLK_4: ascii = '4'; break;
	case SDLK_5: ascii = '5'; break;
	case SDLK_6: ascii = '6'; break;
	case SDLK_7: ascii = '7'; break;
	case SDLK_8: ascii = '8'; break;
	case SDLK_9: ascii = '9'; break;
	case SDLK_COLON: ascii = ':'; break;
	case SDLK_SEMICOLON: ascii = ';'; break;
	case SDLK_LESS: ascii = '<'; break;
	case SDLK_EQUALS: ascii = '='; break;
	case SDLK_GREATER: ascii = '>'; break;
	case SDLK_QUESTION: ascii = '?'; break;
	case SDLK_AT: ascii = '@'; break;
	case SDLK_LEFTBRACKET: ascii = '['; break;
	case SDLK_BACKSLASH: ascii = '\\'; break;
	case SDLK_RIGHTBRACKET: ascii = ']'; break;
	case SDLK_CARET: ascii = '^'; break;
	case SDLK_UNDERSCORE: ascii = '_'; break;
	case SDLK_BACKQUOTE: ascii = '`'; break;
	case SDLK_a: ascii = 'a'; alpha = true; break;
	case SDLK_b: ascii = 'b'; alpha = true; break;
	case SDLK_c: ascii = 'c'; alpha = true; break;
	case SDLK_d: ascii = 'd'; alpha = true; break;
	case SDLK_e: ascii = 'e'; alpha = true; break;
	case SDLK_f: ascii = 'f'; alpha = true; break;
	case SDLK_g: ascii = 'g'; alpha = true; break;
	case SDLK_h: ascii = 'h'; alpha = true; break;
	case SDLK_i: ascii = 'i'; alpha = true; break;
	case SDLK_j: ascii = 'j'; alpha = true; break;
	case SDLK_k: ascii = 'k'; alpha = true; break;
	case SDLK_l: ascii = 'l'; alpha = true; break;
	case SDLK_m: ascii = 'm'; alpha = true; break;
	case SDLK_n: ascii = 'n'; alpha = true; break;
	case SDLK_o: ascii = 'o'; alpha = true; break;
	case SDLK_p: ascii = 'p'; alpha = true; break;
	case SDLK_q: ascii = 'q'; alpha = true; break;
	case SDLK_r: ascii = 'r'; alpha = true; break;
	case SDLK_s: ascii = 's'; alpha = true; break;
	case SDLK_t: ascii = 't'; alpha = true; break;
	case SDLK_u: ascii = 'u'; alpha = true; break;
	case SDLK_v: ascii = 'v'; alpha = true; break;
	case SDLK_w: ascii = 'w'; alpha = true; break;
	case SDLK_x: ascii = 'x'; alpha = true; break;
	case SDLK_y: ascii = 'y'; alpha = true; break;
	case SDLK_z: ascii = 'z'; alpha = true; break;
	case SDLK_DELETE: ascii = '\x7f'; break;
	case SDLK_KP0: ascii = '0'; break;
	case SDLK_KP1: ascii = '1'; break;
	case SDLK_KP2: ascii = '2'; break;
	case SDLK_KP3: ascii = '3'; break;
	case SDLK_KP4: ascii = '4'; break;
	case SDLK_KP5: ascii = '5'; break;
	case SDLK_KP6: ascii = '6'; break;
	case SDLK_KP7: ascii = '7'; break;
	case SDLK_KP8: ascii = '8'; break;
	case SDLK_KP9: ascii = '9'; break;
	case SDLK_KP_PERIOD: ascii = '.'; break;
	case SDLK_KP_DIVIDE: ascii = '/'; break;
	case SDLK_KP_MULTIPLY: ascii = '*'; break;
	case SDLK_KP_MINUS: ascii = '-'; break;
	case SDLK_KP_PLUS: ascii = '+'; break;
	case SDLK_KP_ENTER: ascii = '\r'; break;
	case SDLK_KP_EQUALS: ascii = '='; break;
	default: ascii = 0; break;
	}
	if (ascii && (event.modifiers & (eMM_LShift | eMM_RShift | eMM_CapsLock)))
		ascii = toupper(ascii);
	return ascii;
}

int CLinuxKeyboardMouse::ConvertModifiers(unsigned keyEventModifiers)
{
	int modifiers = 0;

	if (keyEventModifiers & KMOD_LCTRL)
		modifiers |= eMM_LCtrl;
	if (keyEventModifiers & KMOD_LSHIFT)
		modifiers |= eMM_LShift;
	if (keyEventModifiers & KMOD_LALT)
		modifiers |= eMM_LAlt;
	if (keyEventModifiers & KMOD_LMETA)
		modifiers |= eMM_LWin;
	if (keyEventModifiers & KMOD_RCTRL)
		modifiers |= eMM_RCtrl;
	if (keyEventModifiers & KMOD_RSHIFT)
		modifiers |= eMM_RShift;
	if (keyEventModifiers & KMOD_RALT)
		modifiers |= eMM_RAlt;
	if (keyEventModifiers & KMOD_RMETA)
		modifiers |= eMM_RWin;
	if (keyEventModifiers & KMOD_NUM)
		modifiers |= eMM_NumLock;
	if (keyEventModifiers & KMOD_CAPS)
		modifiers |= eMM_CapsLock;
	if (keyEventModifiers & KMOD_MODE)
		modifiers |= eMM_ScrollLock;
	return modifiers;
}

void CLinuxKeyboardMouse::SetupKeyNames()
{
	MapSymbol(SDLK_BACKSPACE, eKI_Backspace, "backspace");
	MapSymbol(SDLK_TAB, eKI_Tab, "tab");
	// MapSymbol(SDLK_CLEAR clear);
	MapSymbol(SDLK_RETURN, eKI_Enter, "enter");
	MapSymbol(SDLK_PAUSE, eKI_Pause, "pause");
	MapSymbol(SDLK_ESCAPE, eKI_Escape, "escape");
	MapSymbol(SDLK_SPACE, eKI_Space, "space");
	//MapSymbol(SDLK_EXCLAIM '!' exclamation mark);
	//MapSymbol(SDLK_QUOTEDBL '"' double quote);
	//MapSymbol(SDLK_HASH '#' hash);
	//MapSymbol(SDLK_DOLLAR '$' dollar);
	//MapSymbol(SDLK_AMPERSAND '&' ampersand);
	//MapSymbol(SDLK_QUOTE '\'' single quote);
	//MapSymbol(SDLK_LEFTPAREN '(' left parenthesis);
	//MapSymbol(SDLK_RIGHTPAREN ')' right parenthesis);
	//MapSymbol(SDLK_ASTERISK '*' asterisk);
	//MapSymbol(SDLK_PLUS '+' plus sign);
	MapSymbol(SDLK_COMMA, eKI_Comma, "comma");
	MapSymbol(SDLK_MINUS, eKI_Minus, "minus");
	MapSymbol(SDLK_PERIOD, eKI_Period, "period");
	MapSymbol(SDLK_SLASH, eKI_Slash, "slash");
	MapSymbol(SDLK_0, eKI_0, "0");
	MapSymbol(SDLK_1, eKI_1, "1");
	MapSymbol(SDLK_2, eKI_2, "2");
	MapSymbol(SDLK_3, eKI_3, "3");
	MapSymbol(SDLK_4, eKI_4, "4");
	MapSymbol(SDLK_5, eKI_5, "5");
	MapSymbol(SDLK_6, eKI_6, "6");
	MapSymbol(SDLK_7, eKI_7, "7");
	MapSymbol(SDLK_8, eKI_8, "8");
	MapSymbol(SDLK_9, eKI_9, "9");
	MapSymbol(SDLK_COLON, eKI_Colon, "colon");
	MapSymbol(SDLK_SEMICOLON, eKI_Semicolon, "semicolon");
	//MapSymbol(SDLK_LESS '<' less-than sign);
	MapSymbol(SDLK_EQUALS, eKI_Equals, "equals");
	//MapSymbol(SDLK_GREATER '>' greater-than sign);
	//MapSymbol(SDLK_QUESTION '?' question mark);
	//MapSymbol(SDLK_AT '@' at);
	MapSymbol(SDLK_LEFTBRACKET, eKI_LBracket, "lbracket");
	MapSymbol(SDLK_BACKSLASH, eKI_Backslash, "backslash");
	MapSymbol(SDLK_RIGHTBRACKET, eKI_RBracket, "rbracket");
	//MapSymbol(SDLK_CARET '^' caret);
	MapSymbol(SDLK_UNDERSCORE, eKI_Underline, "underline");
	MapSymbol(SDLK_BACKQUOTE, eKI_Tilde, "tilde"); // Yes, this is correct
	MapSymbol(SDLK_a, eKI_A, "a");
	MapSymbol(SDLK_b, eKI_B, "b");
	MapSymbol(SDLK_c, eKI_C, "c");
	MapSymbol(SDLK_d, eKI_D, "d");
	MapSymbol(SDLK_e, eKI_E, "e");
	MapSymbol(SDLK_f, eKI_F, "f");
	MapSymbol(SDLK_g, eKI_G, "g");
	MapSymbol(SDLK_h, eKI_H, "h");
	MapSymbol(SDLK_i, eKI_I, "i");
	MapSymbol(SDLK_j, eKI_J, "j");
	MapSymbol(SDLK_k, eKI_K, "k");
	MapSymbol(SDLK_l, eKI_L, "l");
	MapSymbol(SDLK_m, eKI_M, "m");
	MapSymbol(SDLK_n, eKI_N, "n");
	MapSymbol(SDLK_o, eKI_O, "o");
	MapSymbol(SDLK_p, eKI_P, "p");
	MapSymbol(SDLK_q, eKI_Q, "q");
	MapSymbol(SDLK_r, eKI_R, "r");
	MapSymbol(SDLK_s, eKI_S, "s");
	MapSymbol(SDLK_t, eKI_T, "t");
	MapSymbol(SDLK_u, eKI_U, "u");
	MapSymbol(SDLK_v, eKI_V, "v");
	MapSymbol(SDLK_w, eKI_W, "w");
	MapSymbol(SDLK_x, eKI_X, "x");
	MapSymbol(SDLK_y, eKI_Y, "y");
	MapSymbol(SDLK_z, eKI_Z, "z");
	MapSymbol(SDLK_DELETE, eKI_Delete, "delete");
	//MapSymbol(SDLK_WORLD_0 world 0);
	//MapSymbol(SDLK_WORLD_1 world 1);
	//MapSymbol(SDLK_WORLD_2 world 2);
	//MapSymbol(SDLK_WORLD_3 world 3);
	//MapSymbol(SDLK_WORLD_4 world 4);
	//MapSymbol(SDLK_WORLD_5 world 5);
	//MapSymbol(SDLK_WORLD_6 world 6);
	//MapSymbol(SDLK_WORLD_7 world 7);
	//MapSymbol(SDLK_WORLD_8 world 8);
	//MapSymbol(SDLK_WORLD_9 world 9);
	//MapSymbol(SDLK_WORLD_10 world 10);
	//MapSymbol(SDLK_WORLD_11 world 11);
	//MapSymbol(SDLK_WORLD_12 world 12);
	//MapSymbol(SDLK_WORLD_13 world 13);
	//MapSymbol(SDLK_WORLD_14 world 14);
	//MapSymbol(SDLK_WORLD_15 world 15);
	//MapSymbol(SDLK_WORLD_16 world 16);
	//MapSymbol(SDLK_WORLD_17 world 17);
	//MapSymbol(SDLK_WORLD_18 world 18);
	//MapSymbol(SDLK_WORLD_19 world 19);
	//MapSymbol(SDLK_WORLD_20 world 20);
	//MapSymbol(SDLK_WORLD_21 world 21);
	//MapSymbol(SDLK_WORLD_22 world 22);
	//MapSymbol(SDLK_WORLD_23 world 23);
	//MapSymbol(SDLK_WORLD_24 world 24);
	//MapSymbol(SDLK_WORLD_25 world 25);
	//MapSymbol(SDLK_WORLD_26 world 26);
	//MapSymbol(SDLK_WORLD_27 world 27);
	//MapSymbol(SDLK_WORLD_28 world 28);
	//MapSymbol(SDLK_WORLD_29 world 29);
	//MapSymbol(SDLK_WORLD_30 world 30);
	//MapSymbol(SDLK_WORLD_31 world 31);
	//MapSymbol(SDLK_WORLD_32 world 32);
	//MapSymbol(SDLK_WORLD_33 world 33);
	//MapSymbol(SDLK_WORLD_34 world 34);
	//MapSymbol(SDLK_WORLD_35 world 35);
	//MapSymbol(SDLK_WORLD_36 world 36);
	//MapSymbol(SDLK_WORLD_37 world 37);
	//MapSymbol(SDLK_WORLD_38 world 38);
	//MapSymbol(SDLK_WORLD_39 world 39);
	//MapSymbol(SDLK_WORLD_40 world 40);
	//MapSymbol(SDLK_WORLD_41 world 41);
	//MapSymbol(SDLK_WORLD_42 world 42);
	//MapSymbol(SDLK_WORLD_43 world 43);
	//MapSymbol(SDLK_WORLD_44 world 44);
	//MapSymbol(SDLK_WORLD_45 world 45);
	//MapSymbol(SDLK_WORLD_46 world 46);
	//MapSymbol(SDLK_WORLD_47 world 47);
	//MapSymbol(SDLK_WORLD_48 world 48);
	//MapSymbol(SDLK_WORLD_49 world 49);
	//MapSymbol(SDLK_WORLD_50 world 50);
	//MapSymbol(SDLK_WORLD_51 world 51);
	//MapSymbol(SDLK_WORLD_52 world 52);
	//MapSymbol(SDLK_WORLD_53 world 53);
	//MapSymbol(SDLK_WORLD_54 world 54);
	//MapSymbol(SDLK_WORLD_55 world 55);
	//MapSymbol(SDLK_WORLD_56 world 56);
	//MapSymbol(SDLK_WORLD_57 world 57);
	//MapSymbol(SDLK_WORLD_58 world 58);
	//MapSymbol(SDLK_WORLD_59 world 59);
	//MapSymbol(SDLK_WORLD_60 world 60);
	//MapSymbol(SDLK_WORLD_61 world 61);
	//MapSymbol(SDLK_WORLD_62 world 62);
	//MapSymbol(SDLK_WORLD_63 world 63);
	//MapSymbol(SDLK_WORLD_64 world 64);
	//MapSymbol(SDLK_WORLD_65 world 65);
	//MapSymbol(SDLK_WORLD_66 world 66);
	//MapSymbol(SDLK_WORLD_67 world 67);
	//MapSymbol(SDLK_WORLD_68 world 68);
	//MapSymbol(SDLK_WORLD_69 world 69);
	//MapSymbol(SDLK_WORLD_70 world 70);
	//MapSymbol(SDLK_WORLD_71 world 71);
	//MapSymbol(SDLK_WORLD_72 world 72);
	//MapSymbol(SDLK_WORLD_73 world 73);
	//MapSymbol(SDLK_WORLD_74 world 74);
	//MapSymbol(SDLK_WORLD_75 world 75);
	//MapSymbol(SDLK_WORLD_76 world 76);
	//MapSymbol(SDLK_WORLD_77 world 77);
	//MapSymbol(SDLK_WORLD_78 world 78);
	//MapSymbol(SDLK_WORLD_79 world 79);
	//MapSymbol(SDLK_WORLD_80 world 80);
	//MapSymbol(SDLK_WORLD_81 world 81);
	//MapSymbol(SDLK_WORLD_82 world 82);
	//MapSymbol(SDLK_WORLD_83 world 83);
	//MapSymbol(SDLK_WORLD_84 world 84);
	//MapSymbol(SDLK_WORLD_85 world 85);
	//MapSymbol(SDLK_WORLD_86 world 86);
	//MapSymbol(SDLK_WORLD_87 world 87);
	//MapSymbol(SDLK_WORLD_88 world 88);
	//MapSymbol(SDLK_WORLD_89 world 89);
	//MapSymbol(SDLK_WORLD_90 world 90);
	//MapSymbol(SDLK_WORLD_91 world 91);
	//MapSymbol(SDLK_WORLD_92 world 92);
	//MapSymbol(SDLK_WORLD_93 world 93);
	//MapSymbol(SDLK_WORLD_94 world 94);
	//MapSymbol(SDLK_WORLD_95 world 95);
	MapSymbol(SDLK_KP0, eKI_NP_0, "np_0");
	MapSymbol(SDLK_KP1, eKI_NP_1, "np_1");
	MapSymbol(SDLK_KP2, eKI_NP_2, "np_2");
	MapSymbol(SDLK_KP3, eKI_NP_3, "np_3");
	MapSymbol(SDLK_KP4, eKI_NP_4, "np_4");
	MapSymbol(SDLK_KP5, eKI_NP_5, "np_5");
	MapSymbol(SDLK_KP6, eKI_NP_6, "np_6");
	MapSymbol(SDLK_KP7, eKI_NP_7, "np_7");
	MapSymbol(SDLK_KP8, eKI_NP_8, "np_8");
	MapSymbol(SDLK_KP9, eKI_NP_9, "np_9");
	MapSymbol(SDLK_KP_PERIOD, eKI_NP_Period, "np_period");
	MapSymbol(SDLK_KP_DIVIDE, eKI_NP_Divide, "np_divide");
	MapSymbol(SDLK_KP_MULTIPLY, eKI_NP_Multiply, "np_multiply");
	MapSymbol(SDLK_KP_MINUS, eKI_NP_Substract, "np_subtract");
	MapSymbol(SDLK_KP_PLUS, eKI_NP_Add, "np_add");
	MapSymbol(SDLK_KP_ENTER, eKI_NP_Enter, "np_enter");
	MapSymbol(SDLK_KP_EQUALS, eKI_NP_Enter, "np_enter");
	MapSymbol(SDLK_UP, eKI_Up, "up");
	MapSymbol(SDLK_DOWN, eKI_Down, "down");
	MapSymbol(SDLK_RIGHT, eKI_Right, "right");
	MapSymbol(SDLK_LEFT, eKI_Left, "left");
	MapSymbol(SDLK_INSERT, eKI_Insert, "insert");
	MapSymbol(SDLK_HOME, eKI_Home, "home");
	MapSymbol(SDLK_END, eKI_End, "end");
	MapSymbol(SDLK_PAGEUP, eKI_PgUp, "pgup");
	MapSymbol(SDLK_PAGEDOWN, eKI_PgDn, "pgdn");
	MapSymbol(SDLK_F1, eKI_F1, "f1");
	MapSymbol(SDLK_F2, eKI_F2, "f2");
	MapSymbol(SDLK_F3, eKI_F3, "f3");
	MapSymbol(SDLK_F4, eKI_F4, "f4");
	MapSymbol(SDLK_F5, eKI_F5, "f5");
	MapSymbol(SDLK_F6, eKI_F6, "f6");
	MapSymbol(SDLK_F7, eKI_F7, "f7");
	MapSymbol(SDLK_F8, eKI_F8, "f8");
	MapSymbol(SDLK_F9, eKI_F9, "f9");
	MapSymbol(SDLK_F10, eKI_F10, "f10");
	MapSymbol(SDLK_F11, eKI_F11, "f11");
	MapSymbol(SDLK_F12, eKI_F12, "f12");
	MapSymbol(SDLK_F13, eKI_F13, "f13");
	MapSymbol(SDLK_F14, eKI_F14, "f14");
	MapSymbol(SDLK_F15, eKI_F15, "f15");
	MapSymbol(SDLK_NUMLOCK, eKI_NumLock, "numlock", SInputSymbol::Toggle, eMM_NumLock);
	MapSymbol(SDLK_CAPSLOCK, eKI_CapsLock, "capslock", SInputSymbol::Toggle, eMM_CapsLock);
	MapSymbol(SDLK_SCROLLOCK, eKI_ScrollLock, "scrolllock", SInputSymbol::Toggle, eMM_ScrollLock);
	MapSymbol(SDLK_RSHIFT, eKI_RShift, "rshift", SInputSymbol::Button, eMM_RShift);
	MapSymbol(SDLK_LSHIFT, eKI_LShift, "lshift", SInputSymbol::Button, eMM_LShift);
	MapSymbol(SDLK_RCTRL, eKI_RCtrl, "rctrl", SInputSymbol::Button, eMM_RCtrl);
	MapSymbol(SDLK_LCTRL, eKI_LCtrl, "lctrl", SInputSymbol::Button, eMM_LCtrl);
	MapSymbol(SDLK_RALT, eKI_RAlt, "ralt", SInputSymbol::Button, eMM_RAlt);
	MapSymbol(SDLK_LALT, eKI_LAlt, "lalt", SInputSymbol::Button, eMM_LAlt);
	//MapSymbol(SDLK_RMETA right meta);
	//MapSymbol(SDLK_LMETA left meta);
	MapSymbol(SDLK_LSUPER, eKI_LWin, "lwin", SInputSymbol::Button, eMM_LWin);
	MapSymbol(SDLK_RSUPER, eKI_RWin, "rwin", SInputSymbol::Button, eMM_RWin);
	//MapSymbol(SDLK_MODE mode shift);
	//MapSymbol(SDLK_COMPOSE compose);
	//MapSymbol(SDLK_HELP help);
	MapSymbol(SDLK_PRINT, eKI_Print, "print");
	//MapSymbol(SDLK_SYSREQ SysRq);
	MapSymbol(SDLK_BREAK, eKI_Pause, "pause");
	//MapSymbol(SDLK_MENU menu);
	//MapSymbol(SDLK_POWER power);
	//MapSymbol(SDLK_EURO euro);
	//MapSymbol(SDLK_UNDO undo);
}

void CLinuxKeyboardMouse::PostEvent(SInputSymbol *pSymbol, unsigned keyMod)
{
	SInputEvent event;
	event.keyName = pSymbol->name;
	event.state = pSymbol->state;
	event.deviceId = eDI_Mouse;
	event.timestamp = GetTickCount();
	if (keyMod != ~0)
		event.modifiers = ConvertModifiers(keyMod);
	else
		event.modifiers = 0; // FIXME add mouse modifier support
	event.value = pSymbol->value;
	GetLinuxInput().PostInputEvent(event);
}

void CLinuxKeyboardMouse::GrabInput()
{
	SInputSymbol *pSymbol = NULL;
	unsigned width, height;

	if (m_bGrabInput)
		return;

	SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_ShowCursor(SDL_DISABLE);

	// Warp the cursor to the upper left corner of the screen.
	width = m_pRenderer->GetWidth();
	height = m_pRenderer->GetHeight();
	pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
	assert(pSymbol);
	pSymbol->state = eIS_Changed;
	pSymbol->value = -width;
	PostEvent(pSymbol);
	pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
	assert(pSymbol);
	pSymbol->state = eIS_Changed;
	pSymbol->value = -height;
	PostEvent(pSymbol);
	pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
	assert(pSymbol);
	pSymbol->state = eIS_Changed;
	pSymbol->value = 0.f;
	PostEvent(pSymbol);

	// Warp the cursor to the stored cursor position.
	if (m_posX != 0 || m_posY != 0)
	{
		pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
		assert(pSymbol);
		pSymbol->state = eIS_Changed;
		pSymbol->value = m_posX;
		PostEvent(pSymbol);
		pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
		assert(pSymbol);
		pSymbol->state = eIS_Changed;
		pSymbol->value = m_posY;
		PostEvent(pSymbol);
		pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
		assert(pSymbol);
		pSymbol->state = eIS_Changed;
		pSymbol->value = 0.f;
		PostEvent(pSymbol);
	}

	m_bGrabInput = true;
}

void CLinuxKeyboardMouse::UngrabInput()
{
	// If the user forces us to release the input grab, then we'll show
	// the system mouse cursor immediately, even if the cursor is still
	// within our window.  The intention is to give a visible feedback
	// that the release has been performed.
	m_bGrabInput = false;
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_ShowCursor(SDL_ENABLE);
}

#endif // USE_LINUXINPUT

// vim:ts=2:sw=2:tw=78

