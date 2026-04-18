// EquipPackDialog.cpp : implementation file
//

#include "stdafx.h"
#include "Resource.h"
#include "EquipPackDialog.h"
#include "EquipPackLib.h"
#include "EquipPack.h"
#include "StringDlg.h"
#include "GameEngine.h"
#include <IEditorGame.h>

// CEquipPackDialog dialog

IMPLEMENT_DYNAMIC(CEquipPackDialog, CXTResizeDialog)
CEquipPackDialog::CEquipPackDialog(CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CEquipPackDialog::IDD, pParent)
{
	m_sCurrEquipPack="";
	m_bEditMode = true;
}

CEquipPackDialog::~CEquipPackDialog()
{
}

void CEquipPackDialog::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROPERTIES, m_AmmoPropWnd);
	DDX_Control(pDX, IDOK, m_OkBtn);
	DDX_Control(pDX, IDC_EXPORT, m_ExportBtn);
	DDX_Control(pDX, IDC_ADD, m_AddBtn);
	DDX_Control(pDX, IDC_DELETE, m_DeleteBtn);
	DDX_Control(pDX, IDC_RENAME, m_RenameBtn);
	DDX_Control(pDX, IDC_EQUIPPACK, m_EquipPacksList);
	DDX_Control(pDX, IDC_EQUIPAVAILLST, m_AvailEquipList);
	DDX_Control(pDX, IDC_EQUIPUSEDLST, m_EquipList);
	DDX_Control(pDX, IDC_INSERT, m_InsertBtn);
	DDX_Control(pDX, IDC_REMOVE, m_RemoveBtn);
	DDX_Control(pDX, IDC_PRIMARY, m_PrimaryEdit);
}

void CEquipPackDialog::UpdateEquipPacksList()
{
	m_EquipPacksList.ResetContent();
	const TEquipPackMap& mapEquipPacks=GetIEditor()->GetEquipPackLib()->GetEquipmentPacks();
	for (TEquipPackMap::const_iterator It=mapEquipPacks.begin();It!=mapEquipPacks.end();++It)
	{
		m_EquipPacksList.AddString(It->first);
	}

	int nCurSel = m_EquipPacksList.FindStringExact(0, m_sCurrEquipPack);
	const int nEquipPacks=GetIEditor()->GetEquipPackLib()->Count();
	if (!nEquipPacks)
		nCurSel=-1;
	else if (nEquipPacks<=nCurSel)
		nCurSel=0;
	else if (nCurSel==-1)
		nCurSel=0;
	m_EquipPacksList.SetCurSel(nCurSel);
	m_DeleteBtn.EnableWindow(nCurSel!=-1);
	m_RenameBtn.EnableWindow(nCurSel!=-1);
	UpdateEquipPackParams();
}

void CEquipPackDialog::UpdateEquipPackParams()
{
	m_AvailEquipList.ResetContent();
	m_EquipList.ResetContent();
	m_AmmoPropWnd.DeleteAllItems();

	int nItem=m_EquipPacksList.GetCurSel();
	m_ExportBtn.EnableWindow(nItem!=-1);
	if (nItem==-1)
	{
		m_sCurrEquipPack="";
		return;
	}
	m_EquipPacksList.GetLBText(nItem, m_sCurrEquipPack);
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack *pEquip=GetIEditor()->GetEquipPackLib()->FindEquipPack(sName);
	if (!pEquip)
	{
		return;
	}
	TEquipmentVec& equipVec=pEquip->GetEquip();
	int nPrimaryItemId=-1;
	/*
	for (TLstStringIt It=m_lstAvailEquip.begin();It!=m_lstAvailEquip.end();++It)
	{
		const SEquipment& Equip=(*It);
		const CString& sType=Equip.sType;
		if (std::find(equipVec.begin(), equipVec.end(), Equip)==equipVec.end())
			m_AvailEquipList.AddString(Equip.sName);
		else
		{
			int nIdx=m_EquipList.AddString(Equip.sName);
			if (sPrimary==Equip.sName)
				nPrimaryItemId=nIdx;
		}
	}
	m_PrimaryEdit.SetWindowText(sPrimary.IsEmpty() ? "<NONE>" : sPrimary);
	*/

	// add in the order equipments appear in the pack
	for(TEquipmentVec::iterator iter = equipVec.begin(), iterEnd = equipVec.end(); iter != iterEnd; ++iter)
	{
		const SEquipment& equip=*iter;
		int index = m_EquipList.AddString(equip.sName);
	}
	nPrimaryItemId = 0;
	// add all other items to available list
	for (TLstStringIt iter=m_lstAvailEquip.begin(), iterEnd = m_lstAvailEquip.end(); iter != iterEnd; ++iter)
	{
		const SEquipment& equip=*iter;
		if (std::find(equipVec.begin(), equipVec.end(), equip)==equipVec.end())
			m_AvailEquipList.AddString(equip.sName);
	}

	UpdatePrimary();

	m_AmmoPropWnd.AddVarBlock(CreateVarBlock(pEquip));
	m_AmmoPropWnd.SetUpdateCallback(functor(*this, &CEquipPackDialog::AmmoUpdateCallback));
	m_AmmoPropWnd.EnableUpdateCallback(true);
	m_AmmoPropWnd.ExpandAllChilds(m_AmmoPropWnd.GetRootItem(), true);
	CRect rc;
	m_AmmoPropWnd.GetWindowRect(rc);
	rc.MoveToXY(0,0);
	m_AmmoPropWnd.SetWindowPos( NULL, 0, 0, rc.right, rc.bottom, SWP_NOMOVE );

	// update lists
	m_AvailEquipList.SetCurSel(0);
	OnLbnSelchangeEquipavaillst();
	OnLbnSelchangeEquipusedlst();
}

