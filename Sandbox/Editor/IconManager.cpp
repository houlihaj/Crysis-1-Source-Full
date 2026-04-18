////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   IconManager.cpp
//  Version:     v1.00
//  Created:     24/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "IconManager.h"

#include <I3DEngine.h>

#define HELPER_MATERIAL "Editor/Objects/Helper"

namespace
{
	// Object names in this array must correspond to EObject enumeration.
	const char *ObjectNames[STATOBJECT_LAST] =
	{
		"Editor/Objects/Arrow.cgf",
		"Editor/Objects/Axis.cgf",
		"Editor/Objects/Sphere.cgf",
		"Editor/Objects/Anchor.cgf",
		"Editor/Objects/entrypoint.cgf",
		"Editor/Objects/hidepoint.cgf",
		"Editor/Objects/hidepoint_sec.cgf",
		"Editor/Objects/reinforcement_point.cgf",
	};

	const char *IconNames[ICON_LAST] =
	{
		"Editor/Icons/quad.tga",
	};
};

//////////////////////////////////////////////////////////////////////////
CIconManager::CIconManager()
{
	ZeroStruct( m_icons );
	ZeroStruct( m_objects );
}

//////////////////////////////////////////////////////////////////////////
CIconManager::~CIconManager()
{
	/*
	std::vector<int> ids;
	m_textures.GetAsVector( ids );
	for (int i = 0; i < ids.size(); i++)
	{
	}
	*/
}


//////////////////////////////////////////////////////////////////////////
void CIconManager::Init()
{
}

//////////////////////////////////////////////////////////////////////////
void CIconManager::Done()
{
}

//////////////////////////////////////////////////////////////////////////
void CIconManager::Reset()
{
	I3DEngine *pEngine = GetIEditor()->Get3DEngine();
	// Do not unload objects. but clears them.
	int i;
	for (i = 0; i < sizeof(m_objects)/sizeof(m_objects[0]); i++)
	{
		if (m_objects[i] && pEngine)
			m_objects[i]->Release();
		m_objects[i] = 0;
	}
	for (i = 0; i < ICON_LAST; i++)
	{
		m_icons[i] = 0;
	}

	// Free icon bitmaps.
	for (IconsMap::iterator it = m_iconBitmapsMap.begin(); it != m_iconBitmapsMap.end(); ++it)
	{
		delete it->second;
	}
}

//////////////////////////////////////////////////////////////////////////
int CIconManager::GetIconTexture( const CString &iconName )
{
	int id = 0;
	if (m_textures.Find( iconName,id ))
	{
		return id;
	}

	CImage image;
	// Load icon.
	if (CImageUtil::LoadImage( iconName,image ))
	{
		IRenderer* pRenderer(GetIEditor()->GetRenderer());
		if (pRenderer->GetRenderType() != eRT_DX10)
			image.SwapRedAndBlue();
		CString ext = Path::GetExt(iconName);
		if (stricmp(ext,"bmp") == 0 || stricmp(ext,"jpg") == 0)
		{
			int sz = image.GetWidth()*image.GetHeight();
			int h = image.GetHeight();
			uint8 *buf = (uint8*)image.GetData();
			for (int i = 0; i < sz; i++)
			{
				uint32 alpha = max( max( buf[i*4],buf[i*4+1] ),buf[i*4+2] );
				alpha *= 2;
				buf[i*4+3] = (alpha > 255) ? 255 : alpha;
			}
		}

		id = pRenderer->DownLoadToVideoMemory( (unsigned char*)image.GetData(),image.GetWidth(),image.GetHeight(),eTF_A8R8G8B8,eTF_A8R8G8B8,0,0,0 );
		m_textures[iconName] = id;
	}
	return id;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* 	CIconManager::GetHelperMaterial()
{
	if (!m_pHelperMtl)
		m_pHelperMtl = GetIEditor()->Get3DEngine()->GetMaterialManager()->LoadMaterial( HELPER_MATERIAL );
	return m_pHelperMtl;
};

//////////////////////////////////////////////////////////////////////////
IStatObj*	CIconManager::GetObject( EObject object )
{
	assert( object >= 0 && object < STATOBJECT_LAST);

	if (m_objects[object])
		return m_objects[object];

	// Try to load this object.
	m_objects[object] = GetIEditor()->Get3DEngine()->LoadStatObj( Path::MakeFullPath(ObjectNames[object]) );
	if (!m_objects[object])
	{
		CLogFile::FormatLine( "Error: Load Failed: %s",ObjectNames[object] );
	}
	m_objects[object]->AddRef();
	if (GetHelperMaterial())
		m_objects[object]->SetMaterial( GetHelperMaterial() );
	return m_objects[object];
}

//////////////////////////////////////////////////////////////////////////
int CIconManager::GetIcon( EIcon icon )
{
	assert( icon >= 0 && icon < ICON_LAST );
	if (m_icons[icon])
		return m_icons[icon];

	int id = 0;
	// Try to load this Icon.
	ITexture *pPic = GetIEditor()->GetRenderer()->EF_LoadTexture( Path::MakeFullPath(IconNames[icon]),FT_DONT_RELEASE,eTT_2D);

	m_icons[icon] = pPic->GetTextureID();
	if (!m_icons[icon])
	{
		CLogFile::FormatLine( "Error: Load Failed: %s",IconNames[icon] );
	}
	return m_icons[icon];
}

//////////////////////////////////////////////////////////////////////////
CBitmap* CIconManager::GetIconBitmap( const CString &filename )
{
	CBitmap *pBitmap = stl::find_in_map( m_iconBitmapsMap,filename,NULL );
	if (pBitmap)
		return pBitmap;

	HBITMAP hBmp = (HBITMAP)::LoadImage( AfxGetInstanceHandle(), filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE|LR_DEFAULTSIZE ); 
	if (hBmp)
	{
		pBitmap = new CBitmap;
		pBitmap->Attach(hBmp);
		m_iconBitmapsMap[filename] = pBitmap;
		return pBitmap;
	}
	return NULL;
}
