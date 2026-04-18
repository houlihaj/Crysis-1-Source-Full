/****************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-----------------------------------------------------------------------------
$Id$
$DateTime$
Description:
Launcher implementation for the linux client and the linux dedicated server.
-----------------------------------------------------------------------------
History:
- Jul 07, 2006:	Created by Sascha Demetrio
****************************************************************************/

#include "StdAfx.h"

//#include <ISystem.h>
//#include "../../CryEngine/Cry3DEngine/StdAfx.h"

#include <platform_impl.h>

#if defined(LINUX32)
#include <Linux32Specific.h>
#elif defined(LINUX64)
#include <Linux64Specific.h>
#endif
#if !defined(DEDICATED_SERVER)
#include <SDL/SDL.h>
#endif

#define USE_UNIXCONSOLE 1
#include <UnixConsole.h>

#include <IGameStartup.h>
#include <IEntity.h>
#include <IGameFramework.h>
#include <IConsole.h>

#include <sys/types.h>
#include <netdb.h>

// FIXME: get the correct version string from somewhere, maybe a -D supplied
// by the build environment?
#undef VERSION_STRING
#define VERSION_STRING "0.0.1"

size_t linux_autoload_level_maxsize = PATH_MAX;
char linux_autoload_level_buf[PATH_MAX];
char *linux_autoload_level = linux_autoload_level_buf;

//////////////////////////////////////////////////////////////////////////
/* XXX
int sEmptyStringBuffer[] = { -1, 0, 0, 0 };
template<>
string::StrHeader* string::m_emptyStringData = (string::StrHeader*)&sEmptyStringBuffer;
template<>
wstring::StrHeader* wstring::m_emptyStringData = (wstring::StrHeader*)&sEmptyStringBuffer;
*/

//ISystem* g_pSystem = 0;
//ISystem *GetISystem()
//{
//	return g_pSystem;
//}

extern CUNIXConsole g_UnixConsole;

// Handle failed assertions.

static CryFastLock g_AssertFailLock;
static std::vector<string> g_AssertIgnore;

void AssertFail(
		const char *assertion,
		int errnum,
		const char *file,
		unsigned int line,
		const char *function)
{
#if !defined(NDEBUG)
	g_AssertFailLock.Lock();

	// Check if the assertion is in the ignore list.
	char key[256];
	snprintf(key, sizeof key, "%s, line %u", file, line);
	key[sizeof key - 1] = 0;
	std::vector<string>::const_iterator
		it = g_AssertIgnore.begin(),
		itEnd = g_AssertIgnore.end();
	for (; it != itEnd; ++it)
	{
		if (!strcmp(key, it->c_str()))
		{
			// Assertion ignored.
			g_AssertFailLock.Unlock();
			return;
		}
	}

	if (g_UnixConsole.IsInitialized())
	{
		g_UnixConsole.PrintF("$4Failed assertion in %s, line %u", file, line);
		g_UnixConsole.PrintF("  Function: %s", function);
		if (assertion != NULL)
			g_UnixConsole.PrintF("  Assertion: %s", assertion);
		else
			g_UnixConsole.PrintF("  Error: %i (%s)", errnum, strerror(errnum));
		if (g_UnixConsole.IsInputThread())
		{
			g_UnixConsole.PrintF("$4Ignored$0 - called from the console input thread");
			return;
		}
		//*
		char action = g_UnixConsole.Prompt(
				"Action? [a=abort/i=ignore/I=ignore all] ", "aiI");
		/*/
		char action = 'I';		
		//*/
		switch (action)
		{
		case 'i':
			g_AssertFailLock.Unlock();
			return;
		case 'I':
			g_AssertIgnore.push_back(key);
			g_UnixConsole.PrintF(
					"Ignoring failed assertions from %s, line %u", file, line);
			g_AssertFailLock.Unlock();
			return;
		default:
			abort();
		}
	}
	else
	{
		fprintf(stderr,
				"Failed assertion in %s, line %u\n"
				"  Function: %s\n",
				file, line,
				function);
		if (assertion != NULL)
			fprintf(stderr, "  Assertion: %s\n", assertion);
		else
			fprintf(stderr, "  Error: %i (%s)\n", errnum, strerror(errnum));
		while (true)
		{
		  fprintf(stderr, "Action? [a=abort/i=ignore/I=ignore all] ");
			fflush(stderr);
			char action = getchar();
			switch (action)
			{
			case 'i':
				g_AssertFailLock.Unlock();
				return;
			case 'I':
				g_AssertIgnore.push_back(key);
				fprintf(stderr,
						"Ignoring failed assertions from %s, line %u\n", file, line);
				g_AssertFailLock.Unlock();
				return;
			case 'a':
				abort();
			}
		}
	}
#endif
}

