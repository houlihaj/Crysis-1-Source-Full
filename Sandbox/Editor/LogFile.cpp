// LogFile.cpp: implementation of the CLogFile class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "LogFile.h"
#include "CryEdit.h"

#include "ProcessInfo.h"

#define EDITOR_LOG_FILE "Editor.log"

// Static member variables
HWND CLogFile::m_hWndListBox = 0;
HWND CLogFile::m_hWndEditBox = 0;
bool CLogFile::m_bShowMemUsage = false;

#define MAX_LOGBUFFER_SIZE 16384

//////////////////////////////////////////////////////////////////////////
void Error( const char *format,... )
{
	va_list	ArgList;
	char		szBuffer[MAX_LOGBUFFER_SIZE];

	va_start(ArgList, format);
	vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	CString str = "####-ERROR-####: ";
	str += szBuffer;

	//CLogFile::WriteLine( str );
	CryWarning( VALIDATOR_MODULE_EDITOR,VALIDATOR_ERROR,str );

	if (!((CCryEditApp*)AfxGetApp())->IsInTestMode() && !((CCryEditApp*)AfxGetApp())->IsInExportMode())
	{
		AfxMessageBox( szBuffer,MB_OK|MB_ICONERROR|MB_APPLMODAL );
	}
}

//////////////////////////////////////////////////////////////////////////
void Warning( const char *format,... )
{
	va_list	ArgList;
	char		szBuffer[MAX_LOGBUFFER_SIZE];

	va_start(ArgList, format);
	vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	CryWarning( VALIDATOR_MODULE_EDITOR,VALIDATOR_WARNING,szBuffer );
//	CLogFile::WriteLine( szBuffer );
	if (!((CCryEditApp*)AfxGetApp())->IsInTestMode() && !((CCryEditApp*)AfxGetApp())->IsInExportMode())
	{
		AfxMessageBox( szBuffer,MB_OK|MB_ICONWARNING|MB_APPLMODAL );
	}
}