CVarBlockPtr CEquipPackDialog::CreateVarBlock(CEquipPack* pEquip)
{
	CVarBlock* pVarBlock = m_pVarBlock ? m_pVarBlock->Clone(true) : new CVarBlock();

	TAmmoVec notFoundAmmoVec;
	TAmmoVec& ammoVec = pEquip->GetAmmoVec();
	for (TAmmoVec::iterator iter = ammoVec.begin(); iter != ammoVec.end(); ++iter)
	{
		// check if var block contains all variables
		IVariablePtr pVar = pVarBlock->FindVariable(iter->sName);
		if (pVar == 0)
		{
			// pVar = new CVariable<int>;
			// pVar->SetFlags(IVariable::UI_DISABLED);
			// pVarBlock->AddVariable(pVar);
			notFoundAmmoVec.push_back(*iter);
		}
		else
		{
			pVar->SetName(iter->sName);
			pVar->Set(iter->nAmount);
		}
	}

	if (notFoundAmmoVec.empty() == false)
	{
		CString notFoundString;
		notFoundString.Format("The following ammo items in pack '%s' could not be found.\r\nDo you want to permanently remove them?\r\n\r\n", pEquip->GetName());
		TAmmoVec::iterator iter = notFoundAmmoVec.begin();
		TAmmoVec::iterator iterEnd = notFoundAmmoVec.end();
		while (iter != iterEnd)
		{
			notFoundString.AppendFormat("'%s'\r\n", iter->sName);
			++iter;
		}

		int res = AfxMessageBox(notFoundString, MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2);
		if (res == IDYES)
		{
			iter = notFoundAmmoVec.begin();
			while (iter != iterEnd)
			{
				const SAmmo& ammo = *iter;
				stl::find_and_erase(ammoVec, ammo);
				++iter;
			}
			pEquip->SetModified(true);
		}
		else
		{
			// add missing ammos, but disabled
			iter = notFoundAmmoVec.begin();
			while (iter != iterEnd)
			{
				IVariablePtr pVar = pVarBlock->FindVariable(iter->sName);
				if (pVar == 0)
				{
					pVar = new CVariable<int>;
					pVar->SetFlags(IVariable::UI_DISABLED);
					pVarBlock->AddVariable(pVar);
				}
				pVar->SetName(iter->sName);
				pVar->Set(iter->nAmount);
				++iter;
			}
		}
		notFoundAmmoVec.resize(0);
	}

	int nCount = pVarBlock->GetVarsCount();
	for (int i=0; i<nCount; ++i)
	{
		IVariable* pVar = pVarBlock->GetVariable(i);
		SAmmo ammo;
		ammo.sName = pVar->GetName();
		if (std::find(ammoVec.begin(), ammoVec.end(), ammo) == ammoVec.end())
		{
			pVar->Get(ammo.nAmount);
			pEquip->AddAmmo(ammo);
			notFoundAmmoVec.push_back(ammo);
		}
	}

	if (notFoundAmmoVec.empty() == false)
	{
		CString notFoundString;
		notFoundString.Format("The following ammo items have been automatically added to pack '%s':\r\n\r\n", pEquip->GetName());
		TAmmoVec::iterator iter = notFoundAmmoVec.begin();
		TAmmoVec::iterator iterEnd = notFoundAmmoVec.end();
		while (iter != iterEnd)
		{
			notFoundString.AppendFormat("'%s'\r\n", iter->sName);
			++iter;
		}
		AfxMessageBox(notFoundString, MB_OK);
	}
	return pVarBlock;
}