// WinAPI debug functions.
DLL_EXPORT void OutputDebugString(const char *outputString)
{
#if !defined(NDEBUG)
	if (g_UnixConsole.IsInitialized())
	{
		char debugString[16 + strlen(outputString)];
		snprintf(debugString, sizeof debugString, "$7%s", outputString);
		debugString[sizeof debugString - 1] = 0;
		char *p = debugString + strlen(debugString);
		while (p > debugString && isspace(p[-1])) --p;
		if (p >= debugString) *p = 0;
		g_UnixConsole.Print(debugString);
	}
	else
	{
		fprintf(stderr, "debug: %s\n", outputString);
	}
#endif
}

DLL_EXPORT void DebugBreak()
{
}

//bool g_bProfilerEnabled = false;
//////////////////////////////////////////////////////////////////////////

extern "C" DLL_IMPORT IGameStartup* CreateGameStartup();

size_t fopenwrapper_basedir_maxsize = MAX_PATH;
namespace { char fopenwrapper_basedir_buffer[MAX_PATH] = ""; }
char * fopenwrapper_basedir = fopenwrapper_basedir_buffer;
bool fopenwrapper_trace_fopen = false;

#define RunGame_EXIT(exitCode) (exit(exitCode))

#define LINUX_LAUNCHER_CONF "launcher.cfg"

static void strip(char *s)
{
	char *p = s, *p_end = s + strlen(s);

	while (*p && isspace(*p)) ++p;
	if (p > s) { memmove(s, p, p_end - s + 1); p_end -= p - s; }
	for (p = p_end; p > s && isspace(p[-1]); --p);
	*p = 0;
}

static void LoadLauncherConfig(void)
{
	char conf_filename[MAX_PATH];
	char line[1024], *eq = 0;
	int n = 0;

	snprintf(conf_filename, sizeof conf_filename - 1,
			"%s/%s", fopenwrapper_basedir, LINUX_LAUNCHER_CONF);
	conf_filename[sizeof conf_filename - 1] = 0;
	FILE *fp = fopen(conf_filename, "r");
	if (!fp) return;
	while (true)
	{
		++n;
		if (!fgets(line, sizeof line - 1, fp)) break;
		line[sizeof line - 1] = 0;
		strip(line);
		if (!line[0] || line[0] == '#') continue;
		eq = strchr(line, '=');
		if (!eq)
		{
			fprintf(stderr, "'%s': syntax error in line %i\n",
					conf_filename, n);
			exit(EXIT_FAILURE);
		}
		*eq = 0;
		strip(line);
		strip(++eq);

		if (!strcasecmp(line, "autoload"))
		{
			if (strlen(eq) >= linux_autoload_level_maxsize)
			{
				fprintf(stderr, "'%s', line %i: autoload value too long\n",
						conf_filename, n);
				exit(EXIT_FAILURE);
			}
			strcpy(linux_autoload_level, eq);
		} else
		{
			fprintf(stderr, "'%s': unrecognized config variable '%s' in line %i\n",
					conf_filename, line, n);
			exit(EXIT_FAILURE);
		}
	}
	fclose(fp);
}

int RunGame(const char *) __attribute__ ((noreturn));

