//같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같�
//  File name:      MusicEditorUI.h
//  Version:        v1.00
//  Last modified:  (21/06/98)
//  Compilers:      Visual C++.NET
//  Description:    
// -------------------------------------------------------------------------
//  Author: Timur Davidenko (timur@crytek.com)
// -------------------------------------------------------------------------
//
//  You are not permitted to distribute, sell or use any part of
//  this source for your software without written permision of author.
//
//같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같�

#ifndef __MusicEditorUI__
#define __MusicEditorUI__
#pragma once

#include <Cry_Math.h>
#include "MusicEditorDialog.h"

//////////////////////////////////////////////////////////////////////////
// Base class for music UIs
class CMusicEditorUI
{
public:
	CVariableArray mainTable;
	CVarBlockPtr m_vars;
	IVariable::OnSetCallback m_onSetCallback;

	virtual CVarBlock* CreateVars() = 0;
protected:
	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVariableArray &varArray,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		var.AddRef(); // Variables are local and must not be released by CVarBlock.
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		if (m_onSetCallback)
			var.AddOnSetCallback(m_onSetCallback);
		varArray.AddChildVar(&var);
	}
	//////////////////////////////////////////////////////////////////////////
	void AddVariable( CVarBlock *vars,CVariableBase &var,const char *varName,unsigned char dataType=IVariable::DT_SIMPLE )
	{
		var.AddRef(); // Variables are local and must not be released by CVarBlock.
		if (varName)
			var.SetName(varName);
		var.SetDataType(dataType);
		if (m_onSetCallback)
			var.AddOnSetCallback(m_onSetCallback);
		vars->AddVariable(&var);
	}
};

//////////////////////////////////////////////////////////////////////////
class CMusicEditorUI_Theme : public CMusicEditorUI
{
public:
	// default mood
	CVariableArray bridgesTable;
	CVariable<CString> defaultMood;
	CVariable<float> defaultMoodTimeout;
	CVariable<float> test;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		if (m_vars)
			return m_vars;
		m_vars = new CVarBlock;
		AddVariable( m_vars,mainTable,"Theme Properties" );
		AddVariable( m_vars,bridgesTable,"Bridges" );
		AddVariable( mainTable,defaultMood,"Default Mood" );
		AddVariable( mainTable,defaultMoodTimeout,"Default Mood Timeout" );
		return m_vars;
	}
	//////////////////////////////////////////////////////////////////////////
	void Set( const SMusicTheme *pTheme )
	{
		defaultMoodTimeout = pTheme->fDefaultMoodTimeout;
		defaultMood = pTheme->sDefaultMood.c_str();

		bridgesTable.DeleteAllChilds();
		for (TThemeBridgeMap::const_iterator it = pTheme->mapBridges.begin(); it != pTheme->mapBridges.end(); ++it)
		{
			const char *sTheme = it->first.c_str();
			const char *sPattern = it->second.c_str();
			CVariable<CString> *pBridgeVar = new CVariable<CString>;
			pBridgeVar->SetDataType( IVariable::DT_SIMPLE );
			pBridgeVar->SetName( sTheme );
			pBridgeVar->Set( sPattern );
			bridgesTable.AddChildVar( pBridgeVar );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Get( SMusicTheme *pTheme )
	{
		pTheme->fDefaultMoodTimeout = defaultMoodTimeout;
		const char *sDefMood  = (CString)defaultMood;
		pTheme->sDefaultMood = sDefMood;
	}
};

//////////////////////////////////////////////////////////////////////////
class CMusicEditorUI_Mood : public CMusicEditorUI
{
public:
	CVariable<int> nPriority;
	CVariable<float> fFadeOutTime;
	CVariable<bool> bPlaySingle;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		if (m_vars)
			return m_vars;
		m_vars = new CVarBlock;
		AddVariable( m_vars,mainTable,"Mood Properties" );
		AddVariable( mainTable,nPriority,"Priority" );
		AddVariable( mainTable,fFadeOutTime,"Fade Out Time" );
		AddVariable( mainTable,bPlaySingle,"Play Single" );
		return m_vars;
	}
	//////////////////////////////////////////////////////////////////////////
	void Set( const SMusicMood *pMood )
	{
		nPriority = pMood->nPriority;
		fFadeOutTime = pMood->fFadeOutTime;
		bPlaySingle = pMood->bPlaySingle;
	}

	//////////////////////////////////////////////////////////////////////////
	void Get( SMusicMood *pMood )
	{
		pMood->nPriority = nPriority;
		pMood->fFadeOutTime = fFadeOutTime;
		pMood->bPlaySingle = bPlaySingle;
	}
};

