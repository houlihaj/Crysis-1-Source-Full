////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   inplacebutton.h
//  Version:     v1.00
//  Created:     6/6/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __inplacebutton_h__
#define __inplacebutton_h__

#if _MSC_VER > 1000
#pragma once
#endif

// CInPlaceButton
#include <XTToolkitPro.h>
#include "ColorButton.h"

//////////////////////////////////////////////////////////////////////////
class CInPlaceButton : public CXTButton
{
	DECLARE_DYNAMIC(CInPlaceButton)

public:
	typedef Functor0 OnClick;

	CInPlaceButton( OnClick onClickFunctor ) { m_onClick = onClickFunctor; }

	// Simuale on click.
	void Click() { OnBnClicked();	}

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClicked()
	{
		if (m_onClick)
			m_onClick();
	}

	OnClick m_onClick;
};

//////////////////////////////////////////////////////////////////////////
class CInPlaceColorButton : public CColorButton
{
	DECLARE_DYNAMIC(CInPlaceColorButton)

public:
	typedef Functor0 OnClick;

	CInPlaceColorButton( OnClick onClickFunctor ) { m_onClick = onClickFunctor; }

	// Simuale on click.
	void Click() { OnBnClicked();	}

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClicked()
	{
		if (m_onClick)
			m_onClick();
	}

	OnClick m_onClick;
};

//////////////////////////////////////////////////////////////////////////
class CInPlaceCheckBox : public CButton
{
	DECLARE_DYNAMIC(CInPlaceCheckBox)

public:
	typedef Functor0 OnClick;

	CInPlaceCheckBox( OnClick onClickFunctor ) { m_onClick = onClickFunctor; }

	// Simuale on click.
	void Click() { OnBnClicked();	}

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClicked()
	{
		if (m_onClick)
			m_onClick();
	}

	OnClick m_onClick;
};

#endif // __inplacebutton_h__