void CEquipPackDialog::UpdatePrimary()
{
	CString name;
	if (m_EquipList.GetCount() > 0)
	{
		m_EquipList.GetText(0, name);
	}
	m_PrimaryEdit.SetWindowText(name);
}

void CEquipPackDialog::AmmoUpdateCallback(IVariable *pVar)
{
	CEquipPack *pEquip=GetIEditor()->GetEquipPackLib()->FindEquipPack(m_sCurrEquipPack);
	if (!pEquip)
		return;
	SAmmo ammo;
	ammo.sName = pVar->GetName();
	pVar->Get(ammo.nAmount);
	pEquip->AddAmmo(ammo);
	m_bChanged=true;
}

SEquipment* CEquipPackDialog::GetEquipment(CString sDesc)
{
	SEquipment Equip;
	Equip.sName=sDesc;
	TLstStringIt It=std::find(m_lstAvailEquip.begin(), m_lstAvailEquip.end(), Equip);
	if (It==m_lstAvailEquip.end())
		return NULL;
	return &(*It);
}

BEGIN_MESSAGE_MAP(CEquipPackDialog, CXTResizeDialog)
	ON_CBN_SELCHANGE(IDC_EQUIPPACK, OnCbnSelchangeEquippack)
	ON_BN_CLICKED(IDC_ADD, OnBnClickedAdd)
//	ON_BN_CLICKED(IDC_REFRESH, OnBnClickedRefresh)
	ON_BN_CLICKED(IDC_DELETE, OnBnClickedDelete)
	ON_BN_CLICKED(IDC_RENAME, OnBnClickedRename)
	ON_BN_CLICKED(IDC_INSERT, OnBnClickedInsert)
	ON_BN_CLICKED(IDC_REMOVE, OnBnClickedRemove)
	ON_BN_CLICKED(IDC_MOVEUP, OnBnClickedMoveUp)
	ON_BN_CLICKED(IDC_MOVEDOWN, OnBnClickedMoveDown)
	ON_LBN_SELCHANGE(IDC_EQUIPAVAILLST, OnLbnSelchangeEquipavaillst)
	ON_LBN_SELCHANGE(IDC_EQUIPUSEDLST, OnLbnSelchangeEquipusedlst)
//	ON_WM_DESTROY()
ON_BN_CLICKED(IDC_IMPORT, OnBnClickedImport)
ON_BN_CLICKED(IDC_EXPORT, OnBnClickedExport)
ON_WM_DESTROY()
END_MESSAGE_MAP()


// CEquipPackDialog message handlers

