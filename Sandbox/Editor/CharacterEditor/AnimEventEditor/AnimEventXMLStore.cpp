//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------

#include "StdAfx.h"
#include "AnimEventXMLStore.h"
#include "IAnimEventVisitable.h"
#include "IAnimEventDocVisitor.h"
#include "IAnimEventDoc.h"
#include <map>

class AnimEventXMLStoreImpl : public AnimEventXMLStore, public IAnimEventVisitable, public IAnimEventDocVisitor
{
public:
	AnimEventXMLStoreImpl();
	virtual ~AnimEventXMLStoreImpl();

	virtual void SetFileName(const string& sFileName);
	virtual bool Save(IAnimEventDoc* pDoc);
	virtual bool Load(IAnimEventDoc* pDoc);
	virtual string GetErrorString();
	virtual void Visit(IAnimEventDocVisitor* pVisitor);
	virtual void VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected);

private:
	class Event
	{
	public:
		Event(const string& sName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir)
			: sName(sName), fTime(fTime), sParameter(szParameter), sBone(szBone), vOffset(vOffset), vDir(vDir) {}
		string sName;
		float fTime;
		string sParameter;
		string sBone;
		Vec3 vOffset;
		Vec3 vDir;
	};
	typedef std::map<string, std::vector<Event> > AnimationMap;
	AnimationMap animationMap;

	string sFileName;
	string sError;
};

AnimEventXMLStore* AnimEventXMLStore::Create()
{
	return new AnimEventXMLStoreImpl();
}

void AnimEventXMLStoreImpl::SetFileName(const string& sFileName)
{
	this->sFileName = sFileName;
}

AnimEventXMLStoreImpl::AnimEventXMLStoreImpl()
{
}

AnimEventXMLStoreImpl::~AnimEventXMLStoreImpl()
{
}

bool AnimEventXMLStoreImpl::Save(IAnimEventDoc* pDoc)
{
	// Clear out the document.
	this->animationMap.clear();

	// Build up a list of the events, by visiting the document.
	pDoc->GetVisitable()->Visit(this);

	// Construct an xml tree representing the document.
	XmlNodeRef root = CreateXmlNode("anim_event_list");

	// Loop through all the animations.
	for (AnimationMap::iterator itAnimation = this->animationMap.begin(); itAnimation != this->animationMap.end(); ++itAnimation)
	{
		const string& sName = (*itAnimation).first;
		const std::vector<Event>& events = (*itAnimation).second;

		// Create a new node for this animation.
		XmlNodeRef animationRoot = root->newChild("animation");
		animationRoot->setAttr("name", sName.c_str());

		// Loop through all the events in this animation.
		for (std::vector<Event>::const_iterator itEvent = events.begin(); itEvent != events.end(); ++itEvent)
		{
			// Create a new node for this event.
			XmlNodeRef eventNode = animationRoot->newChild("event");
			eventNode->setAttr("name", (*itEvent).sName.c_str());
			eventNode->setAttr("time", (*itEvent).fTime);
			eventNode->setAttr("parameter", (*itEvent).sParameter.c_str());
			eventNode->setAttr("bone", (*itEvent).sBone.c_str());
			eventNode->setAttr("offset", (*itEvent).vOffset);
			eventNode->setAttr("dir", (*itEvent).vOffset);
		}
	}

	// Write the xml to file.
	string sFileName = Path::GamePathToFullPath(this->sFileName.c_str());
	SaveXmlNode( root,this->sFileName.c_str());

	return true;
}

bool AnimEventXMLStoreImpl::Load(IAnimEventDoc* pDoc)
{
	// Clear out the document.
	this->animationMap.clear();

	// Parse the xml.
	XmlParser parser;
	XmlNodeRef root = parser.parse(this->sFileName);
	if (!root)
	{
		this->sError = "XML Error";
		//this->sError = parser.getErrorString();
		return false;
	}

	// Load the events from the xml.
	for (int nAnimationNode = 0; nAnimationNode < root->getChildCount(); ++nAnimationNode)
	{
		XmlNodeRef animationRoot = root->getChild(nAnimationNode);

		// Check whether this is an animation.
		if (string("animation") != animationRoot->getTag())
			continue;

		// Get the name of the animation.
		XmlString sName = animationRoot->getAttr("name");

		// Create an animation
		AnimationMap::iterator itAnimation = this->animationMap.insert(std::make_pair(sName, std::vector<Event>())).first;
		std::vector<Event>& events = (*itAnimation).second;

		// Loop through the events for this animation.
		for (int nEventNode = 0; nEventNode < animationRoot->getChildCount(); ++nEventNode)
		{
			XmlNodeRef eventNode = animationRoot->getChild(nEventNode);

			// Check whether this is an event.
			if (string("event") != eventNode->getTag())
				continue;

			// Read the attributes of the event.
			XmlString sEventName;
			if (!(sEventName = eventNode->getAttr("name")))
				sEventName = "__unnamed__";
			float fTime;
			if (!eventNode->getAttr("time", fTime))
				fTime = 0.0f;
			string sParameter;
			if (eventNode->haveAttr("parameter"))
				sParameter = eventNode->getAttr("parameter");
			string sBone;
			if (eventNode->haveAttr("bone"))
				sBone = eventNode->getAttr("bone");
			Vec3 vOffset;
			if (!eventNode->getAttr("offset", vOffset))
				vOffset = Vec3(0, 0, 0);
			Vec3 vDir;
			if (!eventNode->getAttr("dir", vDir))
				vDir = Vec3(0, 0, 0);

			// Add the event to the animation.
			events.push_back(Event(sEventName, fTime, sParameter, sBone, vOffset, vDir));
		}
	}

	// Construct the document from the events we have loaded.
	pDoc->Build(this);

	return true;
}

void AnimEventXMLStoreImpl::Visit(IAnimEventDocVisitor* pVisitor)
{
	// Loop through all the animations.
	for (AnimationMap::iterator itAnimation = this->animationMap.begin(); itAnimation != this->animationMap.end(); ++itAnimation)
	{
		const string& sPath = (*itAnimation).first;
		const std::vector<Event>& events = (*itAnimation).second;

		// Loop through all the events in this animation.
		for (std::vector<Event>::const_iterator itEvent = events.begin(); itEvent != events.end(); ++itEvent)
		{
			// Visit the event - don't pass in a valid id - this should be assigned by the visitor.
			pVisitor->VisitEvent(0, sPath, (*itEvent).sName, (*itEvent).fTime, (*itEvent).sParameter, (*itEvent).sBone, (*itEvent).vOffset, (*itEvent).vDir, false);
		}
	}
}

void AnimEventXMLStoreImpl::VisitEvent(int nID, const char* szAnimPath, const char* szName, float fTime, const char* szParameter, const char* szBone, const Vec3& vOffset, const Vec3& vDir, bool bSelected)
{
	// Look for the animation in the animation map. If this is the first event for a particular animation, then
	// add a new animation.
	AnimationMap::iterator itAnimationMap = this->animationMap.find(szAnimPath);
	if (itAnimationMap == this->animationMap.end())
		itAnimationMap = this->animationMap.insert(std::make_pair(string(szAnimPath), std::vector<Event>())).first;

	// Add the event to the animation.
	(*itAnimationMap).second.push_back(Event(szName, fTime, szParameter, szBone, vOffset, vDir));
}

string AnimEventXMLStoreImpl::GetErrorString()
{
	return this->sError;
}
