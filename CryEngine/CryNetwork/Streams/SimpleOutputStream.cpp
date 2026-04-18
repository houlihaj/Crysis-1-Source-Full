#include "StdAfx.h"
#include "SimpleOutputStream.h"

CSimpleOutputStream::CSimpleOutputStream( size_t numRecords ) : m_numRecords(numRecords), m_curRecord(0), m_records(new SStreamRecord[m_numRecords])
{
}

CSimpleOutputStream::~CSimpleOutputStream()
{
	delete[] m_records;
}

void CSimpleOutputStream::Put( const SStreamRecord& record )
{
	m_records[m_curRecord++] = record;
	if (m_curRecord == m_numRecords)
	{
		Flush(m_records, m_numRecords);
		m_curRecord = 0;
	}
}

void CSimpleOutputStream::Put( const char * key, const char * value )
{
	Put(SStreamRecord(key,value));
}

void CSimpleOutputStream::Put( const char * key, Vec2 value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%f,%f", value.x, value.y );
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, Vec3 value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%f,%f,%f", value.x, value.y, value.z );
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, SNetObjectID value )
{
	SStreamRecord rec(key);
	value.GetText(rec.value);
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, Quat value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%f,%f,%f,%f", value.w, value.v.x, value.v.y, value.v.z );
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, int value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%d", value );
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, int64 value )
{
	SStreamRecord rec(key);
#if defined(__GNUC__)
	sprintf( rec.value, "%lld", (long long)value );
#else
	sprintf( rec.value, "%I64d", value );
#endif
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, uint64 value )
{
	SStreamRecord rec(key);
#if defined(__GNUC__)
	sprintf( rec.value, "%.16llx", value );
#else
	sprintf( rec.value, "%I64u", value );
#endif
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, float value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%f", value );
	Put(rec);
}

void CSimpleOutputStream::Put( const char * key, EntityId value )
{
	SStreamRecord rec(key);
	sprintf( rec.value, "%.8x", value );
	Put(rec);
}

void CSimpleOutputStream::Sync()
{
	if (m_curRecord)
	{
		Flush(m_records, m_curRecord);
		m_curRecord = 0;
	}
}
