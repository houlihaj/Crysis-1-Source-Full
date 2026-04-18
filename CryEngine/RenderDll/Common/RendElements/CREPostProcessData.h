/*=============================================================================
CREPostProcessData.h : Post processing RenderElement common data
Copyright (c) 2001 Crytek Studios. All Rights Reserved.

Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0
* Created by Tiago Sousa
=============================================================================*/

#ifndef _CREPOSTPROCESSDATA_H_
#define _CREPOSTPROCESSDATA_H_

class CREPostProcessData 
{
public:

  CREPostProcessData()
  {
  };

  ~CREPostProcessData()
  {
    Release();
  };
  
  void Create();
  void Release();
  void Reset();
};


#endif
