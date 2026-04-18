#ifndef __ANIMATIONIMAGEMANAGER_H__
#define __ANIMATIONIMAGEMANAGER_H__

#include "AnimationImageGenerator.h"

class CAGState;
class CAnimationGraph;
class CAnimationImage : public _reference_target_t
{
	friend class CAnimationImageManager;
public:
	~CAnimationImage();

	Gdiplus::Image* GetImage();

private:
	CAnimationImage(CAnimationImageManager* pManager, const string& animationName, float fSnapshotTime);

	void Invalidate();

	CAnimationImageManager* m_pManager;
	Gdiplus::Bitmap* m_pBitmap;
	string m_animationName;
	float m_fSnapshotTime;
	bool m_imageValid;
};
typedef _smart_ptr<CAnimationImage> CAnimationImagePtr;

class CAnimationImageManager : public _reference_target_t
{
	friend class CAnimationImage;
public:
	typedef std::pair<int, int> ImageSize;

	CAnimationImageManager(CWnd* pParentWindow, const string& modelAssetName, const ImageSize& imageSize);
	~CAnimationImageManager();

	CAnimationImagePtr GetImage(CAnimationGraph* pGraph, CAGState* pState);
	void InvalidateImage(CAGState* pState);
	void InvalidateAllImages();
	Gdiplus::Image* GetDefaultImage();
	const ImageSize& GetImageSize() {return m_imageSize;}
	void SetImageSize(const ImageSize& size);
	void ChangeModelAssetName(const char* modelAssetName);

private:
	Gdiplus::Bitmap* CreateBitmap();
	Gdiplus::Bitmap* CreateImage(const string& animName, float snapshotTime);

	Gdiplus::Image* m_pDefaultImage;
	typedef std::map<CAGState*, CAnimationImagePtr> ImageMap;
	ImageMap m_imageMap;
	ImageSize m_imageSize;
	CAnimationImageGenerator m_imageGenerator;
};

typedef _smart_ptr<CAnimationImageManager> CAnimationImageManagerPtr;

#endif //__ANIMATIONIMAGEMANAGER_H__
