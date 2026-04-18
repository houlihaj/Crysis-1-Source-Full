// \addtodoc
#ifndef _NXNINTEGRATIONOBSERVER_H_
#define _NXNINTEGRATIONOBSERVER_H_

#include "NXNIntegrationTypes.h"
#include "NXNReferenceVector.h"

/*! \file NXNIntegrationObserver.h
*  NXNIntegrationObserver.h contains declaration of class CNXNIntegrationObserver.
*/

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
#define NXNINTEGRATIONOBSERVER_EXPORTS __declspec(dllexport)
#else
#define NXNINTEGRATIONOBSERVER_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long IntegrationObserverNoError              = 0L;
const unsigned long IntegrationObserverInternalError        = 1L;
const unsigned long IntegrationObserverInvalidFileNameError = 2L;

class CNXNIntegrationObserverImpl;


/*!	\class		CNXNIntegrationObserver NXNIntegrationObserver.h	
 *	\brief		This class is the base class for a custom observer class
 *				
 *              <p>
 *				CNXNIntegrationObserver is the base class for a custom observer class. This
 *              class defines the database callback interface which consists of the following
 *              callback methods:<p><br>
 *  
 *              File event related callbacks:<p><br>
 *              <ul>
 *              <li>OpenFile()</li>
 *              <li>SaveFile()</li>
 *              <li>SaveFileAs()</li>
 *              <li>CheckForSave()</li>
 *              </ul><p> 
 *              Reference Manager related callbacks:<p><br>
 *              <ul>
 *              <li>GetReferenceVector()</li>
 *              <li>RemapReference()</li>
 *              </ul><p> 
 *              Screenshot Related callback:<p><br>
 *              <ul>
 *              <li>GetScreenshotFile()</li>
 *              </ul><p> 
 *
 *              Since a pointer to an observer class is needed for integration registration, it is 
 *              mandatory to derive a custom observer class and to implement all callback methods. 
 *              Even if some of the callbacks aren't of interest to the NXN alienbrain integration, these methods 
 *              must be implemented anyway. In this case, the methods might be implemented as empty methods, 
 *              i.e. without functionality. As an example: if an NXN alienbrain integration doesn't support
 *              references, the implementation for methods GetReferenceVector() and RemapReference() might
 *              be a simple "<code>return false;</code>" statement.<p><br>
 *
 *              CNXNIntegrationObserver additionally provides default implementations for OnPre*()
 *              and OnPost*() events. The default behavior implemented by the OnPre*() and OnPost*() methods 
 *              is the same behavior which is implemented for all the integrations shipped by NXN.<p><br>
 *              The default functionality reacts on the current NXN settings which can be controlled 
 *              with method CNXNIntegrationEnvironmentSetup::SetSetting(). The table below lists the 
 *              implemented default behavior assuming that all settings are set to eSettingValueAskUser 
 *              or eSettingValueAlways:<p><br>
 *
 *              <center>
 *              <table>
 *              <tr>
 *                  <td><center>Callback</center></td>
 *                  <td><center>unmanaged file</center></td>
 *                  <td><center>disk file</center></td>
 *                  <td><center>checked out file</center></td>
 *                  <td><center>checked-in file</center></td>
 *              </tr>
 *              <tr>
 *                  <td><center>OnPreOpen</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>check-out</center></td>
 *              </tr>
 *              <tr>
 *                  <td><center>OnPostOpen</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>check-out (*)</center></td>
 *              </tr>
 *              <tr>
 *                  <td><center>OnPreSave</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>prompt check-out</center></td>
 *              </tr>
 *              <tr>
 *                  <td><center>OnPostSave</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>import</center></td>
 *                  <td><center>upload</center></td>
 *                  <td><center>-</center></td>
 *              </tr>
 *              <tr>
 *                  <td><center>OnPreClose</center></td>
 *                  <td><center>-</center></td>
 *                  <td><center>import</center></td>
 *                  <td><center>upload/check-in</center></td>
 *                  <td><center>-</center></td>
 *              </tr>
 *              </table>
 *              <br>
 *              * if OnPreOpen was not called with the filename of the new document
 *              </center><p><br>
 *
 *              Guide for reading this table: for example, the default behavior when closing a disk
 *              file is that the import dialog will come up. In the table this is denoted by the word
 *              "import" in column "disk file" and row "OnPreClose".<br>
 *              
 *              For most NXN alienbrain integrations, this default OnPre*()/OnPost*() functionality
 *              should be appropriate. In order to use this default behavior, calls of the observer's
 *              OnPre*() and OnPost*() methods must be added to the event handlers of the integration 
 *              application. E.g. to implement default close behavior, a call of 
 *              CNXNIntegrationObserver::OnClose() must be added to the OnCloseDocument() event handler 
 *              of the integration application.<p><br>
 *
 *              <b>Some important notes on memory management and the observer class</b>: Even though a 
 *              pointer to a custom observer class must be passed when registering the integration, it is 
 *              recommended to implement the observer class as a member of the integration application 
 *              class. This will ensure that the custom observer class is properly destroyed when 
 *              the integration application terminates. Allocating the observer class using operator new 
 *              is error prone and might lead to memory leaks.<br>
 *              <b>Please note: The Windows Integration Framework will be left in an inconsistent state 
 *              if the observer instance isn't properly destroyed on integration termination</b>.
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003 
*/
class NXNINTEGRATIONOBSERVER_EXPORTS CNXNIntegrationObserver : public CNxNObject
{
	public:

        // accessor methods
        unsigned long   GetLastError( void ) const;
        CNxNString      GetLastErrorMessage( void ) const;

        // database dialog callbacks
        virtual bool    OpenFile( const CNxNPath& fileName ) = 0;
        virtual bool    SaveFile( void ) = 0;
        virtual bool    SaveFileAs( const CNxNPath& fileName ) = 0;
        virtual bool    CheckForSave( void ) = 0;
        virtual bool    GetReferenceVector( WIF::NXNReferenceType referenceType, CNXNReferenceVector& references ) = 0;
        virtual bool    RemapReference( const CNxNPath& originalPath, const CNxNPath& newPath ) = 0;
        virtual bool    GetScreenshotFile( CNxNPath& fileName ) = 0;

        // default callback handlers for file operations open, save and close
        virtual bool    OnPreOpen( const CNxNPath& currentFile );
        virtual bool    OnPostOpen( const CNxNPath& currentFile );

        virtual bool    OnPreSave( const CNxNPath& currentFile );
        virtual bool    OnPostSave( const CNxNPath& currentFile );

        virtual bool    OnPreClose( const CNxNPath& currentFile );
		
	protected:

        CNXNIntegrationObserver();
        virtual ~CNXNIntegrationObserver();
		
	private:

        // pointer to implementation class
        CNXNIntegrationObserverImpl* m_pImp;
	
        CNXNIntegrationObserver( const CNXNIntegrationObserver& src );
		CNXNIntegrationObserver& operator=( const CNXNIntegrationObserver& rhs );
};


#endif // _NXNINTEGRATIONOBSERVER_H_
