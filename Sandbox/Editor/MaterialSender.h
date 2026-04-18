#ifndef __MaterialSender_h__
#define __MaterialSender_h__
#pragma once

#define WM_MATEDITSEND (WM_USER+315)


enum EMaterialSenderMessage
{
	eMSM_Create = 1,
	eMSM_GetSelectedMaterial = 2,
	eMSM_Init = 3,
};

struct SMaterialMapFileHeader
{
	HWND hwndMax;
	HWND hwndMatEdit;
	int msg;
	int Reserved;
};

class CMaterialSender
{
public:

	CMaterialSender(bool bIsMatEditor):m_bIsMatEditor(bIsMatEditor)
	{
		m_h.hwndMatEdit = 0;
		m_h.hwndMax = 0;
		m_h.msg = 0;
		hMapFile = 0;
	}

	CMaterialSender::~CMaterialSender()
	{
		if(hMapFile)
			CloseHandle(hMapFile);
		hMapFile = 0;
	}

	bool GetMessage()
	{
		LoadMapFile();
		return true;
	}

	bool CheckWindows()
	{
		if(!m_h.hwndMax || !m_h.hwndMatEdit || !::IsWindow(m_h.hwndMax) || !::IsWindow(m_h.hwndMatEdit))
			LoadMapFile();
		if(!m_h.hwndMax || !m_h.hwndMatEdit || !::IsWindow(m_h.hwndMax) || !::IsWindow(m_h.hwndMatEdit))
			return false;
		return true;
	}

	bool Create()
	{
		hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1024*1024, "EditMatMappingObject");
		if(hMapFile)
			return true;

		CryLog( "Can't create File Map" );

		return false;
	}

	bool SendMessage(int msg, const XmlNodeRef & node)
	{
		bool bRet = false;

		if(!CheckWindows())
			return false;

		m_h.msg = msg;

		int nDataSize = sizeof(SMaterialMapFileHeader) + strlen(node->getXML().c_str());

		//hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, nDataSize, "EditMatMappingObject");
		
		HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS,	FALSE, "EditMatMappingObject");
		if(hMapFile)
		{
			void * pMes = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, nDataSize);
			if(pMes)
			{
				memcpy(pMes, &m_h, sizeof(SMaterialMapFileHeader));
				strcpy(((char *)pMes) + sizeof(SMaterialMapFileHeader), node->getXML().c_str());
				UnmapViewOfFile(pMes);
				if(m_bIsMatEditor)
					::SendMessage(m_h.hwndMax, WM_MATEDITSEND, msg , 0);
				else
					::SendMessage(m_h.hwndMatEdit, WM_MATEDITSEND, msg , 0);
				bRet = true;
			}
			CloseHandle(hMapFile);
		}
		else
			CryLog("No File Map");

		return bRet;
	}

	void SetupWindows(HWND hwndMax, HWND hwndMatEdit)
	{
		m_h.hwndMax = hwndMax;
		m_h.hwndMatEdit = hwndMatEdit;
	}

private:

	bool LoadMapFile()
	{
		bool bRet=false;
		HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS,	FALSE, "EditMatMappingObject");
		if(hMapFile)
		{
			void * pMes = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);

			if(pMes)
			{
				memcpy(&m_h, pMes, sizeof(SMaterialMapFileHeader));
				XmlParser parser;
				m_node = parser.parseBuffer(((const char *)pMes) + sizeof(SMaterialMapFileHeader));
				UnmapViewOfFile(pMes);
				bRet = true;
			}
			CloseHandle(hMapFile);
		}

		return bRet;
	}

public:
	SMaterialMapFileHeader m_h;
	XmlNodeRef m_node;
private:
	bool m_bIsMatEditor;
	HANDLE hMapFile;
};


#endif //__MaterialSender_h__