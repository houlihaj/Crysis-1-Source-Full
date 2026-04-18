// \addtodoc
#ifndef _NXNINTEGRATIONTYPESLIMITS_H_
#define _NXNINTEGRATIONTYPESLIMITS_H_

/*! \file NXNIntegrationTypesLimits.h
 *  NXNIntegrationTypesLimits.h contains numeric_limits class specializations for methods
 *  min() and max() for all types defined in NXNIntegrationTypes. All numeric_limits class 
 *  specializations are defined within namespace std.
*/

#include <limits>
#include "NXNIntegrationTypes.h"

namespace std
{  
    template<>
    class numeric_limits<WIF::NXNSetting>
    {
        public:

            //! returns the minimum WIF::NXNSetting enum value 
            static WIF::NXNSetting (__cdecl min)() _THROW0() { return WIF::eSettingCheckoutOnOpen; }
            //! returns the maximum WIF::NXNSetting enum value 
            static WIF::NXNSetting (__cdecl max)() _THROW0() { return WIF::eSettingScreenshotOnVersion; }        

            static const bool   is_specialized;
            static const int    digits;
    };

    template<>
    class numeric_limits<WIF::NXNSettingValue>
    {
        public:

            //! returns the minimum WIF::NXNSettingValue enum value 
            static WIF::NXNSettingValue (__cdecl min)() _THROW0() { return WIF::eSettingValueAskUser; }
            //! returns the maximum WIF::NXNSettingValue enum value 
            static WIF::NXNSettingValue (__cdecl max)() _THROW0() { return WIF::eSettingValueNever; }        

            static const bool   is_specialized;
            static const int    digits;
    };

    template<>
    class numeric_limits<WIF::NXNProperty>
    {
        public:

            //! returns the minimum WIF::NXNProperty enum value 
            static WIF::NXNProperty (__cdecl min)() _THROW0() { return WIF::ePropertySupportsMultipleDocuments; }
            //! returns the maximum WIF::NXNProperty enum value 
            static WIF::NXNProperty (__cdecl max)() _THROW0() { return WIF::ePropertySupportsScreenshot; }        

            static const bool   is_specialized;
            static const int    digits;
    };

    template<>
    class numeric_limits<WIF::NXNConnectPolicySetting>
    {
        public:

            //! returns the minimum WIF::ConnectPolicySetting enum value 
            static WIF::NXNConnectPolicySetting (__cdecl min)() _THROW0() { return WIF::eConnectPolicyDefault; }
            //! returns the maximum WIF::ConnectPolicySetting enum value 
            static WIF::NXNConnectPolicySetting (__cdecl max)() _THROW0() { return WIF::eConnectPolicyHideDialog; }        

            static const bool   is_specialized;
            static const int    digits;
    };

    template<>
    class numeric_limits<WIF::NXNAssertReportModeSetting>
    {
        public:

            //! returns the minimum WIF::NXNAssertReportModeSetting enum value 
            static WIF::NXNAssertReportModeSetting (__cdecl min)() _THROW0() { return WIF::eAssertReportModeMessageBox; }
            //! returns the maximum WIF::NXNAssertReportModeSetting enum value 
            static WIF::NXNAssertReportModeSetting (__cdecl max)() _THROW0() { return WIF::eAssertReportModeDebugOutputWindow; }        

            static const bool   is_specialized;
            static const int    digits;
    };

    template<>
    class numeric_limits<WIF::NXNReferenceType>
    {
    public:

        //! returns the minimum WIF::NXNReferenceType enum value 
        static WIF::NXNReferenceType (__cdecl min)() _THROW0() { return WIF::eReferenceTypeTexture; }
        //! returns the maximum WIF::NXNReferenceType enum value 
        static WIF::NXNReferenceType (__cdecl max)() _THROW0() { return WIF::eReferenceTypeImageList; }        

        static const bool   is_specialized;
        static const int    digits;
    };    
}

#endif // _NXNINTEGRATIONTYPESLIMITS_H_