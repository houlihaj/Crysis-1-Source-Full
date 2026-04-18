// \addtodoc
#ifndef _NXNINTEGRATIONTYPES_H_
#define _NXNINTEGRATIONTYPES_H_

/*! \file NXNIntegrationTypes.h
 *  NXNIntegrationtypes.h contains all type declarations used by the Windows Integration Framework.
 *  All type declarations are defined within namespace WIF.
*/

/*! Namespace containing all type declarations used by the Windows Integration Framework
*/
namespace WIF
{
    /*! Enum for NXN settings.<br><br>
     *  NXN settings control the default behavior for file operations. There is an 
     *  NXN setting for every possible file operation: open, close and save. Each of
     *  the NXN settings may be set to an individual setting value which may be either
     *  "ask user", "always perform action" or "never perform action".
     */
    enum NXNSetting 
    {   eSettingCheckoutOnOpen          /*!< Setting for check out on open. */
    ,   eSettingCheckinOnClose          /*!< Setting for check in on close. */
    ,   eSettingUpdateOnSave            /*!< Setting for update on save. */
    ,   eSettingImportOnSave            /*!< Setting for import on save. */        
    ,   eSettingAutoSynchReferences     /*!< Setting for synching broken file references. */
    ,   eSettingGetLatestReferences     /*!< Setting for get latest operation on referenced files. */
    ,   eSettingUploadOnScreenshot      /*!< Setting for uploading on create screenshot. */
    ,   eSettingScreenshotOnVersion     /*!< Setting for screenshot creation on new version. */
    };

    /*! Enum for NXN setting values.<br><br>
     * NXN setting values are possible values which NXN settings might be set to.
     */
    enum NXNSettingValue 
    {   eSettingValueAskUser    /*!< Setting value for asking user. */
    ,   eSettingValueAlways     /*!< Setting value for always performing action. */
    ,   eSettingValueNever      /*!< Setting value for never performing action. */
    };
    
