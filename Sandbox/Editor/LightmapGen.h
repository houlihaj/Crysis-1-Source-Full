////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   lightmapgen.h
//  Version:     v1.00
//  Created:     18/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __lightmapgen_h__
#define __lightmapgen_h__
#pragma once

#include "LMCompStructures.h" 
#include "Objects\BrushObject.h"

class CLightmapGen
{
public:
	CLightmapGen(volatile SSharedLMEditorData *pSharedData = NULL) : m_pSharedData(pSharedData){};
	~CLightmapGen(){};

	//! Generate lightmaps.
	const bool GenerateSelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);
	const bool GenerateAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);
	const bool GenerateChanged(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);

	const bool SetupLightmapQualityAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);
	const bool SetupLightmapQualitySelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);

	const bool ShowTexelDensityAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);
	const bool ShowTexelDensitySelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL);

	const bool GenerateLightVolume( IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL );

	const bool GenerateLightList( IEditor *pIEditor, ICompilerProgress *pICompilerProgress = NULL );

	static void SetGenParam(LMGenParam sParam) { m_sParam = sParam; };
	static LMGenParam GetGenParam() { return m_sParam; };

	static void Serialize(CXmlArchive xmlAr);
	
protected:
	volatile SSharedLMEditorData* m_pSharedData;

	static LMGenParam m_sParam;

	const bool Generate(ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, ICompilerProgress *pICompilerProgress,
		const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices);

	const bool SetupLightmapQuality(ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, ICompilerProgress *pICompilerProgress,
		const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices);

	const bool ShowTexelDensity(ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, ICompilerProgress *pICompilerProgress,
		const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices);

	bool IsLM_GLM(CBaseObject* pObject, struct IRenderNode **pIEtyRend);


	bool CLightmapGen::CreateLists( IEditor *pIEditor, const bool bInSelectionMode );
	std::vector<std::pair<IRenderNode*,CBaseObject*> > nodes;
	std::set<const CBaseObject*> vSelection;
};

#endif // __lightmapgen_h__
