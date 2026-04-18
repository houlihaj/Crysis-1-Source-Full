/********************************************************************
CryGame Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
File name:   HashSpace.h
Version:     v1.00
Description: 

-------------------------------------------------------------------------
History:
- 20:7:2005   Created by Danny Chapman

*********************************************************************/
#ifndef HASHSPACE_H
#define HASHSPACE_H

#if _MSC_VER > 1000
#pragma once
#endif

#include "CryFile.h"

#include <vector>

template<typename T>
class CHashSpaceTraits
{
public:
	const Vec3& operator() (const T& item) const {return item.GetPos();}
};

/// Contains any type that can be represented with a single position, obtained
/// through T::GetPos()
template<typename T, typename TraitsT=CHashSpaceTraits<T> >
class CHashSpace : public TraitsT
{
public:
	typedef TraitsT Traits;

	// Note: bucket count must be power of 2!
	CHashSpace(const Vec3& cellSize, int buckets);
	CHashSpace(const Vec3& cellSize, int buckets, const Traits& traits);
	~CHashSpace();

	/// Adds a copy of the object
	void AddObject(const T& object);
	/// Removes all objects indicated by operator==
	void RemoveObject(const T& object);
	/// Removes all objects
	void Clear(bool freeMemory);

	/// the functor gets called and passed every object (and the distance-squared) that is within radius of
	/// pos. Returns the number of objects within range
	template<typename Functor>
	void ProcessObjectsWithinRadius(const Vec3& pos, float radius, Functor& functor);

	template<typename Functor>
	void ProcessObjectsWithinRadius(const Vec3& pos, float radius, Functor& functor) const;

	/// returns the total number of objects
	inline unsigned GetNumObjects() const { return m_totalNumObjects; }
	// Returns number of buckets in the hash space.
	inline unsigned GetBucketCount() const { return m_cells.size(); }
	// Returns number of objects in the specified bucket.
	inline unsigned GetObjectCountInBucket(unsigned bucket) const { return m_cells[bucket].objects.size(); }
	// Returns the indexed object in the specified bucket.
	inline const T& GetObjectInBucket(unsigned obj, unsigned bucket) const { return m_cells[bucket].objects[obj]; }

	/// Gets the AABB associated with an i, j, k
	void GetAABBFromIJK(int i, int j, int k, AABB& aabb) const;
	/// return the individual i, j, k for 3D array lookup
	void GetIJKFromPosition(const Vec3& pos, int& i, int& j, int& k) const;
	/// returns bucket index from given i,j,k
	unsigned GetHashBucketIndex(int i, int j, int k) const;

	/// Returns true if successful, false otherwise
	bool WriteToFile(CCryFile& file);
	/// Returns true if successful, false otherwise
	bool ReadFromFile(CCryFile& file);

	/// Returns the memory usage in bytes
	size_t MemStats() const;

	inline unsigned GetLocationMask(int i, int j, int k) const
	{
		const unsigned mi = (1 << 12)-1;
		const unsigned mj = (1 << 12)-1;
		const unsigned mk = (1 << 8)-1;
		unsigned x = (unsigned)(i & mi); 
		unsigned y = (unsigned)(j & mj);
		unsigned z = (unsigned)(k & mk);
		return x | (y << 12) | (z << 24);
	}

private:
	const Vec3& GetPos(const T& item) const;

	typedef std::vector<T> Objects;
	struct Cell
	{
		std::vector<unsigned> masks;
		Objects objects;
	};
	typedef std::vector< Cell > Cells;

	/// returns index into m_cells
	unsigned GetCellIndexFromPosition(const Vec3& pos) const;

	Cells m_cells;
	Vec3 m_cellSize;
	Vec3 m_cellScale;
	int m_buckets;

	unsigned m_totalNumObjects;
};

//====================================================================
// Inline implementation
//====================================================================

//====================================================================
// MemStats
//====================================================================
template<typename T, typename TraitsT>
size_t CHashSpace<T, TraitsT>::MemStats() const
{
	size_t result = 0;
	for (unsigned i = 0 ; i < m_cells.size() ; ++i)
	{
		result += sizeof(Cell) + m_cells[i].objects.capacity() * sizeof(T) + m_cells[i].masks.capacity() * sizeof(unsigned);
	}
	result += sizeof(*this);
	return result;
}

//====================================================================
// CHashSpace
//====================================================================
template<typename T, typename TraitsT>
inline CHashSpace<T, TraitsT>::CHashSpace(const Vec3& cellSize, int buckets, const Traits& traits) :
	Traits(traits),
	m_cellSize(cellSize),
	m_cellScale(1.0f/cellSize.x, 1.0f/cellSize.y, 1.0f/cellSize.z),
	m_buckets(buckets),
	m_totalNumObjects(0)
{
	m_cells.resize(m_buckets);
}

