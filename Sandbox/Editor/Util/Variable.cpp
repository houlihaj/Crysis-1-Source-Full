#include <StdAfx.h>

#include "Variable.h"
#include "UIEnumsDatabase.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CVarBlock::~CVarBlock()
{
	/*
	// When destroying var block, callbacks of all holded variables must be deleted.
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		var->
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CVarBlock::Clone( bool bRecursive ) const
{
	CVarBlock *vb = new CVarBlock;
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		vb->AddVariable( var->Clone(bRecursive) );
	}
	return vb;
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::CopyValues( CVarBlock *fromVarBlock )
{
	// Copy all variables.
	int numSrc = fromVarBlock->GetVarsCount();
	int numTrg = GetVarsCount();
	for (int i = 0; i < numSrc && i < numTrg; i++)
	{
		GetVariable(i)->CopyValue( fromVarBlock->GetVariable(i) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::CopyValuesByName( CVarBlock *fromVarBlock )
{
	// Copy values using saving and loading to/from xml.
	XmlNodeRef node = CreateXmlNode( "Temp" );
	fromVarBlock->Serialize( node,false );
	Serialize( node,true );
	/*
	// Copy all variables.
	int numSrc = fromVarBlock->GetVarsCount();
	for (int i = 0; i < numSrc; i++)
	{
		IVariable *srcVar = fromVarBlock->GetVariable(i);
		IVariable *trgVar = FindVariable( srcVar->GetName(),false );
		if (trgVar)
		{
			CopyVarByName( srcVar,trgVar );
		}
	}
	*/
}

/*
//////////////////////////////////////////////////////////////////////////
void CVarBlock::CopyVarByName( IVariable *src,IVariable *trg )
{
	assert( src && trg );
	// Check if type match.
	if (src->GetType() != trg->GetType())
		return;

	int numSrc = src->NumChildVars();
	if (numSrc == 0)
	{
		// Copy single value.
		trg->CopyValue( src );
		return;
	}

	int i;
	int numTrg = trg->NumChildVars();
	std::map<CString,IVariable*> nameMap;
	for (int i = 0; i < numTrg; i++)
	{
	}

	// Copy array items, matching by name.
	for (int i = 0; i < numSrc; i++)
	{
		IVariable *srcChild = src->GetChildVar(i);
		IVariable *trgChild = 0;
		FindVariable( srcChild->GetName(),false );
		if (trgChild)
		{
			CopyVarByName( srcChild,trgChild );
		}
	}
}
*/

