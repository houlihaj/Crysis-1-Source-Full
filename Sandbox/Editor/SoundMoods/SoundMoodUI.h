//같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같�
//  File name:      SoundMoodUI.h
//  Version:        v1.00
//  Last modified:  (11/07/05)
//  Compilers:      Visual C++.NET
//  Description:    
// -------------------------------------------------------------------------
//  Author: Tomas Neumann
// -------------------------------------------------------------------------
//
//  You are not permitted to distribute, sell or use any part of
//  this source for your software without written permision of author.
//
//같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같같�

#ifndef __SoundMoodUI__
#define __SoundMoodUI__
#pragma once

//#include "Util/Variable.h"

//////////////////////////////////////////////////////////////////////////
// Base class for SoundMood UIs
class CSoundMoodUI
{
public:
	CVariableArray m_mainTable;
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
class CSoundMoodUI_CatProps : public CSoundMoodUI
{
public:
	
	// default properties
	CVariableArray m_Properties;
	CVariable<float> m_fVolume;
	CVariable<float> m_fPitch;
	CVariable<float> m_fLowPass;
	CVariable<float> m_fHighPass;
	//CVariable<int> m_nProbability;
	//CVariable<int> m_nTimeOutMS;
	//CVariable<bool> m_bLooped;
	//CVariable<CString> m_sFileName;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		if (m_vars)
			return m_vars;
		m_vars = new CVarBlock;
		
		// Properties
		AddVariable( m_vars,m_Properties,"Properties" );
		AddVariable( m_Properties,m_fVolume,"Volume [0..1]" );
		m_fVolume.SetLimits(0.0f, 1.0f);
		m_fVolume.Set(1.0f);
		AddVariable( m_Properties,m_fPitch,"Pitch [-4..4]" );
		m_fPitch.SetLimits(-4.0f, 4.0f);
		AddVariable( m_Properties,m_fLowPass,"LowPass Filter [10..22,000]" );
		m_fLowPass.SetLimits(10.0f, 22000.0f);
		AddVariable( m_Properties,m_fHighPass,"HighPass Filter [10..22,000]" );
		m_fHighPass.SetLimits(10.0f, 22000.0f);

		//AddVariable( m_Properties,m_sFileName,"FileName",IVariable::DT_FILE );
		//AddVariable( m_Properties,m_bLooped,"Looped" );

		return m_vars;
	}

	//////////////////////////////////////////////////////////////////////////
	// Writes the current state of SoundGroups to the UI
	void ReadFromPtr( ICategory *pElement )
	{
		enumSG_SEARCHDIRECTION eSearchDirection = eELEMENTONLY;
		//int										nFlags		= 0;
		//int32									nIntValue = 0;
		//ptParamINT32			ParamInt32(nIntValue);
		//string								sStringValue;
		//ptParamCRYSTRING	ParamString(sStringValue);
		//bool							bBoolValue = true;
		//ptParamBOOL				ParamBool(bBoolValue);
		
		// Volume
		{
			f32 							fValue = 1.0f;
			ptParamF32				ParamF32(fValue);

			//if (pElement->GetParam(gspFVOLUME, &ParamF32))

			// TODO Fix n / f
			pElement->GetParam(gspFVOLUME, &ParamF32);
			ParamF32.GetValue(fValue);
			m_fVolume.Set( fValue );
		}

		// Pitch
		{
			f32 							fValue = 0.0f;
			ptParamF32				ParamF32(fValue);

			//if (pElement->GetParam(gspFPITCH, &ParamF32))

				// TODO Fix n / f
			pElement->GetParam(gspFPITCH, &ParamF32);
			ParamF32.GetValue(fValue);
			m_fPitch.Set( fValue );
		}


		// LowPass
		{
			f32 							fValue = 22000.0f;
			ptParamF32				ParamF32(fValue);

			//if (pElement->GetParam(gspFLOWPASS, &ParamF32))

				// TODO Fix n / f
			pElement->GetParam(gspFLOWPASS, &ParamF32);
			ParamF32.GetValue(fValue);
			m_fLowPass.Set( fValue );
		}

		// HighPass
		{
			f32 							fValue = 10.0f;
			ptParamF32				ParamF32(fValue);

			//if (pElement->GetParam(gspFLOWPASS, &ParamF32))

			// TODO Fix n / f
			pElement->GetParam(gspFHIGHPASS, &ParamF32);
			ParamF32.GetValue(fValue);
			m_fHighPass.Set( fValue );
		}

	}

