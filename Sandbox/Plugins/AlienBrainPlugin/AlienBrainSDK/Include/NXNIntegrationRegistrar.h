// \addtodoc
#ifndef _NXNINTEGRATIONREGISTRAR_H_
#define _NXNINTEGRATIONREGISTRAR_H_

/*! \file NXNIntegrationRegistrar.h
*  NXNIntegrationRegistrar.h contains declaration of class CNXNIntegrationRegistrar.
*/

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
#define NXNINTEGRATIONREGISTRAR_EXPORTS __declspec(dllexport)
#else
#define NXNINTEGRATIONREGISTRAR_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long IntegrationRegistrarNoError                     = 0L;
const unsigned long IntegrationRegistrarInternalError               = 1L;
const unsigned long IntegrationRegistrarInvalidEnvironmentNameError = 2L;
const unsigned long IntegrationRegistrarInvalidObserverPointerError = 3L;
const unsigned long IntegrationRegistrarNotRegisteredError          = 4L;
const unsigned long IntegrationRegistrarRegisteredTwiceError        = 5L;

class CNXNIntegrationObserver;
class CNXNIntegrationRegistrarImpl;

/*!	\class		CNXNIntegrationRegistrar NXNIntegrationRegistrar.h	
 *	\brief		This class provides functionality for NXN alienbrain integration registration.
 *				
 *              <p>
 *				CNXNIntegrationRegistrar is the access point for NXN alienbrain integration registration.
 *              In order to integrate NXN alienbrain functionality into an application, the first step
 *              is to register the application as an NXN alienbrain integration by calling RegisterIntegration().<p><br>
 *
 *              For registration, a pointer to a custom observer instance must be passed. See 
 *              CNXNIntegrationObserver for more details on how to implement a custom observer.<p><br>
 *
 *              This class is implemented as a <a href="WIF-Introduction.html#Singleton">singleton</a>. To get 
 *              access to the singleton instance, simply call method CNXNIntegrationService::IntegrationRegistrar(). 
 *              The singleton instance mustn't be deleted - the Windows Integration Framework will take care of 
 *              freeing memory allocated for the singleton instance.
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003
*/
class NXNINTEGRATIONREGISTRAR_EXPORTS CNXNIntegrationRegistrar : public CNxNObject
{
    // CNXNIntegrationRegistrar is implemented as singleton which
    // is managed by class CNXNIntegrationServiceImpl
    friend class CNXNIntegrationServiceImpl;

	public:

        // mutator methods
        bool RegisterIntegration( const CNxNString& environmentName, CNXNIntegrationObserver* integrationObserver ) const;

        // accessor methods
        unsigned long               GetLastError( void ) const;
        CNxNString                  GetLastErrorMessage( void ) const;
        bool                        IsIntegrationRegistered( void ) const;
        void                        GetEnvironmentName( CNxNString& environmentName ) const;
        CNXNIntegrationObserver*    GetIntegrationObserver( void ) const;
		
	protected:
		
	private:

        // pointer to implementation class
        CNXNIntegrationRegistrarImpl* m_pImp;

        CNXNIntegrationRegistrar();
        ~CNXNIntegrationRegistrar();
	
		CNXNIntegrationRegistrar( const CNXNIntegrationRegistrar& src );
		CNXNIntegrationRegistrar& operator=( const CNXNIntegrationRegistrar& rhs );
};

#endif // _NXNINTEGRATIONREGISTRAR_H_

