////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   EquipPack.h
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: Equipment Pack (ported from SandBox 1)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>
#include <deque>

struct SEquipment
{
	SEquipment()
	{
		sName="";
		sType="";
	}
	bool operator==(const SEquipment &e)
	{
		if (sName!=e.sName)
			return false;
		return true;
	}
	bool operator<(const SEquipment &e)
	{
		if (sName<e.sName)
			return true;
		if (sName!=e.sName)
			return false;
		return false;
	}
	CString sName;
	CString sType;
};

struct SAmmo
{
	bool operator==(const SAmmo &e)
	{
		if (sName!=e.sName)
			return false;
		return true;
	}
	bool operator<(const SAmmo &e)
	{
		if (sName<e.sName)
			return true;
		if (sName!=e.sName)
			return false;
		return false;
	}
	CString sName;
	int nAmount;
};

typedef std::deque<SEquipment>		TEquipmentVec;
typedef std::vector<SAmmo>		    TAmmoVec;

class CEquipPackLib;

class CEquipPack
{
public:
	void Release();
	
	const CString& GetName() const { return m_name; }
	
	bool AddEquip(const SEquipment& equip);
	bool RemoveEquip(const SEquipment& equip);
	bool AddAmmo(const SAmmo& ammo);

	void Clear();
	void Load(XmlNodeRef node);
	bool Save(XmlNodeRef node);
	int  Count() const { return m_equipmentVec.size(); }

	TEquipmentVec& GetEquip() { return m_equipmentVec; }
	TAmmoVec& GetAmmoVec() { return m_ammoVec; }

	void SetModified(bool bModified) { m_bModified = bModified; }
	bool IsModified() const { return m_bModified; }

protected:
	friend CEquipPackLib;

	CEquipPack(CEquipPackLib *pCreator);
	~CEquipPack();

	void SetName(const CString& name) 
	{ 
		m_name = name;
		SetModified(true);
	}

	bool InternalSetPrimary(const CString& primary);

protected:
	CString m_name;
	CEquipPackLib *m_pCreator;
	TEquipmentVec m_equipmentVec;
	TAmmoVec m_ammoVec;
	bool m_bModified;
};