//////////////////////////////////////////////////////////////////////////
class CMusicEditorUI_PatternSet : public CMusicEditorUI
{
public:
	CVariable<float> fMinTimeout;
	CVariable<float> fMaxTimeout;
	
	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		if (m_vars)
			return m_vars;
		m_vars = new CVarBlock;
		AddVariable( m_vars,mainTable,"Pattern Set Properties" );
		AddVariable( mainTable,fMinTimeout,"Min Time Out" );
		AddVariable( mainTable,fMaxTimeout,"Max Time Out" );
		return m_vars;
	}
	//////////////////////////////////////////////////////////////////////////
	void Set( const SMusicPatternSet *pPatternSet )
	{
		fMinTimeout = pPatternSet->fMinTimeout;
		fMaxTimeout = pPatternSet->fMaxTimeout;
	}

	//////////////////////////////////////////////////////////////////////////
	void Get( SMusicPatternSet *pPatternSet )
	{
		pPatternSet->fMinTimeout = fMinTimeout;
		pPatternSet->fMaxTimeout = fMaxTimeout;
	}
};

//////////////////////////////////////////////////////////////////////////
class CMusicEditorUI_Layer : public CMusicEditorUI
{
public:
	CVariable<float> fProbability;
	CVariable<int> nMaxSimultaneousPatterns;
	CVariable<int> nLayerType;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = 0;
		mainTable.DeleteAllChilds();
		m_vars = new CVarBlock;
		switch (nLayerType)
		{
		case CMusicEditorDialog::ELAYER_MAIN:
			AddVariable( m_vars,mainTable,"Main Layer Properties" );
			break;
		case CMusicEditorDialog::ELAYER_RHYTHMIC:
			AddVariable( m_vars,mainTable,"Rhythmic Layer Properties" );
			AddVariable( mainTable,fProbability,"Probability" );
			AddVariable( mainTable,nMaxSimultaneousPatterns,"Max Simultaneous Patterns" );
			break;
		case CMusicEditorDialog::ELAYER_INCIDENTAL:
			AddVariable( m_vars,mainTable,"Incidental Layer Properties" );
			AddVariable( mainTable,fProbability,"Probability" );
			AddVariable( mainTable,nMaxSimultaneousPatterns,"Max Simultaneous Patterns" );
			break;
		case CMusicEditorDialog::ELAYER_START:
			AddVariable( m_vars,mainTable,"Start Layer Properties" );
			break;
		case CMusicEditorDialog::ELAYER_END:
			AddVariable( m_vars,mainTable,"End Layer Properties" );
			break;
		case CMusicEditorDialog::ELAYER_STINGER:
			AddVariable( m_vars,mainTable,"Stinger Layer Properties" );
			break;
		};
		