//////////////////////////////////////////////////////////////////////////
void CVarBlock::OnSetValues()
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		var->OnSetValue(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable( IVariable *var )
{
	m_vars.push_back(var);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable( IVariable *pVar,const char *varName,unsigned char dataType )
{
	if (varName)
		pVar->SetName(varName);
	pVar->SetDataType(dataType);
	AddVariable(pVar);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable( CVariableBase &var,const char *varName,unsigned char dataType )
{
	if (varName)
		var.SetName(varName);
	var.SetDataType(dataType);
	AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::RemoveVariable( IVariable *var )
{
  stl::find_and_erase(m_vars,var);
}

//////////////////////////////////////////////////////////////////////////
bool CVarBlock::IsContainVariable( IVariable *pVar,bool bRecursive ) const
{
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		if (*it == pVar)
			return true;
	}
	// If not found search childs.
	if (bRecursive)
	{
		// Search all top level variables.
		for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			if (ContainChildVar(*it,pVar))
				return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
IVariable* CVarBlock::FindVariable( const char *name,bool bRecursive ) const
{
	// Search all top level variables.
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		if (strcmp(var->GetName(),name) == 0)
			return var;
	}
	// If not found search childs.
	if (bRecursive)
	{
		// Search all top level variables.
		for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			IVariable *var = *it;
			IVariable *found = FindChildVar(name,var);
			if (found)
				return found;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
IVariable* CVarBlock::FindChildVar( const char *name,IVariable *pParentVar ) const
{
	if (strcmp(pParentVar->GetName(),name) == 0)
		return pParentVar;
	int numSubVar = pParentVar->NumChildVars();
	for (int i = 0; i < numSubVar; i++)
	{
		IVariable *var = FindChildVar( name,pParentVar->GetChildVar(i) );
		if (var)
			return var;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CVarBlock::ContainChildVar( IVariable *pParentVar,IVariable *pVar ) const
{
	int numSubVar = pParentVar->NumChildVars();
	for (int i = 0; i < numSubVar; i++)
	{
		IVariable *pChild = pParentVar->GetChildVar(i);
		if (pChild == pVar)
			return true;
		if (ContainChildVar(pChild,pVar))
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Serialize( XmlNodeRef &vbNode,bool load )
{
	if (load)
	{
		// Loading.
		CString name;
		for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			IVariable *var = *it;
			if (var->NumChildVars())
			{
				XmlNodeRef child = vbNode->findChild(var->GetName());
				if (child)
					var->Serialize( child,load );
			}
			else
				var->Serialize( vbNode,load );
		}
	}
	else
	{
		// Saving.
		for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			IVariable *var = *it;
			if (var->NumChildVars())
			{
				XmlNodeRef child = vbNode->newChild(var->GetName());
				var->Serialize( child,load );
			}
			else
				var->Serialize( vbNode,load );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::ReserveNumVariables( int numVars )
{
	m_vars.reserve( numVars );
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::WireVar( IVariable *src,IVariable *trg,bool bWire )
{
	if (bWire)
		src->Wire(trg);
	else
		src->Unwire(trg);
	int numSrcVars = src->NumChildVars();
	if (numSrcVars > 0)
	{
		int numTrgVars = trg->NumChildVars();
		for (int i = 0; i < numSrcVars && i < numTrgVars; i++)
		{
			WireVar(src->GetChildVar(i),trg->GetChildVar(i),bWire);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Wire( CVarBlock *toVarBlock )
{
	Variables::iterator tit = toVarBlock->m_vars.begin();
	Variables::iterator sit = m_vars.begin();
	for (; sit != m_vars.end() && tit != toVarBlock->m_vars.end(); ++sit,++tit)
	{
		IVariable *src = *sit;
		IVariable *trg = *tit;
		WireVar(src,trg,true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Unwire( CVarBlock *toVarBlock )
{
	Variables::iterator tit = toVarBlock->m_vars.begin();
	Variables::iterator sit = m_vars.begin();
	for (; sit != m_vars.end() && tit != toVarBlock->m_vars.end(); ++sit,++tit)
	{
		IVariable *src = *sit;
		IVariable *trg = *tit;
		WireVar(src,trg,false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddOnSetCallback( IVariable::OnSetCallback func )
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		SetCallbackToVar( func,var,true );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::RemoveOnSetCallback( IVariable::OnSetCallback func )
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable *var = *it;
		SetCallbackToVar( func,var,false );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::SetCallbackToVar( IVariable::OnSetCallback func,IVariable *pVar,bool bAdd )
{
	if (bAdd)
		pVar->AddOnSetCallback(func);
	else
		pVar->RemoveOnSetCallback(func);
	int numVars = pVar->NumChildVars();
	if (numVars > 0)
	{
		for (int i = 0; i < numVars; i++)
		{
			SetCallbackToVar( func,pVar->GetChildVar(i),bAdd );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::GatherUsedResources( CUsedResources &resources )
{
	for (int i = 0; i < GetVarsCount(); i++)
	{
		IVariable *pVar = GetVariable(i);
		GatherUsedResourcesInVar( pVar,resources );
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::GatherUsedResourcesInVar( IVariable *pVar,CUsedResources &resources )
{
	int type = pVar->GetDataType();
	if (type == IVariable::DT_FILE || type == IVariable::DT_OBJECT || type == IVariable::DT_TEXTURE)
	{
		// this is file.
		CString filename;
		pVar->Get( filename );
		if (!filename.IsEmpty())
			resources.Add( filename );
	}

	if (type == IVariable::DT_SOUND)
	{
		// this is a sound
		CString filename;
		pVar->Get( filename );
		if (!filename.IsEmpty())
		{
			// quick check if the sound is an event
			const char *pColon1 = strstr(filename,":");

			if (pColon1)
			{
				// path/project:group:event

				// sound event needs .fev and .fsb files
				// need to create the event and query for its .fsb file
				// .fev is path/project/project.fev
			}
			else
			{
				// other sound file
				resources.Add( filename );
			}
		}
	}

	for (int i = 0; i < pVar->NumChildVars(); i++)
	{
		GatherUsedResourcesInVar( pVar->GetChildVar(i),resources );
	}
}

//////////////////////////////////////////////////////////////////////////
CVarObject::CVarObject()
{}

//////////////////////////////////////////////////////////////////////////
CVarObject::~CVarObject()
{}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable( CVariableBase &var,const CString &varName,VarOnSetCallback cb,unsigned char dataType )
{
	if (!m_vars)
		m_vars = new CVarBlock;
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	m_vars->AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable( CVariableBase &var,const CString &varName,const CString &varHumanName,VarOnSetCallback cb,unsigned char dataType )
{
	if (!m_vars)
		m_vars = new CVarBlock;
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetHumanName(varHumanName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	m_vars->AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable( CVariableArray &table,CVariableBase &var,const CString &varName,VarOnSetCallback cb,unsigned char dataType )
{
	if (!m_vars)
		m_vars = new CVarBlock;
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	table.AddChildVar(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable( CVariableArray &table,CVariableBase &var,const CString &varName,const CString &varHumanName,VarOnSetCallback cb,unsigned char dataType )
{
	if (!m_vars)
		m_vars = new CVarBlock;
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetHumanName(varHumanName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	table.AddChildVar(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::RemoveVariable( IVariable *var )
{
  m_vars->RemoveVariable(var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::ReserveNumVariables( int numVars )
{
	m_vars->ReserveNumVariables( numVars );
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::CopyVariableValues( CVarObject *sourceObject )
{
	// Check if compatable types.
	assert( GetRuntimeClass() == sourceObject->GetRuntimeClass() );
	if (m_vars != NULL && sourceObject->m_vars != NULL)
		m_vars->CopyValues(sourceObject->m_vars);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::Serialize( XmlNodeRef node,bool load )
{
	if (m_vars)
	{
		m_vars->Serialize( node,load );
	}
}
/*
//////////////////////////////////////////////////////////////////////////
class CEntity : public CVarObject
{
public:
	CEntity();
public:
	CVariable<Vec3> m_pos;
	CVariable<Vec3> m_angles;
	CVariable<float> m_scale;
	CVariable<float> m_fov;

	CVarBlock vars;

	void Serialize( XmlNodeRef node,bool load )
	{
		CVarObject::Serialize( node,load );
		if (load)
		{
			XmlNodeRef props = node->findChild("Properties");
			if (props)
				vars.Serialize( props,load );
		}
		else
		{
			XmlNodeRef props = node->newChild("Properties");
			vars.Serialize( props,load );
		}
	}

private:
	void OnPosChanged( IVariable *var ) const
	{
		std::cout << "Var Changed: " << var->GetName() << "\n";
	}
	void OnFovChanged( IVariable *var ) const
	{
		std::cout << "FOV FOV! Changed: " << var->GetName() << "\n";
	}
	void Cool( int i )
	{
		std::cout << "" << i << "\n";
	}
};

//////////////////////////////////////////////////////////////////////////
CEntity::CEntity()
{
//	typedef void (CEntity::*MemFunc)(IVariable*);
//	MemFunc f = CEntity::OnPosChanged;
//	CBMemberTranslator1<IVariable*,CEntity,MemFunc>(*this,f);

	AddVariable( "Pos",0,m_pos,functor(*this,OnPosChanged) );
	AddVariable( "Angles",0,m_angles );
	AddVariable( "Scale",0,m_scale );
	AddVariable( "Fov",0,m_fov );

	CVariableBase *v;

	v = new CVariable<float>;
	v->SetName("Width");
	v->Set(10);
	vars.AddVariable(v);

	v = new CVariable<float>;
	v->SetName("Height");
	v->Set(20);
	vars.AddVariable(v);
}


void main()
{
	CEntity *entity = new CEntity;
	entity->m_pos = Vec3(4,5,6);
	entity->m_angles = Vec3(1,2,3);
	entity->m_fov = 10;
	entity->m_scale = 30;

	XmlNodeRef node = CreateXmlNode( "Root" );
	//XmlParser parser;
	//node = parser.parse("c:\\test.xml");
	entity->Serialize( node,false );
	node->saveToFile("c:\\test.xml");

	std::vector<int> v;

	std::list<char> lst;

	float f = 4.452f;
	double d;
	int i;
	short s;
	unsigned char c;
	Vec3 vec;

	std::stringstream ss;
	ss << f;
	ss >> f;

	//cast_stream(f,i);
	//cast_stream(f,d);
	//cast_stream(f,s);
	//cast_stream(f,c);

	//Tvareter<int> a("aa");
	//Tvareter<Vec3> b("bb");
	std::stringstream str;

	str << f;
	str >> i;
	str >> d;
	str >> s;
	str >> c;

	typeid(int);
	
	str >> s;

	CVariable<float> vara;
	f = vara;
	vara = f;
}
*/

CVarGlobalEnumList::CVarGlobalEnumList(const CString& enumName)
{
	m_pEnum = GetIEditor()->GetUIEnumsDatabase()->FindEnum(enumName);
}

//! Get the number of entries in enumeration.
int CVarGlobalEnumList::GetItemsCount() {
	if (m_pEnum)
		return m_pEnum->strings.size();
	else
		return 0;
}

//! Get the name of specified value in enumeration.
const char* CVarGlobalEnumList::GetItemName( int index )
{
	if (m_pEnum == 0)
		return "";
	assert( index >= 0 && index < m_pEnum->strings.size() );
	return m_pEnum->strings[index];
}

CString CVarGlobalEnumList::NameToValue(const CString& name)
{
	if (m_pEnum)
		return m_pEnum->NameToValue(name);
	else
		return name;
}

CString CVarGlobalEnumList::ValueToName(const CString& value)
{
	if (m_pEnum)
		return m_pEnum->ValueToName(value);
	else
		return value;
}

