#include "StdAfx.h"

#include "AILog.h"
#include "CAISystem.h"
#include "CryAISystem.h"

#include "ISystem.h"
#include "ITimer.h"
#include "IValidator.h"
#include "IConsole.h"
#include "IRenderer.h"
#include "ITestSystem.h"

// these should all be in sync - so testing one for 0 should be the same for all
ISystem *pSystem = 0;
ICVar * pAILogConsoleVerbosity = 0;
ICVar *	pAILogFileVerbosity = 0;
ICVar *	pAIEnableAsserts = 0;
ICVar *	pAIEnableWarningsErrors = 0;
ICVar * pAIOverlayMessageDuration = 0;

static const char outputPrefix[] = "AI: ";
static const unsigned outputPrefixLen = sizeof(outputPrefix) - 1;
static char outputBufferLog[MAX_WARNING_LENGTH + outputPrefixLen - 512];
static unsigned outputBufferSize = sizeof(outputBufferLog);

static const int maxSavedMsgs = 5;
static const int maxSavedMsgLength = MAX_WARNING_LENGTH + outputPrefixLen + 1;
enum ESavedMsgType {SMT_WARNING, SMT_ERROR};
struct SSavedMsg
{
	ESavedMsgType savedMsgType;
	char savedMsg[maxSavedMsgLength];
	CTimeValue time;
};
static SSavedMsg savedMsgs[maxSavedMsgs];
static int savedMsgIndex = 0;

enum AI_LOG_VERBOSITY
{
	LOG_OFF = 0,
	LOG_PROGRESS = 1,
	LOG_EVENT = 2,
	LOG_COMMENT = 3
};

//====================================================================
// DebugDrawLabel
//====================================================================
static void DebugDrawLabel(IRenderer *pRenderer, ESavedMsgType type, float timeFrac, int col, int row, const char* szText)
{
	float ColumnSize = 11;
	float RowSize = 11;
	float baseY = 10;
	float colorWarning[4] = { 0,1,1,1 };
	float colorError[4] = { 1,1,0,1 };
	float* color = type == SMT_ERROR ? colorError : colorWarning;

	float alpha = 1.0f;
	static float fadeFrac = 0.5f;
	if (timeFrac < fadeFrac && fadeFrac > 0.0f)
		alpha = timeFrac / fadeFrac;
	color[3] = alpha;

	float actualCol = ColumnSize*static_cast<float>(col);
	float actualRow;
	if (row >= 0)
		actualRow = baseY+RowSize*static_cast<float>(row);
	else
		actualRow = pRenderer->GetHeight() - (baseY+RowSize*static_cast<float>(-row));

	pRenderer->Draw2dLabel( actualCol, actualRow, 1.2f, color, false,"%s",szText );
}

//====================================================================
// DisplaySavedMsgs
//====================================================================
void AILogDisplaySavedMsgs(IRenderer *pRenderer)
{
  if (!pAIOverlayMessageDuration)
    return;
  float savedMsgDuration = pAIOverlayMessageDuration->GetFVal();
  if (savedMsgDuration < 0.01f)
    return;
  static int col = 1;

  int row = -1;
  CTimeValue currentTime = gEnv->pTimer->GetFrameStartTime();
  CTimeValue time = currentTime - CTimeValue(savedMsgDuration);
  for (int i = 0 ; i < maxSavedMsgs ; ++i)
  {
    int index = (maxSavedMsgs + savedMsgIndex - i) % maxSavedMsgs;
    if (savedMsgs[index].time < time)
      return;
    // get rid of msgs from the future - can happen during load/save
    if (savedMsgs[index].time > currentTime)
			savedMsgs[index].time = time;
//      savedMsgIndex = (maxSavedMsgs + savedMsgIndex - 1) % maxSavedMsgs;

    float timeFrac = (savedMsgs[index].time - time).GetSeconds() / savedMsgDuration;
    DebugDrawLabel(pRenderer, savedMsgs[index].savedMsgType, timeFrac, col, row, savedMsgs[index].savedMsg);
    --row;
  }
}

