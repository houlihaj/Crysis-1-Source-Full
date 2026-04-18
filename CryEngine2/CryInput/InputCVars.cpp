#include "StdAfx.h"
#include "InputCVars.h"
#include <IConsole.h>

CInputCVars* g_pInputCVars=0;

CInputCVars::CInputCVars()
{
	gEnv->pConsole->Register("i_debug", &i_debug, 0, 0,
		"Toggles input event debugging.\n"
		"Usage: i_debug [0/1]\n"
		"Default is 0 (off). Set to 1 to spam console with key events (only press and release).");
	gEnv->pConsole->Register("i_forcefeedback", &i_forcefeedback, 1, 0, "Enable/Disable force feedback output.");

	// mouse
	gEnv->pConsole->Register("i_mouse_buffered", &i_mouse_buffered, 0, 0,
		"Toggles mouse input buffering.\n"
		"Usage: i_mouse_buffered [0/1]\n"
		"Default is 0 (off). Set to 1 to process buffered mouse input.");
	gEnv->pConsole->Register("i_mouse_accel", &i_mouse_accel, 0.0f, VF_DUMPTODISK,
		"Set mouse acceleration, 0.0 means no acceleration.\n"
		"Usage: i_mouse_accel [float number] (usually a small number, 0.1 is a good one)\n"
		"Default is 0.0 (off)");
	gEnv->pConsole->Register("i_mouse_accel_max", &i_mouse_accel_max, 100.0f, VF_DUMPTODISK,
		"Set mouse max mouse delta when using acceleration.\n"
		"Usage: i_mouse_accel_max [float number]\n"
		"Default is 100.0");	
	gEnv->pConsole->Register("i_mouse_smooth", &i_mouse_smooth, 0.0f, VF_DUMPTODISK,
		"Set mouse smoothing value, also if 0 (disabled) there will be a simple average between the old and the actual input.\n"
		"Usage: i_mouse_smooth [float number] (1.0 = very very smooth, 30 = almost istant)\n"
		"Default is 0.0");	
	gEnv->pConsole->Register("i_mouse_inertia", &i_mouse_inertia, 0.0f, VF_DUMPTODISK,
		"Set mouse inertia. It is disabled (0.0) by default.\n"
		"Usage: i_mouse_inertia [float number]\n"
		"Default is 0.0");

	// keyboard
	gEnv->pConsole->Register("i_bufferedkeys", &i_bufferedkeys, 1, 0,
		"Toggles key buffering.\n"
		"Usage: i_bufferedkeys [0/1]\n"
		"Default is 0 (off). Set to 1 to process buffered key strokes.");

	// xinput
	gEnv->pConsole->Register("i_xinput", &i_xinput, 1, 0,
		"Number of XInput controllers to process\n"
		"Usage: i_xinput [0/1/2/3/4]\n"
		"Default is 1.");
	gEnv->pConsole->Register("i_xinput_poll_time", &i_xinput_poll_time, 1000, 0,
		"Number of ms between device polls in polling thread\n"
		"Usage: i_xinput_poll_time 500\n"
		"Default is 1000ms. Value must be >=0.");
}

CInputCVars::~CInputCVars()
{
	gEnv->pConsole->UnregisterVariable("i_debug");
	gEnv->pConsole->UnregisterVariable("i_forcefeedback");

	// mouse
	gEnv->pConsole->UnregisterVariable("i_mouse_buffered");
	gEnv->pConsole->UnregisterVariable("i_mouse_accel");
	gEnv->pConsole->UnregisterVariable("i_mouse_accel_max");
	gEnv->pConsole->UnregisterVariable("i_mouse_smooth");
	gEnv->pConsole->UnregisterVariable("i_mouse_inertia");

	// keyboard
	gEnv->pConsole->UnregisterVariable("i_bufferedkeys");

	// xinput
	gEnv->pConsole->UnregisterVariable("i_xinput");
	gEnv->pConsole->UnregisterVariable("i_xinput_poll_time");
}