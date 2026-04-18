/*=============================================================================
  ShadowUtils.h : 
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Tiago Sousa
=============================================================================*/

#ifndef __WATERUTILS_H__
#define __WATERUTILS_H__

class CWaterSim;

// todo. refactor me - should be more general

class CWater
{
public:

  CWater(): m_pWaterSim(0)
  {
  }

  ~CWater()
  {
    Release();
  }

  // Create/Initialize simulation  
  void Create(float fA, float fWind, float fWindScale, float fWorldSizeX, float fWorldSizeY );
  void Release();
  void SaveToDisk(const char *pszFileName);

  // Update water simulation
  void Update(float fTime, bool bOnlyHeight = false);

  // Get water simulation data
  Vec3 GetPositionAt(int x, int y) const;
  float GetHeightAt(int x, int y) const;
  Vec4 *GetDisplaceGrid() const;

  const int GetGridSize() const;

private:

  CWaterSim *m_pWaterSim;
};

#endif