////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FlowGraphVariables.cpp
//  Version:     v1.00
//  Created:     20/3/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowGraphVariables.h"
#include "FlowGraph.h"
#include "Objects/Entity.h"
#include "Util/Variable.h"

#include "UIEnumsDatabase.h"

#include <IMovieSystem.h>
#include <IAnimationGraphSystem.h>
#include <ICryAnimation.h>
#include <IDialogSystem.h>

namespace
{
	typedef IVariable::IGetCustomItems::SItem SItem;

	//////////////////////////////////////////////////////////////////////////
	bool ChooseDialog(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		IDialogSystem *pDS = gEnv->pDialogSystem;
		if (!pDS)
			return false;

		IDialogScriptIteratorPtr pIter = pDS->CreateScriptIterator();
		if (!pIter)
			return false;

		IDialogScriptIterator::SDialogScript script;
		while (pIter->Next(script))
		{
			SItem item;
			item.name = script.id;
			item.desc = _T("Script: ");
			item.desc+= script.id;
			if (script.bIsLegacyExcel)
			{
				item.desc+= _T(" [Excel]");
			}
			item.desc.AppendFormat(" - Required Actors: %d", script.numRequiredActors);
			if (script.desc && script.desc[0] != '\0')
			{
				item.desc += _T("\r\nDescription: ");
				item.desc += script.desc;
			}
			outItems.push_back(item);
		}
		outText = _T("Choose Dialog");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseSequence(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		IMovieSystem *pMovieSys = GetIEditor()->GetMovieSystem();
		ISequenceIt* pIter = pMovieSys->GetSequences();
		IAnimSequence* pSeq = pIter->first();
		while (pSeq != 0)
		{
			const char* seqName = pSeq->GetName();
			outItems.push_back(SItem(seqName));
			pSeq = pIter->next();
		}
		pIter->Release();
		outText = _T("Choose Sequence");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	bool GetValue(CFlowNodeGetCustomItemsBase* pGetCustomItems, const CString& portName, T& value)
	{
		CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
		if (pFlowNode == 0)
			return false;

		assert (pFlowNode != 0);

		// Get the CryAction FlowGraph
		IFlowGraph* pGameFG = pFlowNode->GetIFlowGraph();

		CHyperNodePort* pPort = pFlowNode->FindPort(portName, true);
		if (!pPort)
			return false;
		const TFlowInputData* pFlowData = pGameFG->GetInputValue(pFlowNode->GetFlowNodeId(), pPort->nPortIndex);
		if (!pFlowData)
			return false;
		assert (pFlowData != 0);

		const bool success = pFlowData->GetValueWithConversion(value);
		return success;
	}

	typedef std::map<CString, CString> TParamMap;
	bool GetParamMap(CFlowNodeGetCustomItemsBase* pGetCustomItems, TParamMap& outMap)
	{
		CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
		assert (pFlowNode != 0);

		const CString& uiConfig = pGetCustomItems->GetUIConfig();
		// fill in Enum Pairs
		const CString& values = uiConfig;
		int pos = 0;
		CString resToken = TokenizeString( values, " ,", pos);
		while (!resToken.IsEmpty())
		{
			CString str = resToken;
			CString value = str;
			int pos_e = str.Find('=');
			if (pos_e >= 0)
			{
				value = str.Mid(pos_e+1);
				str = str.Mid(0,pos_e);
			}
			outMap[str]=value;
			resToken = TokenizeString( values, " ,", pos);
		};
		return outMap.empty() == false;
	}

	IEntity* GetRefEntityByUIConfig(CFlowNodeGetCustomItemsBase* pGetCustomItems, const CString& refEntity)
	{
		CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
		assert (pFlowNode != 0);

		// If the node is part of an AI Action Graph we always use the selected entity
		CFlowGraph* pFlowGraph = static_cast<CFlowGraph*> (pFlowNode->GetGraph());
		if (pFlowGraph && pFlowGraph->GetAIAction())
		{
			IEntity* pSelEntity = 0;
			CBaseObject* pObject = GetIEditor()->GetSelectedObject();
			if (pObject != 0 && pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
			{
				CEntity* pCEntity = static_cast<CEntity*> (pObject);
				pSelEntity = pCEntity->GetIEntity();
			}
			if (pSelEntity == 0)
			{
				Warning( _T("FlowGraph: Please select first an Entity to be used as reference for this AI Action.") );
			}
			return pSelEntity;
		}

		// Get the CryAction FlowGraph
		IFlowGraph* pGameFG = pGetCustomItems->GetFlowNode()->GetIFlowGraph();

		EntityId targetEntityId;
		CString targetEntityPort;
		const CString& uiConfig = pGetCustomItems->GetUIConfig();
		int pos = uiConfig.Find(refEntity);
		if (pos >= 0)
		{
			targetEntityPort = uiConfig.Mid(pos+refEntity.GetLength());
		}
		else
		{
			targetEntityPort = "entityId";
		}

		if (targetEntityPort == "entityId")
		{
			// use normal target entity of flownode as target
			targetEntityId = pGameFG->GetEntityId(pFlowNode->GetFlowNodeId());
			if (targetEntityId == 0)
			{
				Warning( _T("FlowGraph: No valid Target Entity assigned to node") );
				return 0;
			}
		}
		else
		{
			// use entity in port targetEntity as target
			CHyperNodePort* pPort = pFlowNode->FindPort(targetEntityPort, true);
			if (!pPort)
			{
				Warning( _T("FlowGraphVariables.cpp: Internal error - Cannot resolve port '%s' on ref-lookup. Check C++ Code!"), targetEntityPort.GetString() );
				return 0;
			}
			const TFlowInputData* pFlowData = pGameFG->GetInputValue(pFlowNode->GetFlowNodeId(), pPort->nPortIndex);
			assert (pFlowData != 0);
			const bool success = pFlowData->GetValueWithConversion(targetEntityId);
			if (!success || targetEntityId == 0)
			{
				Warning( _T("FlowGraph: No valid Target Entity set on port '%s'"), targetEntityPort.GetString() );
				return 0;
			}
		}

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);
		if (!pEntity)
		{
			Warning( _T("FlowGraph: Cannot find entity with id %u, set on port '%s'"), targetEntityId, targetEntityPort.GetString() );
			return 0;
		}

		return pEntity;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseCharacterAnimation(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;

		IAnimationGraphStateIteratorPtr pSFIter = GetISystem()->GetIAnimationGraphSystem()->CreateStateIterator(pEntity->GetId());
		if (pSFIter)
		{
			Warning( _T("FlowGraph: Attached entity '%s' seems to have an AnimationGraph attached. Consider using the AI:Anim node instead!"), pEntity->GetName());
		}

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (pCharacter == 0)
			return false;

		IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
		if (pAnimSet == 0)
			return false;

		const uint32 numAnims = pAnimSet->numAnimations();
		for (int i=0; i<numAnims; ++i)
		{
			const char* pName = pAnimSet->GetNameByAnimID(i);
			if (pName != 0)
			{
				outItems.push_back(SItem(pName));
			}
		}
		outText = _T("Choose Animation");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseBone(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (pCharacter == 0)
			return false;

		ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
		if (pSkeletonPose == 0)
			return false;

		const uint32 numJoints = pSkeletonPose->GetJointCount();
		for (uint32 i=0; i<numJoints; ++i)
		{
			const char* pName = pSkeletonPose->GetJointNameByID(i);
			if (pName != 0)
			{
				outItems.push_back(SItem(pName));
			}
		}
		outText = _T("Choose Bone");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseAttachment(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;

		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (pCharacter == 0)
			return false;

		IAttachmentManager* pMgr = pCharacter->GetIAttachmentManager();
		if (pMgr == 0)
			return false;

		const int32 numAttachment = pMgr->GetAttachmentCount();
		for (uint32 i=0; i<numAttachment; ++i)
		{
			IAttachment* pAttachment = pMgr->GetInterfaceByIndex(i);
			if (pAttachment != 0)
			{
				outItems.push_back(SItem(pAttachment->GetName()));
			}
		}
		outText = _T("Choose Attachment");
		return true;
	}

	// Chooses from AnimationState 
	// UIConfig can contain a 'ref_entity=[portname]' entry to specify the entity for AnimationGraph lookups
	// if bUseEx is true, an optional [signal] port will be queried to choose from [0->States, 1->Signal, 2->Action]
	bool DoChooseAnimationState(IVariable* pVar, std::vector<SItem>& outItems, CString& outText, bool bUseEx)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;

		int method = 0;
		if (bUseEx)
		{
			if (!GetValue(pGetCustomItems, "signal", method))
			{
				bool oneShot = true;
				GetValue(pGetCustomItems, "OneShot", oneShot);
				method = oneShot ? 1 : 2;
			}
		}

		if (method == 0)
		{
			IAnimationGraphStateIteratorPtr pSFIter = GetISystem()->GetIAnimationGraphSystem()->CreateStateIterator(pEntity->GetId());
			if (pSFIter == 0)
				return false;
			IAnimationGraphStateIterator::SState state;
			while (pSFIter->Next(state))
			{
				outItems.push_back(SItem(state.name));
			}
			outText = _T("Choose AnimationState");
			return true;
		}
		else 
		{
			IAnimationGraphInputsPtr pAllInputs = GetISystem()->GetIAnimationGraphSystem()->RetrieveInputs(pEntity->GetId());
			if (pAllInputs == 0)
				return false;
			const char* inputName = method == 1 ? "Signal" : "Action";
			for (int i=0; i<pAllInputs->GetNumInputs(); ++i)
			{
				const IAnimationGraphInputs::SInput* pInput = pAllInputs->GetInput(i);
				if (strcmp(pInput->GetName(), inputName) == 0)
				{
					for (int j=0; j<pInput->GetValueCount(); ++j)
					{
						outItems.push_back(SItem(pInput->GetValue(j)));
					}
					outText = _T("Choose Input Value for ");
					outText.Append(inputName);
					return true;
				}
			}
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseAnimationState(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		return DoChooseAnimationState(pVar,outItems,outText,false);
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseAnimationStateEx(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		return DoChooseAnimationState(pVar,outItems,outText,true);
	}

	//////////////////////////////////////////////////////////////////////////
  bool GetMatParams(IEntity* pEntity, int slot, int subMtlId, DynArray<SShaderParam>** outParams)
	{
		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy == 0)
			return false;

		IMaterial* pMtl = pRenderProxy->GetRenderMaterial(slot);
		if (pMtl == 0)
		{
			Warning("EntityMaterialShaderParam: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
			return false;
		}

		pMtl = (subMtlId > 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
		if (pMtl == 0)
		{
			Warning("EntityMaterialShaderParam: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
			return false;
		}

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			Warning("EntityMaterialShaderParam: Material '%s' has no shader resources ", pMtl->GetName());
			return false;
		}
		*outParams = &pRendRes->GetParameters();
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
  void FillOutParamsByShaderParams(std::vector<SItem>& outItems, DynArray<SShaderParam>* params, int limit)
	{
		if (limit == 1)
		{
			outItems.push_back(SItem("diffuse"));
			outItems.push_back(SItem("specular"));
			outItems.push_back(SItem("emissive"));
		}
		else if (limit == 2)
		{
			outItems.push_back(SItem("glow"));
			outItems.push_back(SItem("shininess"));
			outItems.push_back(SItem("alpha"));
			outItems.push_back(SItem("opacity"));
		}
	
		// fill in params
		SItem item;
		for (int i=0; i<params->size(); ++i)
		{
			SShaderParam& param = (*params)[i];
			EParamType paramType = param.m_Type;
			if (limit == 1 && paramType != eType_VECTOR && paramType != eType_FCOLOR)
				continue;
			else if (limit == 2 && paramType != eType_FLOAT && paramType != eType_INT && paramType != eType_SHORT && paramType != eType_BOOL)
				continue;
			item.name = param.m_Name;
			outItems.push_back(item);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseMatParamSlot(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;
		TParamMap paramMap;
		GetParamMap(pGetCustomItems, paramMap);

		CString& slotString = paramMap["slot_ref"];
		CString& subString = paramMap["sub_ref"];
		CString& paramTypeString = paramMap["param"];
		int slot = -1;
		int subMtlId = -1;
		if (GetValue(pGetCustomItems, slotString, slot) == false)
		{
			return false;
		}
		if (GetValue(pGetCustomItems, subString, subMtlId) == false)
		{
			return false;
		}

		int limit = 0;
		if (paramTypeString == "vec")
		{
			limit = 1;
		}
		else if (paramTypeString == "float")
		{
			limit = 2;
		}

    DynArray<SShaderParam>* params;
		if (GetMatParams(pEntity, slot, subMtlId, &params) == false)
		{
			return false;
		}

		FillOutParamsByShaderParams(outItems, params, limit);

		outText = _T("Choose Param");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseMatParamName(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		TParamMap paramMap;
		GetParamMap(pGetCustomItems, paramMap);

		CString& nameString = paramMap["name_ref"];
		CString& subString = paramMap["sub_ref"];
		CString& paramTypeString = paramMap["param"];
		string  matName;
		int subMtlId = -1;
		if (GetValue(pGetCustomItems, nameString, matName) == false || matName.empty())
		{
			Warning("MaterialParam: No/Invalid Material given.");
			return false;
		}
		if (GetValue(pGetCustomItems, subString, subMtlId) == false)
		{
			return false;
		}

		int limit = 0;
		if (paramTypeString == "vec")
		{
			limit = 1;
		}
		else if (paramTypeString == "float")
		{
			limit = 2;
		}

		IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName);
		if (pMtl == 0)
		{
			Warning("MaterialParam: Material '%s' not found.", matName.c_str());
			return false;
		}

		pMtl = (subMtlId >= 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
		if (pMtl == 0)
		{
			Warning("MaterialParam: Material '%s' has no sub-material %d ", matName.c_str(), subMtlId);
			return false;
		}

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			Warning("MaterialParam: Material '%s' has no shader resources ", pMtl->GetName());
			return false;
		}
    DynArray<SShaderParam>* params = &pRendRes->GetParameters(); 

		FillOutParamsByShaderParams(outItems, params, limit);

		outText = _T("Choose Param");
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool ChooseMatParamCharAttachment(IVariable* pVar, std::vector<SItem>& outItems, CString& outText)
	{
		CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (pVar->GetUserData());
		assert (pGetCustomItems != 0);

		IEntity* pEntity = GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
		if (pEntity == 0)
			return false;
		TParamMap paramMap;
		GetParamMap(pGetCustomItems, paramMap);

		CString& slotString = paramMap["charslot_ref"];
		CString& attachString = paramMap["attachment_ref"];
		CString& paramTypeString = paramMap["param"];
		CString& subString = paramMap["sub_ref"];
		int charSlot = -1;
		int subMtlId = -1;
		if (GetValue(pGetCustomItems, slotString, charSlot) == false)
		{
			return false;
		}
		if (GetValue(pGetCustomItems, subString, subMtlId) == false)
		{
			return false;
		}
		string attachmentName;
		if (GetValue(pGetCustomItems, attachString, attachmentName) == false || attachmentName.empty())
		{
			Warning("MaterialParam: No/Invalid Attachment given.");
			return false;
		}

		ICharacterInstance* pCharacter = pEntity->GetCharacter(charSlot);
		if (pCharacter == 0)
			return false;

		IAttachmentManager* pMgr = pCharacter->GetIAttachmentManager();
		if (pMgr == 0)
			return false;

		IAttachment* pAttachment = pMgr->GetInterfaceByName(attachmentName.c_str());
		if (pAttachment == 0)
			return false;
		IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
		if (pAttachmentObject == 0)
			return false;

		IMaterial* pMtl = pAttachmentObject->GetMaterial();
		if (pMtl == 0)
			return false;

		int limit = 0;
		if (paramTypeString == "vec")
		{
			limit = 1;
		}
		else if (paramTypeString == "float")
		{
			limit = 2;
		}

		string matName = pMtl->GetName();

		pMtl = (subMtlId >= 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
		if (pMtl == 0)
		{
			Warning("MaterialParam: Material '%s' has no sub-material %d ", matName.c_str(), subMtlId);
			return false;
		}

		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
		if (pRendRes == 0)
		{
			Warning("MaterialParam: Material '%s' has no shader resources ", pMtl->GetName());
			return false;
		}
		DynArray<SShaderParam>* params = &pRendRes->GetParameters();

		FillOutParamsByShaderParams(outItems, params, limit);

		outText = _T("Choose Param");
		return true;
	}
};

// FIXME: not so nice macros. Get rid of them and make real classes instead of wrappers...
//        or use some more template magic
#ifdef FGVARIABLES_REAL_CLONE
#define DECL_CHOOSE_EX(FOO, TREE, SEP) \
class	C##FOO : public CFlowNodeGetCustomItemsBase \
{ \
	virtual bool GetItems (IVariable* pVar, std::vector<SItem>& items, CString& outDialogTitle) \
	{ \
		return FOO(pVar, items, outDialogTitle); \
	} \
	virtual bool UseTree() { return TREE; } \
	virtual const char* GetTreeSeparator() { return SEP; } \
	virtual CFlowNodeGetCustomItemsBase* Clone() const\
	{\
  	C##FOO* pNew = new C##FOO();\
		pNew->m_pFlowNode = m_pFlowNode; \
		pNew->m_config = m_config;\
		return pNew;\
	}\
};
#else
#define DECL_CHOOSE(FOO, TREE, SEP) \
class	C##FOO : public CFlowNodeGetCustomItemsBase \
{ \
	virtual bool GetItems (IVariable* pVar, std::vector<CString>& items, CString& outDialogTitle) \
	{ \
   	return FOO(pVar, items, outDialogTitle); \
  } \
	virtual bool UseTree() { return TREE; } \
	virtual const char* GetTreeSeparator() { return SEP; } \
	virtual CFlowNodeGetCustomItemsBase* Clone() const\
	{\
	return const_cast<C##FOO*> (this); \
}\
};
#endif
#define DECL_CHOOSE(FOO) DECL_CHOOSE_EX(FOO, false, "")

DECL_CHOOSE(ChooseCharacterAnimation);
DECL_CHOOSE(ChooseAnimationState);
DECL_CHOOSE(ChooseAnimationStateEx);
DECL_CHOOSE(ChooseBone);
DECL_CHOOSE(ChooseAttachment);
DECL_CHOOSE_EX(ChooseDialog, true, ".");
DECL_CHOOSE(ChooseMatParamSlot);
DECL_CHOOSE(ChooseMatParamName);
DECL_CHOOSE(ChooseMatParamCharAttachment);

// And here's the map...
// Used to specify specialized property editors for input ports.
// If the name starts with one of these prefixes the data type
// of the port will be changed to allow the specialized editor.
// The name will be shown without the prefix.
// if datatype is IVariable::DT_USERITEMCB a GenericSelectItem dialog is used
// and the callback should provide the selectable items (optionally context
// [==node]-sensitive items) e.g. Animation Lookup
static const FlowGraphVariables::MapEntry prefix_dataType_table[] =
{
	{ "snd_",         IVariable::DT_SOUND, 0 },
	{ "sound_",       IVariable::DT_SOUND, 0 },

	{ "clr_",         IVariable::DT_COLOR, 0 },
	{ "color_",       IVariable::DT_COLOR, 0 },

	{ "tex_",         IVariable::DT_TEXTURE, 0 },
	{ "texture_",     IVariable::DT_TEXTURE, 0 },

	{ "obj_",         IVariable::DT_OBJECT, 0 },
	{ "object_",      IVariable::DT_OBJECT, 0 },

	{ "file_",        IVariable::DT_FILE, 0 },

	{ "text_",        IVariable::DT_LOCAL_STRING, 0 },
	{ "equip_",       IVariable::DT_EQUIP, 0 },
	{ "reverbpreset_",IVariable::DT_REVERBPRESET, 0 },

	{ "aianchor_",    IVariable::DT_AI_ANCHOR, 0 },
	{ "aibehavior_",  IVariable::DT_AI_BEHAVIOR, 0 },
	{ "aicharacter_", IVariable::DT_AI_CHARACTER, 0 },
	{ "soclass_",     IVariable::DT_SOCLASS, 0 },
	{ "soclasses_",   IVariable::DT_SOCLASSES, 0 },
	{ "sostate_",     IVariable::DT_SOSTATE, 0 },
	{ "sostates_",    IVariable::DT_SOSTATES, 0 },
	{ "sopattern_",   IVariable::DT_SOSTATEPATTERN, 0 },
	{ "soaction_",    IVariable::DT_SOACTION, 0 },
	{ "sohelper_",    IVariable::DT_SOHELPER, 0 },
	{ "sonavhelper_", IVariable::DT_SONAVHELPER, 0 },
	{ "soevent_",     IVariable::DT_SOEVENT, 0 },
	{ "gametoken_",   IVariable::DT_GAMETOKEN, 0 },
	{ "mat_",         IVariable::DT_MATERIAL, 0 },
	{ "seq_",         IVariable::DT_SEQUENCE, 0 },
	{ "mission_",     IVariable::DT_MISSIONOBJ, 0 },
	{ "anim_",        IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseCharacterAnimation> },
	{ "animstate_",   IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseAnimationState> },
	{ "animstateEx_", IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseAnimationStateEx> },
	{ "bone_",        IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseBone> },
	{ "attachment_",  IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseAttachment> },
	{ "dialog_",      IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseDialog> },
	{ "matparamslot_",      IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseMatParamSlot> },
	{ "matparamname_",      IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseMatParamName> },
	{ "matparamcharatt_",      IVariable::DT_USERITEMCB, &FlowGraphVariables::CreateIt<CChooseMatParamCharAttachment> },
};
static const int prefix_dataType_numEntries = sizeof( prefix_dataType_table ) / sizeof( FlowGraphVariables::MapEntry );


namespace FlowGraphVariables
{
	const MapEntry* FindEntry(const char* sPrefix)
	{
		for ( int i = 0; i < prefix_dataType_numEntries; ++i )
		{
			if ( strstr( sPrefix, prefix_dataType_table[i].sPrefix ) == sPrefix )
			{
				return &prefix_dataType_table[i];
			}
		}
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////
struct CUIEnumsDatabase_SEnum* CVariableFlowNodeDynamicEnum::GetSEnum()
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*> (this->GetUserData());
	if (pGetCustomItems == 0)
		return 0;

	assert (pGetCustomItems != 0);
	// CryLogAlways("CVariableFlowNodeDynamicEnum::GetSEnum 0x%p pGetCustomItems=0x%p FlowNode=0x%p", this, pGetCustomItems, pGetCustomItems->GetFlowNode());

	string enumName;
	bool ok = ::GetValue(pGetCustomItems, m_refPort, enumName);
	if (!ok)
		return 0;

	CString enumKey;
	enumKey.Format(m_refFormatString, enumName.c_str());
	CUIEnumsDatabase_SEnum* pEnum = GetIEditor()->GetUIEnumsDatabase()->FindEnum(enumKey);
	return pEnum;
}

//////////////////////////////////////////////////////////////////////////
CVariableFlowNodeDynamicEnum::CDynamicEnumList::CDynamicEnumList(CVariableFlowNodeDynamicEnum* pDynEnum)
{
	m_pDynEnum = pDynEnum;
	// CryLogAlways("CDynamicEnumList::ctor 0x%p", this);
}

//////////////////////////////////////////////////////////////////////////
CVariableFlowNodeDynamicEnum::CDynamicEnumList::~CDynamicEnumList()
{
	// CryLogAlways("CDynamicEnumList::dtor 0x%p", this);
}

//////////////////////////////////////////////////////////////////////////
int CVariableFlowNodeDynamicEnum::CDynamicEnumList::GetItemsCount()
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return 0;
	return pEnum->strings.size();
}

//! Get the name of specified value in enumeration.
const char* CVariableFlowNodeDynamicEnum::CDynamicEnumList::GetItemName( int index )
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return "";
	assert( index >= 0 && index < pEnum->strings.size() );
	return pEnum->strings[index];
}

//////////////////////////////////////////////////////////////////////////
CString CVariableFlowNodeDynamicEnum::CDynamicEnumList::NameToValue(const CString& name)
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return "";
	return pEnum->NameToValue(name);
}

//////////////////////////////////////////////////////////////////////////
CString CVariableFlowNodeDynamicEnum::CDynamicEnumList::ValueToName(const CString& name)
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return "";
	return pEnum->ValueToName(name);
}


