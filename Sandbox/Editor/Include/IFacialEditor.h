////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   IFacialEditor.h
//  Version:     v1.00
//  Created:     30/11/2006 by MichaelS
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __IFACIALEDITOR_H__
#define __IFACIALEDITOR_H__

class IFacialEditor
{
public:
	enum EyeType
	{
		EYE_LEFT,
		EYE_RIGHT
	};

	virtual int GetNumMorphTargets() const = 0;
	virtual const char* GetMorphTargetName(int index) const = 0;
	virtual void PreviewEffector(int index, float value) = 0;
	virtual void ClearAllPreviewEffectors() = 0;
	virtual void SetForcedNeckRotation(const Quat& rotation) = 0;
	virtual void SetForcedEyeRotation(const Quat& rotation, EyeType eye) = 0;
	virtual int GetJoystickCount() const = 0;
	virtual const char* GetJoystickName(int joystickIndex) const = 0;
	virtual void SetJoystickPosition(int joystickIndex, float x, float y) = 0;
	virtual void GetJoystickPosition(int joystickIndex, float& x, float& y) const = 0;
	virtual void LoadJoystickFile(const char* filename) = 0;
	virtual void LoadCharacter(const char* filename) = 0;
	virtual void LoadSequence(const char* filename) = 0;
	virtual void SetVideoFrameResolution(int width, int height, int bpp) = 0;
	virtual int GetVideoFramePitch() = 0;
	virtual void* GetVideoFrameBits() = 0;
	virtual void ShowVideoFramePane() = 0;
};

#endif //__IFACIALEDITOR_H__
