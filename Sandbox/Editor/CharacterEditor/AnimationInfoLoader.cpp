#include "StdAfx.h"

#include "AnimationInfoLoader.h"
#include "IXML.h"


CAnimationInfoLoader::CAnimationInfoLoader()
{
}

CAnimationInfoLoader::~CAnimationInfoLoader(void)
{
}

int GetIntFromStringBOOL(const string& val)
{
	if (!val.c_str())
		return -1;

	int ret = 0;
	if ((stricmp(val.c_str(), "1") == 0) || (stricmp(val.c_str(), "yes") == 0))
		ret = 1;

	return ret;

}

EMoveDirection GetMoveDirectionFromString(const string& val)
{
	EMoveDirection res(EMD_Ignore);

	if (stricmp(val.c_str(), "forward") == 0)
	{
		res =  eMD_Forward;
	}
	else
		if (stricmp(val.c_str(), "back") == 0)
		{

			res = eMD_Back;
		}
		else
			if (stricmp(val.c_str(), "left") == 0)
			{

				res = eMD_Left;
			}
			else
				if (stricmp(val.c_str(), "right") == 0)
				{

					res = eMD_Right;
				}


				return res;
}


EDirection GetDirectionFromString(const string& val)
{
	EDirection res(ED_Ignore);
	if (stricmp(val.c_str(), "identity") == 0)
	{
		res =  ED_Identity;
	}
	else
		if (stricmp(val.c_str(), "l90") == 0)
		{
			res =  ED_L90;
		}
		else
			if (stricmp(val.c_str(), "r90") == 0)
			{
				res =  ED_R90;
			}

			return res;
}


