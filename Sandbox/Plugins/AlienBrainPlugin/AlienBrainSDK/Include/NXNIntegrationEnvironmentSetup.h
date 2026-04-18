// \addtodoc
#ifndef _NXNINTEGRATIONENVIRONMENTSETUP_H_
#define _NXNINTEGRATIONENVIRONMENTSETUP_H_

/*! \file NXNIntegrationEnvironmentSetup.h
*  NXNIntegrationEnvironmentSetup.h contains declaration of class CNXNIntegrationEnvironmentSetup.
*/

#include "NXNIntegrationTypes.h"

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
#define NXNINTEGRATIONENVIRONMENTSETUP_EXPORTS __declspec(dllexport)
#else
#define NXNINTEGRATIONENVIRONMENTSETUP_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long IntegrationEnvironmentNoError                   = 0L;
const unsigned long IntegrationEnvironmentInternalError             = 1L;
const unsigned long IntegrationEnvironmentInvalidProperty           = 2L;
const unsigned long IntegrationEnvironmentInvalidSetting            = 3L;
const unsigned long IntegrationEnvironmentInvalidSettingValue       = 4L;
const unsigned long IntegrationEnvironmentInvalidConnectPolicy      = 5L;
const unsigned long IntegrationEnvironmentInvalidAssertReportMode   = 6L;
const unsigned long IntegrationEnvironmentPropertySetTwiceError     = 7L;
const unsigned long IntegrationEnvironmentInvalidPropertyValue      = 8L;

class CNXNIntegrationEnvironmentSetupImpl;         

/*!	\class		CNXNIntegrationEnvironmentSetup NXNIntegrationEnvironmentSetup.h	
 *	\brief		This class provides access to all integration environment settings.
 *				
 *              <p>
 *				CNXNIntegrationEnvironmentSetup is the access point for setting and 
 *              retrieving environment settings of the integration environment.<p><br>
 *              This class gives access to:
 *              <ul>
 *              <li><b>NXN properties</b> -> SetProperty()/GetProperty()</li>
 *              <li><b>NXN settings</b> -> SetSetting()/GetSetting()</li>
 *              <li><b>NXN connect policy</b> -> SetConnectPolicy()/GetConnectPolicy()</li>
 *              <li><b>Windows Integration Framework assert report mode</b> -> SetAssertReportMode()/GetAssertReportMode()</li>
 *              </ul><p>
 *              NXN properties control different aspects of the Windows Integration Framework behavior. The
 *              properties are a means for informing the framework about the characteristics and needs of the
 *              integration application.<p><br>
 *              NXN settings control the behavior of the Windows Integration Framework when file events occur.
 *              By changing NXN settings, the framework can be configured to either not react on file events, to 
 *              react automatically, or to react by prompting the user.<p><br>              
 *              Access to the NXN connect policy and access to the Windows Integration Framework assert report 
 *              mode are a means for implementing a more "silent" connection to the database. With the help of 
 *              these methods, dialogs can be suppressed.
 *              It is recommended not to redirect the assert report mode to the debug window if you are not
 *              familiar with the Windows Integration Framework. When beginning to use the 
 *              Framework, the assert dialogs will help in awareness of wrong usage and 
 *              unhandled error conditions.
 *              <p><br>
 *              Class CNXNIntegrationEnvironmentSetup is implemented as a 
 *              <a href="WIF-Introduction.html#Singleton">singleton</a>. To get access to the singleton instance, 
 *              method CNXNIntegrationService::EnvironmentSetup() must be called. The singleton  instance mustn't 
 *              be deleted - the Windows Integration Framework will take care of freeing memory allocated for the 
 *              singleton instance.
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003 
*/
class NXNINTEGRATIONENVIRONMENTSETUP_EXPORTS CNXNIntegrationEnvironmentSetup : public CNxNObject
{
        // CNXNIntegrationEnvironmentSetup is implemented as singleton which
        // is managed by class CNXNIntegrationServiceImpl
        friend class CNXNIntegrationServiceImpl;

	public:
        
        // mutator methods
        bool SetProperty( WIF::NXNProperty property, const CNxNString& propValue );
        bool SetSetting( WIF::NXNSetting setting, WIF::NXNSettingValue value );        
        bool SetConnectPolicy( WIF::NXNConnectPolicySetting connectPolicy );
        bool SetAssertReportMode( WIF::NXNAssertReportModeSetting reportMode );        

        // accessor methods
        unsigned long                   GetLastError( void ) const;
        CNxNString                      GetLastErrorMessage( void ) const;
        bool                            GetProperty( WIF::NXNProperty property, const CNxNString& defaultValue, CNxNString& value ) const;
        bool                            GetSetting( WIF::NXNSetting setting, WIF::NXNSettingValue& value ) const;        
        WIF::NXNConnectPolicySetting    GetConnectPolicy( void ) const;
        WIF::NXNAssertReportModeSetting GetAssertReportMode( void ) const;        
		
	protected:
        
	private:

        // pointer to implementation class
        CNXNIntegrationEnvironmentSetupImpl* m_pImp;

        CNXNIntegrationEnvironmentSetup();
        ~CNXNIntegrationEnvironmentSetup();
	
		CNXNIntegrationEnvironmentSetup( const CNXNIntegrationEnvironmentSetup& src );
		CNXNIntegrationEnvironmentSetup& operator=( const CNXNIntegrationEnvironmentSetup& rhs );
};

#endif // _NXNINTEGRATIONENVIRONMENTSETUP_H_