		return m_vars;
	}
	//////////////////////////////////////////////////////////////////////////
	void Set( const SMusicPatternSet *pPatternSet,int layer )
	{
		switch (layer)
		{
		case CMusicEditorDialog::ELAYER_MAIN:
			break;
		case CMusicEditorDialog::ELAYER_RHYTHMIC:
			fProbability = pPatternSet->fRhythmicLayerProbability;
			nMaxSimultaneousPatterns = pPatternSet->nMaxSimultaneousRhythmicPatterns;
			break;
		case CMusicEditorDialog::ELAYER_INCIDENTAL:
			fProbability = pPatternSet->fIncidentalLayerProbability;
			nMaxSimultaneousPatterns = pPatternSet->nMaxSimultaneousIncidentalPatterns;
			break;
		case CMusicEditorDialog::ELAYER_START:
			break;
		case CMusicEditorDialog::ELAYER_END:
			break;
		case CMusicEditorDialog::ELAYER_STINGER:
			break;
		};
	}

	//////////////////////////////////////////////////////////////////////////
	void Get( SMusicPatternSet *pPatternSet,int layer )
	{
		switch (layer)
		{
		case CMusicEditorDialog::ELAYER_MAIN:
			break;
		case CMusicEditorDialog::ELAYER_RHYTHMIC:
			pPatternSet->fRhythmicLayerProbability = fProbability;
			pPatternSet->nMaxSimultaneousRhythmicPatterns = nMaxSimultaneousPatterns;
			break;
		case CMusicEditorDialog::ELAYER_INCIDENTAL:
			pPatternSet->fIncidentalLayerProbability = fProbability;
			pPatternSet->nMaxSimultaneousIncidentalPatterns = nMaxSimultaneousPatterns;
			break;
		case CMusicEditorDialog::ELAYER_START:
			break;
		case CMusicEditorDialog::ELAYER_END:
			break;
		case CMusicEditorDialog::ELAYER_STINGER:
			break;
		};
	}
};

//////////////////////////////////////////////////////////////////////////
class CMusicEditorUI_Pattern : public CMusicEditorUI
{
public:
	CVariable<CString>	sFilename;
	CVariable<CString>	sFadePoints;
	CVariable<int>			nPreFadeIn;
	CVariable<float>		fLayeringVolume;
	CVariable<float>		fProbability;
	CVariable<int>			nLayerType;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		m_vars = 0;
		mainTable.DeleteAllChilds();
		m_vars = new CVarBlock;

		AddVariable( m_vars,mainTable,"Pattern Properties" );
		AddVariable( mainTable,sFilename,"Filename",IVariable::DT_SOUND );
		AddVariable( mainTable,sFadePoints,"Fade Points" );
		AddVariable( mainTable,nPreFadeIn,"Pre-FadeIn" );
		AddVariable( mainTable,fLayeringVolume,"Volume" );
		fLayeringVolume.SetLimits(0.0f, 1.0f);
		
		// dont add probability for start/end/stinger
		if (nLayerType < CMusicEditorDialog::ELAYER_START)
			AddVariable( mainTable,fProbability,"Probability" );
		
		return m_vars;
	}
	//////////////////////////////////////////////////////////////////////////
	void Set( const SPatternDef *pPattern )
	{
		fLayeringVolume = pPattern->fLayeringVolume;
		sFilename = pPattern->sFilename.c_str();
		nPreFadeIn = pPattern->nPreFadeIn;
		
		if (nLayerType < CMusicEditorDialog::ELAYER_START)
			fProbability = pPattern->fProbability;

		CString str;
		for (int i = 0; i < pPattern->vecFadePoints.size(); i++)
		{
			CString temp;
			if (i > 0)
				temp.Format( ",%d",(int)pPattern->vecFadePoints[i] );
			else
				temp.Format( "%d",(int)pPattern->vecFadePoints[i] );
			str += temp;
		}
		sFadePoints = str;
	}

	//////////////////////////////////////////////////////////////////////////
	void Get( SPatternDef *pPattern )
	{
		CString sfile = sFilename;
		pPattern->sFilename = (const char*)sfile;
		pPattern->fLayeringVolume = min(1.0f, (float)fLayeringVolume);
		pPattern->nPreFadeIn = nPreFadeIn;
		
		if (nLayerType < CMusicEditorDialog::ELAYER_START)
			pPattern->fProbability = fProbability;
		
		CString str = sFadePoints;
		int iStart = 0;
		pPattern->vecFadePoints.clear();
		CString token = TokenizeString( str,",",iStart );
		while (!token.IsEmpty())
		{
			pPattern->vecFadePoints.push_back( atoi(token) );
			token = TokenizeString( str,",",iStart );
		}
	}
};

#endif // __MusicEditorUI__
