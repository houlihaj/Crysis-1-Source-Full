////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   ScriptBind_Movie.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Movie_h__
#define __ScriptBind_Movie_h__
#pragma once


#include "IScriptSystem.h"

// <title Movie>
// Syntax: Movie
// Description:
//    Interface to movie system.
class CScriptBind_Movie : public CScriptableBase
{
public:
	CScriptBind_Movie(IScriptSystem *pScriptSystem, ISystem *pSystem);
	virtual ~CScriptBind_Movie();
	virtual void GetMemoryStatistics(ICrySizer *pSizer) { pSizer->Add(this); };

	int PlaySequence( IFunctionHandler *pH,const char *sSequenceName );
	int StopSequence( IFunctionHandler *pH,const char *sSequenceName );
	int StopAllSequences(IFunctionHandler *pH);
	int StopAllCutScenes(IFunctionHandler *pH);
	int PauseSequences(IFunctionHandler *pH);
	int ResumeSequences(IFunctionHandler *pH);
private:
	ISystem *m_pSystem;
	IMovieSystem *m_pMovieSystem;
};



#endif // __ScriptBind_Movie_h__