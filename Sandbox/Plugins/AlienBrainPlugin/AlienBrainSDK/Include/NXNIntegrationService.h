// \addtodoc
#ifndef _NXNINTEGRATIONSERVICE_H_
#define _NXNINTEGRATIONSERVICE_H_

/*! \file NXNIntegrationService.h
*  NXNIntegrationService.h contains declaration of class CNXNIntegrationService.
*/

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
    #define NXNINTEGRATIONSERVICE_EXPORTS __declspec(dllexport)
#else
    #define NXNINTEGRATIONSERVICE_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long IntegrationServiceNoError                       = 0L;
const unsigned long IntegrationServiceInternalError                 = 1L;
const unsigned long IntegrationServiceAccessBeforeRegistrationError = 2L;

class CNxNMapper;
class CNXNIntegrationUITools;
class CNXNIntegrationRegistrar;
class CNXNIntegrationServiceImpl;
class CNXNIntegrationEnvironmentSetup;

/*!	\class		CNXNIntegrationService NXNIntegrationService.h	
 *	\brief		This class is the main access point to all services of the Windows Integration Framework.
 *				
 *              <p>
 * 				CNXNIntegrationService is the main access point to all services of the Windows Integration
 *              Framework. The following services are available:<p>
 *
 *              <ul>
 *              <li>IntegrationRegistrar() -> see also CNXNIntegrationRegistrar</li>
 *              <li>UiTools() -> see also CNXNIntegrationUITools</li>
 *              <li>EnvironmentSetup() -> see also CNXNIntegrationEnvironmentSetup</li>
 *              <li>XDKMapper() -> see also Integrator SDK documentation</li>
 *              </ul><p>
 *
 *              All services are implemented as <a href="WIF-Introduction.html#Singleton">singletons</a>. To get 
 *              access to the service singleton instances, simply call the method corresponding to the service. 
 *              None of the service singleton instances must be deleted; the Windows Integration Framework will 
 *              take care of freeing the allocated memory.<p><br>
 *
 *              CNXNIntegrationService itself is implemented as a singleton as well. To get access to the 
 *              singleton instance, simply call method CNXNIntegrationService::Instance(). Again: the 
 *              CNXNIntegrationService singleton instance mustn't be deleted; the Windows Integration 
 *              Framework will take care of freeing the allocated memory.
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003
 */
class NXNINTEGRATIONSERVICE_EXPORTS CNXNIntegrationService : public CNxNObject
{
        // this class takes care of freeing this singleton instance
        friend class CNXNIntegrationGarbageCollector;

	public:

        // method for singleton access
        static CNXNIntegrationService*      Instance( void );

        // methods for access to error codes/messages
        unsigned long                       GetLastError( void ) const;
        CNxNString                          GetLastErrorMessage( void ) const;

        // methods for access to Windows Integration Framework services
        CNXNIntegrationRegistrar*           IntegrationRegistrar( void );
        CNXNIntegrationUITools*             UiTools( void );
        CNXNIntegrationEnvironmentSetup*    EnvironmentSetup( void );
        CNxNMapper*                         XDKMapper( void );        
		
	protected:
		
	private:

        // pointer to CNXNIntegrationService singleton instance
        static CNXNIntegrationService*  m_pThis;
        // pointer to implementation class
        CNXNIntegrationServiceImpl*     m_pImp;

        CNXNIntegrationService();
        ~CNXNIntegrationService();
	
		CNXNIntegrationService( const CNXNIntegrationService& src );
		CNXNIntegrationService& operator=( const CNXNIntegrationService& rhs );
};


#endif // _NXNINTEGRATIONSERVICE_H_
