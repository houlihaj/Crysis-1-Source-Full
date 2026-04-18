////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MaterialFGManager
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialFGManager.h"

CMaterialFGManager::CMaterialFGManager()
{
	m_pFlowSystem = gEnv->pFlowSystem;
	assert (m_pFlowSystem != 0);
}

CMaterialFGManager::~CMaterialFGManager()
{
	m_flowGraphVector.clear();
}

//------------------------------------------------------------------------
void CMaterialFGManager::Reset()
{
	for(int i=0; i< m_flowGraphVector.size(); i++)
	{
		CMaterialFGManager::SFlowGraphData& current = m_flowGraphVector[i];
		InternalEndFGEffect(&current, true);
	}
}

//------------------------------------------------------------------------
void CMaterialFGManager::Serialize(TSerialize ser)
{
	for(int i=0; i< m_flowGraphVector.size(); i++)
	{
		CMaterialFGManager::SFlowGraphData& current = m_flowGraphVector[i];
		if (ser.BeginOptionalGroup(current.m_name,true))
		{
			ser.Value("run", current.m_bRunning);
			if (ser.BeginOptionalGroup("fg", current.m_bRunning))
			{
				current.m_pFlowGraph->Serialize(ser);
				ser.EndGroup();
			}
			ser.EndGroup();
		}
	}
}

//------------------------------------------------------------------------
// CMaterialFGManager::LoadLibs()
// Iterates through all the files in the folder and load the graphs
// PARAMS
// - path : Folder where the FlowGraph xml files are located
//------------------------------------------------------------------------
bool CMaterialFGManager::LoadLibs(const char* path)
{
	if (m_pFlowSystem == 0)
		return false;

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	int numLoaded = 0;

	string realPath (path);
	realPath.TrimRight("/\\");
	string search (realPath);
	search += "/*.xml";

	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			// fd.name contains the profile name
			string filename = realPath;
			filename += "/" ;
			filename += fd.name;
		bool ok = LoadFG(filename);
			if (ok)
				++numLoaded;
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );

		pCryPak->FindClose( handle );
	}

	return numLoaded > 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::LoadFG()
// Here is where the FlowGraph is loaded, storing a pointer to it, its name
// and also the IDs of the special "start" and "end" nodes
// PARAMS
// - filename: ...
//------------------------------------------------------------------------
bool CMaterialFGManager::LoadFG(const string& filename)
{
	//Create FG from the XML file
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFile(filename);
	if (rootNode == 0)
		return false;
	IFlowGraphPtr pFlowGraph = m_pFlowSystem->CreateFlowGraph();
	if (pFlowGraph->SerializeXML(rootNode, true) == false)
	{
		// give warning
		GameWarning("MaterialFGManager on LoadFG(%s)==> FlowGraph->SerializeXML failed",filename.c_str());
		return false;
	}

	//Deactivated it by default
	pFlowGraph->SetEnabled(false);

	const TFlowNodeId nodeTypeId_StartFX = m_pFlowSystem->GetTypeId("MaterialFX:HUDStartFX");
	const TFlowNodeId nodeTypeId_EndFX = m_pFlowSystem->GetTypeId("MaterialFX:HUDEndFX");

	//Store needed data...
	SFlowGraphData fgData;
	fgData.m_pFlowGraph = pFlowGraph;

	// search for start and end nodes
	IFlowNodeIteratorPtr pNodeIter = pFlowGraph->CreateNodeIterator();
	TFlowNodeId nodeId;
	while (IFlowNodeData* pNodeData = pNodeIter->Next(nodeId))
	{
		if (pNodeData->GetNodeTypeId() == nodeTypeId_StartFX)
		{
			fgData.m_startNodeId = nodeId;
		}
		else if (pNodeData->GetNodeTypeId() == nodeTypeId_EndFX)
		{
			fgData.m_endNodeId = nodeId;
		}
	}
	
	if (fgData.m_startNodeId == InvalidFlowNodeId || fgData.m_endNodeId == InvalidFlowNodeId)
	{
		// warning no start/end node found
		GameWarning("MaterialFGManager on LoadFG(%s) ==> No Start/End node found",filename.c_str());
		return false;
	}

	//Finally store the name
	fgData.m_name = PathUtil::GetFileName(filename);
	PathUtil::RemoveExtension(fgData.m_name);
	m_flowGraphVector.push_back(fgData);

#ifdef USER_benito
	CryLogAlways("MaterialFGManager on LoadFG(%s) ==> FlowGraph HUD effect loaded",filename.c_str());
#endif

	// send initialize event to allow for resource caching
	if (gEnv->pCryPak->GetLvlResStatus())
	{
		SFlowGraphData* pFGData = FindFG(pFlowGraph);
		if (pFGData)
			InternalEndFGEffect(pFGData, true);
	}
	return true;
}

