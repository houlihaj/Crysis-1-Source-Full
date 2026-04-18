#include "stdafx.h"
#include "AnimEventLoader.h"
#include "Model.h"
#include "CharacterManager.h"

// loads the data from the animation event database file (.animevent) - this is usually
// specified in the CAL file.
bool AnimEventLoader::loadAnimationEventDatabase( CCharacterModel* pModel, const char* pFileName )
{
	if (pFileName == 0 || pFileName[0] == 0)
		return false;

	// Parse the xml.
	XmlNodeRef root = g_pISystem->LoadXmlFile(pFileName);
	if (!root)
	{
		//AnimWarning("Animation Event Database (.animevent) file \"%s\" could not be read (associated with model \"%s\")", pFileName, pModel->GetFile() );
		return false;
	}

	// Load the events from the xml.
	uint32 numAnimations = root->getChildCount();
	for (uint32 nAnimationNode=0; nAnimationNode<numAnimations; ++nAnimationNode)
	{
		XmlNodeRef animationRoot = root->getChild(nAnimationNode);
		// Check whether this is an animation.
		if (string("animation") != animationRoot->getTag())
			continue;

		// Get the name of the animation.
		XmlString sName = animationRoot->getAttr("name");

		// Look up the animation id in the animation set.
		int nGlobalAnimID = g_AnimationManager.GetGlobalIDbyFilePath(sName.c_str());
		if (nGlobalAnimID != -1)
		{
			uint32 HowManyEvents = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_AnimEvents.size();
			if (HowManyEvents==0)
			{
				// Loop through the events for this animation.
				uint32 numEvents = animationRoot->getChildCount();
				for (uint32 nEventNode = 0; nEventNode<numEvents; ++nEventNode)
				{
					XmlNodeRef eventNode = animationRoot->getChild(nEventNode);

					// Check whether this is an event.
					if (string("event") != eventNode->getTag())
						continue;

					// Read the attributes of the event.
					XmlString sEventName;
					if (!(sEventName = eventNode->getAttr("name")))
						continue;
					float fTime;
					if (!eventNode->getAttr("time", fTime))
						continue;
					XmlString sParameter;
					if (!(sParameter = eventNode->getAttr("parameter")))
						continue;
					XmlString sBoneName;
					if (!(sBoneName = eventNode->getAttr("bone")))
						continue;
					Vec3 vOffset(0,0,0);
					eventNode->getAttr("offset", vOffset);
					Vec3 vDir(0,0,0);
					eventNode->getAttr("dir", vDir);

					AnimEvents aevents;
					aevents.m_time=fTime;
					aevents.m_strCustomParameter=sParameter;
					aevents.m_strEventName=sEventName;
					aevents.m_strBoneName=sBoneName;
					aevents.m_vOffset=vOffset;
					aevents.m_vDir=vDir;
					g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimID].m_AnimEvents.push_back(aevents);
				}
			}
		}	

	}

	return true;
}