//====================================================================
// AIInitLog
//====================================================================
void AIInitLog(ISystem * system)
{
	if (pSystem)
		AIWarning("Re-registering AI Logging");

	AIAssert(system);
	if (!system)
		return;
	IConsole *console = system->GetIConsole();

  bool inDevMode = IsAIInDevMode();
#ifdef _DEBUG
  int isDebug = 1;
#else
  int isDebug = 0;
#endif

	if(console)
	{
		pSystem = system;
		if (!pAILogConsoleVerbosity)
      pAILogConsoleVerbosity = console->RegisterInt("ai_LogConsoleVerbosity", inDevMode ? LOG_PROGRESS : LOG_OFF, VF_DUMPTODISK, "None = 0, progress = 1, event = 2, comment = 3");
		if (!pAILogFileVerbosity)
      pAILogFileVerbosity = console->RegisterInt("ai_LogFileVerbosity", inDevMode ? LOG_EVENT : LOG_PROGRESS, VF_DUMPTODISK, "None = 0, progress = 1, event = 2, comment = 3");
		if (!pAIEnableAsserts)
      pAIEnableAsserts = console->RegisterInt("ai_EnableAsserts", inDevMode ? 1 : isDebug, VF_DUMPTODISK, "Enable AI asserts: 1 or 0");
		if (!pAIEnableWarningsErrors)
			pAIEnableWarningsErrors = console->RegisterInt("ai_EnableWarningsErrors", 1, VF_DUMPTODISK, "Enable AI warnings and errors: 1 or 0");
    if (!pAIOverlayMessageDuration)
			pAIOverlayMessageDuration = console->RegisterFloat("ai_OverlayMessageDuration", 5.0f, VF_DUMPTODISK, "How long (seconds) to overlay AI warnings/errors");
	}

	for (int i = 0 ; i < maxSavedMsgs ; ++i)
	{
		savedMsgs[i].savedMsg[0] = '\0';
		savedMsgs[i].savedMsgType = SMT_WARNING;
		savedMsgs[i].time = CTimeValue(0.0f);
		savedMsgIndex = 0;
	}
}

//====================================================================
// AIGetLogConsoleVerbosity
//====================================================================
int AIGetLogConsoleVerbosity()
{
	if (pAILogConsoleVerbosity)
		return pAILogConsoleVerbosity->GetIVal();
	else
		return -1;
}

//====================================================================
// AIGetLogFileVerbosity
//====================================================================
int AIGetLogFileVerbosity()
{
	if (pAILogFileVerbosity)
		return pAILogFileVerbosity->GetIVal();
	else
		return -1;
}

//====================================================================
// AIGetAssertEnabled
//====================================================================
bool AIGetAssertEnabled()
{
	if (pAIEnableAsserts)
		return pAIEnableAsserts->GetIVal() != 0;
	else 
		return true;
}

//===================================================================
// AIGetWarningErrorsEnabled
//===================================================================
bool AIGetWarningErrorsEnabled()
{
	if (pAIEnableWarningsErrors)
		return pAIEnableWarningsErrors->GetIVal() != 0;
	else 
		return true;
}

//====================================================================
// AIWarning
//====================================================================
void AIWarning( const char *format,... )
{
	if (!pSystem) 
		return;
  if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
    return;

	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog, outputBufferSize, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);
	pSystem->Warning( VALIDATOR_MODULE_AI, VALIDATOR_WARNING, VALIDATOR_FLAG_AI, 0, "AI: Warning: %s", outputBufferLog );

	savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	savedMsgs[savedMsgIndex].savedMsgType = SMT_WARNING;
	strncpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog, maxSavedMsgLength);
	savedMsgs[savedMsgIndex].savedMsg[maxSavedMsgLength-1] = '\0';
	savedMsgs[savedMsgIndex].time = gEnv->pTimer->GetFrameStartTime();
}

