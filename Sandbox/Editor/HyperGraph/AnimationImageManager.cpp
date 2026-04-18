#include "stdafx.h"
#include "AnimationImageManager.h"
#include "smartptr.h"
#include "AnimationGraph.h"

CAnimationImage::CAnimationImage(CAnimationImageManager* pManager, const string& animationName, float fSnapshotTime)
:	m_pManager(pManager),
	m_pBitmap(0),
	m_animationName(animationName),
	m_fSnapshotTime(fSnapshotTime),
	m_imageValid(false)
{
}

CAnimationImage::~CAnimationImage()
{
	if (m_pBitmap)
		delete m_pBitmap;
}

Gdiplus::Image* CAnimationImage::GetImage()
{
	if (!m_imageValid)
	{
		m_pBitmap = m_pManager->CreateImage(m_animationName, m_fSnapshotTime);
		m_imageValid = true;
	}

	Gdiplus::Image* pImage = 0;
	if (m_pBitmap)
		pImage = m_pBitmap;
	if (pImage == 0)
		pImage = m_pManager->GetDefaultImage();
	return pImage;
}

void CAnimationImage::Invalidate()
{
	if (m_pBitmap)
	{
		delete m_pBitmap;
		m_pBitmap = 0;
	}
	m_imageValid = false;
}

CAnimationImageManager::CAnimationImageManager(CWnd* pParentWindow, const string& modelAssetName, const ImageSize& imageSize)
:	m_pDefaultImage(0),
	m_imageSize(imageSize),
	m_imageGenerator(pParentWindow, modelAssetName)
{
}

CAnimationImageManager::~CAnimationImageManager()
{
	if (m_pDefaultImage)
		delete m_pDefaultImage;
}

CAnimationImagePtr CAnimationImageManager::GetImage(CAnimationGraph* pGraph, CAGState* pState)
{
	ImageMap::iterator itImage = m_imageMap.find(pState);
	if (itImage == m_imageMap.end())
	{
		TParameterizationId paramId;
		if (pState->IsParameterized())
		{
			const CParamsDeclaration* pParams = pState->GetParamsDeclaration();
			CParamsDeclaration::const_iterator itParams, itParamsEnd = pParams->end();
			for ( itParams = pParams->begin(); itParams != itParamsEnd; ++itParams )
				if ( !itParams->second.empty() )
					paramId[ itParams->first ] = *itParams->second.begin();
		}
		CAGStateAnimationEnumerator animEnumerator(pState, paramId);
		string animName;
		string firstFoundAnimName; // May refer to anim not found in cal file.
		while (!animEnumerator.IsDone())
		{
			string name;
			animEnumerator.Get(name);
			if (m_imageGenerator.IsAnimationValid(name))
			{
				animName = name;
				break;
			}
			firstFoundAnimName = name;
			animEnumerator.Step();
		}
		// If we could not find an animation that is in the cal file, use one that is not in the cal file.
		if (animName.empty())
			animName = firstFoundAnimName;

		if (!animName.empty())
		{
			CAnimationImage* pNewImage = new CAnimationImage(this, animName, pState->GetIconSnapshotTime());
			itImage = m_imageMap.insert(std::make_pair(pState, pNewImage)).first;
		}
	}

	if (itImage != m_imageMap.end())
		return (*itImage).second;
	else
		return CAnimationImagePtr(0);
}

void CAnimationImageManager::InvalidateImage(CAGState* pState)
{
	ImageMap::iterator itImage = m_imageMap.find(pState);
	if (itImage != m_imageMap.end())
	{
		CAnimationImagePtr& pImage = (*itImage).second;
		pImage->m_fSnapshotTime = pState->GetIconSnapshotTime();
		pImage->Invalidate();
	}
}

Gdiplus::Image* CAnimationImageManager::GetDefaultImage()
{
	if (!m_pDefaultImage)
		m_pDefaultImage = CreateBitmap();
	return m_pDefaultImage;
}

void CAnimationImageManager::SetImageSize(const ImageSize& size)
{
	m_imageSize = size;
	InvalidateAllImages();
}

void CAnimationImageManager::InvalidateAllImages()
{
	// Invalidate every icon.
	for (ImageMap::iterator itImageEntry = m_imageMap.begin(); itImageEntry != m_imageMap.end(); ++itImageEntry)
	{
		CAnimationImagePtr pImage = (*itImageEntry).second;
		pImage->Invalidate();
	}
}

Gdiplus::Bitmap* CAnimationImageManager::CreateBitmap()
{
	Gdiplus::Bitmap* pBitmap = new Gdiplus::Bitmap(this->m_imageSize.first, this->m_imageSize.second, PixelFormat32bppARGB);
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
			pixels[row * bitmapData.Stride / 4 + col] = 0xFF000000;
	}

	pBitmap->UnlockBits(&bitmapData);

	return pBitmap;
}

Gdiplus::Bitmap* CAnimationImageManager::CreateImage(const string& animName, float snapshotTime)
{
	Gdiplus::Bitmap* pBitmap = CreateBitmap();
	m_imageGenerator.GetAnimationImage(pBitmap, animName, snapshotTime);
	return pBitmap;
}

void CAnimationImageManager::ChangeModelAssetName(const char* modelAssetName)
{
	m_imageGenerator.ChangeModelAssetName( modelAssetName );
	InvalidateAllImages();
}
