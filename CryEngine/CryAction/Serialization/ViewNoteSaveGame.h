#ifndef __VIEWNOTESAVEGAME_H__
#define __VIEWNOTESAVEGAME_H__

#pragma once

#include "XmlSaveGame.h"

class CViewNoteSaveGame : public CXmlSaveGame
{
public:
	
private:
	// override CXmlSaveGame
	bool Write( const char * filename, XmlNodeRef data );
};

#endif
