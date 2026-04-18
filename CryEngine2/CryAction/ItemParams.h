/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Item parameters structure.

-------------------------------------------------------------------------
History:
- 7:10:2005   14:20 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMPARAMS_H__
#define __ITEMPARAMS_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IItemSystem.h"
#include <ConfigurableVariant.h>
#include <StlUtils.h>


template <class F, class T>
struct SItemParamConversion
{
	static ILINE bool ConvertValue(const F &from, T &to)
	{
		to = (T)from;
		return true;
	};
};


// taken from IFlowSystem.h and adapted...
#define ITEMSYSTEM_STRING_CONVERSION(type, fmt) \
	template <> \
	struct SItemParamConversion<type, string> \
	{ \
		static ILINE bool ConvertValue( const type& from, string& to ) \
	{ \
		to.Format( fmt, from ); \
		return true; \
	} \
	}; \
	template <> \
	struct SItemParamConversion<string, type> \
	{ \
		static ILINE bool ConvertValue( const string& from, type& to ) \
	{ \
		return 1 == sscanf( from.c_str(), fmt, &to ); \
	} \
	};

ITEMSYSTEM_STRING_CONVERSION(int, "%d");
ITEMSYSTEM_STRING_CONVERSION(float, "%f");

#undef ITEMSYSTEM_STRING_CONVERSION


template<>
struct SItemParamConversion<Vec3, bool>
{
	static ILINE bool ConvertValue(const Vec3 &from, float &to)
	{
		to = from.GetLengthSquared()>0;
		return true;
	}
};

template<>
struct SItemParamConversion<bool, Vec3>
{
	static ILINE bool ConvertValue(const float &from, Vec3 &to)
	{
		to = Vec3(from?1:0, from?1:0, from?1:0);
		return true;
	}
};

template<>
struct SItemParamConversion<Vec3, float>
{
	static ILINE bool ConvertValue(const Vec3 &from, float &to)
	{
		to = from.x;
		return true;
	}
};

template<>
struct SItemParamConversion<float, Vec3>
{
	static ILINE bool ConvertValue(const float &from, Vec3 &to)
	{
		to = Vec3(from, from, from);
		return true;
	}
};

template<>
struct SItemParamConversion<Vec3, int>
{
	static ILINE bool ConvertValue(const Vec3 &from, int &to)
	{
		to = (int)from.x;
		return true;
	}
};

template<>
struct SItemParamConversion<int, Vec3>
{
	static ILINE bool ConvertValue(const int &from, Vec3 &to)
	{
		to = Vec3((float)from, (float)from, (float)from);
		return true;
	}
};

template<>
struct SItemParamConversion<Vec3, string>
{
	static ILINE bool ConvertValue(const Vec3 &from, string &to)
	{
		to.Format("%s,%s,%s", from.x, from.y, from.z);
		return true;
	}
};

template<>
struct SItemParamConversion<string, Vec3>
{
	static ILINE bool ConvertValue(const string &from, Vec3 &to)
	{
		return sscanf(from.c_str(), "%f,%f,%f", &to.x, &to.y, &to.z) == 3;
	}
};


typedef	CConfigurableVariant<TItemParamTypes, sizeof(void *), SItemParamConversion> TItemParamValue;


class CItemParamsNode: public IItemParamsNode
{
public:
	CItemParamsNode();
	virtual ~CItemParamsNode();

	virtual void AddRef() const { ++m_refs; };
	virtual uint GetRefCount() const { return m_refs; };
	virtual void Release() const { if (!--m_refs) delete this; };

	virtual int GetMemorySize() const;

	virtual int GetAttributeCount() const;
	virtual const char *GetAttributeName(int i) const;
	virtual const char *GetAttribute(int i) const;
	virtual bool GetAttribute(int i, Vec3 &attr) const;
	virtual bool GetAttribute(int i, Ang3 &attr) const;
	virtual bool GetAttribute(int i, float &attr) const;
	virtual bool GetAttribute(int i, int &attr) const;
	virtual int GetAttributeType(int i) const;

	virtual const char *GetAttribute(const char *name) const;
	virtual bool GetAttribute(const char *name, Vec3 &attr) const;
	virtual bool GetAttribute(const char *name, Ang3 &attr) const;
	virtual bool GetAttribute(const char *name, float &attr) const;
	virtual bool GetAttribute(const char *name, int &attr) const;
	virtual int GetAttributeType(const char *name) const;

	virtual const char *GetNameAttribute() const;
	
	virtual int GetChildCount() const;
	virtual const char *GetChildName(int i) const;
	virtual const IItemParamsNode *GetChild(int i) const;
	virtual const IItemParamsNode *GetChild(const char *name) const;

	virtual void SetAttribute(const char *name, const char *attr);
	virtual void SetAttribute(const char *name, const Vec3 &attr);
	virtual void SetAttribute(const char *name, float attr);
	virtual void SetAttribute(const char *name, int attr);

	virtual void SetName(const char *name) { m_name = name; };
	virtual const char *GetName() const { return m_name.c_str(); };

	virtual IItemParamsNode *InsertChild(const char *name);
	virtual void ConvertFromXML(XmlNodeRef &root);

private:
	struct SAttribute
	{
		CCryName first; // Using CryName to save memory on duplicate strings
		TItemParamValue second;

		SAttribute() {}
		SAttribute( const char *key,const TItemParamValue &val) { first = key; second = val; }
	};

	//typedef std::map<string, TItemParamValue, stl::less_stricmp<string> >	TAttributeMap;
	typedef std::vector<SAttribute>	TAttributeMap;
	typedef std::vector<CItemParamsNode *>																TChildVector;

	template<typename MT>
		typename MT::const_iterator GetConstIterator(const MT &m, int i) const
	{
		typename MT::const_iterator it = m.begin();
		//std::advance(it, i);
		it += i;
		return it;
	};
	TAttributeMap::const_iterator FindAttrIterator( const TAttributeMap &m,const char *name) const
	{
		TAttributeMap::const_iterator it;
		for (it = m.begin(); it != m.end(); it++)
		{
			if (stricmp(it->first.c_str(),name) == 0)
				return it;
		}
		return m.end();
	};
	TAttributeMap::iterator FindAttrIterator( TAttributeMap &m,const char *name) const
	{
		TAttributeMap::iterator it;
		for (it = m.begin(); it != m.end(); it++)
		{
			if (stricmp(it->first.c_str(),name) == 0)
				return it;
		}
		return m.end();
	};
	void AddAttribute( const char *name,const TItemParamValue &val )
	{
		TAttributeMap::iterator it = FindAttrIterator(m_attributes,name);
		if (it == m_attributes.end())
		{
			m_attributes.push_back( SAttribute(name,val) );
		}
		else
			it->second = val;
	}

	CCryName      m_name;
	string				m_nameAttribute;
	TAttributeMap	m_attributes;
	TChildVector	m_children;
	
	mutable uint	m_refs;
};


#endif