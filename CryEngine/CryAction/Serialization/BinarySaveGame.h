#ifndef __BINARYSAVEGAME_H__
#define __BINARYSAVEGAME_H__

#pragma once

#include "ISaveGame.h"

class CBinarySaveGame : public ISaveGame
{
public:
	CBinarySaveGame();
	virtual ~CBinarySaveGame();

	// ISaveGame
	virtual bool Init( const char * outputPath );
	virtual void AddMetadata( const char * tag, const char * value );
	virtual void AddMetadata( const char * tag, int value );
	virtual void AddConsoleVariable( ICVar * pVar );
	virtual TSerialize AddSection( const char * section );
	virtual bool Complete( bool successfulSoFar );
	virtual const char* GetFileName() const { return NULL; }
	virtual void SetSaveGameReason(ESaveGameReason reason) { }
	virtual ESaveGameReason GetSaveGameReason() const { return eSGR_QuickSave; }
	// ~ISaveGame

private:
	class CSerializeCtx;
	struct Impl;
	std::auto_ptr<Impl> m_pImpl;

	virtual FILE * Open( const char * outputPath );
};

#endif
