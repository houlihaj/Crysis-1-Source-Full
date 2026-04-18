////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	29/11/2006 - Created by Michael Smith
//
//  Contains:
//  loading of animation events
/////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __ANIMEVENTLOADER_H__
#define __ANIMEVENTLOADER_H__

class CCharacterModel;

namespace AnimEventLoader
{
	// loads the data from the animation event database file (.animevent) - this is usually
	// specified in the CAL file.
	bool loadAnimationEventDatabase( CCharacterModel* pModel, const char* pFileName );
}

#endif //__ANIMEVENTLOADER_H__
