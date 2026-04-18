////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MaterialFGManager
//  Version:     v1.00
//  Created:     29/11/2006 by AlexL-Benito GR
//  Compilers:   Visual Studio.NET
//  Description: This class manage all the FlowGraph HUD effects related to a
//								material FX.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MATERIALFGMANAGER_H__
#define __MATERIALFGMANAGER_H__

#pragma once

#include <IFlowSystem.h>
#include "MaterialEffects/MFXFlowGraphEffect.h"

class CMaterialFGManager
{
	public:
		CMaterialFGManager();
		virtual ~CMaterialFGManager();

		// load FlowGraphs from specified path
		bool LoadLibs(const char* path = "Libs/MaterialEffects/FlowGraphs");

		// reset (deactivate all FlowGraphs)
		void Reset();

		// serialize
		void Serialize(TSerialize ser);

		bool StartFGEffect(const SMFXFlowGraphParams& fgParams, float curDistance);
		bool EndFGEffect(const string & fgName);
		bool EndFGEffect(IFlowGraphPtr pFG);
		bool IsFGEffectRunning(const string& fgName);

		void ReloadFlowGraphs();			

		void GetMemoryStatistics(ICrySizer * s);

	protected:
		struct SFlowGraphData
		{
			SFlowGraphData()
			{
				m_startNodeId = InvalidFlowNodeId;
				m_endNodeId = InvalidFlowNodeId;
				m_bRunning = false;
			}

			string				m_name;
			IFlowGraphPtr	m_pFlowGraph;
			TFlowNodeId   m_startNodeId;
			TFlowNodeId   m_endNodeId;
			bool          m_bRunning;
		};

		bool LoadFG(const string& filename);
		SFlowGraphData* FindFG(const string& fgName);
		SFlowGraphData* FindFG(IFlowGraphPtr pFG);
		bool InternalEndFGEffect(SFlowGraphData* pFGData, bool bInitialize);

	protected:
	
		IFlowSystem* m_pFlowSystem;
		std::vector<SFlowGraphData> m_flowGraphVector;		//List of FlowGraph Effects
};

#endif

