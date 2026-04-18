#ifndef INC_NXN_INTEGRATORSDK_H
#define INC_NXN_INTEGRATORSDK_H

/*! \file NXNIntegrationIncludesIntegratorSDK.h
 *  NXNIntegrationIncludesIntegratorSDK.h contains includes for all Integrator SDK include files.
 *  <b>Important note:</b> when using the Windows Integration Framework, include file "NxNIntegratorSDK.h"
 *  shouldn't be directly included: directly including file "NxNIntegratorSDK.h" might lead to the effect that 
 *  the application is linked against the debug and the release version of the Integrator SDK library.<p><br>
 *  Instead header file "NXNIntegrationIncludes.h" should be included. By including this header file, all 
 *  needed Windows Integration Framework and Integrator SDK headers will be included, and it will be ensured 
 *  that the application will be linked against the correct libraries.
 */

#include "NxNVersion.h"

#pragma warning( disable : 4786 )

#include "NxNFwdDecl.h"
#include "NxNClassList.h"

#include "NxNErrors.h"
#include "NxNExport.h"
#include "NxNConstants.h"

#include "NxNArray.h"
#include "NxNString.h"
#include "NxNCString.h"
#include "NxNError.h"
#include "NxNParam.h"
#include "NxNPath.h"

#include "NxNObject.h"
#include "NxNIterator.h"

#include "NxNProperty.h"
#include "NxNPropertyCollection.h"
#include "NxNPathCollection.h"
#include "NxNCommand.h"
#include "NxNVarCommand.h"
#include "NxNType.h"
#include "NxNBrowseFilter.h"
#include "NxNNode.h"
#include "NxNIntegrator.h"
#include "NxNIntegratorFactory.h"
#include "NxNResponse.h"
#include "NxNMenu.h"
#include "NxNMapper.h"
#include "NxNTime.h"

#include "NxNVirtualNode.h"
#include "NxNDbNode.h"
#include "NxNNodeList.h"

#include "NxNDbNodeList.h"
#include "NxNFile.h"
#include "NxNDbFolder.h"
#include "NxNWorkspace.h"
#include "NxNVersionControlItem.h"
#include "NxNDiskItem.h"

#include "NxNUserManagementItem.h"
#include "NxNUser.h"
#include "NxNUserGroup.h"

#include "NxNFolder.h"
#include "NxNProject.h"
#include "NxNHistory.h"
#include "NxNHistoryLabel.h"
#include "NxNHistoryVersion.h"
#include "NxNDiskFile.h"
#include "NxNDiskFolder.h"
#include "NxNDiskItemList.h"

#include "NxNGlobalSelection.h"

#include "NxNEventMsg.h"
#include "NxNEventTarget.h"
#include "NxNEventManager.h"

#include "NxNConstants.h"									

#include "NxNItem.h"
#include "NxNItemCollection.h"

#include "NxNEnvironment.h"

DECLARE_INTERNAL_CLASSLIST()

#endif // INC_NXN_INTEGRATORSDK_H
