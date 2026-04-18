#ifndef __INPUTCVARS_H__
#define __INPUTCVARS_H__

class CInputCVars
{
public:
	int		i_debug;
	int		i_forcefeedback;

	int		i_mouse_buffered;
	float	i_mouse_accel;
	float	i_mouse_accel_max;
	float	i_mouse_smooth;
	float	i_mouse_inertia;

	int		i_bufferedkeys;

	int		i_xinput;
	int		i_xinput_poll_time;

	CInputCVars();
	~CInputCVars();
};

extern CInputCVars* g_pInputCVars;
#endif //__INPUTCVARS_H__
