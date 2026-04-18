#include "stdafx.h"

#include "NameCRCHelper.h"
#include "crc32.h"

#define _USE_LOWERCASE

#ifdef _USE_LOWERCASE
static char m_StringArray[MAX_STATIC_CHARS];
#endif

#define __ascii_tolower(c)      ( (((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c) )


void MakeLowercase(char * dest, const char * name)
{
	char * p = m_StringArray;

	for (const char * src = name; *src; ++src)
	{
		*(p++) = __ascii_tolower(*src);
	}
	*p = 0;

}

//----------------------------------------------------------------------------------
// Set name and compute CRC value for it
//----------------------------------------------------------------------------------
void CNameCRCHelper::SetName(const string& name)
{ 
	m_Name = name; 

#ifdef _USE_LOWERCASE

	assert (name.size() < MAX_STATIC_CHARS);

	MakeLowercase(m_StringArray, name.c_str());

	m_CRC32Name = g_pCrc32Gen->GetCRC32(m_StringArray); 
	
#else
	m_CRC32Name = g_pCrc32Gen->GetCRC32(m_Name.c_str()); 
#endif
}

void CNameCRCHelper::SetNameChar(const char * name)
{
	m_Name = name; 
#ifdef _USE_LOWERCASE

	assert (strlen(name) < MAX_STATIC_CHARS);

	MakeLowercase(m_StringArray, name);

	m_CRC32Name = g_pCrc32Gen->GetCRC32(m_StringArray); 

#else
	m_CRC32Name = g_pCrc32Gen->GetCRC32(m_Name.c_str()); 
#endif
}

//void CNameCRCHelper::SetNameLower(const string& name)
//{ 
//	m_Name = name; 
//	m_CRC32Name = g_pCrc32Gen->GetCRC32(name.c_str()); 
//
//}


//----------------------------------------------------------------------------------
// Returns the index of the animation in the vector, -1(npos) if there's no such animation
//----------------------------------------------------------------------------------
size_t CNameCRCMap::GetValue(const char * name) const
{
#ifdef _USE_LOWERCASE

	assert (strlen(name) < MAX_STATIC_CHARS);
	MakeLowercase(m_StringArray, name);

	return GetValueCRC(g_pCrc32Gen->GetCRC32(m_StringArray));
#else
	return GetValueCRC(g_pCrc32Gen->GetCRC32(name));
#endif
}

//----------------------------------------------------------------------------------
// Returns the index of the animation in the vector, -1(npos) if there's no such animation
//----------------------------------------------------------------------------------
size_t CNameCRCMap::GetValueLower(const char * name) const
{
	return GetValueCRC(g_pCrc32Gen->GetCRC32(name));
}