//------------------------------------------------------------------------
// CMaterialFGManager::FindFG(const string& fgName)
// Find a FlowGraph by name
// PARAMS
// - fgName: Name of the FlowGraph
//------------------------------------------------------------------------
CMaterialFGManager::SFlowGraphData* CMaterialFGManager::FindFG(const string& fgName) 
{
	std::vector<SFlowGraphData>::iterator iter = m_flowGraphVector.begin();
	std::vector<SFlowGraphData>::iterator iterEnd = m_flowGraphVector.end();
	while (iter != iterEnd)
	{
		if (fgName.compareNoCase(iter->m_name) == 0)
			return &*iter;
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::FindFG(IFlowGraphPtr pFG)
// Find a FlowGraph "by pointer"
// PARAMS
// - pFG: FlowGraph pointer
//------------------------------------------------------------------------
CMaterialFGManager::SFlowGraphData* CMaterialFGManager::FindFG(IFlowGraphPtr pFG) 
{
	std::vector<SFlowGraphData>::iterator iter = m_flowGraphVector.begin();
	std::vector<SFlowGraphData>::iterator iterEnd = m_flowGraphVector.end();
	while (iter != iterEnd)
	{
		if (iter->m_pFlowGraph == pFG)
			return &*iter;
		++iter;
	}
	return 0;
}

//------------------------------------------------------------------------
// CMaterialFGManager::StartFGEffect(const string& fgName)
// Activate the MaterialFX:StartHUDEffect node, this way the FG will be executed
// PARAMS
// - fgName: Name of the flowgraph effect
//------------------------------------------------------------------------
bool CMaterialFGManager::StartFGEffect(const SMFXFlowGraphParams& fgParams, float curDistance)
{
	SFlowGraphData* pFGData = FindFG(fgParams.fgName);
	if (pFGData == 0)
	{
		GameWarning("CMaterialFXManager::StartFXEffect: Can't execute FG '%s'", fgParams.fgName.c_str());
	}
	else if (/*pFGData && */!pFGData->m_bRunning)
	{
		IFlowGraphPtr pFG = pFGData->m_pFlowGraph;
		pFG->SetEnabled(true);
		pFG->InitializeValues();
		
		TFlowInputData data;

		// set distance output port [1]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 1, true), curDistance);
		// set custom params [2] - [5]
		for (int i=0; i<4; ++i)
			pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, i+2, true), fgParams.params[i]);

		// set input port [0]
		pFG->ActivatePort(SFlowAddress(pFGData->m_startNodeId, 0, false), data);	// port0: InputPortConfig_Void ("Trigger")
		//data = fgName;
		//pFG->ActivatePort(SFlowAddress(pFGData->m_endNodeId, 0, false), data);		// port0: InputPortConfig<string> ("FGName")
		pFGData->m_bRunning = true;

		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(const string& fgName)
// This method will be automatically called when the effect finish
// PARAMS
// - fgName: Name of the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::EndFGEffect(const string& fgName)
{
	SFlowGraphData* pFGData = FindFG(fgName);
	if (pFGData)
		return InternalEndFGEffect(pFGData, false);
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
// This method will be automatically called when the effect finish
// PARAMS
// - pFG: Pointer to the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
{
	SFlowGraphData* pFGData = FindFG(pFG);
	if (pFGData)
		return InternalEndFGEffect(pFGData, false);
	return false;
}

//------------------------------------------------------------------------
// internal method to end effect. can also initialize values
//------------------------------------------------------------------------
bool CMaterialFGManager::InternalEndFGEffect(SFlowGraphData* pFGData, bool bInitialize)
{
	if (pFGData == 0)
		return false;
	if (pFGData->m_bRunning || bInitialize)
	{
		IFlowGraphPtr pFG = pFGData->m_pFlowGraph;
		if (bInitialize)
		{
			if (pFG->IsEnabled() == false)
			{
				pFG->SetEnabled(true);
				pFG->InitializeValues();
			}
		}
		pFG->SetEnabled(false);
		pFGData->m_bRunning = false;
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::EndFGEffect(IFlowGraphPtr pFG)
// This method will be automatically called when the effect finish
// PARAMS
// - pFG: Pointer to the FlowGraph
//------------------------------------------------------------------------
bool CMaterialFGManager::IsFGEffectRunning(const string& fgName)
{
	SFlowGraphData* pFGData = FindFG(fgName);
	if (pFGData)
		return pFGData->m_bRunning;
	return false;
}

//------------------------------------------------------------------------
// CMaterialFGManager::ReloadFlowGraphs
// Reload the FlowGraphs (invoked through console command, see MaterialEffectsCVars.cpp)
//------------------------------------------------------------------------
void CMaterialFGManager::ReloadFlowGraphs()
{
		m_flowGraphVector.resize(0);

		LoadLibs();
}

void CMaterialFGManager::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "flowgraphs");
	s->Add(*this);
	s->AddContainer(m_flowGraphVector);
	for (std::vector<SFlowGraphData>::iterator iter = m_flowGraphVector.begin(); iter != m_flowGraphVector.end(); ++iter)
		s->Add(iter->m_name);
}
