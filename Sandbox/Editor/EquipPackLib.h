#pragma once

#include <map>

class CEquipPack;
typedef std::map<CString,CEquipPack*>	TEquipPackMap;

class CEquipPackLib
{
public:
	CEquipPackLib();
	~CEquipPackLib();
	CEquipPack* CreateEquipPack(const CString& name);
	// currently we ignore the bDeleteFromDisk.
	// will have to be manually removed via Source control 
	bool RemoveEquipPack(const CString& name, bool bDeleteFromDisk=false);
	bool RenameEquipPack(const CString& name, const CString& newName);
	CEquipPack* FindEquipPack(const CString& name);

	void Serialize(XmlNodeRef &xmlNode, bool bLoading, bool bResetWhenLoad=true);
	bool LoadLibs(bool bExportToGame);
	bool SaveLibs(bool bExportToGame);
	void ExportToGame();
	void Reset();
	int  Count() const { return m_equipmentPacks.size(); }
	const TEquipPackMap& GetEquipmentPacks() const { return m_equipmentPacks; }
private:
	TEquipPackMap m_equipmentPacks;
};
