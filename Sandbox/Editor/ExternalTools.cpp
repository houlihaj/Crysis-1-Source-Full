////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   externaltools.cpp
//  Version:     v1.00
//  Created:     27/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ExternalTools.h"

#define TOOLS_FILE "Editor/UserTools.xml"

//////////////////////////////////////////////////////////////////////////
CExternalToolsManager::CExternalToolsManager()
{
}

//////////////////////////////////////////////////////////////////////////
CExternalToolsManager::~CExternalToolsManager()
{
	// delete tools.
	for (int i = 0; i < m_tools.size(); i++)
	{
		delete m_tools[i];
	}
	m_tools.clear();
}

//////////////////////////////////////////////////////////////////////////
int CExternalToolsManager::GetToolsCount() const
{
	return m_tools.size();
}

//////////////////////////////////////////////////////////////////////////
CExternalTool* CExternalToolsManager::GetTool( int iIndex ) const
{
	return m_tools[iIndex];
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::AddTool( CExternalTool *tool )
{
	m_tools.push_back(tool);
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::DeleteTool( CExternalTool* tool )
{
	for (int i = 0; i < m_tools.size(); i++)
	{
		if (m_tools[i] == tool)
		{
			m_tools.erase( m_tools.begin() + i );
			delete tool;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::Load()
{
	XmlParser parser;
	XmlNodeRef toolsNode = parser.parse( TOOLS_FILE );
	if (toolsNode)
	{
		for (int i = 0; i < toolsNode->getChildCount(); i++)
		{
			CExternalTool *pTool = new CExternalTool;
			m_tools.push_back(pTool);

			XmlNodeRef node = toolsNode->getChild(i);
			node->getAttr( "Title",m_tools[i]->m_title );
			node->getAttr( "Command",m_tools[i]->m_command );
			node->getAttr( "EditorCmd",m_tools[i]->m_editorCommand );
			node->getAttr( "ToggleVar",m_tools[i]->m_variableToggle );
		}
	}
	else
	{
		CWinApp *pApp = AfxGetApp();
		int i = 0;
		bool finished = false;
		do
		{
			CString tool;
			tool.Format( "Tool%d",i );

			CString title = pApp->GetProfileString( "Tools",tool+"Title" );
			if (title.IsEmpty())
				break;

			CExternalTool *pTool = new CExternalTool;
			m_tools.push_back(pTool);
			pTool->m_title = title;
			pTool->m_command = pApp->GetProfileString( "Tools",tool+"Cmd" );
			pTool->m_editorCommand = pApp->GetProfileString( "Tools",tool+"EditorCmd" );
			pTool->m_variableToggle = pApp->GetProfileInt( "Tools",tool+"ToggleVar",0 ) != 0;
			i++;
		}
		while (!finished);
	}
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::Save()
{
	XmlNodeRef toolsNode = CreateXmlNode( "Tools" );
	for (int i = 0; i < m_tools.size(); i++)
	{
		if (m_tools[i])
		{
			XmlNodeRef node = toolsNode->newChild( "Tool" );
			node->setAttr( "Title",m_tools[i]->m_title );
			node->setAttr( "Command",m_tools[i]->m_command );
			node->setAttr( "EditorCmd",m_tools[i]->m_editorCommand );
			node->setAttr( "ToggleVar",m_tools[i]->m_variableToggle );
		}
	}
	SaveXmlNode( toolsNode,TOOLS_FILE );
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::ClearTools()
{
	m_tools.clear();
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::ExecuteTool( int iIndex )
{
	if (iIndex < 0 || iIndex >= m_tools.size())
		return;

	if (!m_tools[iIndex]->m_editorCommand.IsEmpty())
	{
		GetIEditor()->ExecuteCommand(m_tools[iIndex]->m_editorCommand);
	}
	if (!m_tools[iIndex]->m_command.IsEmpty())
	{
		if (!m_tools[iIndex]->m_variableToggle)
		{
			GetIEditor()->GetSystem()->GetIConsole()->ExecuteString( m_tools[iIndex]->m_command );
		}
		else
		{
			// Toggle variable.
			float val = GetIEditor()->GetConsoleVar(m_tools[iIndex]->m_command);
			bool bOn = val != 0;
			GetIEditor()->SetConsoleVar( m_tools[iIndex]->m_command,(bOn)?0:1 );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CExternalToolsManager::ExecuteTool( const CString &name )
{
	// Find tool with this name.
	for (int i = 0; i < m_tools.size(); i++)
	{
		if (stricmp(m_tools[i]->m_title,name) == 0)
		{
			ExecuteTool(i);
		}
	}
}
