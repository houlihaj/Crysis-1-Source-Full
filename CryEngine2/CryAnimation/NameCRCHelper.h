#ifndef _NAMECRCHELPER
#define _NAMECRCHELPER

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


//////////////////////////////////////////////////////////////////////////
// Class with CRC sum. Used for optimizations  s
//////////////////////////////////////////////////////////////////////////

//#include "VectorMap.h"
#include "CryName.h"

#define MAX_STATIC_CHARS	4096

#if !defined(USE_STATIC_NAME_TABLE)
	#define USE_STATIC_NAME_TABLE
#endif

struct CNameCRCHelper
{
public:
	CNameCRCHelper(void) : m_CRC32Name(~0) {};
	virtual ~CNameCRCHelper(void) {};

	uint32 GetCRC32() const { return m_CRC32Name;};

protected:
	string m_Name;		// the name of the animation (not the name of the file) - unique per-model
	//CCryName m_Name;
	uint32 m_CRC32Name;			//hash value for searching animations

	const char * GetName() const { return m_Name.c_str(); };
//	const string& GetNameString() const { return m_Name; };

	void SetName(const string& name);
//	void SetNameLower(const string& name);

	void SetNameChar(const char * name);
};




//////////////////////////////////////////////////////////////////////////
// Custom hash map class. 
//////////////////////////////////////////////////////////////////////////
struct CNameCRCMap
{


	// fast method 
	typedef std::map<uint32, size_t> NameHashMap;
		//typedef VectorMap<uint32, size_t> NameHashMap;


	//----------------------------------------------------------------------------------
	// Returns the index of the animation from crc value
	//----------------------------------------------------------------------------------
	size_t	GetValueCRC(uint32 crc) const
	{
		NameHashMap::const_iterator it = m_HashMap.find(crc);

		if (it == m_HashMap.end())
			return -1;

		return it->second;
	}

	//----------------------------------------------------------------------------------
	// Returns the index of the animation from name. Name converted in lower case in this function
	//----------------------------------------------------------------------------------

	size_t GetValue(const char * name) const;

	//----------------------------------------------------------------------------------
	// Returns the index of the animation from name. Name should be in lower case!!!
	//----------------------------------------------------------------------------------

	size_t GetValueLower(const char * name) const;

	//----------------------------------------------------------------------------------
	// In
	//----------------------------------------------------------------------------------

	bool InsertValue(CNameCRCHelper * header, size_t num)
	{
		bool res = m_HashMap.find(header->GetCRC32()) == m_HashMap.end();
		//if (m_HashMap.find(header->GetCRC32()) != m_HashMap.end())
		//{
		//	AnimWarning("[Animation] %s exist in the model!", header->GetName());
		//}
		m_HashMap[header->GetCRC32()] = num;

		return res;

	}

	bool InsertValue(uint32 crc, size_t num)
	{
		bool res = m_HashMap.find(crc) == m_HashMap.end();
		m_HashMap[crc] = num;

		return res;
	}

	size_t GetSize() const
	{
		return m_HashMap.size() * (sizeof(uint32) + sizeof(size_t));
	}

	size_t GetMapSize() const
	{
		return m_HashMap.size();
	}

protected:
	NameHashMap		m_HashMap;
};

//////////////////////////////////////////////////////////////////////////
// Class with definition GetAnimName and SetAnimName functions. For convenience 
//////////////////////////////////////////////////////////////////////////
/*
struct CAnimNameHelper : public CNameCRCHelper
{
	const char * GetAnimName() const { return GetName(); };
	const string& GetAnimNameString() const { return GetNameString(); };

	void SetAnimName(const string& name) { SetName(name); };

};

//////////////////////////////////////////////////////////////////////////
// Class with definition GetAnimName and SetAnimName functions. For convenience 
//////////////////////////////////////////////////////////////////////////
struct CPathNameHelper : public CNameCRCHelper
{
	const char * GetPathName() const { return GetName(); };
	const string& GetPathNameString() const { return GetNameString(); };

	void SetPathName(const string& name) { SetName(name); };
};

//////////////////////////////////////////////////////////////////////////
// Class with definition GetAnimName and SetAnimName functions. For convenience 
//////////////////////////////////////////////////////////////////////////
struct CJointNameHelper : public CNameCRCHelper
{
	const char * GetJointName() const { return GetName(); };
	const string& GetJointNameString() const { return GetNameString(); };

	void SetJointName(const string& name) { SetName(name); };
};
*/


#endif