////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   AreaGrid.h
//  Version:     v1.00
//  Created:     08/03/2007 by Michael Smith.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __AREAGRID_H__
#define __AREAGRID_H__

class CArea;

class CAreaGrid
{
public:
	class iterator
	{
		friend class CAreaGrid;
	public:
		iterator(): m_pGrid(0), m_areaListNodeIndex(0) {}
		iterator(const iterator& other): m_pGrid(other.m_pGrid), m_areaListNodeIndex(other.m_areaListNodeIndex) {}
		iterator& operator=(const iterator& other) {m_pGrid = other.m_pGrid; m_areaListNodeIndex = other.m_areaListNodeIndex; return *this;}
		iterator& operator++() {m_areaListNodeIndex = m_pGrid->m_areaListNodes[m_areaListNodeIndex].nextIndex; return *this;}
		int operator*() {return m_pGrid->m_areaListNodes[m_areaListNodeIndex].areaIndex;}
		bool operator==(const iterator& other) const {return m_pGrid == other.m_pGrid && m_areaListNodeIndex == other.m_areaListNodeIndex;}
		bool operator!=(const iterator& other) const {return m_pGrid != other.m_pGrid || m_areaListNodeIndex != other.m_areaListNodeIndex;}

	private:
		iterator(CAreaGrid* pGrid, int areaListNodeIndex): m_pGrid(pGrid), m_areaListNodeIndex(areaListNodeIndex) {}

		CAreaGrid* m_pGrid;
		int m_areaListNodeIndex;
	};

	CAreaGrid();

	void Compile(CEntitySystem* pEntitySystem, std::vector<CArea*>& areas);
	void Reset();
	iterator BeginAreas(const Vec3& position);
	iterator EndAreas(const Vec3& position);
	void Draw();

private:
	struct HashNode
	{
		unsigned short nextIndex;
		unsigned short firstAreaListNodeIndex;
		unsigned int positionTag;
	};

	struct AreaListNode
	{
		unsigned short nextIndex;
		unsigned short areaIndex;
	};

	static const float GRID_SIZE;
	static const int BUCKET_COUNT;

	CEntitySystem* m_pEntitySystem;
	std::vector<CArea*>* m_pAreas;
	std::vector<unsigned short> m_hashBuckets;
	std::vector<HashNode> m_hashNodes;
	std::vector<AreaListNode> m_areaListNodes;

	float GetMaximumEffectRadius(CArea *pArea);
	unsigned GetHashNodeIndex(const std::vector<unsigned short>& hashBuckets, const std::vector<HashNode>& hashNodes, unsigned positionTag);
	void SetFirstAreaIndex(std::vector<unsigned short>& hashBuckets, std::vector<HashNode>& hashNodes, unsigned positionTag, unsigned firstAreaIndex);
};

#endif //__AREAGRID_H__
