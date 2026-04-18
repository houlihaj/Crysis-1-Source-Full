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

#include "StdAfx.h"
#include "DefenceData.h"
#include "Network.h"
#include "Config.h"
#include "StringUtils.h"

#if USE_DEFENCE

CDefenceData::CDefenceData()
{
	version = 0;
	level = -1;
}

void CDefenceData::UnifyFilename( string &sFilename )
{
	string file = sFilename;
	file.replace( '\\','/' );
	file.replace("%GAME_FOLDER%", PathUtil::GetGameFolder().c_str());
	strlwr( const_cast<char*>(file.c_str()) );
	sFilename = file;
}

void CDefenceData::AddProtectedFile( SProtectedFile file )
{
	if (!GetISystem()->GetIDataProbe())
	{
		NetWarning("No data probe: not adding files");
		return;
	}

#if !defined(LINUX) || 1
	UnifyFilename( file.name );

	TProtectedFiles::iterator lower = m_vProtectedFiles.lower_bound(file);
	if (lower != m_vProtectedFiles.end() && lower->name.compareNoCase(file.name) == 0)
		return;
	TProtectedFiles::iterator it = m_vProtectedFiles.insert(lower, file);
	m_listProt.push_back(*it);
	//NetLogAlways("Adding file to protection: %s", file.name);
	version ++;
#endif
}

void CDefenceData::ClearProtectedFiles()
{
	m_vProtectedFiles.clear();
	version ++;
}

void CDefenceData::WalkFolderAndAddToVector( const string& toppath, int recurse, std::vector<SProtectedFile>& where, SProtectedFile tplt )
{
	ICryPak * pSys = gEnv->pCryPak;

	std::queue<string> searchPaths;
	searchPaths.push( toppath );

	_finddata_t fd;
	while (!searchPaths.empty())
	{
		string path = searchPaths.front();
		searchPaths.pop();

		string search = path + "/*.*";
		intptr_t handle = pSys->FindFirst( search.c_str(), &fd );
		if (handle != -1)
		{
			int res = 0;

			do 
			{
				string filename = path + "/" + fd.name;
				UnifyFilename(filename);

				if (fd.attrib & _A_SUBDIR)
				{
					switch (recurse)
					{
					case 0:
						break;
					case 1:
						if (0 != strcmp(fd.name, ".") && 0 != strcmp(fd.name, ".."))
							searchPaths.push(filename);
						break;
					case 2:
						if (0 != strcmp(fd.name, ".") && 0 != strcmp(fd.name, ".."))
						{
							SProtectedFile newtplt = tplt;
							static const char * levelsStr = "/levels/";
							string::size_type levelspos = CryStringUtils::toLower(filename).find(levelsStr);
							if (levelspos != string::npos)
								newtplt.level = filename.substr(levelspos + strlen(levelsStr));
							else
								newtplt.level = fd.name;
							WalkFolderAndAddToVector( filename, 1, where, newtplt );
						}
						break;
					}
				}
				else
				{
					tplt.name = filename;
					where.push_back(tplt);
				}

				res = pSys->FindNext( handle, &fd );
			}
			while (res >= 0);

			pSys->FindClose( handle );
		}
	}
}

void CDefenceData::FoldersToFiles( const std::map<string, int>& mapFolders, std::vector<SProtectedFile>& vecFiles )
{
	for (std::map<string, int>::const_iterator it = mapFolders.begin(); it != mapFolders.end(); ++it)
	{
		WalkFolderAndAddToVector(it->first, it->second, vecFiles, SProtectedFile());
	}
}

void CDefenceData::FoldersToFiles( const std::vector<std::pair<string, int>>& vecFolders, std::vector<SProtectedFile>& vecFiles )
{
	for (std::vector<std::pair<string, int>>::const_iterator it = vecFolders.begin(); it != vecFolders.end(); ++it)
	{
		WalkFolderAndAddToVector(it->first, it->second, vecFiles, SProtectedFile());
	}
}

