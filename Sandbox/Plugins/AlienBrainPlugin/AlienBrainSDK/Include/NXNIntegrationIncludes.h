// this ifndef check ensures that the pragma comment ( lib, "..." )  
// statement will be issued only once
#ifndef ALIENBRAIN_LIB_IMPORT
#define ALIENBRAIN_LIB_IMPORT

// this pragmas ensure that any application using the windows integration
// framework links agains the NxN_alienbrain_WIF and NxN_alienbrain_XDK libraries

#ifndef NXNINTEGRATIONSTOOLS_EXPORTS

#if _MSC_VER == 1200  // Visual Studio 6.0
    #pragma comment( lib, "NxN_alienbrain_WIF_VS60_128.lib" )
    #pragma comment( lib, "NxN_alienbrain_XDK_VS60_128.lib" )
#else
    #pragma comment( lib, "NxN_alienbrain_WIF_128.lib" )
    #pragma comment( lib, "NxN_alienbrain_XDK_128.lib" )
#endif

#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

#endif  // ALIENBRAIN_LIB_IMPORT

#ifndef _NXNINTEGRATIONINCLUDES_H_
#define _NXNINTEGRATIONINCLUDES_H_

/*! \file NXNIntegrationIncludes.h
 *  NXNIntegrationIncludes.h contains includes for all Windows Integration Framework include files.
*/

#include "NXNIntegrationIncludesIntegratorSDK.h"
#include "NXNIntegrationTypes.h"
#include "NXNIntegrationTypesLimits.h"
#include "NXNIntegrationService.h"
#include "NXNReferenceVector.h"
#include "NXNIntegrationObserver.h"
#include "NXNIntegrationRegistrar.h"
#include "NXNIntegrationUITools.h"
#include "NXNIntegrationEnvironmentSetup.h"

#endif // _NXNINTEGRATIONINCLUDES_H_
