// \addtodoc
#ifndef _NXNINTEGRATIONUITOOLS_H_
#define _NXNINTEGRATIONUITOOLS_H_

/*! \file NXNIntegrationUITools.h
*  NXNIntegrationUITools.h contains declaration of class CNXNIntegrationUITools.
*/

#ifdef NXNINTEGRATIONSTOOLS_EXPORTS
#define NXNINTEGRATIONUITOOLS_EXPORTS __declspec(dllexport)
#else
#define NXNINTEGRATIONUITOOLS_EXPORTS __declspec(dllimport)
#endif  // NXNINTEGRATIONSTOOLS_EXPORTS

//
// error code constants
//
const unsigned long IntegrationUIToolsNoError                       = 0L;
const unsigned long IntegrationUIToolsInternalError                 = 1L;
const unsigned long IntegrationUIToolsInvalidFilePathError          = 2L;
const unsigned long IntegrationUIToolsInvalidFilePathSelectionError = 3L;

class CNXNIntegrationUIToolsImpl;

/*!	\class		CNXNIntegrationUITools NXNIntegrationUITools.h
 *	\brief		This class provides functionality for launching database dialogs.
 *				
 *              <p>
 *				CNXNIntegrationUITools is the access point for UI related services, i.e.
 *              it provides functionality for launching database dialogs. The following
 *              database dialogs are supported:<p>
 *              
 *              <ul>
 *              <li><b>Import To Database dialog</b> -> ShowImportToDatabaseDialog()</li>
 *              <li><b>Open From Database dialog</b> -> ShowOpenFromDatabaseDialog()</li>
 *              <li><b>Database Explorer dialog</b> -> ShowDatabaseExplorer()</li>
 *              <li><b>File History dialog</b> -> ShowFileHistoryDialog()</li>
 *              <li><b>File Properties dialog</b> -> ShowFilePropertiesDialog()</li>
 *              <li><b>NXN Settings dialog</b> -> ShowSettingsDialog()</li>
 *              <li><b>Reference Manager</b> -> ShowReferenceManager()</li>
 *              </ul><p>
 *              
 *              This class is implemented as a <a href="WIF-Introduction.html#Singleton">singleton</a>. To get 
 *              access to the singleton instance, simply call method CNXNIntegrationService::UiTools(). The 
 *              singleton instance mustn't be deleted; the Windows Integration Framework will take care of 
 *              freeing memory allocated for the singleton instance. 
 *	
 *	\author		Robert Meisenecker, (c) 2003 by NxN Software AG
 *	\version	1.00
 *	\date		2003  
*/
class NXNINTEGRATIONUITOOLS_EXPORTS CNXNIntegrationUITools : public CNxNObject
{
        // CNXNIntegrationUITools is implemented as singleton which
        // is managed by class CNXNIntegrationServiceImpl
        friend class CNXNIntegrationServiceImpl;

	public:	

        // methods for access to error codes/messages
        unsigned long   GetLastError( void ) const;
        CNxNString      GetLastErrorMessage( void ) const;
		
        // methods for launching database dialogs
        bool ShowImportToDatabaseDialog( CNxNPath& selectedFilePath, HWND parentWindow = 0 ) const;
        bool ShowOpenFromDatabaseDialog( CNxNPath& selectedFilePath, HWND parentWindow = 0 ) const;
        bool ShowDatabaseExplorer( HWND parentWindow = 0 ) const;
        bool ShowFileHistoryDialog( const CNxNPath& filePath, HWND parentWindow = 0 ) const;
        bool ShowFilePropertiesDialog( const CNxNPath& filePath, HWND parentWindow = 0 ) const;
        bool ShowSettingsDialog( HWND parentWindow = 0 ) const;
        bool ShowReferenceManager( const CNxNPath& filePath, HWND parentWindow = 0 ) const;

	protected:
		
	private:

        // pointer to implementation class
        CNXNIntegrationUIToolsImpl* m_pImp;

        CNXNIntegrationUITools();
        ~CNXNIntegrationUITools();
	
		CNXNIntegrationUITools( const CNXNIntegrationUITools& src );
		CNXNIntegrationUITools& operator=( const CNXNIntegrationUITools& rhs );
};


#endif // _NXNINTEGRATIONUITOOLS_H_