void CDefenceData::Populate()
{
	ClearProtectedFiles();

	level = CNetCVars::Get().CheatProtectionLevel;
	if (!gEnv->bMultiplayer || !gEnv->bServer)
		level = 0;

	if (!level)
		return;

	std::vector<XmlNodeRef> roots;
	// check for a user defined file
	XmlNodeRef nodeRoot = gEnv->pSystem->LoadXmlFile( "Protect.xml" );
	if (nodeRoot)
		roots.push_back(nodeRoot);
	// if that file exists, does it override the system version?
	bool userOverride = false;
	if (nodeRoot && nodeRoot->getAttr("override", userOverride) && userOverride)
		;
	else
	{
		// if not, load the system version if it exists
		nodeRoot = gEnv->pSystem->LoadXmlFile( PathUtil::GetGameFolder() + "/Scripts/Network/Protect.xml" );
		if (nodeRoot)
			roots.push_back(nodeRoot);
	}

	//add mod directories if mod is loaded
	const char* modDir = gEnv->pCryPak->GetModDir();
	if(strlen(modDir) && nodeRoot)
	{
		XmlNodeRef modRoot = nodeRoot->clone();
		int children = modRoot->getChildCount();
		for(int c = 0; c < children; ++c)
		{
			XmlNodeRef child = modRoot->getChild(c);
			if(child)
			{
				int attribs = child->getNumAttributes();
				const char *key, *value;
				//get file/folder path
				if(child->getAttributeByIndex(1, &key, &value)) 
				{
					if(!strcmp("file", key) || !strcmp("folder", key))
					{
						string val(modDir);
						val += value;
						child->setAttr(key, val.c_str());
					}
				}
			}
		}
		roots.push_back(modRoot);
	}

	// files we don't want
	std::vector<SProtectedFile> explicitExclusions;
	// folders we don't want
	std::map<string, int> folderExclusions;
	// wildcards we don't want
	std::vector<string> wildcardExclusions;
	// files we want
	std::vector<SProtectedFile> explicitFiles;
	// folders we want
	std::vector<std::pair<string, int>> explicitFolders;

	// gather the initial data structures
	for (std::vector<XmlNodeRef>::iterator itRoot = roots.begin(); itRoot != roots.end(); ++itRoot)
	{
		nodeRoot = *itRoot;
		int children = nodeRoot->getChildCount();
		for (int i=0; i<children; i++)
		{
			XmlNodeRef node = nodeRoot->getChild(i);

			int nodelevel = 0;
			node->getAttr("level", nodelevel);
			if (nodelevel < 1)
				nodelevel = 1;
			if (nodelevel > CNetCVars::Get().CheatProtectionLevel)
				continue;

			const char * tag = node->getTag();
			if (0 == strcmp(tag, "add"))
			{
				XmlString s;
				if (node->getAttr("file", s) && !s.empty())
				{
					SProtectedFile f;
					f.name = s.c_str();
					UnifyFilename(f.name);
					explicitFiles.push_back(f);
				}
				else if (node->getAttr("folder", s) && !s.empty())
				{
					int recurse = 1;
					node->getAttr("recurse", recurse);
					string file = s.c_str();
					UnifyFilename(file);

					// if this folder already in list, just update the recursion if necessary.
					bool found = false;
					for (std::vector<std::pair<string, int>>::iterator it = explicitFolders.begin(); it != explicitFolders.end(); ++it)
					{
						if(!stricmp(file, it->first))
						{
							it->second = std::max(it->second, recurse);// only ever become more recursive in the case of dupes
							found = true;
							break;
						}
					}
					if(!found)
					{
						explicitFolders.push_back(std::make_pair(file, recurse));
					}
				}
				else
					NetWarning("Invalid add node: %s", node->getXML().c_str());
			}
			else if (0 == strcmp(tag, "exclude"))
			{
				XmlString s;
				if (node->getAttr("file", s) && !s.empty())
				{
					SProtectedFile f;
					f.name = s.c_str();
					UnifyFilename(f.name);
					explicitExclusions.push_back(f);
				}
				else if (node->getAttr("folder", s) && !s.empty())
				{
					int recurse = 1;
					node->getAttr("recurse", recurse);
					string file = s.c_str();
					UnifyFilename(file);
					folderExclusions[file] = std::max(folderExclusions[file], recurse); // only ever become more recursive in the case of dupes
				}
				else if (node->getAttr("wildcard", s) && !s.empty())
					wildcardExclusions.push_back(s.c_str());
				else
					NetWarning("Invalid add node: %s", node->getXML().c_str());
			}
			else
			{
				NetWarning("Invalid protection tag %s", tag);
			}
		}
	}

	// turn folders into files
	FoldersToFiles(explicitFolders, explicitFiles);
	FoldersToFiles(folderExclusions, explicitExclusions);

	// sort the explicit exclusions for faster rejecting
	std::sort(explicitExclusions.begin(), explicitExclusions.end());

	// now walk through the files we want, rejecting them using the exclusions if needed, and adding them to the main data structure
	for (std::vector<SProtectedFile>::const_iterator itAdd = explicitFiles.begin(); itAdd != explicitFiles.end(); ++itAdd)
	{
		std::vector<SProtectedFile>::const_iterator itExplicitExclude = std::lower_bound( explicitExclusions.begin(), explicitExclusions.end(), *itAdd );
		if (itExplicitExclude != explicitExclusions.end() && *itExplicitExclude == *itAdd)
			continue;
		bool matchesExclusionWildcard = false;
		for (std::vector<string>::const_iterator itWildcard = wildcardExclusions.begin(); !matchesExclusionWildcard && itWildcard != wildcardExclusions.end(); ++itWildcard)
		{
			if (PathUtil::MatchWildcard(itAdd->name.c_str(), itWildcard->c_str()))
				matchesExclusionWildcard = true;
		}
		if (matchesExclusionWildcard)
			continue;
		AddProtectedFile(*itAdd);
	}
}

#endif //NOT_USE_DEFENCE