template<typename T, typename TraitsT>
inline CHashSpace<T, TraitsT>::CHashSpace(const Vec3& cellSize, int buckets) :
	m_cellSize(cellSize),
	m_cellScale(1.0f/cellSize.x, 1.0f/cellSize.y, 1.0f/cellSize.z),
	m_buckets(buckets),
	m_totalNumObjects(0)
{
	m_cells.resize(m_buckets);
}

//====================================================================
// ~CHashSpace
//====================================================================
template<typename T, typename TraitsT>
inline CHashSpace<T, TraitsT>::~CHashSpace()
{
}

//====================================================================
// Clear
//====================================================================
template<typename T, typename TraitsT>
inline void CHashSpace<T, TraitsT>::Clear(bool clearMemory) 
{
	for (unsigned i = 0 ; i < m_cells.size() ; ++i)
	{
		if (clearMemory)
		{
			ClearVectorMemory(m_cells[i].objects);
			ClearVectorMemory(m_cells[i].masks);
		}
		else
		{
			m_cells[i].objects.resize(0);
			m_cells[i].masks.resize(0);
		}
	}
	m_totalNumObjects = 0;
}

//====================================================================
// GetIJKFromPosition
//====================================================================
template<typename T, typename TraitsT>
inline void CHashSpace<T, TraitsT>::GetIJKFromPosition(const Vec3& pos, int& i, int& j, int& k) const
{
	i = (int)floorf(pos.x * m_cellScale.x);
	j = (int)floorf(pos.y * m_cellScale.y);
	k = (int)floorf(pos.z * m_cellScale.z);
}

//====================================================================
// GetHashBucketIndex
//====================================================================
template<typename T, typename TraitsT>
inline unsigned CHashSpace<T, TraitsT>::GetHashBucketIndex(int i, int j, int k) const
{
	const int h1 = 0x8da6b343;
	const int h2 = 0xd8163841;
	const int h3 = 0xcb1ab31f;
	int n = h1 * i + h2 * j + h3 * k;
	return (unsigned)(n & (m_buckets-1));

/*	const int h1 = 73856093;
	const int h2 = 19349663;
	const int h3 = 83492791;
	int n = (h1*i) ^ (h2*j) ^ (h3*k);
	return (unsigned)(n & (m_buckets-1));*/

/*	const int shift = 5;
	const int mask = (1 << shift)-1;
	int n = (i & mask) | ((j&mask) << shift) | ((k&mask) << (shift*2));
	return (unsigned)(n & (m_buckets-1));*/
}


//====================================================================
// GetCellIndexFromPosition
//====================================================================
template<typename T, typename TraitsT>
inline unsigned CHashSpace<T, TraitsT>::GetCellIndexFromPosition(const Vec3& pos) const
{
	int i, j, k;
	GetIJKFromPosition(pos, i, j, k);
	return GetHashBucketIndex(i, j, k);
}

//====================================================================
// GetAABBFromIJK
//====================================================================
template<typename T, typename TraitsT>
inline void CHashSpace<T, TraitsT>::GetAABBFromIJK(int i, int j, int k, AABB& aabb) const
{
	aabb.min.Set(i * m_cellSize.x, j * m_cellSize.y, k * m_cellSize.z);
	aabb.max = aabb.min + m_cellSize;
}


//====================================================================
// AddObject
//====================================================================
template<typename T, typename TraitsT>
inline void CHashSpace<T, TraitsT>::AddObject(const T& object)
{
	int i, j, k;
	GetIJKFromPosition(GetPos(object), i, j, k);
	unsigned index = GetHashBucketIndex(i, j, k);
	unsigned mask = GetLocationMask(i, j, k);

	m_cells[index].objects.push_back(object);
	m_cells[index].masks.push_back(mask);

	++m_totalNumObjects;
}

//====================================================================
// RemoveObject
//====================================================================
template<typename T, typename TraitsT>
inline void CHashSpace<T, TraitsT>::RemoveObject(const T& object)
{
	unsigned index = GetCellIndexFromPosition(GetPos(object));
	Cell &cell = m_cells[index];
	for (unsigned i = 0, ni = cell.objects.size(); i < ni; )
	{
		T &obj = cell.objects[i];
		if (obj == object)
		{
			cell.objects[i] = cell.objects.back();
			cell.objects.pop_back();
			cell.masks[i] = cell.masks.back();
			cell.masks.pop_back();
			--ni;
			--m_totalNumObjects;
		}
		else
		{
			++i;
		}
	}
}