//====================================================================
// AILogAlways
//====================================================================
void AILogAlways(const char *format, ...)
{
	if (!pSystem) 
		return;

	strncpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog + outputPrefixLen, outputBufferSize - outputPrefixLen, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);

	pSystem->GetILog()->Log(outputBufferLog);
}

void AILogLoading(const char *format, ...)
{
  if (!pSystem) 
    return;

  strncpy(outputBufferLog, outputPrefix, outputPrefixLen);
  va_list args;
  va_start(args, format);
  vsnprintf(outputBufferLog + outputPrefixLen, outputBufferSize - outputPrefixLen, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
  va_end(args);

  pSystem->GetILog()->UpdateLoadingScreen(outputBufferLog);
}


//====================================================================
// AILogProgress
//====================================================================
void AILogProgress(const char *format, ...)
{
	if (!pSystem) 
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_PROGRESS && fV < LOG_PROGRESS)
		return;

	strncpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog + outputPrefixLen, outputBufferSize - outputPrefixLen, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);

	if ( (cV >= LOG_PROGRESS) && (fV >= LOG_PROGRESS) )
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_PROGRESS)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_PROGRESS)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

//====================================================================
// AILogEvent
//====================================================================
void AILogEvent(const char *format, ...)
{
	if (!pSystem) 
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_EVENT && fV < LOG_EVENT)
		return;

	strncpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog + outputPrefixLen, outputBufferSize - outputPrefixLen, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);

	if ( (cV >= LOG_EVENT) && (fV >= LOG_EVENT) )
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_EVENT)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_EVENT)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

//====================================================================
// AILogComment
//====================================================================
void AILogComment(const char *format, ...)
{
	if (!pSystem) 
		return;
	int cV = pAILogConsoleVerbosity->GetIVal();
	int fV = pAILogFileVerbosity->GetIVal();
	if (cV < LOG_COMMENT && fV < LOG_COMMENT)
		return;

	strncpy(outputBufferLog, outputPrefix, outputPrefixLen);
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog + outputPrefixLen, outputBufferSize - outputPrefixLen, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);

	if ( (cV >= LOG_COMMENT) && (fV >= LOG_COMMENT) )
		pSystem->GetILog()->Log(outputBufferLog);
	else if (cV >= LOG_COMMENT)
		pSystem->GetILog()->LogToConsole(outputBufferLog);
	else if (fV >= LOG_COMMENT)
		pSystem->GetILog()->LogToFile(outputBufferLog);
}

// for assert and error - we want a message box
#ifdef WIN32
#include <windows.h>
#include <signal.h>
#define BOXINTRO    "AI Assertion failed!"
#define PROGINTRO   "Program: "
#define FILEINTRO   "File: "
#define LINEINTRO   "Line: "
#define EXPRINTRO   "Expression: "
#define HELPINTRO   "(Press Retry to debug the application - JIT must be enabled)\n" \
                    "AI asserts can be temporarily disabled using the ai_EnableAsserts console variable"

static char * dotdotdot = "...";
static char * newline = "\n";
static char * dblnewline = "\n\n";

#define DOTDOTDOTSZ 3
#define NEWLINESZ   1
#define DBLNEWLINESZ   2

#define MAXLINELEN  60 /* max length for line in message box */
#define ASSERTBUFSZ (MAXLINELEN * 9) /* 9 lines in message box */

#if defined (_M_IX86)
#define _DbgBreak() __asm { int 3 }
#elif defined (_M_ALPHA)
void _BPT();
#pragma intrinsic(_BPT)
#define _DbgBreak() _BPT()
#elif defined (_M_IA64)
void __break(int);
#pragma intrinsic (__break)
#define _DbgBreak() __break(0x80016)
#else  /* defined (_M_IA64) */
#define _DbgBreak() DebugBreak()
#endif  /* defined (_M_IA64) */

