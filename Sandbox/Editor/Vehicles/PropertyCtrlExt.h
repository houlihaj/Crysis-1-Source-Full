////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   PropertyCtrlExt.h
//  Version:     v1.00
//  Created:     19-10-2005 by MichaelR.
//  Compilers:   Visual Studio.NET
//  Description: Subclassed CPropertyCtrl for extensions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __propertyctrlext_h__
#define __propertyctrlext_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Controls/PropertyCtrl.h"


/** 
*/
class CPropertyCtrlExt : public CPropertyCtrl
{
	DECLARE_DYNAMIC(CPropertyCtrlExt)

public:  
  typedef Functor1<CPropertyItem*> PreSelChangeCallback;

  CPropertyCtrlExt();

  void SetPreSelChangeCallback( PreSelChangeCallback &callback ) { m_preSelChangeFunc = callback; }
  void SelectItem( CPropertyItem *item );    
  void OnItemChange( CPropertyItem *item );

  
protected:  
  DECLARE_MESSAGE_MAP()
  afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

  void OnAddChild();
  void OnDeleteChild(CPropertyItem* pItem);
  void OnGetEffect(CPropertyItem* pItem);


  PreSelChangeCallback m_preSelChangeFunc;
  	
};


#endif // __propertyctrlext_h__