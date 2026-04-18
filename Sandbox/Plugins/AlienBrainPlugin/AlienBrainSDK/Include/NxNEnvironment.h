// \addtodoc

#ifndef INC_NXN_ENVIRONMENT_H
#define INC_NXN_ENVIRONMENT_H

#define BOOLEAN_SETTING_CHANGESETS_AVAILABLE  1
#define BOOLEAN_SETTING_SIGNOFF_AVAILABLE     2
#define BOOLEAN_SETTING_BRANCHING_AVAILABLE   3

/*	\class		CNxNEnvironment NxNEnvironment.h
 *
 *  \brief		This class gives access to common environment settings
 *
 *  \author		Thomas Gawlik
 *
 *  \version	1.00
 *
 *  \date		2003
 *
 */

class NXNINTEGRATORSDK_API CNxNEnvironment
{
	public:
		//---------------------------------------------------------------------------
		//	construction/destruction
		//---------------------------------------------------------------------------
		CNxNEnvironment();
		virtual ~CNxNEnvironment();

		//---------------------------------------------------------------------------
		//---------------------------------------------------------------------------
		static CNxNString NXNINTEGRATORSDK_API_CALL GetStringSetting( CNxNString& sSetting );
		static bool NXNINTEGRATORSDK_API_CALL GetBooleanSetting( int iSetting );

	private:
};

#endif // INC_NXN_ENVIRONMENT_H