//====================================================================
// ProcessObjectsWithinRadius
//====================================================================
template<typename T, typename TraitsT> 
template <typename Functor>
void CHashSpace<T, TraitsT>::ProcessObjectsWithinRadius(const Vec3& pos, float radius, Functor& functor)
{
	float radiusSq = square(radius);
	int i0, j0, k0;
	int i1, j1, k1;
	GetIJKFromPosition(pos - Vec3(radius, radius, radius), i0, j0, k0);
	GetIJKFromPosition(pos + Vec3(radius, radius, radius), i1, j1, k1);

	for (int i = i0 ; i <= i1 ; ++i)
	{
		for (int j = j0 ; j <= j1 ; ++j)
		{
			for (int k = k0 ; k <= k1 ; ++k)
			{
				unsigned index = GetHashBucketIndex(i, j, k);
				unsigned mask = GetLocationMask(i, j, k);
				Objects& objects = m_cells[index].objects;
				std::vector<unsigned>& masks = m_cells[index].masks;
				for (unsigned l = 0 ; l < objects.size() ; ++l)
				{
					if (mask != masks[l]) continue;
					T& object = objects[l];
					float distSq = Distance::Point_PointSq(pos, GetPos(object));
					if (distSq < radiusSq)
					{
						functor(object, distSq);
					}
				}
			}
		}
	}
}

//====================================================================
// ProcessObjectsWithinRadius
//====================================================================
template<typename T, typename TraitsT> 
template <typename Functor>
void CHashSpace<T, TraitsT>::ProcessObjectsWithinRadius(const Vec3& pos, float radius, Functor& functor) const
{
	float radiusSq = square(radius);
	int i0, j0, k0;
	int i1, j1, k1;
	GetIJKFromPosition(pos - Vec3(radius, radius, radius), i0, j0, k0);
	GetIJKFromPosition(pos + Vec3(radius, radius, radius), i1, j1, k1);

	for (int i = i0 ; i <= i1 ; ++i)
	{
		for (int j = j0 ; j <= j1 ; ++j)
		{
			for (int k = k0 ; k <= k1 ; ++k)
			{
				unsigned index = GetHashBucketIndex(i, j, k);
				unsigned mask = GetLocationMask(i, j, k);
				const Objects& objects = m_cells[index].objects;
				const std::vector<unsigned>& masks = m_cells[index].masks;
				for (unsigned l = 0 ; l < objects.size() ; ++l)
				{
					if (mask != masks[l]) continue;
					const T& object = objects[l];
					float distSq = Distance::Point_PointSq(pos, GetPos(object));
					if (distSq < radiusSq)
					{
						functor(object, distSq);
					}
				}
			}
		}
	}
}

//====================================================================
// WriteToFile
//====================================================================
template<typename T, typename TraitsT> 
bool CHashSpace<T, TraitsT>::WriteToFile(CCryFile& file)
{
	int version = 1;
	file.Write(&version, sizeof(version));
	file.Write(&m_buckets, sizeof(m_buckets));
	file.Write(&m_cellSize, sizeof(m_cellSize));
	unsigned num = m_cells.size();
	file.Write(&num, sizeof(num));
	for (unsigned i = 0 ; i < num ; ++i)
	{
		Cell& cell = m_cells[i];
		unsigned numObjects = cell.objects.size();
		file.Write(&numObjects, sizeof(numObjects));
		if (numObjects > 0)
		{
			file.Write(&cell.objects[0], numObjects * sizeof(cell.objects[0]));
			file.Write(&cell.masks[0], numObjects * sizeof(cell.masks[0]));
		}
	}
	return true;
}

//====================================================================
// ReadFromFile
//====================================================================
template<typename T, typename TraitsT> 
bool CHashSpace<T, TraitsT>::ReadFromFile(CCryFile& file)
{
	Clear(false);
	int version = 1;
	file.Write(&version, sizeof(version));
	file.Write(&m_buckets, sizeof(m_buckets));
	file.ReadType(&m_cellSize);
	m_cellScale.x = 1.0f/m_cellSize.x;
	m_cellScale.y = 1.0f/m_cellSize.y;
	m_cellScale.z = 1.0f/m_cellSize.z;
	unsigned num;
	file.ReadType(&num);
	m_cells.resize(num);
	for (unsigned i = 0 ; i < num ; ++i)
	{
		Cell& cell = m_cells[i];
		unsigned numObjects;
		file.ReadType(&numObjects);
		cell.objects.resize(numObjects);
		cell.masks.resize(numObjects);
		if (numObjects > 0)
		{
			file.ReadType(&cell.objects[0], numObjects);
			file.ReadType(&cell.masks[0], numObjects);
		}
		m_totalNumObjects += numObjects;
	}
	return true;
}

//====================================================================
// GetPos
//====================================================================
template<typename T, typename TraitsT> 
const Vec3& CHashSpace<T, TraitsT>::GetPos(const T& item) const
{
	return (*static_cast<const Traits*>(this))(item);
}

#endif
