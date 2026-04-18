////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   ClassDesc.cpp
//  Version:     v1.00
//  Created:     8/11/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ClassDesc.h"
#include "IconManager.h"

//////////////////////////////////////////////////////////////////////////
int CObjectClassDesc::GetTextureIconId()
{
	if (!m_nTextureIcon)
	{
		const char *sTexName = GetTextureIcon();
		if (sTexName && *sTexName != 0)
		{
			m_nTextureIcon = GetIEditor()->GetIconManager()->GetIconTexture( sTexName );
		}
	}
	return m_nTextureIcon;
}