int GetNodeIDFromName(const char * name, const XmlNodeRef& node)
{
	uint32 numChildren= node->getChildCount();

	for (uint32 chield = 0; chield < numChildren; ++ chield )
	{
		if (stricmp(name, node->getChild(chield)->getTag()) == 0)
		{
			return chield;
		}
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int SAnimationDefinition::GetAnimationNumber(const string& name )
{


	string searchname(name);
	FixString(searchname);

	for (uint32 i = 0; i < m_OverrideAnimations.size(); ++i)
	{
		string mapname(m_OverrideAnimations[i].GetAnimationName());


		if ((strstr(mapname.c_str(), searchname.c_str()) != 0)/* || (strstr(searchname.c_str(), mapname.c_str()) != 0)*/)
		{
			return i;
		}
	}

	return -1;
}

void SAnimationDefinition::GetAnimationNumbers(const string& name, std::vector<int>& anims )
{


	string searchname(name);
	FixString(searchname);

	for (uint32 i = 0; i < m_OverrideAnimations.size(); ++i)
	{
		string mapname(m_OverrideAnimations[i].GetAnimationName());


		if ((strstr(mapname.c_str(), searchname.c_str()) != 0)/* || (strstr(searchname.c_str(), mapname.c_str()) != 0)*/)
		{
			anims.push_back(i);
		}
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void SAnimationDefinition::AddAnimationPreset(CompressionPreset * pPreset, const char *animName)
{
	// 1. Trying to find appropriate animation. if it not exist, create new node

	std::vector<int> anims;

	GetAnimationNumbers(animName, anims);


	if (anims.size() == 0)
	{
		// create new desc

		SAnimationDesc newDesc;
		
		newDesc.SetAnimationName(animName);
		newDesc.m_Preset = *pPreset;
		//newDesc.m_pNodeRef = m_pNodeRef->newChild("Animation");

		uint32 details = GetNodeIDFromName("SpecialAnimsList", m_pNodeRef);
		XmlNodeRef pAttrib;
		if (details != -1)
		{
			pAttrib = m_pNodeRef->getChild(details);
		}
		else
		{
			pAttrib = m_pNodeRef->newChild("SpecialAnimsList");
		}

		newDesc.m_pNodeRef = pAttrib->newChild("Animation");

		newDesc.m_pNodeRef->setAttr("APath", animName);
		newDesc.m_pNodeRef->setAttr("preset", pPreset->m_Name.c_str());

		m_OverrideAnimations.push_back(newDesc);

	}
	else
	{
		for (uint32 anim = 0; anim < anims.size(); ++anim)
		{
			if (m_OverrideAnimations[anims[anim]].m_pNodeRef)
			{
				m_OverrideAnimations[anims[anim]].m_pNodeRef->setAttr("preset", pPreset->m_Name.c_str());
			}
			else
			{
				uint32 details = GetNodeIDFromName("SpecialAnimsList", m_pNodeRef);
				XmlNodeRef pAttrib;
				if (details != -1)
				{
					pAttrib = m_pNodeRef->getChild(details);
				}
				else
				{
					pAttrib = m_pNodeRef->newChild("SpecialAnimsList");
				}

				m_OverrideAnimations[anims[anim]].m_pNodeRef = pAttrib->newChild("Animation");
				m_OverrideAnimations[anims[anim]].m_pNodeRef->setAttr("APath", animName);
				m_OverrideAnimations[anims[anim]].m_pNodeRef->setAttr("preset", pPreset->m_Name.c_str());
			}
		}
	}
		
	
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CAnimationInfoLoader::InsertPresetToXML(int num)//CompressionPreset& preset)
{
	CompressionPreset& preset = m_PresetFinder.m_Presets[num];

	if (!preset.m_pNodeRef)
	{
		preset.m_pNodeRef = m_pRoot->newChild("preset");
		preset.m_pNodeRef->setAttr("Name", preset.m_Name.c_str());
	}

	TBoneInfoMap::iterator end = preset.m_BoneInfoMap.end();
	TBoneInfoMap::iterator it = preset.m_BoneInfoMap.begin();

	for (; it != end; ++it)
	{
		if (!it->second.m_BoneRef)
		{
			it->second.m_BoneRef = preset.m_pNodeRef->newChild("Bone");
			it->second.m_BoneRef->setAttr("id", it->first);
		}

		it->second.m_BoneRef->setAttr("poserror", it->second.m_fPosError);
		it->second.m_BoneRef->setAttr("roterror", it->second.m_fRotError);
		it->second.m_BoneRef->setAttr("rotformat", it->second.m_RotCompressionFormat);
		it->second.m_BoneRef->setAttr("posformat", it->second.m_PosCompressionFormat);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void CAnimationInfoLoader::AddPreset(CompressionPreset& preset)
{
	int res = m_PresetFinder.FindPreset(preset.m_Name.c_str());

	if (res == -1)
	{
		// simple add new preset
		m_PresetFinder.m_Presets.push_back(preset);
		res = m_PresetFinder.m_Presets.size() - 1;
	}
	else
	{
		preset.m_pNodeRef = m_PresetFinder.m_Presets[res].m_pNodeRef;
		m_PresetFinder.m_Presets[res] = preset;

		if (preset.m_pNodeRef)
		{
			// destroy 
			preset.m_pNodeRef->removeAllChilds();
		}

	}

	InsertPresetToXML(res);
}

bool CAnimationInfoLoader::SaveDescription( const string& name ) 
{
	return m_pRoot->saveToFile(name.c_str());
}


bool CAnimationInfoLoader::LoadDescription(const string& name)//, IAnimationLoaderListener * pListener)
{
	// fill structures
	//char error[4096];

	m_pRoot =  GetISystem()->GetXmlUtils()->LoadXmlFile(name.c_str());
	if (!m_pRoot)
	{
		//ReportError(pListener, "Cannot read file \"%s\": %s\n", name.c_str(), error);
		return false;
	}

	// temporary storage for the presets

	std::vector<CompressionPreset> presets;
	//	PresetFinder finder;

	uint32 numDefinitions = m_pRoot->getChildCount();

	for (uint32 nDefinitionNode=0; nDefinitionNode<numDefinitions; ++nDefinitionNode)
	{
		XmlNodeRef pRoot = m_pRoot->getChild(nDefinitionNode);
		if(!pRoot)
		{
			//ReportError(pListener, "Node not found");
			continue;
		}



		if (stricmp(pRoot->getTag(), "preset") == 0)
		{

			// this is preset for compression
			CompressionPreset preset;

			uint32 numBonesInfo = pRoot->getChildCount();


			string presetName = pRoot->getAttr("Name");
			preset.m_Name = presetName;

			preset.m_pNodeRef = pRoot;

			for (uint32 nBone = 0; nBone < numBonesInfo; ++nBone )
			{
				BonePreset bone;

				XmlNodeRef pBone = pRoot->getChild(nBone);
				//<Bone id="12345" error="0.00001" rotformat="auto" posformat="auto"/>
				uint32 boneId;

				bone.m_BoneRef = pBone;
				pBone->getAttr("id", boneId);
				pBone->getAttr("poserror", bone.m_fPosError);
				pBone->getAttr("roterror", bone.m_fRotError);
				pBone->getAttr("rotformat", bone.m_RotCompressionFormat);
				pBone->getAttr("posformat", bone.m_PosCompressionFormat);

				preset.m_BoneInfoMap[boneId] = bone;
			}

			m_PresetFinder.m_Presets.push_back(preset);

//			continue;
		}
	}

	for (uint32 nDefinitionNode=0; nDefinitionNode<numDefinitions; ++nDefinitionNode)
	{
		XmlNodeRef pRoot = m_pRoot->getChild(nDefinitionNode);
		if(!pRoot)
		{
			//ReportError(pListener, "Node not found");
			continue;
		}

		if (stricmp(pRoot->getTag(), "preset") == 0)
		{
			continue;
		}


		SAnimationDefinition def;

		def.m_pNodeRef = pRoot;

		uint32 numAttribs = pRoot->getChildCount();

		{
			uint32 model = GetNodeIDFromName("model", pRoot);
			if (model == -1)
			{
				//ReportError(pListener, "Model name not found in \"%s\" node", pRoot->getTag());
			}
			else
			{
				def.m_Model = pRoot->getChild(model)->getAttr("File");
				if (def.m_Model.empty())
				{
					//ReportError(pListener, "Model name not found in \"%s\" node", pRoot->getTag());
				}
			}

		}


		{
			uint32 path = GetNodeIDFromName("animation", pRoot);
			if (path == -1)
			{
				//ReportError(pListener, "Path not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{
				const char * pChar = pRoot->getChild(path)->getAttr("Path");
				if (!pChar)
				{
					//ReportError(pListener, "Path not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				def.SetAnimationPath(string(pChar));
			}
		}

		{
			uint32 compression = GetNodeIDFromName("compression", pRoot);
			if (compression == -1)
			{
				//ReportError(pListener, "Compression value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{

				if (!pRoot->getChild(compression)->getAttr("value", def.m_MainDesc.m_CompressionQuality))
				{
					//ReportError(pListener, "Compression value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				};
			}
		}

		{
			uint32 compression = GetNodeIDFromName("autocompression", pRoot);
			if (compression == -1)
			{
				//ReportError(pListener, "Compression value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{

				if (!pRoot->getChild(compression)->getAttr("value", def.m_MainDesc.m_FixedToFoots))
				{
					//ReportError(pListener, "Compression value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				};
			}
		}

		{
			uint32 preset = GetNodeIDFromName("preset", pRoot);
			if (preset == -1)
			{
				//ReportWarning(pListener, "Preset not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{
				const char * pChar = pRoot->getChild(preset)->getAttr("name");
				if (!pChar)
				{
				//	ReportError(pListener, "Preset not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				//def.SetAnimationPath(string(pChar));

				m_PresetFinder.FillPreset(pChar, def.m_MainDesc.m_Preset);
			}
		}


		{
			uint32 footplant = GetNodeIDFromName("footplants", pRoot);
			if (footplant == -1)
			{
				//ReportError(pListener, "Footplants value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{
				string val = pRoot->getChild(footplant)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "Footplants value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				def.m_MainDesc.m_FootPlant = GetIntFromStringBOOL(val);
			}
		}

		{
			uint32 deletepos = GetNodeIDFromName("DeletePosController", pRoot);
			if (deletepos != -1)
			{
				string val = pRoot->getChild(deletepos)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "DeletePosController value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				def.m_MainDesc.m_DeletePosController = GetIntFromStringBOOL(val);
			}
		}

		{
			uint32 deleterot = GetNodeIDFromName("DeleteRotController", pRoot);
			if (deleterot != -1)
			{
				string val = pRoot->getChild(deleterot)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "DeletePosController value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				def.m_MainDesc.m_DeleteRotController = GetIntFromStringBOOL(val);
			}
		}

		{
			uint32 human = GetNodeIDFromName("human", pRoot);
			if (human == -1)
			{
				//ReportError(pListener, "Human value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
			}
			else
			{
				string val = pRoot->getChild(human)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "Human value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}
				def.m_MainDesc.m_Human = GetIntFromStringBOOL(val);
			}
		}

		{
			uint32 move = GetNodeIDFromName("movedirection", pRoot);
			if (move != -1)
			{

				string val = pRoot->getChild(move)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "MoveDirection value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}
				else
					def.m_MainDesc.m_MoveDirection = GetMoveDirectionFromString(val);
			}
		}

		{
			uint32 dir = GetNodeIDFromName("bodydirection", pRoot);
			if (dir != -1)
			{

				string val = pRoot->getChild(dir)->getAttr("value");
				if (val.empty())
				{
					//ReportError(pListener, "MoveDirection value not found in \"%s\" node \"%s\"", pRoot->getTag(), def.m_Model.c_str());
				}

				def.m_MainDesc.m_Direction = GetDirectionFromString(val);
			}
		}


		XmlNodeRef pAttrib;

		uint32 details = GetNodeIDFromName("SpecialAnimsList", pRoot);
		if (details != -1)
		{
			pAttrib = pRoot->getChild(details);

			uint32 numAnims = pAttrib->getChildCount();

			for (uint32 nAnims = 0; nAnims < numAnims; ++nAnims )
			{


				SAnimationDesc desc(def.m_MainDesc);
				XmlNodeRef pAnim = pAttrib->getChild(nAnims);

				desc.m_pNodeRef = pAnim;

				if(!pAnim)
				{
					//ReportError(pListener, "Node not found");
				}
				else
				{
					const char *pPath = pAnim->getAttr("APath");

					if (!pPath)
					{
						//ReportError(pListener, "Animation name not found in \"%s\" node \"subnode\" \"%s\"", pRoot->getTag(), pAnim->getTag(), def.m_Model.c_str());
						continue;
					}
					desc.SetAnimationName(string(pPath));

					string val;


					val = pAnim->getAttr("compression");//, desc.m_CompressionQuality);
					if (!val.empty())
					{
						pAnim->getAttr("compression", desc.m_CompressionQuality);
					}

					val = pAnim->getAttr("autocompression");//, desc.m_CompressionQuality);
					if (!val.empty())
					{
						pAnim->getAttr("autocompression", desc.m_FixedToFoots);
					}

					val = pAnim->getAttr("footplants");
					if (!val.empty())
					{
						desc.m_FootPlant = GetIntFromStringBOOL(val);
					}


					val = pAnim->getAttr("movedirection");
					if (!val.empty())
					{
						desc.m_MoveDirection = GetMoveDirectionFromString(val);
					}


					val = pAnim->getAttr("bodydirection");
					if (!val.empty())
					{
						desc.m_Direction = GetDirectionFromString(val);
					}

					val = pAnim->getAttr("human");
					if (!val.empty())
					{
						desc.m_Human = GetIntFromStringBOOL(val);
					}

					val = pAnim->getAttr("preset");
					if (!val.empty())
					{
						//	desc.m_Human = GetIntFromStringBOOL(val);
						m_PresetFinder.FillPreset(val.c_str(), desc.m_Preset);
					}

					size_t num = def.m_OverrideAnimations.size();


					if (def.FindIdentical(string(pPath)))
					{
						//ReportInfo(pListener, "Duplicate  \"%s\" \"%s\"", pPath,  def.m_Model.c_str());
					}
					else
					{
						def.m_OverrideAnimations.push_back(desc);
					}


				}

			}

		}


		size_t num = m_Definitions.size();
		m_Definitions.push_back(def);


	}

	return true;
}



SAnimationDesc * SAnimationDefinition::GetAnimationDesc(const string& name )
{

	string searchname(name);
	FixString(searchname);


	for (uint32 i = 0; i < m_OverrideAnimations.size(); ++i)
	{
		string mapname(m_OverrideAnimations[i].GetAnimationName());


		if (((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0)) && !mapname.empty() )
		{
			return &m_OverrideAnimations[i];
		}
	}

	//if (int ret = m_AnimationMap.GetValue(name.c_str()) != ~0)
	//{
	//	return &m_OverrideAnimations[ret];
	//}

	// create

//	SAnimationDesc newDesc(m_MainDesc);
	
	m_OverrideAnimations.push_back(m_MainDesc);
	
	int num = m_OverrideAnimations.size() - 1;

	m_OverrideAnimations[num].SetAnimationName(searchname);
	m_OverrideAnimations[num].m_Preset.ClearNodeRefs();


	return &m_OverrideAnimations[num];
}

bool SAnimationDefinition::FindIdentical(const string& name )
{
	return GetAnimationNumber(name) == -1 ? false : true;
	//string searchname(name);
	//FixString(searchname);

	//for (uint32 i = 0; i < m_OverrideAnimations.size(); ++i)
	//{
	//	string mapname(m_OverrideAnimations[i].GetAnimationName());


	//	if ((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0))
	//	{
	//		return true;
	//	}
	//}

	//return false;
}

const SAnimationDefinition * CAnimationInfoLoader::GetAnimationDefinition(const string& name ) const
{

	string searchname(name);
	FixString(searchname);

	for (uint32 i = 0; i < m_Definitions.size(); ++i )
	{
		string mapname(m_Definitions[i].GetAnimationPath());

		if ((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0))
		{
			return &m_Definitions[i];
		}

	}

	return 0;
}

SAnimationDefinition * CAnimationInfoLoader::GetAnimationDefinitionFromModel(const string& name )
{
	string searchname(name);
	FixString(searchname);

	for (uint32 i = 0; i < m_Definitions.size(); ++i )
	{
		string mapname(m_Definitions[i].m_Model);
		FixString(mapname);

		if ((strstr(mapname.c_str(), searchname.c_str()) != 0) || (strstr(searchname.c_str(), mapname.c_str()) != 0))
		{
			return &m_Definitions[i];
		}

	}

	return 0;

}

/*
void ReportWarning(IAnimationLoaderListener* pListener, const char* szFormat, ...)
{

	va_list args;
	va_start(args, szFormat);
	char szBuffer[1024];
	vsprintf(szBuffer, szFormat, args);
	pListener->OnAnimationLoaderMessage(IAnimationLoaderListener::MESSAGE_WARNING, szBuffer);
	va_end(args);
}

void ReportInfo(IAnimationLoaderListener* pListener, const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	char szBuffer[1024];
	vsprintf(szBuffer, szFormat, args);
	pListener->OnAnimationLoaderMessage(IAnimationLoaderListener::MESSAGE_INFO, szBuffer);
	va_end(args);
}

void ReportError(IAnimationLoaderListener* pListener, const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	char szBuffer[1024];
	vsprintf(szBuffer, szFormat, args);
	pListener->OnAnimationLoaderMessage(IAnimationLoaderListener::MESSAGE_ERROR, szBuffer);
	va_end(args);
}
*/
