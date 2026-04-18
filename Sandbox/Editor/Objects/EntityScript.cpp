////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   EntityScript.cpp
//  Version:     v1.00
//  Created:     10/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CEntityScript class implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "EntityScript.h"
#include "Entity.h"
#include "IconManager.h"

#include <IScriptSystem.h>
#include <IEntitySystem.h>

struct CScriptMethodsDump : public IScriptTableDumpSink
{
	std::vector<CString> methods;
	std::vector<CString> events;
	virtual void OnElementFound(int nIdx,ScriptVarType type){}
	virtual void OnElementFound(const char *sName,ScriptVarType type)
	{
		if (type == svtFunction)
		{
			if (strncmp(sName,EVENT_PREFIX,6) == 0)
				events.push_back( sName+6 );
			else
				methods.push_back( sName );
		}/* else if (type == svtObject && stricmp(sName,PROPERTIES_TABLE)==0)
		{
			// Properties found.
		}
		*/
	}
};

enum EScriptParamFlags
{
	SCRIPTPARAM_POSITIVE = 0x01,
};

//////////////////////////////////////////////////////////////////////////
static struct {
	const char *prefix;
	IVariable::EType type;
	IVariable::EDataType dataType;
	int flags;
} s_paramTypes[] =
{
	{ "n",				IVariable::INT,			IVariable::DT_SIMPLE, SCRIPTPARAM_POSITIVE },
	{ "i",				IVariable::INT,			IVariable::DT_SIMPLE,0 },
	{ "b",				IVariable::BOOL,		IVariable::DT_SIMPLE,0 },
	{ "f",				IVariable::FLOAT,		IVariable::DT_SIMPLE,0 },
	{ "s",				IVariable::STRING,	IVariable::DT_SIMPLE,0 },

	{ "ei",				IVariable::INT,			IVariable::DT_UIENUM,0 },
	{ "es",				IVariable::STRING,	IVariable::DT_UIENUM,0 },

	{ "shader",		IVariable::STRING,	IVariable::DT_SHADER,0 },
	{ "clr",			IVariable::VECTOR,	IVariable::DT_COLOR,0 },
	{ "color",		IVariable::VECTOR,	IVariable::DT_COLOR,0 },

	{ "vector",		IVariable::VECTOR,	IVariable::DT_SIMPLE,0 },

	{ "snd",			IVariable::STRING,	IVariable::DT_SOUND,0 },
	{ "sound",		IVariable::STRING,	IVariable::DT_SOUND,0 },

	{ "tex",			IVariable::STRING,	IVariable::DT_TEXTURE,0 },
	{ "texture",	IVariable::STRING,	IVariable::DT_TEXTURE,0 },

	{ "obj",			IVariable::STRING,	IVariable::DT_OBJECT,0 },
	{ "object",		IVariable::STRING,	IVariable::DT_OBJECT,0 },

	{ "file",			IVariable::STRING,	IVariable::DT_FILE,0 },
	{ "aibehavior",	IVariable::STRING,IVariable::DT_AI_BEHAVIOR,0 },
	{ "aicharacter",IVariable::STRING,IVariable::DT_AI_CHARACTER,0 },

	{ "text",			IVariable::STRING,	IVariable::DT_LOCAL_STRING,0 },
	{ "equip",		IVariable::STRING,	IVariable::DT_EQUIP,0 },
	{ "reverbpreset",IVariable::STRING,	IVariable::DT_REVERBPRESET,0 },
	{ "eaxpreset",IVariable::STRING,	IVariable::DT_REVERBPRESET,0 },

	{ "aianchor",	IVariable::STRING,	IVariable::DT_AI_ANCHOR,0 },

	{ "soclass",	IVariable::STRING,	IVariable::DT_SOCLASS,0 },
	{ "soclasses",	IVariable::STRING,	IVariable::DT_SOCLASSES,0 },
	{ "sostate",	IVariable::STRING,	IVariable::DT_SOSTATE,0 },
	{ "sostates",	IVariable::STRING,	IVariable::DT_SOSTATES,0 },
	{ "sopattern",	IVariable::STRING,	IVariable::DT_SOSTATEPATTERN,0 },
	{ "soaction",	IVariable::STRING,	IVariable::DT_SOACTION,0 },
	{ "sohelper",	IVariable::STRING,	IVariable::DT_SOHELPER,0 },
	{ "sonavhelper",IVariable::STRING,	IVariable::DT_SONAVHELPER,0 },
	{ "soevent",	IVariable::STRING,	IVariable::DT_SOEVENT,0 },
	{ "sotemplate", IVariable::STRING,	IVariable::DT_SOTEMPLATE,0 },
	{ "gametoken", IVariable::STRING, IVariable::DT_GAMETOKEN, 0 },
	{ "seq_",      IVariable::STRING, IVariable::DT_SEQUENCE, 0 },
	{ "mission_", IVariable::STRING, IVariable::DT_MISSIONOBJ, 0 },
};

