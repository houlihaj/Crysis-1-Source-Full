////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MaterialManager.cpp
//  Version:     v1.00
//  Created:     22/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MaterialManager.h"

#include "Material.h"
#include "MaterialLibrary.h"
#include "ErrorReport.h"

#include "Viewport.h"
#include "ModelViewport.h"
#include "MaterialSender.h"

#include "Objects\ObjectManager.h"
#include "ICryAnimation.h"

#define MATERIALS_LIBS_PATH "/Materials/"

//////////////////////////////////////////////////////////////////////////
// CMaterialManager implementation.
//////////////////////////////////////////////////////////////////////////
CMaterialManager::CMaterialManager( CRegistrationContext &regCtx )
{	
	m_bUniqGuidMap = false;
	m_bUniqNameMap = true;	
	m_p3DEngine=0;

	m_bMaterialsLoaded = false;
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );

	m_MatSender = new CMaterialSender(true);

	RegisterCommands(regCtx);
}

//////////////////////////////////////////////////////////////////////////
CMaterialManager::~CMaterialManager()
{
	if (m_p3DEngine)
		m_p3DEngine->GetMaterialManager()->SetListener( NULL );

	if(m_MatSender)
	{
		delete m_MatSender;
		m_MatSender =0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Set3DEngine( I3DEngine *p3DEngine )
{	
	m_p3DEngine = p3DEngine;
	if (m_p3DEngine)
		m_p3DEngine->GetMaterialManager()->SetListener( this );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ClearAll()
{
	SetCurrentMaterial(NULL);
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary( "Level" );
	m_pLevelLibrary->SetLevelLibrary( true );
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::CreateMaterial( const CString &sMaterialName,XmlNodeRef &node,int nMtlFlags )
{
	CMaterial* pMaterial = new CMaterial(this,sMaterialName,nMtlFlags);

	if (node)
	{
		CBaseLibraryItem::SerializeContext serCtx( node,true );
		serCtx.bUniqName = true;
		pMaterial->Serialize( serCtx );
	}
	if (!pMaterial->IsPureChild() && !(pMaterial->GetFlags() & MTL_FLAG_UIMATERIAL) )
	{
		RegisterItem(pMaterial);
	}

	return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Export( XmlNodeRef &node )
{
	XmlNodeRef libs = node->newChild( "MaterialsLibrary" );
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		// Level libraries are saved in in level.
		XmlNodeRef libNode = libs->newChild( "Library" );

		// Export library.
		libNode->setAttr( "Name",pLib->GetName() );
	}
}

//////////////////////////////////////////////////////////////////////////
int CMaterialManager::ExportLib( CMaterialLibrary *pLib,XmlNodeRef &libNode )
{
	int num = 0;
	// Export library.
	libNode->setAttr( "Name",pLib->GetName() );
	libNode->setAttr( "File",pLib->GetFilename() );
	libNode->setAttr( "SandboxVersion",(const char*)GetIEditor()->GetFileVersion().ToFullString() );
	// Serialize prototypes.
	for (int j = 0; j < pLib->GetItemCount(); j++)
	{
		CMaterial *pMtl = (CMaterial*)pLib->GetItem(j);

		// Only export real used materials.
		if (pMtl->IsDummy() || !pMtl->IsUsed() || pMtl->IsPureChild())
			continue;

		XmlNodeRef itemNode = libNode->newChild( "Material" );
		itemNode->setAttr("Name",pMtl->GetName());
		num++;
	}
	return num;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetSelectedItem( IDataBaseItem *pItem )
{
	m_pSelectedItem = (CBaseLibraryItem*)pItem;
	SetCurrentMaterial( (CMaterial*)pItem );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SetCurrentMaterial( CMaterial *pMtl )
{
	if (m_pCurrentMaterial == pMtl)
		return;

	if (m_pCurrentMaterial)
	{
		// Changing current material. save old one.
		if (m_pCurrentMaterial->IsModified())
			m_pCurrentMaterial->Save();
	}

	m_pCurrentMaterial = pMtl;
	if (m_pCurrentMaterial)
	{
		m_pCurrentMaterial->OnMakeCurrent();
		m_pCurrentEngineMaterial = m_pCurrentMaterial->GetMatInfo();
	}
	else
	{
		m_pCurrentEngineMaterial = 0;
	}
	m_pSelectedItem = pMtl;

	NotifyItemEvent(m_pCurrentMaterial,EDB_ITEM_EVENT_SELECTED);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::GetCurrentMaterial() const
{
	return m_pCurrentMaterial;
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CMaterialManager::MakeNewItem()
{
	CMaterial *pMaterial = new CMaterial(this,"",0);
	return pMaterial;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CMaterialManager::MakeNewLibrary()
{
	return new CMaterialLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
CString CMaterialManager::GetRootNodeName()
{
	return "MaterialsLibs";
}
//////////////////////////////////////////////////////////////////////////
CString CMaterialManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = Path::GetGameFolder()+MATERIALS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::ReportDuplicateItem( CBaseLibraryItem *pItem,CBaseLibraryItem *pOldItem )
{
	CString sLibName;
	if (pOldItem->GetLibrary())
		sLibName = pOldItem->GetLibrary()->GetName();
	CErrorRecord err;
	err.pItem = (CMaterial*)pOldItem;
	err.error.Format( "Material %s with the duplicate name to the loaded material %s ignored",(const char*)pItem->GetName(),(const char*)pOldItem->GetName() );
	GetIEditor()->GetErrorReport()->ReportError( err );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Serialize( XmlNodeRef &node,bool bLoading )
{
	//CBaseLibraryManager::Serialize( node,bLoading );
	if (bLoading)
	{
	}
	else
	{
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	CBaseLibraryManager::OnEditorNotifyEvent(event);
	switch (event)
	{
	case eNotify_OnInit:
		InitMatSender();
		break;

	case eNotify_OnBeginNewScene:
		SetCurrentMaterial( 0 );
		break;
	case eNotify_OnBeginSceneOpen:
		SetCurrentMaterial( 0 );
		break;
	case eNotify_OnMissionChange:
		SetCurrentMaterial( 0 );
		break;
	case eNotify_OnCloseScene:
		SetCurrentMaterial( 0 );
		break;
	case eNotify_OnQuit:
		SetCurrentMaterial( 0 );
		if (m_p3DEngine)
			m_p3DEngine->GetMaterialManager()->SetListener( NULL );
		m_p3DEngine = NULL;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::LoadMaterial( const CString &sMaterialName,bool bMakeIfNotFound )
{
	// Load material with this name if not yet loaded.
	CMaterial *pMaterial = (CMaterial*)FindItemByName(sMaterialName);
	if (pMaterial)
	{
		return pMaterial;
	}

	CString filename = Path::MakeGamePath( MaterialToFilename(sMaterialName) );

	XmlNodeRef mtlNode = GetISystem()->LoadXmlFile( filename );
	if (mtlNode)
	{
		pMaterial = CreateMaterial( sMaterialName,mtlNode );
	}
	else
	{
		if (bMakeIfNotFound)
		{
			pMaterial = new CMaterial( this,sMaterialName );
			pMaterial->SetDummy(true);
			RegisterItem( pMaterial );

			CErrorRecord err;
			err.error.Format( "Material %s not found",(const char*)sMaterialName );
			GetIEditor()->GetErrorReport()->ReportError(err);
		}
	}
	//

	return pMaterial;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMaterialManager::OnLoadMaterial( const char *sMtlName,bool bForceCreation )
{
	_smart_ptr<CMaterial> pMaterial = LoadMaterial( sMtlName,bForceCreation );
	if (pMaterial)
	{
		return pMaterial->GetMatInfo();
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnCreateMaterial( IMaterial *pMatInfo )
{
	if (!(pMatInfo->GetFlags() & MTL_FLAG_PURE_CHILD) && !(pMatInfo->GetFlags() & MTL_FLAG_UIMATERIAL))
	{
		CMaterial *pMaterial = new CMaterial( this,pMatInfo->GetName() );
		pMaterial->SetFromMatInfo( pMatInfo );
		RegisterItem( pMaterial );
	}
	
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::OnDeleteMaterial( IMaterial *pMaterial )
{
	CMaterial *pMtl = (CMaterial*)pMaterial->GetUserData();
	if (pMtl)
	{
		pMtl->ClearMatInfo();
	}
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::FromIMaterial( IMaterial *pMaterial )
{
	if (!pMaterial)
		return 0;
	CMaterial *pMtl = (CMaterial*)pMaterial->GetUserData();
	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SaveAllLibs()
{
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialManager::FilenameToMaterial( const CString &filename )
{
	CString name = Path::RemoveExtension(filename);
	name.Replace( '\\','/' );

	CString sDataFolder=Path::GetGameFolder();	
	// Remove "DATA_FOLDER/" sub path from the filename.
	if (name.GetLength() > (sDataFolder.GetLength()) && strnicmp(name,sDataFolder,sDataFolder.GetLength()) == 0)
	{
		name = name.Mid(sDataFolder.GetLength()+1); // skip the slash...
	}

	/*
	// Remove "materials/" sub path from the filename.
	if (name.GetLength() > sizeof(MATERIALS_PATH)-1 && strnicmp(name,MATERIALS_PATH,sizeof(MATERIALS_PATH)-1) == 0)
	{
		//name = name.Mid(sizeof(MATERIALS_PATH)+1);
	}
	*/
	return name;
}

//////////////////////////////////////////////////////////////////////////
CString CMaterialManager::MaterialToFilename( const CString &sMaterialName )
{
	CString filename = Path::GamePathToFullPath( Path::ReplaceExtension(sMaterialName,MATERIAL_FILE_EXT) );
	return filename;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterialManager::DeleteMaterial( CMaterial *pMtl )
{
	assert(pMtl);
	_smart_ptr<CMaterial> _ref(pMtl);
	if (pMtl == GetCurrentMaterial())
		SetCurrentMaterial(NULL);

	DeleteItem( pMtl );

	// Unassign this material from all objects.
	CBaseObjectsArray objects;
	GetIEditor()->GetObjectManager()->GetObjects( objects );
	int i;
	for (i = 0; i < objects.size(); i++)
	{
		CBaseObject *pObject = objects[i];
		if (pObject->GetMaterial() == pMtl)
		{
			pObject->SetMaterial(NULL);
		}
	}
	// Delete it from all sub materials.
	for (i = 0; i < m_pLevelLibrary->GetItemCount(); i++)
	{
		CMaterial *pMultiMtl = (CMaterial*)m_pLevelLibrary->GetItem(i);
		if (pMultiMtl->IsMultiSubMaterial())
		{
			for (int slot = 0; slot < pMultiMtl->GetSubMaterialCount(); slot++)
			{
				if (pMultiMtl->GetSubMaterial(slot) == pMultiMtl)
				{
					// Clear this sub material slot.
					pMultiMtl->SetSubMaterial(slot,0);
				}
			}
		}
	}
	bool bRes = true;
	// Delete file on disk.
	if (!pMtl->GetFilename().IsEmpty())
	{
		if (!::DeleteFile( pMtl->GetFilename() ))
			bRes = false;
	}

	return bRes;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::RegisterCommands( CRegistrationContext &regCtx )
{
	regCtx.pCommandManager->RegisterCommand( "Material.Create",functor(*this,&CMaterialManager::Command_Create) );
	regCtx.pCommandManager->RegisterCommand( "Material.CreateMulti",functor(*this,&CMaterialManager::Command_CreateMulti) );
	regCtx.pCommandManager->RegisterCommand( "Material.Delete",functor(*this,&CMaterialManager::Command_Delete) );
	regCtx.pCommandManager->RegisterCommand( "Material.AssignToSelection",functor(*this,&CMaterialManager::Command_AssignToSelection) );
	regCtx.pCommandManager->RegisterCommand( "Material.ResetSelection",functor(*this,&CMaterialManager::Command_ResetSelection) );
	regCtx.pCommandManager->RegisterCommand( "Material.SelectAssignedObjects",functor(*this,&CMaterialManager::Command_SelectAssignedObjects) );
	regCtx.pCommandManager->RegisterCommand( "Material.SelectFromObject",functor(*this,&CMaterialManager::Command_SelectFromObject) );
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterialManager::SelectNewMaterial( int nMtlFlags,const char *sStartPath )
{
	CString startPath = GetIEditor()->GetSearchPath(EDITOR_PATH_MATERIALS);
	if (m_pCurrentMaterial)
	{
		startPath = Path::GetPath( m_pCurrentMaterial->GetFilename() );
	}
	if (sStartPath)
		startPath = sStartPath;

	CString filename;
	if (!CFileUtil::SelectSaveFile( "Material Files (*.mtl)|*.mtl","mtl",startPath,filename ))
	{
		return 0;
	}
	filename = Path::GetRelativePath(filename);
	CString itemName = Path::RemoveExtension(filename);
	itemName = Path::MakeGamePath(itemName);
	if (itemName.IsEmpty())
		return 0;

	if (FindItemByName( itemName ))
	{
		Warning( "Material with name %s already exist",(const char*)itemName );
		return 0;
	}

	_smart_ptr<CMaterial> mtl = CreateMaterial( itemName,XmlNodeRef(),nMtlFlags );
	mtl->Update();
	mtl->Save();
	SetCurrentMaterial( mtl );
	return mtl;
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Create()
{
	SelectNewMaterial(0);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_CreateMulti()
{
	SelectNewMaterial( MTL_FLAG_MULTI_SUBMTL );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_Delete()
{
	CMaterial *pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo("Delete Material");
		CString str;
		str.Format( _T("Delete Material %s?\r\nNote: Material file %s will also be deleted."),
			(const char*)pMtl->GetName(),(const char*)pMtl->GetFilename() );
		if (MessageBox(AfxGetMainWnd()->GetSafeHwnd(),str,_T("Delete Confirmation"),MB_YESNO|MB_ICONQUESTION) == IDYES)
		{
			DeleteMaterial( pMtl );
			SetCurrentMaterial( 0 );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_AssignToSelection()
{
	CMaterial *pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo( "Assign Material" );

		CSelectionGroup *pSel = GetIEditor()->GetSelection();
		if (!pSel->IsEmpty())
		{
			for (int i = 0; i < pSel->GetCount(); i++)
			{
				pSel->GetObject(i)->SetMaterial( pMtl );
			}
		}
	}
	CViewport *pViewport = GetIEditor()->GetActiveView();
	if (pViewport)
	{
		pViewport->Drop( CPoint(-1,-1),pMtl );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_ResetSelection()
{
	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (!pSel->IsEmpty())
	{
		CUndo undo( "Reset Material" );
		for (int i = 0; i < pSel->GetCount(); i++)
		{
			pSel->GetObject(i)->SetMaterial( 0 );
		}
	}
	CViewport *pViewport = GetIEditor()->GetActiveView();
	if (pViewport)
	{
		pViewport->Drop( CPoint(-1,-1),0 );
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectAssignedObjects()
{
	CMaterial *pMtl = GetCurrentMaterial();
	if (pMtl)
	{
		CUndo undo("Select Object(s)");
		CBaseObjectsArray objects;
		GetIEditor()->GetObjectManager()->GetObjects( objects );
		for (int i = 0; i < objects.size(); i++)
		{
			CBaseObject *pObject = objects[i];
			if (pObject->GetMaterial() == pMtl || pObject->GetRenderMaterial() == pMtl)
			{
				if (pObject->IsHidden() || pObject->IsFrozen())
					continue;
				GetIEditor()->GetObjectManager()->SelectObject( pObject );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::Command_SelectFromObject()
{
	if (GetIEditor()->IsInPreviewMode())
	{
		CViewport *pViewport = GetIEditor()->GetActiveView();
		if (pViewport && pViewport->IsKindOf(RUNTIME_CLASS(CModelViewport)))
		{
			CMaterial *pMtl = ((CModelViewport*)pViewport)->GetMaterial();
			SetCurrentMaterial( pMtl );
		}
		return;
	}

	CSelectionGroup *pSel = GetIEditor()->GetSelection();
	if (pSel->IsEmpty())
		return;

	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CMaterial *pMtl = pSel->GetObject(i)->GetRenderMaterial();
		if (pMtl)
		{
			SetCurrentMaterial( pMtl );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::PickPreviewMaterial(HWND hWndCaller)
{
	XmlNodeRef data = CreateXmlNode("ExportMaterial");
	CMaterial * pMtl = GetCurrentMaterial();
	if(!pMtl)
		return;

	if(pMtl->IsPureChild() && pMtl->GetParent())
		pMtl = pMtl->GetParent();

	if (pMtl->GetFlags()&MTL_FLAG_WIRE)
		data->setAttr("Flag_Wire",1);
	if (pMtl->GetFlags()&MTL_FLAG_2SIDED)
		data->setAttr("Flag_2Sided",1);

	data->setAttr("Name", pMtl->GetName());
	data->setAttr("FileName", pMtl->GetFilename() );

	XmlNodeRef node = data->newChild("Material");

	CBaseLibraryItem::SerializeContext serCtx( node, false );
	pMtl->Serialize( serCtx );


	if(!pMtl->IsMultiSubMaterial())
	{
		XmlNodeRef texturesNode = node->findChild( "Textures" );
		if(texturesNode)
		{
			for (int i = 0; i < texturesNode->getChildCount(); i++)
			{
				XmlNodeRef texNode = texturesNode->getChild(i);
				CString file;
				if (texNode->getAttr( "File", file))
					texNode->setAttr( "File", Path::GamePathToFullPath(file) );
			}
		}
	}
	else
	{
		XmlNodeRef childsNode = node->findChild( "SubMaterials" );
		if (childsNode)
		{
			int nSubMtls = childsNode->getChildCount();
			for (int i = 0; i < nSubMtls; i++)
			{
				XmlNodeRef node = childsNode->getChild(i);
				XmlNodeRef texturesNode = node->findChild( "Textures" );
				if (texturesNode)
				{
					for (int i = 0; i < texturesNode->getChildCount(); i++)
					{
						XmlNodeRef texNode = texturesNode->getChild(i);
						CString file;
						if (texNode->getAttr( "File", file))
							texNode->setAttr( "File", Path::GamePathToFullPath(file) );
					}
				}
			}
		}
	}


	m_MatSender->SendMessage(eMSM_GetSelectedMaterial, data);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::SyncMaterialEditor()
{
	if(!m_MatSender)
		return;

	if(!m_MatSender->GetMessage())
		return;

	if(m_MatSender->m_h.msg==eMSM_Create)
	{
		XmlNodeRef node = m_MatSender->m_node->findChild("Material");
		if(!node)
			return;

		CString sMtlName;
		CString sMaxFile;

		XmlNodeRef root = m_MatSender->m_node;
		root->getAttr("Name",sMtlName);
		root->getAttr("MaxFile",sMaxFile);

		int IsMulti=0;
		root->getAttr("IsMulti", IsMulti);

		int nMtlFlags = 0;
		if (IsMulti)
			nMtlFlags |= MTL_FLAG_MULTI_SUBMTL;

		if (root->haveAttr("Flag_Wire"))
			nMtlFlags |= MTL_FLAG_WIRE;
		if (root->haveAttr("Flag_2Sided"))
			nMtlFlags |= MTL_FLAG_2SIDED;

		_smart_ptr<CMaterial> pMtl = SelectNewMaterial( nMtlFlags,Path::GetPath(sMaxFile) );

		if(!pMtl)
			return;

		if(!IsMulti)
		{
			node->delAttr( "Shader" ); // Remove shader attribute.
			XmlNodeRef texturesNode = node->findChild( "Textures" );
			if (texturesNode)
			{
				for (int i = 0; i < texturesNode->getChildCount(); i++)
				{
					XmlNodeRef texNode = texturesNode->getChild(i);
					CString file;
					if (texNode->getAttr( "File", file))
					{
						CString newfile = Path::MakeGamePath(file);
						if(newfile.GetLength()>0)
							file=newfile;
						texNode->setAttr( "File", file);
					}
				}
			}
		}
		else
		{
			XmlNodeRef childsNode = node->findChild( "SubMaterials" );
			if (childsNode)
			{
				int nSubMtls = childsNode->getChildCount();
				for (int i = 0; i < nSubMtls; i++)
				{
					XmlNodeRef node = childsNode->getChild(i);
					node->delAttr( "Shader" ); // Remove shader attribute.
					XmlNodeRef texturesNode = node->findChild( "Textures" );
					if (texturesNode)
					{
						for (int i = 0; i < texturesNode->getChildCount(); i++)
						{
							XmlNodeRef texNode = texturesNode->getChild(i);
							CString file;
							if (texNode->getAttr( "File", file))
							{
								CString newfile = Path::MakeGamePath(file);
								if(newfile.GetLength()>0)
									file=newfile;
								texNode->setAttr( "File", file);
							}
						}
					}
				}
			}
		}

		CBaseLibraryItem::SerializeContext ctx( node, true );
		ctx.bUndo = true;
		pMtl->Serialize( ctx );

		pMtl->Update();

		SetCurrentMaterial(0);
		SetCurrentMaterial(pMtl);
	}

	if(m_MatSender->m_h.msg==eMSM_GetSelectedMaterial)
	{
		PickPreviewMaterial(m_MatSender->m_h.hwndMax);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::InitMatSender()
{
	//MatSend(true);
	m_MatSender->Create();
	m_MatSender->SetupWindows( AfxGetMainWnd()->m_hWnd,AfxGetMainWnd()->m_hWnd );
	XmlNodeRef node = CreateXmlNode("Temp");
	m_MatSender->SendMessage(eMSM_Init, node);
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial( CMaterial *pMaterial )
{
	if (pMaterial)
		GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pMaterial );
}

//////////////////////////////////////////////////////////////////////////
void CMaterialManager::GotoMaterial( IMaterial *pMtl )
{
	if (pMtl)
	{
		CMaterial *pEdMaterial = FromIMaterial(pMtl);
		if (pEdMaterial)
			GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pEdMaterial );
	}
}
