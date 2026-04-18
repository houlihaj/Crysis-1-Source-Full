/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  common anti-cheat defence data
 -------------------------------------------------------------------------
 History:
 - 10/08/2004   : Created by Craig Tiller, based on CDefenceWall by Timur
*************************************************************************/

#ifndef __DEFENCEDATA_H__
#define __DEFENCEDATA_H__

#pragma once

#include "Config.h"

#if USE_DEFENCE

#include "IDataProbe.h"
#include <list>

class CDefenceData
{
public:
	CDefenceData();

	struct SProtectedFile
	{
		string name;
		string level;
		bool operator==(const SProtectedFile& rhs) const
		{
			return name == rhs.name;
		}
		bool operator<(const SProtectedFile& rhs) const
		{
			return name < rhs.name || (name == rhs.name && level < rhs.level);
		}
	};
		
	static void UnifyFilename( string& sFilename );
	static void FoldersToFiles( const std::map<string, int>& mapFolders, std::vector<SProtectedFile>& vecFiles );
	static void FoldersToFiles( const std::vector<std::pair<string, int>>& vecFolders, std::vector<SProtectedFile>& vecFiles );
	static void WalkFolderAndAddToVector( const string& toppath, int recurse, std::vector<SProtectedFile>& where, SProtectedFile tplt );

	// list of all protected files
	typedef std::set<SProtectedFile> TProtectedFiles;
	TProtectedFiles m_vProtectedFiles;
	typedef std::list<SProtectedFile> TProtectedFileList;
	TProtectedFileList m_listProt;

	int version;
	int level;

	void AddProtectedFile( SProtectedFile file );
	void ClearProtectedFiles();

	void Populate();
};
#endif //NOT_USE_DEFENCE

#endif
