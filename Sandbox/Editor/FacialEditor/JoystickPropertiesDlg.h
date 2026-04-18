////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   JoystickPropertiesDlg.h
//  Version:     v1.00
//  Created:     9/2/2007 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __JOYSTICKPROPERTIES_H__
#define __JOYSTICKPROPERTIES_H__

#pragma once

#include "resource.h"

class CJoystickPropertiesDlg : public CDialog
{
	DECLARE_DYNAMIC(CJoystickPropertiesDlg)

public:
	CJoystickPropertiesDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CJoystickPropertiesDlg();

	void SetChannelName(int axis, const string& channelName);
	void SetChannelFlipped(int axis, bool flipped);
	bool GetChannelFlipped(int axis) const;
	void SetVideoScale(int axis, float offset);
	float GetVideoScale(int axis) const;
	void SetVideoOffset(int axis, float offset);
	float GetVideoOffset(int axis) const;

	void SetChannelEnabled(int axis, bool enabled);

	// Dialog Data
	enum { IDD = IDD_FACEED_JOYSTICK_PROPERTIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	BOOL OnInitDialog();
	virtual void OnOK();

	CEdit m_nameEdits[2];
	CNumberCtrl m_scaleEdits[2];
	CButton m_flippedButtons[2];
	CNumberCtrl m_offsetEdits[2];

	string m_names[2];
	float m_scales[2];
	bool m_flippeds[2];
	float m_offsets[2];
	bool m_enableds[2];
};

#endif //__JOYSTICKPROPERTIES_H__
