#ifndef __XMLLOADGAME_H__
#define __XMLLOADGAME_H__

#pragma once

#include "ILoadGame.h"

class CXmlLoadGame : public ILoadGame
{
public:
	CXmlLoadGame();
	virtual ~CXmlLoadGame();

	// ILoadGame
	virtual bool Init( const char * name );
	virtual const char * GetMetadata( const char * tag );
	virtual bool GetMetadata( const char * tag, int& value );
	virtual bool HaveMetadata( const char * tag );
	virtual bool GetConsoleVariable( ICVar * pVar );
	virtual std::auto_ptr<TSerialize> GetSection( const char * section );
	virtual bool HaveSection( const char * section );
	virtual void Complete();
	virtual const char* GetFileName() const;
	// ~ILoadGame

protected:
	bool Init( const XmlNodeRef& root, const char * fileName );

private:
	class CSerializeCtx;
	struct Impl;
	std::auto_ptr<Impl> m_pImpl;
};

#endif