int RunGame(const char *commandLine)
{
	int exitCode = 0;

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hInstance = 0;
	startupParams.sLogFileName = "fp_log.txt";
	strcpy(startupParams.szSystemCmdLine, commandLine);
#if defined(DEDICATED_SERVER)
	startupParams.bDedicatedServer = true;
#endif
	startupParams.pUserCallback = NULL;

	// create the startup interface
	IGameStartup* pGameStartup = CreateGameStartup();
	const char *const szAutostartLevel
		= linux_autoload_level[0] ? linux_autoload_level : NULL;

	if (!pGameStartup)
	{
		fprintf(stderr, "ERROR: Failed to create the GameStartup Interface!\n");
		RunGame_EXIT(1);
	}

	// run the game
	IGame *game = pGameStartup->Init(startupParams);
	if (game)
	{
		exitCode = pGameStartup->Run(szAutostartLevel);
		pGameStartup->Shutdown();
		pGameStartup = 0;
		RunGame_EXIT(exitCode);
	}

	// if initialization failed, we still need to call shutdown
	pGameStartup->Shutdown();
	pGameStartup = 0;

	fprintf(stderr, "ERROR: Failed to initialize the GameStartup Interface!\n");
	RunGame_EXIT(exitCode);
}

// An unreferenced function.  This function is needed to make sure that
// unneeded functions don't make it into the final executable.  The function
// section for this function should be removed in the final linking.
void this_function_is_not_used(void)
{
}

extern void InitFileList(void);

#if defined(_DEBUG)
// Debug code for running the executable with GDB attached.  We'll start an
// XTerm with a GDB attaching to the running process.
void AttachGDB(const char *program)
{
	int pid = getpid();
	char command[1024];

	snprintf(
			command, sizeof command,
			"xterm -n 'GDB [Crysis]' -T 'GDB [Crysis]' -e gdb '%s' %i",
			program,
			pid);
	int shell_pid = fork();
	if (shell_pid == -1)
	{
		perror("fork()");
		exit(EXIT_FAILURE);
	}
	if (shell_pid > 0)
	{
		// Allow a few seconds for GDB to attach.
		sleep(5);
		return;
	}
	// Detach.
	for (int i = 0; i < 100; ++i)
		close(i);
	setsid();
	system(command);
	exit(EXIT_SUCCESS);
}
#endif

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int err;

#if defined(_DEBUG)
	// If the first command line option is -debug, then we'll attach GDB.
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-debug"))
		{
			AttachGDB(argv[0]);
			break;
		}
	}
#endif

	InitFileList();
	LoadLauncherConfig();

#if !defined(DEDICATED_SERVER)
	// Initialize SDL.
	if (SDL_Init(0) < 0)
	{
		fprintf(stderr, "ERROR: SDL initialization failed: %s\n",
				SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);
#endif

	// Build the command line.
	// We'll attempt to re-create the argument quoting that was used in the
	// command invocation.
	size_t cmdLength = 0;
	char needQuote[argc];
	for (int i = 0; i < argc; ++i)
	{
		bool haveSingleQuote = false, haveDoubleQuote = false, haveBrackets = false;
		bool haveSpace = false;
		for (const char *p = argv[i]; *p; ++p)
		{
			switch (*p)
			{
			case '"': haveDoubleQuote = true; break;
			case '\'': haveSingleQuote = true; break;
			case '[': case ']': haveBrackets = true; break;
			case ' ': haveSpace = true; break;
			default: break;
			}
		}
		needQuote[i] = 0;
		if (haveSpace || haveSingleQuote || haveDoubleQuote || haveBrackets)
		{
			if (!haveSingleQuote)
				needQuote[i] = '\'';
			else if (!haveDoubleQuote)
				needQuote[i] = '"';
			else if (!haveBrackets)
				needQuote[i] = '[';
			else
			{
				fprintf(stderr, "CRYSIS LinuxLauncher Error: Garbled command line\n");
				exit(EXIT_FAILURE);
			}
		}
		cmdLength += strlen(argv[i]) + (needQuote[i] ? 2 : 0);
		if (i > 0)
			++cmdLength;
	}
	char cmdLine[cmdLength + 1], *q = cmdLine;
	for (int i = 0; i < argc; ++i)
	{
		if (i > 0)
			*q++ = ' ';
		if (needQuote[i])
			*q++ = needQuote[i];
		strcpy(q, argv[i]);
		q += strlen(q);
		if (needQuote[i])
			if (needQuote[i] == '[')
				*q++ = ']';
		else
			*q++ = needQuote[i];
	}
	*q = 0;
	assert(q - cmdLine == cmdLength);

	return RunGame(cmdLine);
}

// vim:sw=2:ts=2:si

