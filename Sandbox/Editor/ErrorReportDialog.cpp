////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   errorreportdialog.cpp
//  Version:     v1.00
//  Created:     30/5/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ErrorReportDialog.h"
#include "ErrorReport.h"
#include "Clipboard.h"
#include "Util/CryMemFile.h"					// CCryMemFile
#include "Viewport.h"

#include "Objects\BaseObject.h"
#include "material\Material.h"
#include "Util\Mailer.h"
#include "GameEngine.h"

//////////////////////////////////////////////////////////////////////////
CErrorReportDialog* CErrorReportDialog::m_instance = 0;

// CErrorReportDialog dialog
#define BITMAP_ERROR 0
#define BITMAP_WARNING 1
#define BITMAP_COMMENT 2

#define COLUMN_SEVERITY  0
#define COLUMN_COUNT     1
#define COLUMN_TEXT      2
#define COLUMN_FILE      3  
#define COLUMN_OBJECT    4
#define COLUMN_MODULE    5
#define COLUMN_DESCRIPTION 6

#define COLUMN_MAIL_ICON	0
#define COLUMN_CHECK_ICON	2

#define ID_REPORT_CONTROL 100

static int __stdcall CompareItems( LPARAM p1,LPARAM p2,LPARAM sort )
{
	CErrorRecord *err1 = (CErrorRecord*)p1;
	CErrorRecord *err2 = (CErrorRecord*)p2;
	if (err1->severity < err2->severity)
		return -1;
	else if (err1->severity > err2->severity)
		return 1;
	else
	{
		if (err1->module == err2->module)
			return stricmp(err1->error,err2->error);
		else
			return err1->module < err2->module;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CErrorMessageRecord : public CXTPReportRecord  
{
	DECLARE_DYNAMIC(CErrorMessageRecord)
public:
	CErrorRecord *m_pRecord;
	CXTPReportRecordItem *m_pIconItem;

	CErrorMessageRecord( CErrorRecord &err )
	{
		m_pRecord = &err;
		//m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_SEVERITY, _T("Severity"), 18, TRUE, BITMAP_ERROR);
		//m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TEXT, _T("Warning"), 18, FALSE));
		//m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_FILE, _T("File"), 30, FALSE));
		//m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_OBJECT, _T("Object/Material"), 30, FALSE));
		//m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_MODULE, _T("Module"), 20, FALSE));

		CString text,file,object,module,desc;
		int count = err.count;

		text = err.error;
		desc = err.description;
		file = err.file;

		if (err.pObject)
			object = err.pObject->GetName();

		if (err.pItem)
			object = err.pItem->GetFullName();

		switch (err.module)
		{
		case VALIDATOR_MODULE_RENDERER: module = "Renderer"; break;
		case VALIDATOR_MODULE_3DENGINE: module = "3DEngine"; break;
		case VALIDATOR_MODULE_AI: module = "AI"; break;
		case VALIDATOR_MODULE_ANIMATION: module = "Animation"; break;
		case VALIDATOR_MODULE_ENTITYSYSTEM: module = "EntitySystem"; break;
		case VALIDATOR_MODULE_SCRIPTSYSTEM: module = "ScriptSystem"; break;
		case VALIDATOR_MODULE_SYSTEM: module = "System"; break;
		case VALIDATOR_MODULE_SOUNDSYSTEM: module = "SoundSystem"; break;
		case VALIDATOR_MODULE_GAME: module = "Game"; break;
		case VALIDATOR_MODULE_MOVIE: module = "Movie"; break;
		case VALIDATOR_MODULE_EDITOR: module = "Editor"; break;
		case VALIDATOR_MODULE_NETWORK: module = "Network"; break;
		case VALIDATOR_MODULE_PHYSICS: module = "Physics"; break;
		case VALIDATOR_MODULE_FLOWGRAPH: module = "FlowGraph"; break;
		case VALIDATOR_MODULE_UNKNOWN:
		default:
			module = "Unknown";
		}

		m_pIconItem = AddItem(new CXTPReportRecordItem());
		AddItem(new CXTPReportRecordItemNumber(count));
		AddItem(new CXTPReportRecordItemText(text));
		AddItem(new CXTPReportRecordItemText(file));
		AddItem(new CXTPReportRecordItemText(object))->AddHyperlink(new CXTPReportHyperlink(0,object.GetLength()));
		AddItem(new CXTPReportRecordItemText(module));
		AddItem(new CXTPReportRecordItemText(desc));


		//SetPreviewItem(new CXTPReportRecordItemPreview(strPreview));
		int nIcon = 0;
		if (err.severity == CErrorRecord::ESEVERITY_ERROR)
			nIcon = BITMAP_ERROR;
		else if (err.severity == CErrorRecord::ESEVERITY_WARNING)
			nIcon = BITMAP_WARNING;
		else if (err.severity == CErrorRecord::ESEVERITY_COMMENT)
			nIcon = BITMAP_COMMENT;
		m_pIconItem->SetIconIndex(nIcon);
		m_pIconItem->SetGroupPriority(nIcon);
		m_pIconItem->SetSortPriority(nIcon);
	}

	virtual void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics)
	{
		if (m_pIconItem == pDrawArgs->pItem && pDrawArgs->pItem->GetIconIndex() == 0)
		{
			// Red error text.
			pItemMetrics->clrForeground = RGB(0xFF, 0, 0);
		}
	}
};


IMPLEMENT_DYNAMIC(CErrorMessageRecord,CXTPReportRecord)

IMPLEMENT_DYNAMIC(CErrorReportDialog, CXTResizeDialog)
CErrorReportDialog::CErrorReportDialog( CErrorReport *report,CWnd* pParent /*=NULL*/)
	: CXTResizeDialog(CErrorReportDialog::IDD, pParent)
{
	m_pErrorReport = report;
}

CErrorReportDialog::~CErrorReportDialog()
{
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::Open( CErrorReport *pReport )
{
	if (m_instance)
	{
		delete m_instance;
	}
	m_instance = new CErrorReportDialog( pReport );
	m_instance->Create( CErrorReportDialog::IDD,AfxGetMainWnd() );
	m_instance->ShowWindow( SW_SHOW );
}

BEGIN_MESSAGE_MAP(CErrorReportDialog, CXTResizeDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_ERRORS, OnNMDblclkErrors)
	ON_BN_CLICKED(IDC_SELECTOBJECTS, OnSelectObjects)
	ON_WM_SYSCOMMAND()
	ON_WM_SIZE()

	ON_NOTIFY(NM_CLICK, ID_REPORT_CONTROL, OnReportItemClick)
	ON_NOTIFY(NM_RCLICK, ID_REPORT_CONTROL, OnReportItemRClick)
	ON_NOTIFY(NM_DBLCLK, ID_REPORT_CONTROL, OnReportItemDblClick)
	ON_NOTIFY(XTP_NM_REPORT_HEADER_RCLICK, ID_REPORT_CONTROL, OnReportColumnRClick)
	ON_NOTIFY(XTP_NM_REPORT_HYPERLINK, ID_REPORT_CONTROL, OnReportHyperlink)
	ON_NOTIFY(NM_KEYDOWN, ID_REPORT_CONTROL, OnReportKeyDown)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::Close()
{
	if (m_instance)
	{
		/*
		CCryMemFile memFile( new BYTE[256], 256, 256 );
		CArchive ar( &memFile, CArchive::store );
		m_instance->m_wndReport.SerializeState( ar );
		ar.Close();

		UINT nSize = (UINT)memFile.GetLength();
		LPBYTE pbtData = memFile.Detach();
		CXTRegistryManager regManager;
		regManager.WriteProfileBinary( "Dialogs\\ErrorReport", "Configuration", pbtData, nSize);
		if ( pbtData )
			delete [] pbtData;
*/
		m_instance->DestroyWindow();
	}
}

void CErrorReportDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}



// CErrorReportDialog message handlers

BOOL CErrorReportDialog::OnInitDialog()
{
	__super::OnInitDialog();

	VERIFY( m_wndReport.Create(WS_CHILD|WS_TABSTOP|WS_VISIBLE|WM_VSCROLL, CRect(0, 0, 0, 0), this, ID_REPORT_CONTROL) );

	//m_imageList.Create(IDB_ERROR_REPORT, 16, 1, RGB (255, 255, 255));
	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_ERROR_REPORT,16,RGB(255,255,255) );

	m_wndReport.SetImageList(&m_imageList); 

	CXTPReportColumn *pModuleCol = 0;

	//  Add sample columns
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_SEVERITY, _T(""), 18, FALSE ));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_COUNT, _T("N"), 30, FALSE ));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_TEXT, _T("Text"), 40, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_FILE, _T("File"), 30, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_OBJECT, _T("Object/Material"), 30, TRUE));
	m_wndReport.AddColumn(pModuleCol=new CXTPReportColumn(COLUMN_MODULE, _T("Module"), 20, TRUE));
	m_wndReport.AddColumn(new CXTPReportColumn(COLUMN_DESCRIPTION, _T("Description"), 20, TRUE));

	m_wndReport.GetColumns()->GetGroupsOrder()->Add( pModuleCol );
	/*
	if (m_wndSubList.GetSafeHwnd() == NULL)
	{
		m_wndSubList.SubclassDlgItem(IDC_COLUMNLIST, &pWnd->m_wndFieldChooser);
		m_wndReport.GetColumns()->GetReportHeader()->SetSubListCtrl(&m_wndSubList);
	}

	if (m_wndFilterEdit.GetSafeHwnd() == NULL)
	{
		m_wndFilterEdit.SubclassDlgItem(IDC_FILTEREDIT, &pWnd->m_wndFilterEdit);
		m_wndReport.GetColumns()->GetReportHeader()->SetFilterEditCtrl(&m_wndFilterEdit);
	}
	*/

	AutoLoadPlacement( "Dialogs\\ErrorReport" );

	UINT nSize = 0;
	LPBYTE pbtData = NULL;
	CXTRegistryManager regManager;
	if (regManager.GetProfileBinary( "Dialogs\\ErrorReport", "Configuration", &pbtData, &nSize))
	{
		CCryMemFile memFile( pbtData, nSize );
		CArchive ar( &memFile, CArchive::load );
		m_wndReport.SerializeState( ar );
	}

	ReloadErrors();
	m_wndReport.RedrawControl();

	RedrawWindow();

	CRect rc;
	GetClientRect(rc);
	m_wndReport.MoveWindow(rc);

	/*
	CXTPReportColumns* pColumns = m_wndReport.GetColumns();
	CXTPReportColumn* pColumn = pColumns->GetAt(0);
	if (pColumn && pColumn->IsSortable())
	{
		pColumns->SetSortColumn(pColumn, true);
		m_wndReport.Populate();
	}
	*/

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnSize( UINT nType,int cx,int cy )
{
	__super::OnSize(nType,cx,cy);

	if (m_wndReport)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndReport.MoveWindow(rc);
	}
}

void CErrorReportDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
	if (nID == SC_CLOSE)
	{
		Close();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}



//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::ReloadErrors()
{
	m_wndReport.BeginUpdate();

	// Store localarray of error records.
	m_errorRecords.clear();
	m_errorRecords.reserve( m_pErrorReport->GetErrorCount() );
	for (int i = 0; i < m_pErrorReport->GetErrorCount(); i++)
	{
		CErrorRecord &err = m_pErrorReport->GetError(i);
		m_errorRecords.push_back( err );
	}
	
	for (int i = 0; i < m_errorRecords.size(); i++)
	{
		CErrorRecord &err = m_errorRecords[i];
		const char *str = err.error;

		m_wndReport.AddRecord( new CErrorMessageRecord(err) );
	}

	m_wndReport.EndUpdate();
	m_wndReport.Populate();
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::PostNcDestroy()
{
	__super::PostNcDestroy();
	if (m_instance)
		delete m_instance;
	m_instance = 0;
}

void CErrorReportDialog::OnNMDblclkErrors(NMHDR *pNMHDR, LRESULT *pResult)
{
	/*
	NMITEMACTIVATE *lpNM = (NMITEMACTIVATE*)pNMHDR;

	CErrorRecord *err = (CErrorRecord*)m_errors.GetItemData( lpNM->iItem );
	if (err)
	{
		if (err->pObject)
		{
			CUndo undo( "Select Object(s)" );
			// Clear other selection.
			GetIEditor()->ClearSelection();
			// Select this object.
			GetIEditor()->SelectObject( err->pObject );
		}
		else if (err->pMaterial)
		{
			GetIEditor()->OpenDataBaseLibrary( EDB_MATERIAL_LIBRARY,err->pMaterial );
		}
	}
	*/

	// TODO: Add your control notification handler code here
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnOK()
{
	DestroyWindow();
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnCancel()
{
	DestroyWindow();
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnSelectObjects()
{
	/*
	CUndo undo( "Select Object(s)" );
	// Clear other selection.
	GetIEditor()->ClearSelection();
	POSITION pos = m_errors.GetFirstSelectedItemPosition();
	while (pos)
	{
		int nItem = m_errors.GetNextSelectedItem(pos);
		CErrorRecord *pError = (CErrorRecord*)m_errors.GetItemData( nItem );
		if (pError && pError->pObject)
		{
			// Select this object.
			GetIEditor()->SelectObject( pError->pObject );
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
#define ID_REMOVE_ITEM	1
#define ID_SORT_ASC		2
#define ID_SORT_DESC		3
#define ID_GROUP_BYTHIS	4
#define ID_SHOW_GROUPBOX		5
#define ID_SHOW_FIELDCHOOSER 6
#define ID_COLUMN_BESTFIT		7
#define ID_COLUMN_ARRANGEBY	100
#define ID_COLUMN_ALIGMENT	200
#define ID_COLUMN_ALIGMENT_LEFT	ID_COLUMN_ALIGMENT + 1
#define ID_COLUMN_ALIGMENT_RIGHT	ID_COLUMN_ALIGMENT + 2
#define ID_COLUMN_ALIGMENT_CENTER	ID_COLUMN_ALIGMENT + 3
#define ID_COLUMN_SHOW		500

void CErrorReportDialog::OnReportColumnRClick(NMHDR * pNotifyStruct, LRESULT * /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	// create main menu items
	menu.AppendMenu(MF_STRING, ID_SORT_ASC, _T("Sort &Ascending") );
	menu.AppendMenu(MF_STRING, ID_SORT_DESC, _T("Sort Des&cending") );
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_GROUP_BYTHIS, _T("&Group by this field") );
	menu.AppendMenu(MF_STRING, ID_SHOW_GROUPBOX, _T("Group &by box") );
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_REMOVE_ITEM, _T("&Remove column") );
	menu.AppendMenu(MF_STRING, ID_SHOW_FIELDCHOOSER, _T("Field &Chooser") );
	menu.AppendMenu(MF_SEPARATOR, (UINT)-1, (LPCTSTR)NULL);
	menu.AppendMenu(MF_STRING, ID_COLUMN_BESTFIT, _T("Best &Fit") );

	if (m_wndReport.IsGroupByVisible())
	{
		menu.CheckMenuItem(ID_SHOW_GROUPBOX, MF_BYCOMMAND|MF_CHECKED);
	}
	if (m_wndReport.GetReportHeader()->IsShowItemsInGroups())
	{
		menu.EnableMenuItem(ID_GROUP_BYTHIS, MF_BYCOMMAND|MF_DISABLED);
	}

	CXTPReportColumns* pColumns = m_wndReport.GetColumns();
	CXTPReportColumn* pColumn = pItemNotify->pColumn;

	// create arrange by items
	CMenu menuArrange;
	VERIFY(menuArrange.CreatePopupMenu());
	int nColumnCount = pColumns->GetCount();
	for (int nColumn = 0; nColumn < nColumnCount; nColumn++) 
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
		if (pCol && pCol->IsVisible())
		{
			CString sCaption = pCol->GetCaption();
			if (!sCaption.IsEmpty())
				menuArrange.AppendMenu(MF_STRING, ID_COLUMN_ARRANGEBY + nColumn, sCaption);
		}
	}

	menuArrange.AppendMenu(MF_SEPARATOR, 60, (LPCTSTR)NULL);

	menuArrange.AppendMenu(MF_STRING, ID_COLUMN_ARRANGEBY + nColumnCount,_T("Show in groups") );
	menuArrange.CheckMenuItem(ID_COLUMN_ARRANGEBY + nColumnCount, 
		MF_BYCOMMAND | ((m_wndReport.GetReportHeader()->IsShowItemsInGroups()) ? MF_CHECKED : MF_UNCHECKED)  );
	menu.InsertMenu(0, MF_BYPOSITION | MF_POPUP, (UINT) menuArrange.m_hMenu,
		_T("Arrange By") );

	// create columns items
	CMenu menuColumns;
	VERIFY(menuColumns.CreatePopupMenu());
	for (int nColumn = 0; nColumn < nColumnCount; nColumn++) 
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
		CString sCaption = pCol->GetCaption();
		//if (!sCaption.IsEmpty())
		menuColumns.AppendMenu(MF_STRING, ID_COLUMN_SHOW + nColumn, sCaption);
		menuColumns.CheckMenuItem(ID_COLUMN_SHOW + nColumn, 
			MF_BYCOMMAND | (pCol->IsVisible() ? MF_CHECKED : MF_UNCHECKED) );
	}

	menu.InsertMenu(0, MF_BYPOSITION | MF_POPUP, (UINT) menuColumns.m_hMenu,_T("Columns"));

	//create Text alignment submenu
	CMenu menuAlign;
	VERIFY(menuAlign.CreatePopupMenu());

	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_LEFT,_T("Align Left") );
	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_RIGHT,_T("Align Right") );
	menuAlign.AppendMenu(MF_STRING, ID_COLUMN_ALIGMENT_CENTER,_T("Align Center") );

	int nAlignOption = 0;
	switch (pColumn->GetAlignment())
	{
	case DT_LEFT :
		nAlignOption = ID_COLUMN_ALIGMENT_LEFT;
		break;
	case DT_RIGHT :
		nAlignOption = ID_COLUMN_ALIGMENT_RIGHT;
		break;
	case DT_CENTER :
		nAlignOption = ID_COLUMN_ALIGMENT_CENTER;
		break;
	}

	menuAlign.CheckMenuItem(nAlignOption, MF_BYCOMMAND | MF_CHECKED);
	menu.InsertMenu(11, MF_BYPOSITION | MF_POPUP, (UINT) menuAlign.m_hMenu,_T("&Alignment") );

	// track menu	
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// arrange by items
	if (nMenuResult >= ID_COLUMN_ARRANGEBY && nMenuResult < ID_COLUMN_ALIGMENT)
	{
		for (int nColumn = 0; nColumn < nColumnCount; nColumn++) 
		{
			CXTPReportColumn* pCol = pColumns->GetAt(nColumn);
			if (pCol && pCol->IsVisible())
			{
				if (nMenuResult == ID_COLUMN_ARRANGEBY + nColumn)
				{
					nMenuResult = ID_SORT_ASC;
					pColumn = pCol;
					break;
				}
			}
		}
		// group by item
		if (ID_COLUMN_ARRANGEBY + nColumnCount == nMenuResult)
		{
			m_wndReport.GetReportHeader()->ShowItemsInGroups(
				!m_wndReport.GetReportHeader()->IsShowItemsInGroups());
		}
	}

	// process Alignment options
	if (nMenuResult > ID_COLUMN_ALIGMENT && nMenuResult < ID_COLUMN_SHOW)
	{
		switch (nMenuResult)
		{
		case ID_COLUMN_ALIGMENT_LEFT :	
			pColumn->SetAlignment(DT_LEFT);
			break;
		case ID_COLUMN_ALIGMENT_RIGHT :
			pColumn->SetAlignment(DT_RIGHT);
			break;
		case ID_COLUMN_ALIGMENT_CENTER	:
			pColumn->SetAlignment(DT_CENTER);
			break;
		}
	}

	// process column selection item
	if (nMenuResult >= ID_COLUMN_SHOW)
	{
		CXTPReportColumn* pCol = pColumns->GetAt(nMenuResult - ID_COLUMN_SHOW);
		if (pCol)
		{
			pCol->SetVisible(!pCol->IsVisible());
		}
	}

	// other general items
	switch (nMenuResult)
	{
	case ID_REMOVE_ITEM:
		pColumn->SetVisible(FALSE);
		m_wndReport.Populate();
		break;
	case ID_SORT_ASC:
	case ID_SORT_DESC: 
		if (pColumn && pColumn->IsSortable())
		{
			pColumns->SetSortColumn(pColumn, nMenuResult == ID_SORT_ASC);
			m_wndReport.Populate();
		}
		break;

	case ID_GROUP_BYTHIS:
		if (pColumns->GetGroupsOrder()->IndexOf(pColumn) < 0)
		{
			pColumns->GetGroupsOrder()->Add(pColumn);
		}
		m_wndReport.ShowGroupBy(TRUE);
		m_wndReport.Populate();
		break;
	case ID_SHOW_GROUPBOX:
		m_wndReport.ShowGroupBy(!m_wndReport.IsGroupByVisible());
		break;
	case ID_SHOW_FIELDCHOOSER:
		//OnShowFieldChooser();
		break;
	case ID_COLUMN_BESTFIT:
		m_wndReport.GetColumns()->GetReportHeader()->BestFit(pColumn);
		break;
	} 
}

#define ID_POPUP_COLLAPSEALLGROUPS 1
#define ID_POPUP_EXPANDALLGROUPS 2


//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::CopyToClipboard()
{
	CClipboard clipboard;
	CString str;
	CXTPReportSelectedRows * selRows = m_wndReport.GetSelectedRows();
	for(int i=0; i<selRows->GetCount(); i++)
	{
		CErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CErrorMessageRecord,selRows->GetAt(i)->GetRecord());
		if(pRecord)
		{
			str+=pRecord->m_pRecord->GetErrorText();
			if(pRecord->m_pRecord->pObject)
				str+=" [Object: " + pRecord->m_pRecord->pObject->GetName() + "]";
			if(pRecord->m_pRecord->pItem)
				str+=" [Material: " + pRecord->m_pRecord->pItem->GetName() + "]";
			str+="\r\n";
		}
	}
	if(!str.IsEmpty())
		clipboard.PutString( str,"Errors" );

}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnReportItemRClick(NMHDR * pNotifyStruct, LRESULT * /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow)
		return;

	if (pItemNotify->pRow->IsGroupRow())
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, ID_POPUP_COLLAPSEALLGROUPS,_T("Collapse &All Groups") );
		menu.AppendMenu(MF_STRING, ID_POPUP_EXPANDALLGROUPS,_T("E&xpand All Groups") );

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);

		// general items processing
		switch (nMenuResult)
		{
		case ID_POPUP_COLLAPSEALLGROUPS:
			pItemNotify->pRow->GetControl()->CollapseAll();
			break;
		case ID_POPUP_EXPANDALLGROUPS:
			pItemNotify->pRow->GetControl()->ExpandAll();
			break;
		}
	} else if (pItemNotify->pItem)
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 0,_T("Select Object(s)") );
		menu.AppendMenu(MF_STRING, 1,_T("Copy Warning(s) To Clipboard") );
		menu.AppendMenu(MF_STRING, 2,_T("E-mail Error Report") );
		menu.AppendMenu(MF_STRING, 3,_T("Open in Excel") );

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu( &menu, TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN|TPM_RIGHTBUTTON, pItemNotify->pt.x, pItemNotify->pt.y, this, NULL);
		switch (nMenuResult)
		{
		case 1:
			CopyToClipboard();
			break;
		case 2:
			SendInMail();
			break;
		case 3:
			OpenInExcel();
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::SendInMail()
{
	if (!m_pErrorReport)
		return;

	// Send E-Mail.
	CString textMsg;
	for (int i = 0; i < m_errorRecords.size(); i++)
	{
		CErrorRecord &err = m_errorRecords[i];
		textMsg += err.GetErrorText() + "\n";
	}

	std::vector<const char*> who;
	CString subject;
	subject.Format( "Level %s Error Report",(const char*)GetIEditor()->GetGameEngine()->GetLevelName() );
	CMailer::SendMail( subject,textMsg,who,who,true );
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OpenInExcel()
{
	if (!m_pErrorReport)
		return;

	CString levelName = Path::GetFileName(GetIEditor()->GetGameEngine()->GetLevelName());

	CString filename;
	filename.Format( "ErrorList_%s.csv",(const char*)levelName );

	// Save to temp file.
	FILE *f = fopen(filename,"wt");
	if (f)
	{
		for (int i = 0; i < m_errorRecords.size(); i++)
		{
			CErrorRecord &err = m_errorRecords[i];
			CString text = (const char*)err.GetErrorText();
			text.Replace( ',','.' );
			text.Replace( '\t',',' );
			fprintf( f,"%s\n",text );
		}
		fclose(f);
		::ShellExecute( NULL, "open", filename, NULL, NULL, SW_SHOW );
	}
	else
	{
		Warning( "Failed to save %s",(const char*)filename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnReportItemClick(NMHDR * pNotifyStruct, LRESULT * /*result*/)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (!pItemNotify->pRow || !pItemNotify->pColumn)
		return;

	TRACE(_T("Click on row %d, col %d\n"), 
		pItemNotify->pRow->GetIndex(), pItemNotify->pColumn->GetItemIndex());

	
	/*
	if (pItemNotify->pColumn->GetItemIndex() == COLUMN_CHECK)
	{
		m_wndReport.Populate();
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnReportKeyDown(NMHDR * pNotifyStruct, LRESULT * /*result*/)
{
	LPNMKEY lpNMKey = (LPNMKEY)pNotifyStruct;

	if (!m_wndReport.GetFocusedRow())
		return;

	if (lpNMKey->nVKey == VK_RETURN)
	{	
		CErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CErrorMessageRecord,m_wndReport.GetFocusedRow()->GetRecord());
		/*
		if (pRecord)
		{
			if (pRecord->SetRead())
			{			
				m_wndReport.Populate();
			}
		}
		*/
	}

	if(lpNMKey->nVKey == 'C' && GetAsyncKeyState(VK_CONTROL))
	{
		CopyToClipboard();
	}
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnReportItemDblClick(NMHDR * pNotifyStruct, LRESULT * result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if(!pItemNotify->pRow)
		return;
	CErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CErrorMessageRecord, pItemNotify->pRow->GetRecord());
	if (pRecord)
	{
		CErrorRecord *pError = pRecord->m_pRecord;
		if (pError && pError->pObject != NULL)
		{
			CUndo undo( "Select Object(s)" );
			// Clear other selection.
			GetIEditor()->ClearSelection();
			// Select this object.
			GetIEditor()->SelectObject( pError->pObject );

			CViewport *vp = GetIEditor()->GetActiveView();
			if (vp)
				vp->CenterOnSelection();
		}
		if (pError && pError->pItem != NULL)
		{
			GetIEditor()->OpenDataBaseLibrary( pError->pItem->GetType(),pError->pItem );
		}
	}

	/*
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;

	if (pItemNotify->pRow)
	{
		TRACE(_T("Double Click on row %d\n"), 
			pItemNotify->pRow->GetIndex());

		CErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CErrorMessageRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			/*
			{			
				m_wndReport.Populate();
			}
			* /
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnReportHyperlink(NMHDR * pNotifyStruct, LRESULT * result)
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	if (pItemNotify->nHyperlink >= 0)
	{
		CErrorMessageRecord* pRecord = DYNAMIC_DOWNCAST(CErrorMessageRecord, pItemNotify->pRow->GetRecord());
		if (pRecord)
		{
			CErrorRecord *pError = pRecord->m_pRecord;
			if (pError && pError->pObject != NULL)
			{
				CUndo undo( "Select Object(s)" );
				// Clear other selection.
				GetIEditor()->ClearSelection();
				// Select this object.
				GetIEditor()->SelectObject( pError->pObject );
			}
			if (pError && pError->pItem != NULL)
			{
				GetIEditor()->OpenDataBaseLibrary( pError->pItem->GetType(),pError->pItem );
			}
		}
		TRACE(_T("Hyperlink Click : \n row %d \n col %d \n Hyperlink %d\n"), 
			pItemNotify->pRow->GetIndex(), pItemNotify->pColumn->GetItemIndex(), pItemNotify->nHyperlink);
	}
}

/*
//////////////////////////////////////////////////////////////////////////
void CErrorReportDialog::OnShowFieldChooser()
{
	CMainFrm* pMainFrm = (CMainFrame*)AfxGetMainWnd();
	if (pMainFrm)
	{
		BOOL bShow = !pMainFrm->m_wndFieldChooser.IsVisible();
		pMainFrm->ShowControlBar(&pMainFrm->m_wndFieldChooser, bShow, FALSE);
	}
}
*/