BOOL CEquipPackDialog::OnInitDialog()
{
	CXTResizeDialog::OnInitDialog();

	SetResize(IDOK, SZ_REPOS(1));
	SetResize(IDCANCEL, SZ_REPOS(1));
	SetResize(IDC_EXPORT, SZ_HORREPOS(1));
	SetResize(IDC_IMPORT, SZ_HORREPOS(1));
	SetResize(IDC_ADD, SZ_HORREPOS(1));
	SetResize(IDC_DELETE, SZ_HORREPOS(1));
	SetResize(IDC_RENAME, SZ_HORREPOS(1));

	SetResize(IDC_EQUIPPACK, CXTResizeRect(0,0,1,0)); 
	SetResize(IDC_EQUIPAVAILLST, CXTResizeRect(0,0,0,1)); 
	SetResize(IDC_EQUIPUSEDLST,  CXTResizeRect(0,0,0,1)); 
	SetResize(IDC_PROPERTIES,    CXTResizeRect(0,0,1,1)); 
	SetResize(IDC_INSERT, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_REMOVE, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_STATIC_NOTE, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_PRIMARY_STATIC, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_PRIMARY, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_MOVEUP, CXTResizeRect(0,1,0,1)); 
	SetResize(IDC_MOVEDOWN, CXTResizeRect(0,1,0,1)); 

	if (m_bEditMode == true)
	{
		CWnd* pWnd = GetDlgItem(IDCANCEL);
		if (pWnd)
			pWnd->ShowWindow(SW_HIDE);
		m_OkBtn.SetWindowText(_T("Close"));
	}

	m_bChanged=false;
	m_lstAvailEquip.clear();

	// Fill available weapons ListBox with enumerated weapons from game.
	IEquipmentSystemInterface::IEquipmentItemIteratorPtr iter = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->CreateEquipmentItemIterator("ItemGivable");
	IEquipmentSystemInterface::IEquipmentItemIterator::SEquipmentItem item;

	if (iter)
	{
		SEquipment Equip;
		while (iter->Next(item))
		{
			Equip.sName=item.name;
			Equip.sType=item.type;
			m_lstAvailEquip.push_back(Equip);
		}
	}
	else
		AfxMessageBox("GetAvailableWeaponNames() returned NULL. Unable to retrieve weapons.", MB_ICONEXCLAMATION | MB_OK);

	m_pVarBlock = new CVarBlock();
	iter = GetIEditor()->GetGameEngine()->GetIEquipmentSystemInterface()->CreateEquipmentItemIterator("Ammo");
	if (iter)
	{
		while (iter->Next(item))
		{
			static const int defaultAmount = 0;
			IVariablePtr pVar = new CVariable<int>;
			pVar->SetName(item.name);
			pVar->Set(defaultAmount);
			m_pVarBlock->AddVariable(pVar);
		}
	}
	else
		AfxMessageBox("GetAvailableWeaponNames() returned NULL. Unable to retrieve weapons.", MB_ICONEXCLAMATION | MB_OK);

	UpdateEquipPacksList();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CEquipPackDialog::OnCbnSelchangeEquippack()
{
	UpdateEquipPackParams();
}

void CEquipPackDialog::OnBnClickedAdd()
{
	CStringDlg Dlg("Enter name for new Equipment-Pack", this);
	if (Dlg.DoModal()!=IDOK)
		return;
	if (Dlg.GetString().GetLength()<=0)
		return;
	CString packName = Dlg.GetString();
	if (!GetIEditor()->GetEquipPackLib()->CreateEquipPack(packName))
		AfxMessageBox("An Equipment-Pack with this name exists already !", MB_OK | MB_ICONEXCLAMATION);
	else
	{
		m_bChanged=true;
		m_sCurrEquipPack = packName;
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedDelete()
{
	if (AfxMessageBox("Are you sure ?", MB_YESNO | MB_ICONEXCLAMATION)==IDNO)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(m_EquipPacksList.GetCurSel(), sName);
	if (!GetIEditor()->GetEquipPackLib()->RemoveEquipPack(sName, true))
		AfxMessageBox("Unable to delete Equipment-Pack !", MB_OK | MB_ICONEXCLAMATION);
	else
	{
		m_bChanged=true;
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedRename()
{
	CString sName;
	m_EquipPacksList.GetLBText(m_EquipPacksList.GetCurSel(), sName);
	CStringDlg Dlg("Enter new name for Equipment-Pack", this);
	if (Dlg.DoModal()!=IDOK)
		return;
	if (Dlg.GetString().GetLength()<=0)
		return;
	if (!GetIEditor()->GetEquipPackLib()->RenameEquipPack(sName, Dlg.GetString()))
		AfxMessageBox("Unable to rename Equipment-Pack ! Probably the new name is already used.", MB_OK | MB_ICONEXCLAMATION);
	else
	{
		m_bChanged=true;
		UpdateEquipPacksList();
	}
}

void CEquipPackDialog::OnBnClickedInsert()
{
	int nItem=m_EquipPacksList.GetCurSel();
	if (nItem==-1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack *pEquipPack=GetIEditor()->GetEquipPackLib()->FindEquipPack(sName);
	if (!pEquipPack)
		return;
	m_AvailEquipList.GetText(m_AvailEquipList.GetCurSel(), sName);
	SEquipment *pEquip=GetEquipment(sName);
	if (!pEquip)
		return;
	pEquipPack->AddEquip(*pEquip);
	m_bChanged=true;
	UpdateEquipPackParams();
}

void CEquipPackDialog::OnBnClickedRemove()
{
	int nItem=m_EquipPacksList.GetCurSel();
	if (nItem==-1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack *pEquipPack=GetIEditor()->GetEquipPackLib()->FindEquipPack(sName);
	if (!pEquipPack)
		return;
	m_EquipList.GetText(m_EquipList.GetCurSel(), sName);
	SEquipment *pEquip=GetEquipment(sName);
	if (!pEquip)
		return;
	pEquipPack->RemoveEquip(*pEquip);
	m_bChanged=true;
	UpdateEquipPackParams();
}

void CEquipPackDialog::OnBnClickedMoveUp()
{
	int nItem=m_EquipPacksList.GetCurSel();
	if (nItem==-1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack *pEquipPack=GetIEditor()->GetEquipPackLib()->FindEquipPack(sName);
	if (!pEquipPack)
		return;

	nItem = m_EquipList.GetCurSel();
	if (nItem == 0)
		return; // can't move up first item
	m_EquipList.GetText(nItem, sName);
	m_EquipList.DeleteString(nItem);
	m_EquipList.InsertString(nItem-1, sName);

	pEquipPack->Clear();
	for (int i=0; i<m_EquipList.GetCount(); ++i)
	{
		m_EquipList.GetText(i, sName);
		SEquipment *pEquip=GetEquipment(sName);
		if (pEquip)
			pEquipPack->AddEquip(*pEquip);
	}
	m_bChanged=true;
	UpdateEquipPackParams();
	m_EquipList.SetCurSel(nItem-1);
	OnLbnSelchangeEquipusedlst();
}

void CEquipPackDialog::OnBnClickedMoveDown()
{
	int nItem=m_EquipPacksList.GetCurSel();
	if (nItem==-1)
		return;
	CString sName;
	m_EquipPacksList.GetLBText(nItem, sName);
	CEquipPack *pEquipPack=GetIEditor()->GetEquipPackLib()->FindEquipPack(sName);
	if (!pEquipPack)
		return;

	nItem = m_EquipList.GetCurSel();
	if (nItem == m_EquipList.GetCount()-1)
		return; // can't move down last item
	m_EquipList.GetText(nItem, sName);
	m_EquipList.DeleteString(nItem);
	m_EquipList.InsertString(nItem+1, sName);

	pEquipPack->Clear();
	for (int i=0; i<m_EquipList.GetCount(); ++i)
	{
		m_EquipList.GetText(i, sName);
		SEquipment *pEquip=GetEquipment(sName);
		if (pEquip)
			pEquipPack->AddEquip(*pEquip);
	}
	m_bChanged=true;
	UpdateEquipPackParams();
	m_EquipList.SetCurSel(nItem+1);
	OnLbnSelchangeEquipusedlst();
}

void CEquipPackDialog::OnLbnSelchangeEquipavaillst()
{
	m_InsertBtn.EnableWindow(m_AvailEquipList.GetCount()!=0);
}

void CEquipPackDialog::OnLbnSelchangeEquipusedlst()
{
	int nCurSel=m_EquipList.GetCurSel();
	m_RemoveBtn.EnableWindow(nCurSel>=0);
	GetDlgItem(IDC_MOVEUP)->EnableWindow(nCurSel > 0);
	GetDlgItem(IDC_MOVEDOWN)->EnableWindow(nCurSel >= 0 && nCurSel < m_EquipList.GetCount()-1);
}

void CEquipPackDialog::OnBnClickedImport()
{
	CAutoDirectoryRestoreFileDialog Dlg(TRUE, "eqp", "", OFN_ENABLESIZING|OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR, "Equipment-Pack-Files (*.eqp)|*.eqp||");
	if (Dlg.DoModal()==IDOK)
	{
		if (Dlg.GetPathName().GetLength()>0)
		{
			XmlParser parser;
			XmlNodeRef root=parser.parse(Dlg.GetPathName());
			if (root)
			{
				GetIEditor()->GetEquipPackLib()->Serialize(root, true);
				m_bChanged=true;
				UpdateEquipPacksList();
			}
		}
	}
}

void CEquipPackDialog::OnBnClickedExport()
{
	CAutoDirectoryRestoreFileDialog Dlg(FALSE, "eqp", "", OFN_ENABLESIZING|OFN_EXPLORER|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR, "Equipment-Pack-Files (*.eqp)|*.eqp||");
	if (Dlg.DoModal()==IDOK)
	{
		if (Dlg.GetPathName().GetLength()>0)
		{
			XmlNodeRef root = CreateXmlNode("EquipPackDB");
			GetIEditor()->GetEquipPackLib()->Serialize(root, false);
			SaveXmlNode( root,Dlg.GetPathName());
		}
	}
}

void CEquipPackDialog::OnDestroy()
{
	CXTResizeDialog::OnDestroy();
	if (true || m_bChanged) // we always save the libs and re-export. the save will only save modified libs
	{
		GetIEditor()->GetEquipPackLib()->SaveLibs(true);
	}
}
