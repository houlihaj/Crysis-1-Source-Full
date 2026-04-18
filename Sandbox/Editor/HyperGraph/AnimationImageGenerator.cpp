#include "stdafx.h"
#include <ICryAnimation.h>
#include "AnimationImageGenerator.h"

CAnimationImageGenerator::CAnimationImageGenerator(CWnd* pParentWindow, const string& modelAssetName)
:	m_modelAssetName(modelAssetName)
{
	m_renderCtrl.Create( pParentWindow,CRect(0, 0, 100, 100),WS_CHILD );
	m_renderCtrl.SetGrid(false);
	m_renderCtrl.SetAxis(false);
	//m_renderCtrl.LoadFile( "Editor/Objects/Sphere.cgf" );
	m_renderCtrl.LoadFile(modelAssetName.c_str());
	m_renderCtrl.SetClearColor( ColorF(0.82f) );
}

void CAnimationImageGenerator::ChangeModelAssetName(const char* modelAssetName)
{
	if ( m_modelAssetName != modelAssetName )
	{
		m_modelAssetName = modelAssetName;
		m_renderCtrl.LoadFile( m_modelAssetName.c_str() );
	}
}

void CAnimationImageGenerator::GetAnimationImage(Gdiplus::Bitmap* pBitmap, const string& animationName, float snapshotTime)
{
	CImage image;

	PrepareCharacterForGrab(animationName, snapshotTime);

	CRect rect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
	m_renderCtrl.MoveWindow( rect );
	//m_renderCtrl.ShowWindow(SW_SHOW);
	m_renderCtrl.GetImageOffscreen( image );
	//m_renderCtrl.GetImage(image);
	//m_renderCtrl.ShowWindow(SW_HIDE);

	unsigned int *pImageData = image.GetData();
	int w = image.GetWidth();
	int h = image.GetHeight();

	Gdiplus::BitmapData bitmapData;

	pBitmap->LockBits(
		0,
		Gdiplus::ImageLockModeWrite,
		PixelFormat32bppARGB,
		&bitmapData);

	UINT* pixels = (UINT*)bitmapData.Scan0;

	for(UINT row = 0; row < bitmapData.Height; ++row)
	{
		for(UINT col = 0; col < bitmapData.Width; ++col)
		{
			unsigned int uPixel = pImageData[row * w + col];
			pixels[row * bitmapData.Stride / 4 + col] = 0xFF000000 | uPixel;
		}
	}

	pBitmap->UnlockBits(&bitmapData);
}

void CAnimationImageGenerator::PrepareCharacterForGrab(const string& animationName, float snapshotTime)
{
	ICharacterInstance *pCharacter = m_renderCtrl.GetCharacter();
	if (!pCharacter)
		return;

	if (pCharacter->GetIAnimationSet()->GetAnimIDByName(animationName.c_str()) >= 0)
	{
		m_renderCtrl.SetShowObject(true);

		CryCharAnimationParams params;
		params.m_nLayerID = 0;
		params.m_fTransTime = 0;
		params.m_nFlags = 0;

		ISkeletonAnim* pISkeletonAnim=pCharacter->GetISkeletonAnim();

		pISkeletonAnim->StartAnimation(animationName.c_str(),0, 0, 0, params);

		CAnimation* pAnimation = &pISkeletonAnim->GetAnimFromFIFO(0, 0);

		// Update the animation time - we have to convert from normalised time to
		// a time in seconds.
		float fProgressInSeconds = snapshotTime * pAnimation->m_LMG0.m_fDurationQQQ[0];
		pISkeletonAnim->SetLayerTime(0, fProgressInSeconds);

		m_renderCtrl.UpdateAnimation();
		m_renderCtrl.SetCameraLookAt( 1.8f,Vec3(0,1.0f,0) );
		m_renderCtrl.SetCameraRadius( 1.8f );
	}
	else
	{
		m_renderCtrl.SetShowObject(false);
	}
}

bool CAnimationImageGenerator::IsAnimationValid(const string& animationName)
{
	ICharacterInstance *pCharacter = m_renderCtrl.GetCharacter();
	if (!pCharacter)
		return false;

	return pCharacter->GetIAnimationSet()->GetAnimIDByName(animationName.c_str()) >= 0;
}