//////////////////////////////////////////////////////////////////////////
void Log( const char *format,... )
{
	va_list	ArgList;
	char		szBuffer[MAX_LOGBUFFER_SIZE];

	va_start(ArgList, format);
	vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	CLogFile::WriteLine(szBuffer);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLogFile::CLogFile()
{
	AboutSystem();
}

CLogFile::~CLogFile()
{
}

//////////////////////////////////////////////////////////////////////////
const char* CLogFile::GetLogFileName()
{
	// Return the path
	return gEnv->pLog->GetFileName();
}

void CLogFile::FormatLine(PSTR format,...)
{
	////////////////////////////////////////////////////////////////////////
	// Write a line with printf style formatting
	////////////////////////////////////////////////////////////////////////

	va_list		ArgList;
	char		szBuffer[MAX_LOGBUFFER_SIZE];

	va_start(ArgList, format);
	vsprintf(szBuffer, format, ArgList);
	va_end(ArgList);

	CryLog(szBuffer);
}

void CLogFile::AboutSystem()
{
	//////////////////////////////////////////////////////////////////////
	// Write the system informations to the log
	//////////////////////////////////////////////////////////////////////

	char szBuffer[MAX_LOGBUFFER_SIZE];
	char szProfileBuffer[128];
	char szLanguageBuffer[64];
	//char szCPUModel[64];
	char *pChar = 0;
	MEMORYSTATUS MemoryStatus;
	DEVMODE DisplayConfig;
	OSVERSIONINFO OSVerInfo;
	OSVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	//////////////////////////////////////////////////////////////////////
	// Display editor and Windows version
	//////////////////////////////////////////////////////////////////////

	// Get system language
	GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLANGUAGE, 
		szLanguageBuffer, sizeof(szLanguageBuffer));

	// Format and send OS information line
	sprintf(szBuffer, "Current Language: %s ", szLanguageBuffer);
	CryLog(szBuffer);

	// Format and send OS version line
	CString str = "Windows ";
	GetVersionEx(&OSVerInfo);
	if (OSVerInfo.dwMajorVersion == 4)
	{
		if (OSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		{
			if (OSVerInfo.dwMinorVersion > 0)
				// Windows 98
				str += "98";
			else
				// Windows 95
				str += "95";
		}
		else
			// Windows NT
			str += "NT";
	}
	else if (OSVerInfo.dwMajorVersion == 5)
	{
		if (OSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
			// Windows Millenium
			str += "ME";
		else
		{
			if (OSVerInfo.dwMinorVersion > 0)
				// Windows XP
				str += "XP";
			else
				// Windows 2000
				str += "2000";
		}
	}
	else if (OSVerInfo.dwMajorVersion == 6)
	{
		//if (OSVerInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		// Windows Vista
		str += "Vista";
	}
	sprintf(szBuffer, " %d.%d", OSVerInfo.dwMajorVersion, OSVerInfo.dwMinorVersion);
	str += szBuffer;

	//////////////////////////////////////////////////////////////////////
	// Show Windows directory
	//////////////////////////////////////////////////////////////////////

	str += " (";
	GetWindowsDirectory(szBuffer, sizeof(szBuffer));
	str += szBuffer;
	str += ")";
	CryLog(str);
	
	//////////////////////////////////////////////////////////////////////
	// Send system time & date
	//////////////////////////////////////////////////////////////////////

	str = "Local time is ";
	_strtime(szBuffer);
	str += szBuffer;
	str += " ";
	_strdate(szBuffer);
	str += szBuffer;
	sprintf(szBuffer, ", system running for %d minutes", GetTickCount() / 60000);
	str += szBuffer;
	CryLog(str);
	
	//////////////////////////////////////////////////////////////////////
	// Send system CPU informations
	//////////////////////////////////////////////////////////////////////

	/*
	GetCPUModel(szCPUModel);
#ifdef _DEBUG
	sprintf(szBuffer, "System CPU is an %s running at %d Mhz", 
		szCPUModel, GetCPUSpeed());
#else
	sprintf(szBuffer, "System CPU is an %s, frequency only measured in debug versions", 
		szCPUModel);
#endif
	LogLine(szBuffer);
	*/

	//////////////////////////////////////////////////////////////////////
	// Send system memory status
	//////////////////////////////////////////////////////////////////////

	GlobalMemoryStatus(&MemoryStatus);
	sprintf(szBuffer, "%dMB phys. memory installed, %dMB paging available", 
		MemoryStatus.dwTotalPhys / 1048576 + 1, 
		MemoryStatus.dwAvailPageFile / 1048576);
	CryLog(szBuffer);

	//////////////////////////////////////////////////////////////////////
	// Send display settings
	//////////////////////////////////////////////////////////////////////

	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DisplayConfig);
	GetPrivateProfileString("boot.description", "display.drv", 
		"(Unknown graphics card)", szProfileBuffer, sizeof(szProfileBuffer), 
		"system.ini");
	sprintf(szBuffer, "Current display mode is %dx%dx%d, %s",
		DisplayConfig.dmPelsWidth, DisplayConfig.dmPelsHeight,
		DisplayConfig.dmBitsPerPel, szProfileBuffer);
	CryLog(szBuffer);

	//////////////////////////////////////////////////////////////////////
	// Send input device configuration
	//////////////////////////////////////////////////////////////////////

	str = "";
	// Detect the keyboard type
	switch (GetKeyboardType(0))
	{
		case 1:
			str = "IBM PC/XT (83-key)";
			break;
		case 2:
			str = "ICO (102-key)";
			break;
		case 3:
			str = "IBM PC/AT (84-key)";
			break;
		case 4:
			str = "IBM enhanced (101/102-key)";
			break;
		case 5:
			str = "Nokia 1050";
			break;
		case 6:
			str = "Nokia 9140";
			break;
		case 7:
			str = "Japanese";
			break;
		default:
			str = "Unknown";
			break;
	}

	// Any mouse attached ?
	if (!GetSystemMetrics(SM_MOUSEPRESENT))
		CryLog( str + " keyboard and no mouse installed");
	else
	{
		sprintf(szBuffer, " keyboard and %i+ button mouse installed", 
			GetSystemMetrics(SM_CMOUSEBUTTONS));
		CryLog( str + szBuffer);
	}

	CryLog("--------------------------------------------------------------------------------");
}

unsigned int CLogFile::GetCPUSpeed()
{
	//////////////////////////////////////////////////////////////////////
	// Return system CPU speed in Mhz (Code by Frank Blaha)
	//////////////////////////////////////////////////////////////////////

	/*
	Basic goal is use high precision performance counter.
	Reason is described in VC HELP as follow:

	Each time the specified interval (or time-out value) 
	for a timer elapses, the system notifies the window associated 
	with the timer. 

	Because the accuracy of a timer depends on the system clock 
	rate and how often the application retrieves messages from 
	the message queue, the time-out value is only approximate. 
	If you need a timer with higher precision, use the high-resolution 
	timer. For more information, see High-Resolution Timer. 
	If you need to be notified when a timer elapses, 
	use the waitable timers. 
	For more information on these, see Waitable Timer Objects. 

	About RDSTC instruction:

	Beginning with the Pentium« processor, Intel processors allow the 
	programmer to access a time-stamp counter. 
	The time-stamp counter keeps an accurate count of every cycle 
	that occurs on the processor. The Intel time-stamp counter is a 
	64-bit MSR (model specific register) that is incremented every 
	clock cycle. 
	On reset, the time-stamp counter is set to zero.

	To access this counter, programmers can use the 
	RDTSC (read time-stamp counter) instruction. 
	This instruction loads the high-order 32 bits of 
	the register into EDX, and the low-order 32 bits into EAX.

	Remember that the time-stamp counter measures "cycles" and not "time". 
	For example, two hundred million cycles on a 200 MHz processor is equivalent
	to one second of real time, while the same number of cycles on a 400 MHz
	processor is only one-half second of real time. Thus, comparing cycle counts
	only makes sense on processors of the same speed. To compare processors of
	different speeds, the cycle counts should be converted into time units, where: 

	# seconds = # cycles / frequency

	Note: frequency is given in Hz, where: 1,000,000 Hz = 1 MHz

	*/

	/*
	LARGE_INTEGER   ulFreq,			// No. ticks/s ( frequency). 
									// This is exactly 1 second interval
					ulTicks,		// Curent value of ticks 
					ulValue,		// for calculation, how many tick is system
									// (depend on instaled HW) able    
					ulStartCounter, // Start No. of processor counter
					ulEAX_EDX,		// We need 64 bits value and it is stored in EAX, EDX registry
					ulResult;		// Calculate result of "measuring"

	// Function retrieves the frequency of the high-resolution performance counter (HW not CPU)   
	// It is number of ticks per second       
	QueryPerformanceFrequency(&ulFreq); 	   

	// Current value of the performance counter        
    QueryPerformanceCounter(&ulTicks);   

	// Calculate one sec interval 
	// ONE SEC interval  = start nuber of the ticks + # of ticks/s
	// Loop (do..while statement bellow) until actual # of ticks this number is <= then 1 sec

	ulValue.QuadPart = ulTicks.QuadPart + ulFreq.QuadPart;    

	// (read time-stamp counter) instruction.    
	// This asm instruction loads the high-order 32 bits of the register into EDX, and the low-order 32 bits into EAX.      

	__asm RDTSC     

	// Load 64-bits counter from registry to LARGE_INTEGER variable (take a look to HELP)
	__asm mov ulEAX_EDX.LowPart, EAX         
	__asm mov ulEAX_EDX.HighPart, EDX       

	// Starting number of processor ticks    

	ulStartCounter.QuadPart = ulEAX_EDX.QuadPart;                 

	// Loop for 1 sec and  check ticks        
	// this is descibed bellow
	do
	{	         
		// Just read actual HW counter
		QueryPerformanceCounter(&ulTicks); 
	} while (ulTicks.QuadPart <= ulValue.QuadPart);

	// Get actual number of processor ticks      
	__asm RDTSC       

	__asm mov ulEAX_EDX.LowPart, EAX        
	__asm mov ulEAX_EDX.HighPart,EDX       

	// Calculate result from current processor ticks count
	ulResult.QuadPart = ulEAX_EDX.QuadPart - ulStartCounter.QuadPart;     

	// Return the value
	return (unsigned int) ulResult.QuadPart / 1000000;
	*/
	return 0;
}

void CLogFile::GetCPUModel(char szType[])
{
	/*
	//////////////////////////////////////////////////////////////////////
	// Get the CPU Model
	//////////////////////////////////////////////////////////////////////

	int CPUfamily,InstCache,DataCache,L2Cache;
	char VendorID[16];
	bool SupportCMOVs,Support3DNow,Support3DNowExt,SupportMMX,SupportMMXext,SupportSSE;

	#ifndef CPUID
	#define CPUID __asm _emit 0x0F __asm _emit 0xA2
	#endif
	#ifndef RDTSC
	#define RDTSC __asm _emit 0x0F __asm _emit 0x31
	#endif

	__asm {
		PUSHFD
		POP		EAX
		MOV		EBX, EAX
		XOR		EAX, 00200000h
		PUSH	EAX
		POPFD
		PUSHFD
		POP		EAX
		CMP		EAX, EBX
		JZ		ExitCpuTest

			XOR		EAX, EAX
			CPUID

			MOV		DWORD PTR [VendorID],		EBX
			MOV		DWORD PTR [VendorID + 4],	EDX
			MOV		DWORD PTR [VendorID + 8],	ECX
			MOV		DWORD PTR [VendorID + 12],	0

			MOV		EAX, 1
			CPUID
			TEST	EDX, 0x00008000
			SETNZ	AL
			MOV		SupportCMOVs, AL
			TEST	EDX, 0x00800000
			SETNZ	AL
			MOV		SupportMMX, AL
	
			TEST	EDX, 0x02000000
			SETNZ	AL
			MOV		SupportSSE, AL

			SHR		EAX, 8
			AND		EAX, 0x0000000F
			MOV		CPUfamily, EAX
	
			MOV		Support3DNow, 0
			MOV		EAX, 80000000h
			CPUID
			CMP		EAX, 80000000h
			JBE		NoExtendedFunction
				MOV		EAX, 80000001h
				CPUID
				TEST	EDX, 80000000h
				SETNZ	AL
				MOV		Support3DNow, AL

				TEST	EDX, 40000000h
				SETNZ	AL
				MOV		Support3DNowExt, AL

				TEST	EDX, 0x00400000
				SETNZ	AL
				MOV		SupportMMXext, AL

				MOV		EAX, 80000005h
				CPUID
				SHR		ECX, 24
				MOV		DataCache, ECX
				SHR		EDX, 24
				MOV		InstCache, EDX
				
				MOV		EAX, 80000006h
				CPUID
				SHR		ECX, 16
				MOV		L2Cache, ECX

				
			JMP		ExitCpuTest

			NoExtendedFunction:
			MOV		EAX, 2
			CPUID

			MOV		ESI, 4
			TestCache:
				CMP		DL, 0x40
				JNA		NotL2
					MOV		CL, DL
					SUB		CL, 0x40
					SETZ	CH
					DEC		CH
					AND		CL, CH
					MOV		EBX, 64
					SHL		EBX, CL
					MOV		L2Cache, EBX
				NotL2:
				CMP		DL, 0x06
				JNE		Next1
					MOV		InstCache, 8
				Next1:
				CMP		DL, 0x08
				JNE		Next2
					MOV		InstCache, 16
				Next2:
				CMP		DL, 0x0A
				JNE		Next3
					MOV		DataCache, 8
				Next3:
				CMP		DL, 0x0C
				JNE		Next4
					MOV		DataCache, 16
				Next4:
				SHR		EDX, 8
				DEC		ESI
			JNZ	TestCache

		ExitCpuTest:
	}

	// Build the CPU ID string
	sprintf(szType, "%s (%s%s%s)", VendorID, 
		(SupportMMX || SupportMMXext) ? "MMX, " : "No MMX, ",
		(Support3DNow || Support3DNowExt) ? "3DNow!, " : "No 3DNow!, ",
		SupportSSE ? "SSE" : "No SSE");
		*/
}

//////////////////////////////////////////////////////////////////////////
CString CLogFile::GetMemUsage()
{
	ProcessMemInfo mi;
	CProcessInfo::QueryMemInfo( mi );
	int MB = 1024*1024;

	CString str;
	str.Format( "Memory=%dMb, Pagefile=%dMb",mi.WorkingSet/MB,mi.PagefileUsage/MB );
	//FormatLine( "PeakWorkingSet=%dMb, PeakPagefileUsage=%dMb",pc.PeakWorkingSetSize/MB,pc.PeakPagefileUsage/MB );
	//FormatLine( "PagedPoolUsage=%d",pc.QuotaPagedPoolUsage/MB );
	//FormatLine( "NonPagedPoolUsage=%d",pc.QuotaNonPagedPoolUsage/MB );
	
	return str;
}

//////////////////////////////////////////////////////////////////////////
void CLogFile::WriteLine( const char * pszString )
{
	CryLog( pszString );
}

//////////////////////////////////////////////////////////////////////////
void CLogFile::WriteString( const char * pszString )
{
	gEnv->pLog->LogPlus( pszString );
}

static inline CString CopyAndRemoveColorCode( const char *sText )
{
	CString ret=sText;		// alloc and copy

	char *s,*d;
	
	s=ret.GetBuffer();d=s;

	// remove color code in place
	while(*s!=0)
	{
		if(*s=='$' && *(s+1)>='0' && *(s+1)<='9')
		{
			s+=2;
			continue;
		}

		*d++=*s++;
	}

	ret.ReleaseBuffer(d-ret.GetBuffer());

	return ret;
}

//////////////////////////////////////////////////////////////////////////
void CLogFile::OnWriteToConsole( const char *sText,bool bNewLine )
{
	if (!gEnv)
		return;
//	if (GetIEditor()->IsInGameMode())
	//	return;

	// Skip any non character.
	if (*sText != 0 && ((uchar)*sText) < 32)
		sText++;

	CString str = CopyAndRemoveColorCode(sText);		// editor prinout doesn't support color coded log messages

	if (m_bShowMemUsage)
	{
		str = CString("(") + GetMemUsage() + ")" + str;
	}

	// If we have a listbox attached, also output the string to this listbox
	if (m_hWndListBox)
	{

		// Add the string to the listbox
		SendMessage(m_hWndListBox, LB_ADDSTRING, 0, (LPARAM) (const char*)str );

		// Make sure the recently added string is visible
		SendMessage(m_hWndListBox, LB_SETTOPINDEX, 
		SendMessage(m_hWndListBox, LB_GETCOUNT, 0, 0) - 1, 0);
	}

	//bool bNewLine = str[0] == '\n';

	if (m_hWndEditBox)
	{
		// remember selection and the top row
		int len = ::GetWindowTextLength( m_hWndEditBox );
		int top, from, to;
		SendMessage( m_hWndEditBox, EM_GETSEL, (WPARAM)&from, (LPARAM)&to );
		bool keepPos = false;
		bool locking = false;

		if ( from!=len || to!=len )
		{
			keepPos = GetFocus() == m_hWndEditBox;
			if ( keepPos )
			{
				top = SendMessage( m_hWndEditBox, EM_GETFIRSTVISIBLELINE, 0, 0 );
				locking = bNewLine && ::IsWindowVisible( m_hWndEditBox );
				if ( locking )
					::LockWindowUpdate( m_hWndEditBox );
			}
			SendMessage( m_hWndEditBox, EM_SETSEL, len, len );
		}
		if ( bNewLine )
		{
			//str = CString("\r\n") + str.TrimLeft();
			//str = CString("\r\n") + str;
			//str = CString("\r") + str;
			str.TrimRight();
			str = CString("\r\n") + str;
		}
		const char *szStr = str;
		SendMessage( m_hWndEditBox, EM_REPLACESEL, FALSE, (LPARAM)szStr );

		// restore selection and the top line
		if ( keepPos )
		{
			SendMessage( m_hWndEditBox, EM_SETSEL, from, to );
			top -= SendMessage( m_hWndEditBox, EM_GETFIRSTVISIBLELINE, 0, 0 );
			SendMessage( m_hWndEditBox, EM_LINESCROLL, 0, (LPARAM)top );
		}

		if ( locking )
			::LockWindowUpdate( NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CLogFile::OnWriteToFile( const char *sText,bool bNewLine )
{
}