//====================================================================
// AIAssertHit
// Crib MSVC implementation a bit
//====================================================================
void AIAssertHit(const char * expr, const char * filename, unsigned lineno)
{
	// allow a way to use the debugger to skip asserts when they hit repeatedly
	static bool skipAsserts = false;
	if (skipAsserts)
		return;

	int nCode;
	char * pch;
	char assertbuf[ASSERTBUFSZ];
	char progname[MAX_PATH + 1];

	// Make sure this gets logged even if the user ignores it:
	AIWarning("Assert: %s is false at %s:%d", expr, filename, lineno);

	// Line 1: box intro line
	strcpy( assertbuf, BOXINTRO );
	strcat( assertbuf, dblnewline );

	// Line 2: program line
	strcat( assertbuf, PROGINTRO );

	progname[MAX_PATH] = '\0';
	if ( !GetModuleFileName( NULL, progname, MAX_PATH ))
		strcpy( progname, "<program name unknown>");

	pch = (char *)progname;

	// sizeof(PROGINTRO) includes the NULL terminator
	if ( sizeof(PROGINTRO) + strlen(progname) + NEWLINESZ > MAXLINELEN )
	{
		pch += (sizeof(PROGINTRO) + strlen(progname) + NEWLINESZ) - MAXLINELEN;
		strncpy( pch, dotdotdot, DOTDOTDOTSZ );
	}

	strcat( assertbuf, pch );
	strcat( assertbuf, newline );

	// Line 3: file line
	strcat( assertbuf, FILEINTRO );

	// sizeof(FILEINTRO) includes the NULL terminator
	if ( sizeof(FILEINTRO) + strlen(filename) + NEWLINESZ > MAXLINELEN )
	{
		size_t p, len, ffn;

		pch = (char *) filename;
		ffn = MAXLINELEN - sizeof(FILEINTRO) - NEWLINESZ;

		for ( len = strlen(filename), p = 1;
			pch[len - p] != '\\' && pch[len - p] != '/' && p < len;
			p++ );

		// keeping pathname almost 2/3rd of full filename and rest
		// is filename
		if ( (ffn - ffn/3) < (len - p) && ffn/3 > p )
		{
			// too long. using first part of path and the filename string
			strncat( assertbuf, pch, ffn - DOTDOTDOTSZ - p );
			strcat( assertbuf, dotdotdot );
			strcat( assertbuf, pch + len - p );
		}
		else if ( ffn - ffn/3 > len - p )
		{
			// pathname is smaller. keeping full pathname and putting
			// dotdotdot in the middle of filename
			p = p/2;
			strncat( assertbuf, pch, ffn - DOTDOTDOTSZ - p );
			strcat( assertbuf, dotdotdot );
			strcat( assertbuf, pch + len - p );
		}
		else
		{
			// both are long. using first part of path. using first and
			// last part of filename.
			strncat( assertbuf, pch, ffn - ffn/3 - DOTDOTDOTSZ );
			strcat( assertbuf, dotdotdot );
			strncat( assertbuf, pch + len - p, ffn/6 - 1 );
			strcat( assertbuf, dotdotdot );
			strcat( assertbuf, pch + len - (ffn/3 - ffn/6 - 2) );
		}
	}
	else
		// plenty of room on the line, just append the filename 
		strcat( assertbuf, filename );

	strcat( assertbuf, newline );

	// Line 4: line line
	strcat( assertbuf, LINEINTRO );
	_itoa( lineno, assertbuf + strlen(assertbuf), 10 );
	strcat( assertbuf, dblnewline );

	// Line 5: message line
	strcat( assertbuf, EXPRINTRO );

	// sizeof(HELPINTRO) includes the NULL terminator
	if ( strlen(assertbuf) +
		strlen(expr) +
		2*DBLNEWLINESZ +
		sizeof(HELPINTRO) > ASSERTBUFSZ )
	{
		strncat( assertbuf, expr,
			ASSERTBUFSZ -
			(strlen(assertbuf) +
			DOTDOTDOTSZ +
			2*DBLNEWLINESZ +
			sizeof(HELPINTRO)) );
		strcat( assertbuf, dotdotdot );
	}
	else
		strcat( assertbuf, expr );

	strcat( assertbuf, dblnewline );

	// Line 6: help line
	strcat(assertbuf, HELPINTRO);

	// Write out via MessageBox
	nCode = MessageBox(
		0,
		assertbuf,
		"AI system assert hit",
		MB_ABORTRETRYIGNORE|MB_ICONHAND|MB_SETFOREGROUND|MB_TASKMODAL|MB_DEFBUTTON2);

	// Abort: abort the program
	if (nCode == IDABORT)
	{
		// move the execution pointer outside of this block to prevent accidentally pressed A key
		assert( !"WARNING: Closing debug session!" );

		// raise abort signal
		raise(SIGABRT);

		// We usually won't get here, but it's possible that
		// SIGABRT was ignored.  So exit the program anyway.
		_exit(3);
	}

	// Retry: call the debugger 
	if (nCode == IDRETRY)
	{
		_DbgBreak();
		// return to user code
		return;
	}

	// Ignore: continue execution
	if (nCode == IDIGNORE)
		return;

	abort();
}

