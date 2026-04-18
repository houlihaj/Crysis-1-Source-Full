#pragma once

#ifndef WINVER
#define WINVER 0x0400
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <afxwin.h>     
#include <afxext.h>     

#include <afxdtctl.h>		
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			
#endif // _AFX_NO_AFXCMN_SUPPORT



//////////////////////////////////////////////////////////////////////////
// C runtime lib includes
//////////////////////////////////////////////////////////////////////////
#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>


/////////////////////////////////////////////////////////////////////////////
// STL
/////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <list>
#include <map>	
#include <set>
#include <algorithm>

/////////////////////////////////////////////////////////////////////////////
// CRY Stuff ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#define NOT_USE_CRY_MEMORY_MANAGER
#include <platform.h>
#include <Cry_Math.h>
//#include <Cry_Angle3.h>
#include <Cry_Camera.h>
#include <Range.h>
#include <StlUtils.h>

#include <smartptr.h>
#define TSmartPtr _smart_ptr
#define SMARTPTR_TYPEDEF(Class) typedef _smart_ptr<Class> Class##Ptr

#include "ISystem.h"
#define SANDBOX_API
#include "Util/BBox.h"
#include "Util/EditorUtils.h"
#ifndef RGBA8
#define RGBA8(r,g,b,a)     ((uint32)(((uint8)(r)|((uint16)((uint8)(g))<<8))|(((uint32)(uint8)(b))<<16)) | (((uint32)(uint8)(a))<<24) )
#endif
#include "Settings.h"
#include "IXml.h"
#include "IEditor.h"

#define CRYEDIT_API