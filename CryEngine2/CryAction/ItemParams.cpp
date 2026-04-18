/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:8:2005   17:27 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ItemParams.h"

//------------------------------------------------------------------------
CItemParamsNode::CItemParamsNode()
: m_refs(1)
{
};

//------------------------------------------------------------------------
CItemParamsNode::~CItemParamsNode()
{
	for (TChildVector::iterator it=m_children.begin(); it!=m_children.end();++it)
		delete (*it);
}

//------------------------------------------------------------------------
int CItemParamsNode::GetAttributeCount() const
{
	return m_attributes.size();
}

//------------------------------------------------------------------------
const char *CItemParamsNode::GetAttributeName(int i) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
		return it->first.c_str();
	return 0;
}

//------------------------------------------------------------------------
const char *CItemParamsNode::GetAttribute(int i) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
	{
		const string *str=it->second.GetPtr<string>();
		if (str)
		{
			return str->c_str();
		}
	}
	return 0;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(int i, Vec3 &attr) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(int i, Ang3 &attr) const
{
	Vec3 temp;
	bool r = GetAttribute(i, temp);
	attr = Ang3(temp);
	return r;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(int i, float &attr) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(int i, int &attr) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
int CItemParamsNode::GetAttributeType(int i) const
{
	TAttributeMap::const_iterator it = GetConstIterator<TAttributeMap>(m_attributes, i);
	if (it != m_attributes.end())
		return it->second.GetType();
	return eIPT_None;
}

//------------------------------------------------------------------------
const char *CItemParamsNode::GetNameAttribute() const
{
	return m_nameAttribute.c_str();
}

//------------------------------------------------------------------------
const char *CItemParamsNode::GetAttribute(const char *name) const
{
	TAttributeMap::const_iterator it = FindAttrIterator(m_attributes,name);
	if (it != m_attributes.end())
	{
		const string *str=it->second.GetPtr<string>();
		if (str)
			return str->c_str();
	}
	return 0;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(const char *name, Vec3 &attr) const
{
	TAttributeMap::const_iterator it = FindAttrIterator(m_attributes,name);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(const char *name, Ang3 &attr) const
{
	Vec3 temp;
	bool r = GetAttribute(name, temp);
	if (r)
		attr = Ang3(temp);
	return r;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(const char *name, float &attr) const
{
	TAttributeMap::const_iterator it = FindAttrIterator(m_attributes,name);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
bool CItemParamsNode::GetAttribute(const char *name, int &attr) const
{
	TAttributeMap::const_iterator it = FindAttrIterator(m_attributes,name);
	if (it != m_attributes.end())
		return it->second.GetValueWithConversion(attr);
	return false;
}

//------------------------------------------------------------------------
int CItemParamsNode::GetAttributeType(const char *name) const
{
	TAttributeMap::const_iterator it = FindAttrIterator(m_attributes,name);
	if (it != m_attributes.end())
		return it->second.GetType();
	return eIPT_None;
}

//------------------------------------------------------------------------
int CItemParamsNode::GetChildCount() const
{
	return (int)m_children.size();
}

//------------------------------------------------------------------------
const char *CItemParamsNode::GetChildName(int i) const
{
	if (i>=0&&i<m_children.size())
		return m_children[i]->GetName();
	return 0;
}

//------------------------------------------------------------------------
const IItemParamsNode *CItemParamsNode::GetChild(int i) const
{
	if (i>=0&&i<m_children.size())
		return m_children[i];
	return 0;
}

//------------------------------------------------------------------------
const IItemParamsNode *CItemParamsNode::GetChild(const char *name) const
{
	for (TChildVector::const_iterator it = m_children.begin(); it != m_children.end(); it++)
	{
		if (!strcmpi((*it)->GetName(), name))
			return (*it);
	}
	return 0;
}

//------------------------------------------------------------------------
void CItemParamsNode::SetAttribute(const char *name, const char *attr)
{
	//m_attributes.insert(TAttributeMap::value_type(name, string(attr)));
	if (!strcmpi(name, "name"))
	{
		m_nameAttribute = attr;
		AddAttribute( name,TItemParamValue(m_nameAttribute) );
	}
	else
		AddAttribute( name,TItemParamValue(string(attr)) );
}

//------------------------------------------------------------------------
void CItemParamsNode::SetAttribute(const char *name, const Vec3 &attr)
{
	//m_attributes.insert(TAttributeMap::value_type(name, attr));
	AddAttribute( name,TItemParamValue(attr) );
}

//------------------------------------------------------------------------
void CItemParamsNode::SetAttribute(const char *name, float attr)
{
	//m_attributes.insert(TAttributeMap::value_type(name, attr));
	AddAttribute( name,TItemParamValue(attr) );
}

//------------------------------------------------------------------------
void CItemParamsNode::SetAttribute(const char *name, int attr)
{
	//m_attributes.insert(TAttributeMap::value_type(name, attr));
	AddAttribute( name,TItemParamValue(attr) );
}

//------------------------------------------------------------------------
IItemParamsNode *CItemParamsNode::InsertChild(const char *name)
{
	m_children.push_back(new CItemParamsNode());
	IItemParamsNode *inserted = m_children.back();
	inserted->SetName(name);
	return inserted;
}


//------------------------------------------------------------------------
bool IsInteger(const char *v, int *i=0)
{
	errno=0;
	char *endptr=0;
	int r = strtol(v, &endptr, 0);
	if (errno && (errno!=ERANGE))
		return false;
	if(endptr==v || *endptr!='\0')
		return false;
	if (i)
		*i=r;
	return true;
}

//------------------------------------------------------------------------
bool IsFloat(const char *v, float *f=0)
{
	errno=0;
	char *endptr=0;
	float r = strtod(v, &endptr);
	if (errno && (errno!=ERANGE))
		return false;
	if(endptr==v || *endptr!='\0')
		return false;
	if (f)
		*f=r;
	return true;
}

//------------------------------------------------------------------------
bool IsVec3(const char *v, Vec3 *vec)
{
	float x,y,z;
	if (sscanf(v, "%f,%f,%f", &x, &y, &z) != 3)
		return false;
	if (vec)
		vec->Set(x, y, z);
	return true;
}

//------------------------------------------------------------------------
void CItemParamsNode::ConvertFromXML(XmlNodeRef &root)
{
	int nattributes = root->getNumAttributes();
	m_attributes.reserve(nattributes);
	for (int a=0; a<nattributes; a++)
	{
		const char *name=0;
		const char *value=0;
		if (root->getAttributeByIndex(a, &name, &value))
		{
			float f;
			int i;
			Vec3 v;
			if (!stricmp(value, "true"))
				SetAttribute(name, 1);
			else if (!stricmp(value, "false"))
				SetAttribute(name, 0);
			else if (IsInteger(value, &i))
				SetAttribute(name, i);
			else if (IsFloat(value, &f))
				SetAttribute(name, f);
			else if (IsVec3(value, &v))
				SetAttribute(name, v);
			else
				SetAttribute(name, value);
		}
	}

	int nchildren = root->getChildCount();
	m_children.reserve(nchildren);
	for (int c=0; c<nchildren; c++)
	{
		XmlNodeRef child = root->getChild(c);
		InsertChild(child->getTag())->ConvertFromXML(child);
	}
}

int CItemParamsNode::GetMemorySize() const
{
	int nSize = sizeof(*this);
	nSize += m_name.length();
	nSize += m_nameAttribute.length();
	for (TAttributeMap::const_iterator iter = m_attributes.begin(); iter != m_attributes.end(); ++iter)
	{
		nSize += iter->first.length();
		nSize += iter->second.GetMemorySize();
	}
	for (TChildVector::const_iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
	{
		nSize += (*iter)->GetMemorySize();
	}
	return nSize;
}