	//////////////////////////////////////////////////////////////////////////
	// Gives the current state of the UI to the SoundGroup manager
	ICategory* WriteToPtr( ICategory *pElement )
	{
		XmlNodeRef ElementNode = CreateXmlNode("Category");
		ElementNode->setAttr("Name", pElement->GetName());
		f32 fValue = 0.0f;

		// VolumeDB
		// insert parameter to xml node
		fValue = 1.0f;
		m_fVolume.Get(fValue);
		ElementNode->setAttr("Volume", fValue);

		// Priority
		// insert parameter to xml node
		fValue = 0.0f;
		m_fPitch.Get(fValue);
		ElementNode->setAttr("Pitch", fValue);

		// Probability
		// insert parameter to xml node
		fValue = 22000.0f;
		m_fLowPass.Get(fValue);
		ElementNode->setAttr("LowPass", fValue);

		fValue = 10.0f;
		m_fHighPass.Get(fValue);
		ElementNode->setAttr("HighPass", fValue);


		//CategoryHandle ParentHandle = pElement->GetParentHandle();
		//ICategory *pParent = pManager->GetCategoryPtr(ParentHandle);
		//CategoryHandle NewElementHandle;

		//pElement->ClearParam();

		//if (!pParent)
			//ElementNode->setTag("SoundMoodsGroup");
		//{
			//pParent->RemoveChild(pElement->GetHandle());
			//NewElementHandle = pParent->AddChild(ElementNode->getAttr("Name"));
		//}
		//else
		//{
			//pManager->RemoveGroup(pElement->GetHandle());
			//NewElementHandle = pManager->AddGroup(ElementNode->getAttr("Name"));
//		}

		//pElement = pManager->GetElementPtr(NewElementHandle);

		pElement->Serialize(ElementNode, true);
		
		ISoundMoodManager *pManager = pElement->GetIMoodManager();

		if (pManager)
			pManager->Update();
		
		return pElement;
	}
};


//////////////////////////////////////////////////////////////////////////
class CSoundMoodUI_MoodProps : public CSoundMoodUI
{
public:

	// default properties
	CVariable<float> m_fPriority;
	CVariable<float> m_fMusicVolume;

	//////////////////////////////////////////////////////////////////////////
	CVarBlock* CreateVars()
	{
		if (m_vars)
			return m_vars;
		m_vars = new CVarBlock;

		// Properties
		AddVariable( m_vars,m_fPriority,"Priority" );
		m_fPriority.SetLimits(1.0f, 100.0f);
		m_fPriority.Set(1.0f);

		AddVariable( m_vars,m_fMusicVolume,"MusicVolume" );
		m_fMusicVolume.SetLimits(0.0f, 1.0f);
		m_fMusicVolume.Set(0.8f);

		return m_vars;
	}

	//////////////////////////////////////////////////////////////////////////
	// Writes the current state of SoundGroups to the UI
	void ReadFromPtr( IMood *pElement )
	{
		m_fPriority.Set(pElement->GetPriority());
		m_fMusicVolume.Set(pElement->GetMusicVolume());
	}

	//////////////////////////////////////////////////////////////////////////
	// Gives the current state of the UI to the SoundGroup manager
	IMood* WriteToPtr( IMood *pElement )
	{
		XmlNodeRef ElementNode = CreateXmlNode("Mood");
		ElementNode->setAttr("Name", pElement->GetName());

		float fValue = 1.0f;
		m_fPriority.Get(fValue);
		ElementNode->setAttr("Priority", fValue);

		fValue = 1.0f;
		m_fMusicVolume.Get(fValue);
		ElementNode->setAttr("MusicVolume", fValue);

		pElement->Serialize(ElementNode, true);
		return pElement;
	}
};

#endif