//////////////////////////////////////////////////////////////////////////
struct CScriptPropertiesDump : public IScriptTableDumpSink
{
private:
	struct Variable {
		CString name;
		ScriptVarType type;
	};
	std::vector<Variable> m_elements;
	
	CVarBlock *m_varBlock;
	IVariable *m_parentVar;

public:
	explicit CScriptPropertiesDump( CVarBlock *pVarBlock,IVariable *pParentVar=NULL )
	{
		m_varBlock = pVarBlock;
		m_parentVar = pParentVar;
	}

	//////////////////////////////////////////////////////////////////////////
	inline bool IsPropertyTypeMatch( const char *type,const char *name,int nameLen )
	{
		int typeLen = strlen(type);
		if (typeLen < nameLen)
		{
			// After type name Must follow Upper case or _.
			if (name[typeLen] != tolower(name[typeLen]) || name[typeLen] == '_' )
			{
				if (strncmp(name,type,strlen(type)) == 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IVariable* CreateVarByType( IVariable::EType type )
	{
		switch(type)
		{
		case IVariable::FLOAT:		return new CVariable<float>;
		case IVariable::INT:			return new CVariable<int>;
		case IVariable::STRING:	return new CVariable<CString>;
		case IVariable::BOOL:		return new CVariable<bool>;
		case IVariable::VECTOR:	return new CVariable<Vec3>;
		case IVariable::QUAT:		return new CVariable<Quat>;
		default:
			assert(0);
		}
		return NULL;
	}
	//////////////////////////////////////////////////////////////////////////
	IVariable* CreateVar( const char *name,IVariable::EType defaultType,const char* &displayName )
	{
		displayName = name;
		// Resolve type from variable name.
		int nameLen = strlen(name);

		// if starts from capital no type encoded.
		if (name[0] == tolower(name[0]))
		{
			// Try to detect type.
			for (int i = 0; i < sizeof(s_paramTypes)/sizeof(s_paramTypes[0]); i++)
			{
				if (IsPropertyTypeMatch(s_paramTypes[i].prefix,name,nameLen))
				{
					//if (s_paramTypes[i].type != var->GetType())
					//continue;
					displayName = name + strlen(s_paramTypes[i].prefix);
					if (displayName[0] == '_')
						displayName++;

					IVariable *var = CreateVarByType(s_paramTypes[i].type);
					if (!var)
						continue;

					var->SetName(name);
					var->SetHumanName(displayName);
					var->SetDataType(s_paramTypes[i].dataType);

					if (s_paramTypes[i].flags & SCRIPTPARAM_POSITIVE)
					{
						float lmin=0,lmax=10000;
						var->GetLimits( lmin,lmax );
						// set min Limit to 0 hard, to make it positive only value.
						var->SetLimits( 0,lmax,true,false );
					}
					return var;
				}
			}
		}
		if (defaultType != IVariable::UNKNOWN)
		{
			IVariable *var = CreateVarByType(defaultType);
			var->SetName(name);
			return var;
		}
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	void OnElementFound(int nIdx,ScriptVarType type)
	{
		/*ignore non string indexed values*/
	};
	//////////////////////////////////////////////////////////////////////////
	virtual void OnElementFound(const char *sName,ScriptVarType type)
	{
		if (sName && sName[0] != 0)
		{
			Variable var;
			var.name = sName;
			var.type = type;
			m_elements.push_back(var);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Dump( IScriptTable *pObject )
	{
		m_elements.reserve(20);
		pObject->Dump( this );
		typedef std::map<CString,IVariablePtr,stl::less_stricmp<CString> > NodesMap;
		NodesMap nodes;
		NodesMap listNodes;

		for (int i = 0; i < m_elements.size(); i++)
		{
			const char *sName = m_elements[i].name;
			ScriptVarType type = m_elements[i].type;

			const char *sDisplayName = sName;

			if (type == svtNumber)
			{
				float fVal;
				pObject->GetValue( sName,fVal );
				IVariable *var = CreateVar(sName,IVariable::FLOAT,sDisplayName);
				if (var)
				{
					var->Set(fVal);
					nodes[sDisplayName] = var;
				}
			} else if (type == svtString)
			{
				const char *sVal;
				pObject->GetValue( sName,sVal );
				IVariable *var = CreateVar(sName,IVariable::STRING,sDisplayName);
				if (var)
				{
					var->Set(sVal);
					nodes[sDisplayName] = var;
				}
			} else if (type == svtFunction)
			{
				// Ignore functions.
			} else if (type == svtObject)
			{
				// Some Table.
				SmartScriptTable pTable(GetIEditor()->GetSystem()->GetIScriptSystem(),true);
				if (pObject->GetValue( sName,pTable ))
				{
					IVariable *var = CreateVar(sName,IVariable::UNKNOWN,sDisplayName);
					if (var && var->GetType() == IVariable::VECTOR)
					{
						nodes[sDisplayName] = var;
						float x,y,z;
						if (pTable->GetValue("x",x) && pTable->GetValue("y",y) && pTable->GetValue("z",z))
						{
							var->Set( Vec3(x,y,z) );
						}
						else
						{
							pTable->GetAt(1,x);
							pTable->GetAt(2,y);
							pTable->GetAt(3,z);
							var->Set( Vec3(x,y,z) );
						}
					}
					else
					{
						var = new CVariableArray;
						var->SetName(sName);
						listNodes[sName] = var;
						
						CScriptPropertiesDump dump(m_varBlock,var);
						dump.Dump( *pTable );
					}
				}
			}
		}

		for (NodesMap::iterator nit = nodes.begin(); nit != nodes.end(); nit++)
		{
			if (m_parentVar)
				m_parentVar->AddChildVar(nit->second);
			else
				m_varBlock->AddVariable(nit->second);
		}
		for (NodesMap::iterator nit1 = listNodes.begin(); nit1 != listNodes.end(); nit1++)
		{
			if (m_parentVar)
				m_parentVar->AddChildVar(nit1->second);
			else
				m_varBlock->AddVariable(nit1->second);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CEntityScript::CEntityScript( IEntityClass *pClass )
{
	assert(pClass);
	m_pClass = pClass;
	m_valid = false;
	m_haveEventsTable = false;
	m_visibilityMask = 0;
	m_usable = !(pClass->GetFlags() & ECLF_INVISIBLE);

	m_nTextureIcon = 0;

	m_nFlags = 0;
}

//////////////////////////////////////////////////////////////////////////
CEntityScript::~CEntityScript()
{
}

//////////////////////////////////////////////////////////////////////////
CString CEntityScript::GetName() const
{
	return m_pClass->GetName();
}

//////////////////////////////////////////////////////////////////////////
CString CEntityScript::GetFile() const
{
	return m_pClass->GetScriptFile();
}

//////////////////////////////////////////////////////////////////////////
int	CEntityScript::GetEventCount()
{
	//return sizeof(sEntityEvents)/sizeof(sEntityEvents[0]);
	return (int)m_events.size();
}

//////////////////////////////////////////////////////////////////////////
const CString& CEntityScript::GetEvent( int i )
{
	//return sEntityEvents[i].name;
	return m_events[i];
}

//////////////////////////////////////////////////////////////////////////
int	CEntityScript::GetEmptyLinkCount()
{
	return (int)m_emptyLinks.size();
}

//////////////////////////////////////////////////////////////////////////
const CString& CEntityScript::GetEmptyLink( int i )
{
	return m_emptyLinks[i];
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::Load()
{
	m_valid = false;

	/*
	IGame *pGame = GetIEditor()->GetGame();
	EntityClass *entCls = pGame->GetClassRegistry()->GetByClassId( m_ClassId );
	if (!entCls)
	{
		m_valid = false;
		//Warning( "Load of entity script %s failed.",(const char*)m_name );

		CErrorRecord err;
		err.error.Format( "Entity Script %s Failed to Load",(const char*)m_name );
		err.file = m_relFile;
		err.severity = CErrorRecord::ESEVERITY_WARNING;
		err.flags = CErrorRecord::FLAG_SCRIPT;
		GetIEditor()->GetErrorReport()->ReportError(err);

		return false;
	}
	*/

	if (strlen(m_pClass->GetScriptFile()) > 0)
	{
		if (m_pClass->LoadScript(false))
		{
			// If class have script parse this script.
			return ParseScript();
		}
		else
		{
			CErrorRecord err;
			err.error.Format( "Entity class %s failed to load, (script: %s could not be loaded)",m_pClass->GetName(),m_pClass->GetScriptFile() );
			err.file = m_pClass->GetScriptFile();
			err.severity = CErrorRecord::ESEVERITY_WARNING;
			err.flags = CErrorRecord::FLAG_SCRIPT;
			GetIEditor()->GetErrorReport()->ReportError(err);
		}
	}

	m_valid = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::ParseScript()
{
	// Parse .lua file.
	IScriptSystem *script = GetIEditor()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pEntity(script,true);
	if (!script->GetGlobalValue( GetName(),pEntity ))
		return false;

	m_valid = true;

	CScriptMethodsDump dump;
	pEntity->Dump( &dump );
	m_methods = dump.methods;
	m_events = dump.events;

	//! Sort methods and events.
	std::sort( m_methods.begin(),m_methods.end() );
	std::sort( m_events.begin(),m_events.end() );

	m_emptyLinks.clear();

	{
		// Normal properties.
		m_properties = 0;
		SmartScriptTable pProps(script,true);
		if (pEntity->GetValue( PROPERTIES_TABLE,pProps ))
		{
			// Properties found in entity.
			m_properties = new CVarBlock;
			CScriptPropertiesDump dump(m_properties);
			dump.Dump( *pProps );
		}
	}

	{
		// Second set of properties.
		m_properties2 = 0;
		SmartScriptTable pProps(script,true);
		if (pEntity->GetValue( PROPERTIES2_TABLE,pProps ))
		{
			// Properties found in entity.
			m_properties2 = new CVarBlock;
			CScriptPropertiesDump dump(m_properties2);
			dump.Dump( *pProps );
		}
	}

	// Destroy variable block if empty.
	if (m_properties != 0 && m_properties->GetVarsCount() < 1)
		m_properties = 0;

	// Destroy variable block if empty.
	if (m_properties2 != 0 && m_properties2->GetVarsCount() < 1)
		m_properties2 = 0;

	m_nTextureIcon = 0;

	// Load visual object.
	SmartScriptTable pEditorTable(script,true);
	if (pEntity->GetValue( "Editor",pEditorTable ))
	{
		const char *modelName;
		if (pEditorTable->GetValue( "Model",modelName ))
		{
			m_visualObject = modelName;
		}
		bool bShowBounds = false;
		pEditorTable->GetValue( "ShowBounds",bShowBounds );
		if (bShowBounds)
			m_nFlags |= ENTITY_SCRIPT_SHOWBOUNDS;
		else
			m_nFlags &= ~ENTITY_SCRIPT_SHOWBOUNDS;

		bool bAbsoluteRadius = false;
		pEditorTable->GetValue( "AbsoluteRadius",bAbsoluteRadius );
		if (bAbsoluteRadius)
			m_nFlags |= ENTITY_SCRIPT_ABSOLUTERADIUS;
		else
			m_nFlags &= ~ENTITY_SCRIPT_ABSOLUTERADIUS;

		const char *iconName = "";
		if (pEditorTable->GetValue( "Icon",iconName ))
		{
			CString iconfile = Path::Make("Editor/ObjectIcons",iconName);
			m_nTextureIcon = GetIEditor()->GetIconManager()->GetIconTexture( iconfile );
		}

		bool bIconOnTop = false;
		pEditorTable->GetValue( "IconOnTop",bIconOnTop );
		if (bIconOnTop)
			m_nFlags |= ENTITY_SCRIPT_ICONONTOP;
		else
			m_nFlags &= ~ENTITY_SCRIPT_ICONONTOP;

		bool bArrow = false;
		pEditorTable->GetValue( "DisplayArrow",bArrow );
		if (bArrow)
			m_nFlags |= ENTITY_SCRIPT_DISPLAY_ARROW;
		else
			m_nFlags &= ~ENTITY_SCRIPT_DISPLAY_ARROW;


		SmartScriptTable pLinksTable(script,true);
		if (pEditorTable->GetValue( "Links",pLinksTable ))
		{
			IScriptTable::Iterator iter = pLinksTable->BeginIteration();
			while (pLinksTable->MoveNext(iter))
			{
				if (iter.value.type == ANY_TSTRING)
				{
					const char *sLinkName = iter.value.str;
					m_emptyLinks.push_back(sLinkName);
				}
			}
			pLinksTable->EndIteration(iter);
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Reload()
{
	bool bReloaded = false;
	IEntityScript *pScript = m_pClass->GetIEntityScript();
	if (pScript)
	{
		// First try compiling script and see if it have any errors.
		bool bLoadScript = CFileUtil::CompileLuaFile( m_pClass->GetScriptFile() );
		if (bLoadScript)
			bReloaded = m_pClass->LoadScript(true);
	}

	if (bReloaded)
	{
		// Script compiled succesfully.
		Load();/*
		if (GetIEditor()->GetSystem()->GetIScriptSystem()->ReloadScript( GetFile(),false ))
		{
			ParseScript();
		}
		*/
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::GotoMethod( const CString &method )
{
	CString line;
	line.Format( "%s:%s",(const char*)GetName(),(const char*)method );

	// Search this line in script file.
	int lineNum = FindLineNum( line );
	if (lineNum >= 0)
	{
		// Open UltraEdit32 with this line.
		CFileUtil::EditTextFile( GetFile(),lineNum );
	}
}

void CEntityScript::AddMethod( const CString &method )
{
	// Add a new method to the file. and start Editing it.
	FILE *f = fopen( GetFile(),"at" );
	if (f)
	{
		fprintf( f,"\n" );
		fprintf( f,"-------------------------------------------------------\n" );
		fprintf( f,"function %s:%s()\n",(const char*)m_pClass->GetName(),(const char*)method );
		fprintf( f,"end\n" );
		fclose(f);
	}
}

//////////////////////////////////////////////////////////////////////////
int CEntityScript::FindLineNum( const CString &line )
{
	FILE *f = fopen( GetFile(),"rb" );
	if (!f)
		return -1;

	int lineFound = -1;
	int lineNum = 1;

	fseek( f,0,SEEK_END );
	int size = ftell(f);
	fseek( f,0,SEEK_SET );
	
	char *text = new char[size+16];
	fread( text,size,1,f );
	text[size] = 0;

	char *token = strtok( text,"\n" );
	while (token)
	{
		if (strstr( token,line ) != 0)
		{
			lineFound = lineNum;
			break;
		}
		token = strtok( NULL,"\n" );
		lineNum++;
	}
	fclose(f);
	delete []text;

	return lineFound;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::SetProperties( IEntity *ientity,CVarBlock *vars,bool bCallUpdate )
{
	if (!IsValid())
		return;
	
	assert( ientity != 0 );
	assert( vars != 0 );

	IScriptTable *scriptObject = ientity->GetScriptTable();
	if (!scriptObject)
		return;

	IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pProperties( scriptSystem,true);
	if (!scriptObject->GetValue( PROPERTIES_TABLE,pProperties ))
	{
		return;
	}

	for (int i = 0; i < vars->GetVarsCount(); i++)
	{
		VarToScriptObject( vars->GetVariable(i),pProperties );
	}

	if (bCallUpdate)
	{
		CallOnPropertyChange(ientity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::SetProperties2( IEntity *ientity,CVarBlock *vars,bool bCallUpdate )
{
	if (!IsValid())
		return;
	
	assert( ientity != 0 );
	assert( vars != 0 );

	IScriptTable *scriptObject = ientity->GetScriptTable();
	if (!scriptObject)
		return;

	IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pProperties( scriptSystem,true);
	if (!scriptObject->GetValue( PROPERTIES2_TABLE,pProperties ))
	{
		return;
	}

	for (int i = 0; i < vars->GetVarsCount(); i++)
	{
		VarToScriptObject( vars->GetVariable(i),pProperties );
	}

	if (bCallUpdate)
	{
		CallOnPropertyChange(ientity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallOnPropertyChange( IEntity *ientity )
{
	if (!IsValid())
		return;

	assert( ientity != 0 );
	IScriptTable *scriptObject = ientity->GetScriptTable();
	if (!scriptObject)
		return;
	Script::CallMethod(scriptObject,"OnPropertyChange");
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::VarToScriptObject( IVariable *var,IScriptTable *obj )
{
	assert(var);

	if (var->GetType() == IVariable::ARRAY)
	{
		int type = obj->GetValueType( var->GetName() );
		if (type != svtObject)
			return;

		IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();
		SmartScriptTable pTableObj( scriptSystem,true );
		if (obj->GetValue( var->GetName(),pTableObj ))
		{
			for (int i = 0; i < var->NumChildVars(); i++)
			{
				IVariable *child = var->GetChildVar(i);
				VarToScriptObject( child,pTableObj );
			}
		}
		return;
	}

	const char *name = var->GetName();
	int type = obj->GetValueType( name );

	if (type == svtString)
	{
		CString value;
		var->Get(value);
		obj->SetValue( name,(const char*)value );
	}
	else if (type == svtNumber)
	{
		float val = 0;
		var->Get(val);
		obj->SetValue( name,val );
	}
	else if (type == svtObject)
	{
		IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();
		// Probably Color/Vector.
		SmartScriptTable pTable( scriptSystem,true );
		if (obj->GetValue( name,pTable ))
		{
			if (var->GetType() == IVariable::VECTOR)
			{
				Vec3 vec;
				var->Get(vec);

				float temp;
				if (pTable->GetValue( "x",temp ))
				{
					// Named vector.
					pTable->SetValue( "x",vec.x );
					pTable->SetValue( "y",vec.y );
					pTable->SetValue( "z",vec.z );
				}
				else
				{
					// Indexed vector.
					pTable->SetAt(1,vec.x);
					pTable->SetAt(2,vec.y);
					pTable->SetAt(3,vec.z);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void	CEntityScript::RunMethod( IEntity *ientity,const CString &method )
{
	if (!IsValid())
		return;

	assert( ientity != 0 );

	IScriptTable *scriptObject = ientity->GetScriptTable();
	if (!scriptObject)
		return;

	IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();

	Script::CallMethod(scriptObject,(const char*)method );
}

//////////////////////////////////////////////////////////////////////////
void	CEntityScript::SendEvent( IEntity *entity,const CString &method )
{
	// Fire event on Entity.
	IEntityScriptProxy *pScriptProxy = (IEntityScriptProxy*)entity->GetProxy(ENTITY_PROXY_SCRIPT);
	if (pScriptProxy)
		pScriptProxy->CallEvent( method );
}

//////////////////////////////////////////////////////////////////////////
void	CEntityScript::SetEventsTable( CEntity *entity )
{
	if (!IsValid())
		return;
	assert( entity != 0 );

	IEntity *ientity = entity->GetIEntity();
	if (!ientity)
		return;

	IScriptTable *scriptObject = ientity->GetScriptTable();
	if (!scriptObject)
		return;

	// If events target table is null, set event table to null either.
	if (entity->GetEventTargetCount() == 0)
	{
		if (m_haveEventsTable)
		{
			scriptObject->SetToNull( "Events" );
		}
		m_haveEventsTable = false;
		return;
	}

	IScriptSystem *scriptSystem = GetIEditor()->GetSystem()->GetIScriptSystem();
	SmartScriptTable pEvents( scriptSystem,false );

	scriptObject->SetValue( "Events",*pEvents );
	m_haveEventsTable = true;
	
	std::set<CString> sourceEvents;
	for (int i = 0; i < entity->GetEventTargetCount(); i++)
	{
		CEntityEventTarget &et = entity->GetEventTarget(i);
		sourceEvents.insert( et.sourceEvent );
	}
	for (std::set<CString>::iterator it = sourceEvents.begin(); it != sourceEvents.end(); it++)
	{
		SmartScriptTable pTrgEvents( scriptSystem,false );
		CString sourceEvent = *it;

		//int srcEventId = EventNameToId( sourceEvent );
		//pEvents->SetAt( srcEventId,*pTrgEvents );
		pEvents->SetValue( sourceEvent,*pTrgEvents );

		// Put target events to table.
		int trgEventIndex = 1;
		for (int i = 0; i < entity->GetEventTargetCount(); i++)
		{
			CEntityEventTarget &et = entity->GetEventTarget(i);
			if (stricmp(et.sourceEvent,sourceEvent) == 0)
			{
				EntityId entityId = 0;
				if (et.target)
				{
					if (et.target->IsKindOf( RUNTIME_CLASS(CEntity) ))
						entityId = ((CEntity*)et.target)->GetEntityId();
				}

				SmartScriptTable pTrgEvent( scriptSystem,false );
				pTrgEvents->SetAt( trgEventIndex,*pTrgEvent );
				trgEventIndex++;

				ScriptHandle idHandle;
				idHandle.n = entityId;

				pTrgEvent->SetAt( 1,idHandle );
				pTrgEvent->SetAt( 2,(const char*)et.event );
			}
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////
// CEntityScriptRegistry implementation.
//////////////////////////////////////////////////////////////////////////
CEntityScriptRegistry* CEntityScriptRegistry::m_instance = 0;

CEntityScriptRegistry::CEntityScriptRegistry()
{
}

CEntityScriptRegistry::~CEntityScriptRegistry()
{
	m_instance = 0;
	m_scripts.Clear();
}

CEntityScript* CEntityScriptRegistry::Find( const CString &name )
{
	CEntityScriptPtr script = 0;
	if (m_scripts.Find( name,script ))
	{
		return script;
	}
	return 0;
}
	
void CEntityScriptRegistry::Insert( CEntityScript *script )
{
	// Check if inserting already exist script, if so ignore.
	CEntityScriptPtr temp;
	if (m_scripts.Find( script->GetName(),temp ))
	{
		Error( "Inserting duplicate Entity Script %s",(const char*)script->GetName() );
		return;
	}
	m_scripts[script->GetName()] = script;
}

void CEntityScriptRegistry::GetScripts( std::vector<CEntityScript*> &scripts )
{
	std::vector<CEntityScriptPtr> s;
	m_scripts.GetAsVector( s );

	scripts.resize( s.size() );
	for (int i = 0; i < s.size(); i++)
	{
		scripts[i] = s[i];
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::Reload()
{
	IEntityClassRegistry *pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	pClassRegistry->LoadClasses( "Entities",true );
	LoadScripts();
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::LoadScripts()
{
	m_scripts.Clear();

	IEntityClassRegistry *pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	IEntityClass *pClass = NULL;
	pClassRegistry->IteratorMoveFirst();
	while (pClass = pClassRegistry->IteratorNext())
	{
		CEntityScript *script = new CEntityScript( pClass );
		Insert( script );
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityScriptRegistry* CEntityScriptRegistry::Instance()
{
	if (!m_instance)
	{
		m_instance = new CEntityScriptRegistry;
	}
	return m_instance;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::Release()
{
	if (m_instance)
	{
		delete m_instance;
	}
	m_instance = 0;
}