    /*! Enum for NXN properties.<br><br>
     *  NXN properties control Windows Integration Framework behavior.
     */
    enum NXNProperty
    {   ePropertySupportsMultipleDocuments          /*!<Property for multiple document support.
                                                     *  <ul>
                                                     *  <li>Setting property to "1" signals: integration is
                                                     *  multiple-document application.</li>
                                                     *  <li>Setting property to "0" signals: integration is
                                                     *  single-document application.</li>
                                                     *  </ul>
                                                     *  This property setting influences the behavior  
                                                     *  of the default implementation of methods 
                                                     *  CNXNIntegrationObserver::OnPre*() and 
                                                     *  CNXNIntegrationObserver::OnPost*().<p><br>                                                    
                                                     */
    ,   ePropertyModalDialogs                       /*!<Property for modal dialog support.
                                                     *  <ul>
                                                     *  <li>Setting property to "1" signals: integration requests
                                                     *  that database dialogs should be modal while being opened.</li>
                                                     *  <li>Setting property to "0" signals: integration requests
                                                     *  that database dialogs should be non-modal while being opened.</li>
                                                     *  </ul>
                                                     *  This setting is useful if the integration processes
                                                     *  shortcut keys: inputs done in database dialogs
                                                     *  might be interpreted as shortcuts if the database
                                                     *  dialog is opened in non-modal mode. If this is the
                                                     *  case, property ePropertyModalDialogs should be set
                                                     *  to "0" to avoid this undesired behavior.<p><br>
                                                     */
    ,   ePropertySupportedFileExtensions            /*!<Property for supported file extensions
                                                     *
                                                     * This property provides the Windows Integration Framework with
                                                     * information about the file extensions which are supported by the
                                                     * NXN alienbrain integration. The file extensions which are set with this 
                                                     * property will be used as a filter in the "Import to database" dialog. See
                                                     * method CNXNIntegrationUITools::ShowImportToDatabaseDialog() for more details on how
                                                     * to open this dialog.<p><br>
                                                     * The property must be set to a piped list of file description / file extension
                                                     * pairs. The syntax for a file description / file extension pair is 
                                                     * "description,extension". The description may be one or more words. The extension
                                                     * must be specified without dots or wildcards.<p><br>
                                                     * As an example, if the NXN alienbrain integration works with HTML and text
                                                     * files (with extensions "*.htm", "*.html" and "*.txt"), property ePropertySupportedFileExtensions
                                                     * must be set to:<br>
                                                     * "HTML File,htm|HTML File,html|Text File,txt".<p><br>
                                                     */
    ,   ePropertySupportsReferences                 /*!<Property for reference support.
                                                     *  <ul>
                                                     *  <li>Setting property to "1" signals: integration supports
                                                     *  references.</li>
                                                     *  <li>Setting property to "0" signals: integration doesn't
                                                     *  support references.</li>
                                                     *  </ul>
                                                     *  This setting is needed for NXN alienbrain integrations which
                                                     *  support file references. As an example, 3D animation 
                                                     *  applications e.g. support references between the file containing 
                                                     *  3D geometry information and the texture files being attached 
                                                     *  to the geometry. For these kinds of applications, reference 
                                                     *  suppport should be enabled.<p><br>
                                                     *
                                                     *  With activated reference support, the database Reference 
                                                     *  Manager might be opened by the NXN alienbrain integration. For 
                                                     *  proper reference support, CNXNIntegrationObserver callback methods 
                                                     *  CNXNIntegrationObserver::GetReferenceVector() and 
                                                     *  CNXNIntegrationObserver::RemapReference() must be implemented as
                                                     *  well. See the documentation on these methods for more details.<p><br>
                                                     *
                                                     *  <b>Important note:</b> If this property is set to "1", property 
                                                     *  WIF::ePropertyReferenceFileExtensions must be set as well.<p><br>
                                                     */
    ,   ePropertyReferenceFileExtensions            /*!<Property for reference file extensions.
                                                     *
                                                     * This property provides the Windows Integration Framework with
                                                     * information about the file types which support references. If
                                                     * property ePropertySupportsReferences is set to "1", this property
                                                     * must be set as well. Setting this property is essential to ensure
                                                     * that references between files are properly managed and updated by 
                                                     * the Windows Integration Framework.<p><br> 
                                                     * The property must be set to a piped list of file extensions. For every
                                                     * file type which supports references, an extension must be given in the
                                                     * piped list. The extensions must be specified without the dots or wildcards.
                                                     * As an example, if the NXN alienbrain integration works with two 
                                                     * reference-supporting file types (with extensions "*.abc" and "*.xyz"), the 
                                                     * property must be set to: "abc|xyz".<p><br>
                                                     */
    ,   ePropertySupportsScreenshot                 /*!<Property for screenshot support.
                                                     *
                                                     *  <ul>
                                                     *  <li>Setting property to "1" signals: integration supports
                                                     *  screenshots.</li>
                                                     *  <li>Setting property to "0" signals: integration doesn't
                                                     *  support screenshots.</li>
                                                     *  </ul><br>
                                                     *  This setting is needed for NXN alienbrain integrations which
                                                     *  support file screenshots.<br>
                                                     *  If screenshots are supported the callback 
                                                     *  CNXNIntegrationObserver::GetScreenshotFile() will be used by
                                                     *  the Windows Integration Framework to retrieve a file to be used as
                                                     *  Screenshot when a screenshot needs to be made.<br>
                                                     *  The Setting eSettingScreenshotOnVersion determines if the Framework
                                                     *  will try to automatically upload screenshots whenever a version is created
                                                     *  as a result of an automatic check in in the database (please note that this does not include 
                                                     *  check in operations executed via the XDK).<br>
                                                     *  The eSettingUploadOnScreenshot will be made available in the Settings Dialog
                                                     *  by the Framework and must be used whenever a Screenshot is uploaded via
                                                     *  the XDK.
                                                     */
    };

    /*! Enum for NXN connect policy settings.<br><br>
     *  NXN connect policy settings control whether the connection to the database
     *  is done silently or by displaying a connect dialog.
     */
    enum NXNConnectPolicySetting           
    {   eConnectPolicyDefault                                 /*!< Default setting of the "connect or work offline?" dialog will be used (hide checkbox tick mark will be taken into consideration). */
    ,   eConnectPolicyHideDialog                              /*!< "connect or work offline?" dialog will never pop up when connecting to the database. */
    };    

    /*! Enum for Windows Integration Framework assert report mode settings.<br><br>
     *  The assert report mode controls how asserts of the Windows Integration Framework are reported.
     */
    enum NXNAssertReportModeSetting
    {   eAssertReportModeMessageBox                           /*!< Asserts will be displayed with a message box along with Abort, Retry and Ignore buttons. */
    ,   eAssertReportModeDebugOutputWindow                    /*!< Asserts will be written to the debugger's output window. */
    };    

    /*! Enum for NXN reference types.<br><br>
     *  The reference type specifies the nature of a file reference.
     */
    enum NXNReferenceType
    {   eReferenceTypeTexture       /*!< Reference type for texture references. */
    ,   eReferenceTypeModel         /*!< Reference type for model references. */
    ,   eReferenceTypeScene         /*!< Reference type for scene references. */
    ,   eReferenceTypeProject       /*!< Reference type for project references. */
    ,   eReferenceTypeImageList     /*!< Reference type for image list references. */
    };    
}

#endif // _NXNINTEGRATIONTYPES_H_