//====================================================================
// AIError
//====================================================================
void AIError( const char *format,... )
{
	if (!pSystem) 
		return;
  if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
    return;
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog, outputBufferSize, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);
	pSystem->Warning( VALIDATOR_MODULE_AI, VALIDATOR_ERROR, VALIDATOR_FLAG_AI, 0, "AI: Error: %s", outputBufferLog );

	savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	savedMsgs[savedMsgIndex].savedMsgType = SMT_ERROR;
	strncpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog, maxSavedMsgLength);
	savedMsgs[savedMsgIndex].savedMsg[maxSavedMsgLength-1] = '\0';
	savedMsgs[savedMsgIndex].time = gEnv->pTimer->GetFrameStartTime();

  if(GetISystem()->GetITestSystem())
  {
    // supress user interaction for automated test
		if (GetISystem()->GetITestSystem()->GetILog())
			GetISystem()->GetITestSystem()->GetILog()->LogError(outputBufferLog);
  }
  else
  {
	  static bool sDoMsgBox = true;
	  if (sDoMsgBox && !pSystem->IsEditor())
	  {
		  static char msg[4096];
		  sprintf(msg, 
			  "AI: %s \n"
			  "ABORT to abort execution\n"
			  "RETRY to continue (Don't expect AI to work properly)\n"
			  "IGNORE to continue without any more of these AI error msg boxes", outputBufferLog);
		  // Write out via MessageBox
		  int nCode = MessageBox(
			  0,
			  msg,
			  "AI Error",
			  MB_ABORTRETRYIGNORE|MB_ICONHAND|MB_SETFOREGROUND|MB_SYSTEMMODAL);

		  // Abort: abort the program
		  if (nCode == IDABORT)
			  pSystem->Error(" AI: %s", outputBufferLog);
		  else if (nCode == IDIGNORE)
			  sDoMsgBox = false;
	  }
  }
}

#else // WIN32

void AIAssertHit(const char * expr, const char * filename, unsigned lineno)
{
}

//====================================================================
// AIError
//====================================================================
void AIError( const char *format,... )
{
	if (!pSystem) 
		return;
  if (pAIEnableWarningsErrors && 0 == pAIEnableWarningsErrors->GetIVal())
    return;
	va_list args;
	va_start(args, format);
	vsnprintf(outputBufferLog, outputBufferSize, format, args);
  outputBufferLog[outputBufferSize-1] = '\0';
	va_end(args);
	pSystem->Warning( VALIDATOR_MODULE_AI, VALIDATOR_ERROR, VALIDATOR_FLAG_AI, 0, "AI: Error: %s", outputBufferLog );

	savedMsgIndex = (savedMsgIndex + 1) % maxSavedMsgs;
	savedMsgs[savedMsgIndex].savedMsgType = SMT_ERROR;
	strncpy(savedMsgs[savedMsgIndex].savedMsg, outputBufferLog, maxSavedMsgLength);
	savedMsgs[savedMsgIndex].savedMsg[maxSavedMsgLength-1] = '\0';
	savedMsgs[savedMsgIndex].time = gEnv->pTimer->GetFrameStartTime();
}



#endif  // WIN32