#include "StdAfx.h"
#include <platform_impl.h>

#include <IGameStartup.h>
#include <IEntity.h>
#include <IGameFramework.h>
#include <IConsole.h>

// The ParticleParamsTypeInfo is required by the Cry3DEngine and CryAction
// modules.  If either one is not compiled as a PRX _or_ everything is
// linked statically, then we'll need to define the typeinfo in a statically
// linked location -> here.
#if !defined PS3_PRX_CryAction && !defined _LIB
#include "ParticleParams.h"
#include "ParticleParamsTypeInfo.cpp"
template class TCurveSpline<ColorF>;
template class TCurveSpline<float>;
#endif

#include <sdk_version.h>

#include <netex/net.h>
#include <netex/errno.h>
#include <cell/fs/cell_fs_errno.h>
#include <cell/fs/cell_fs_file_api.h>

#ifndef USE_SYSTEM_THREADS
	#include <pthread.h>
#endif

#include <sys/types.h>
#if defined(USE_HDD0)
	#include <sys/memory.h>
	#include <cell/sysmodule.h>
	#include <sysutil/sysutil_common.h>
	#include <sysutil/sysutil_gamedata.h>
#endif
#include <sys/process.h>
#include <sys/synchronization.h>
#include <sys/ppu_thread.h>
#include <sys/spu_initialize.h>
#include <sys/paths.h>

#ifndef _LIB
#include <InitPRX.h>
#endif

#include <IJobManSPU.h>

#if defined(SNTUNER)
	void __attribute__ ((noinline)) ProfileSyncPoint()
	{
		volatile int iAmCalled = 0;
	}

	void ProfileThread(uint64_t)
	{
		while(1)
		{
			Sleep(1000);
			ProfileSyncPoint();
		}
	}
#endif

//do not change it, also used for directory creation in cp_mastercd
static const char g_UserDirName[] = "user";

static int g_DeleteShaderCache = 0;

//necessary to be able to dma from stack to spu
#define MAIN_THREAD_STACK_SIZE (512 * 1024)
#define USE_HEAP_AS_STACK 1

static char g_CmdLine[256];

//////////////////////////////////////////////////////////////////////////

extern "C" IGameStartup* CreateGameStartup();

extern "C" void abort()
{
	__asm__ volatile ( "tw 31,1,1" );
	while (true);
};

#define RunGame_EXIT(exitCode) return (exitCode)

//job management stuff

const bool InitJobMan()
{
	gPS3Env->pJobMan = CreateJobManSPUInterface();
	gPS3Env->spuEnabled = 0;//disable by default
	gPS3Env->spuDumpProfStats = 0;
	if (!gPS3Env->pJobMan)
	{
		printf("CreateJobManSPUInterface failed\n");
		return false;
	}
	gPS3Env->pJobMan->RegisterProfileStatVar(&gPS3Env->spuDumpProfStats);
	return true;
}


#if defined(USE_HDD0)
	//gamedata status copy
	static CellGameDataStatGet gsStatcopy;
	//gamedata system parameters template
	static CellGameDataSystemFileParam gsGameParam;
	static void GamedataStatCallback(CellGameDataCBResult* pResult, CellGameDataStatGet* pGet, CellGameDataStatSet* pSet)
	{
		if(pGet->isNewData) 
		{
			//gamedata directory is not created until callback returns
			//system params are required for new gamedata
			//printf("Create new gamedata directory: %s\n", pGet->gameDataPath);
			pSet->setParam = &gsGameParam;
		} 
		else
		{
			//printf("Gamedata directory already exists: %s\n", pGet->gameDataPath);
		}

		//copy the gamedata status for subsequent use
		memcpy(&gsStatcopy, pGet, sizeof(gsStatcopy));
		pResult->result = CELL_GAMEDATA_CBRESULT_OK;
	}
#endif

static void InitCellFs()
{
	static bool sLoaded = false;
	if(!sLoaded)
	{
		const int cRet = cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
		if(cRet != CELL_OK) 
		{
			printf("Fatal error: cellSysmoduleLoadModule(CELL_SYSMODULE_FS) = 0x%08x\n",cRet);
			exit(EXIT_FAILURE);
		}
		sLoaded = true;
	}

  // Initialize async I/O
  CellFsErrno err = cellFsAioInit(SYS_APP_HOME);
  if (err != CELL_OK)
  {
    printf("Fatal error: AIO initialization failed for %s: 0x%08x\n",
        SYS_APP_HOME, static_cast<unsigned>(err));
    exit(EXIT_FAILURE);
  }
  err = cellFsAioInit(SYS_DEV_HDD0);
  if (err != CELL_OK)
  {
    printf("Fatal error: AIO initialization failed for %s: 0x%08x\n",
        SYS_DEV_HDD0, static_cast<unsigned>(err));
    exit(EXIT_FAILURE);
  }
}

void CopyFileHostHDD(const char *cpSrc, const char *cpDst)
{
	int rd, wr;
	int ret = cellFsOpen(cpSrc, CELL_FS_O_RDONLY, &rd, NULL, 0);
	if(0 > ret)
	{
		printf("cellFsOpen(%s) failed\n", cpSrc);
		return;
	}
	ret = cellFsOpen(cpDst, CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY, &wr, NULL, 0);
	if(0 > ret)
	{
		printf("cellFsOpen(%s) failed\n", cpSrc);
		return;
	}

	CellFsStat statsSrc;
	if(cellFsStat(cpSrc, &statsSrc) != CELL_FS_SUCCEEDED)
	{
		printf("cellFsStat for source file %s failed\n", cpSrc);
		return;	
	}
	
	uint64_t numRead;
	static char fileBuf[64 * 1024];
	//copy 64 KB wise
	do
	{
		ret = cellFsRead(rd, fileBuf, sizeof(fileBuf), &numRead);
		if(0 > ret)
		{
			printf("cellFsRead() failed, 0x%x\n", ret);
			break;
		}
		if(numRead == 0) 
			break;
		ret = cellFsWrite(wr, fileBuf, numRead, NULL);
		if(0 > ret) 
		{
			printf("cellFsWrite() failed, 0x%x\n", ret);
			break;
		}
	} 
	while(numRead > 0);

	cellFsClose(rd);
	cellFsClose(wr);
}

//delete all files in directory, does not remove dir itself
//return 0 if succeeded
static int DeleteDir(const char * const cpDir)
{
	int retVal = 0;
	int dir = 0;
	CellFsDirent dirent;
	uint64_t numRead;
	CellFsErrno ret = cellFsOpendir(cpDir, &dir);
	if(0 > ret)
	{
		printf("cellFsOpendir(%s) failed, 0x%x\n", cpDir, ret);
		return -1;
	}
	char filePath[MAX_PATH];
	strcpy(filePath, cpDir);
	char *pRelFilePath = &filePath[strlen(cpDir)];
	*pRelFilePath++ = '/';
	while(CELL_FS_SUCCEEDED == cellFsReaddir(dir, &dirent, &numRead)) 
	{
		if(0 == numRead) 
			break;
		switch(dirent.d_type) 
		{
		case CELL_FS_TYPE_REGULAR:
			strcpy(pRelFilePath, dirent.d_name);
			retVal = (cellFsUnlink(filePath) == CELL_FS_SUCCEEDED)?0 : -1;
		default:
			break;
		}
	}
	if(dir) 
		cellFsClosedir(dir);
	return retVal;
}

static void DeleteShaderCache()
{
	//delete everything in 
	//	game/shaders/cache 
	//	game/shaders/cache/d3d10/cgpshaders
	//	game/shaders/cache/d3d10/cgvshaders
	//it is not done recursively to save time (lots of debug files there too)
	char pathBuf[MAX_PATH];
	strcpy(pathBuf, gPS3Env->pCurDirHDD0);
	char *pPathBufBase = &pathBuf[gPS3Env->nCurDirHDD0Len];
	sprintf(pPathBufBase, "game/shaders/cache");
	int suc = DeleteDir(pathBuf);
	sprintf(pPathBufBase, "game/shaders/cache/d3d10/cgpshaders");
	suc |= DeleteDir(pathBuf);
	sprintf(pPathBufBase, "game/shaders/cache/d3d10/cgvshaders");
	suc |= DeleteDir(pathBuf);
	if(suc==0)
		printf("Done deleting shader cache\n");
	else
		printf("Failed deleting shader cache\n");
}

static void UpdateFiles(const char* const cpSrcDir, const char* const cpDstDir)
{
	//checks if file does already exists and if so, it checks if the modification time is different
	int destDir = 0, srcDir = 0;
	CellFsDirent dirent;
	uint64_t numRead;
	CellFsErrno ret = cellFsOpendir(cpSrcDir, &srcDir);
	if(0 > ret)
	{
		printf("Update shader source files: cellFsOpendir(%s) failed, 0x%x\n", cpSrcDir, ret);
		return;
	}
	ret = cellFsOpendir(cpDstDir, &destDir);
	if(0 > ret)
	{
		printf("Update shader source files: cellFsOpendir(%s) failed, 0x%x\n", cpDstDir, ret);
		return;
	}

	char fileSrcPath[MAX_PATH];
	strcpy(fileSrcPath, cpSrcDir);
	char *pRelSrcFilePath = &fileSrcPath[strlen(cpSrcDir)];
	*pRelSrcFilePath++ = '/';
	char fileDstPath[MAX_PATH];
	strcpy(fileDstPath, cpDstDir);
	char *pRelDstFilePath = &fileDstPath[strlen(cpDstDir)];
	*pRelDstFilePath++ = '/';
	while(CELL_FS_SUCCEEDED == cellFsReaddir(srcDir, &dirent, &numRead)) 
	{
		if(0 == numRead) 
			break;
		switch(dirent.d_type) 
		{
		case CELL_FS_TYPE_REGULAR:
		{
			strcpy(pRelSrcFilePath, dirent.d_name);
			const int cStrLen = strlen(dirent.d_name);
			char *pCurDest = pRelDstFilePath;
			const char *__restrict pCurSrc = dirent.d_name;
			for(int i=0; i<cStrLen; ++i)
				*pCurDest++ = tolower(*pCurSrc++);
			*pCurDest = '\0';
			//check if file does exist and if so, check modification time
			int updateFile = 1;
			CellFsStat statsSrc, statsDst;
			int dstExists = 0;
			if(cellFsStat(fileDstPath, &statsDst) == CELL_FS_SUCCEEDED)
			{
				dstExists = 1;
				//get modification time of source and dest
				if(cellFsStat(fileSrcPath, &statsSrc) == CELL_FS_SUCCEEDED)
				{
					if(statsSrc.st_mtime == statsDst.st_mtime)
						updateFile = 0;
				}
			}
			if(updateFile)
			{
				if(dstExists)
					cellFsUnlink(fileDstPath);
				CopyFileHostHDD(fileSrcPath, fileDstPath);
				//apply same modification time
				CellFsUtimbuf timeBuf;
				timeBuf.actime	= statsSrc.st_mtime;
				timeBuf.modtime = statsSrc.st_mtime;
				if(cellFsUtime(fileDstPath, &timeBuf) == CELL_FS_SUCCEEDED)
					printf("Updating shader source file: \"%s\"\n",dirent.d_name);
			}
		}
		break;
		default:
			break;
		}
	}
	if(srcDir) 
		cellFsClosedir(srcDir);
	if(destDir) 
		cellFsClosedir(destDir);
}

//copies all source files if they do not exist or were modified (checking file time)
static inline void CopyShaderSources()
{
	char pathBuf[MAX_PATH];
	strcpy(pathBuf, gPS3Env->pCurDirHDD0);
	strcpy(&pathBuf[gPS3Env->nCurDirHDD0Len], "game/shaders");
	UpdateFiles(SYS_APP_HOME"/mastercd/game/shaders", pathBuf);
	strcpy(&pathBuf[gPS3Env->nCurDirHDD0Len], "game/shaders/hwscripts/cryfx");
	UpdateFiles(SYS_APP_HOME"/mastercd/game/shaders/hwscripts/cryfx", pathBuf);
}

void InitFileSystem()
{
#if defined(USE_HDD0)
	strcpy(gsGameParam.title, "Crysis");
	strcpy(gsGameParam.titleId, "GAME02");
	strcpy(gsGameParam.dataVersion, "00000");
	gsGameParam.parentalLevel = CELL_SYSUTIL_GAME_PARENTAL_LEVEL01;
	gsGameParam.attribute = CELL_GAMEDATA_ATTR_NORMAL;

	InitCellFs();
	const char *pDirName = gsGameParam.titleId;
	sys_memory_container_t container;
	int ret = sys_memory_container_create(&container, 3*1024*1024);
	if(ret != CELL_OK)
	{
		printf("Fatal error: sys_memory_container_create returned error code 0x%08x\n",ret);
		exit(EXIT_FAILURE);
		return;
	}

	//calls currently gamedata sysutil in blocking mode.
	ret = cellGameDataCheckCreate2 
	(
		CELL_GAMEDATA_VERSION_CURRENT,
		pDirName,
		CELL_GAMEDATA_ERRDIALOG_NONE,
		GamedataStatCallback,
		container
	);
	if(CELL_GAMEDATA_RET_OK != ret) 
	{
		switch(ret)
		{
		case CELL_GAMEDATA_ERROR_CBRESULT:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_CBRESULT,  pDirName=%s\n\n", pDirName);break;
		case CELL_GAMEDATA_ERROR_ACCESS_ERROR:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_ACCESS_ERROR,  pDirName=%s\n\n", pDirName);break;
		case CELL_GAMEDATA_ERROR_INTERNAL:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_INTERNAL,  pDirName=%s\n\n", pDirName);break;
		case CELL_GAMEDATA_ERROR_PARAM:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_PARAM,  pDirName=%s\n\n", pDirName);break;
		case CELL_GAMEDATA_ERROR_NOSPACE:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_NOSPACE,  pDirName=%s\n\n", pDirName);break;
		case CELL_GAMEDATA_ERROR_BROKEN:
			printf("cellGameDataCheckCreate failed with CELL_GAMEDATA_ERROR_BROKEN,  pDirName=%s\n\n", pDirName);break;
		default:
			printf("cellGameDataCheckCreate failed with unknown error code %x,  pDirName=%s\n\n", ret,pDirName);
		}
		exit(EXIT_FAILURE);
	}
	else
	{
		char *pCurDirHDD0 = gsStatcopy.gameDataPath;//set global directory (e.g. "mastercd"-dir on hdd0)
		int nCurDirHDD0Len = strlen(pCurDirHDD0);
		assert(nCurDirHDD0Len > 1);
		gPS3Env->pCurDirHDD0 = pCurDirHDD0;
		if(pCurDirHDD0[nCurDirHDD0Len-1] != '/')
		{	
			pCurDirHDD0[nCurDirHDD0Len++] = '/';
			pCurDirHDD0[nCurDirHDD0Len] = '\0';
		}
		gPS3Env->nCurDirHDD0Len = nCurDirHDD0Len;
		strcpy(fopenwrapper_basedir, pCurDirHDD0);
	}
	sys_memory_container_destroy(container);
#endif

	char pathBuf[MAX_PATH];
	strcpy(pathBuf, gPS3Env->pCurDirHDD0);
	char *pPathBufBase = &pathBuf[gPS3Env->nCurDirHDD0Len];

	//copy config files to be able to edit them and use the updated version on reload
	sprintf(pPathBufBase, "game/system.cfg");
	CopyFileHostHDD(SYS_APP_HOME"/mastercd/system.cfg", pathBuf);
	sprintf(pPathBufBase, "game/libs/config/defaultprofile.xml");
	CopyFileHostHDD(SYS_APP_HOME"/mastercd/game/libs/config/defaultprofile.xml", pathBuf);
}

#define PS3_LAUNCHER_CONF SYS_APP_HOME"/launcher.cfg"

#define CGSERVER_HOSTNAME "192.168.9.125"
#define CGSERVER_HOSTNAME_MAXSIZE (1024)
#define CGSERVER_PORT (4455)

static const size_t ps3_cgserver_hostname_maxsize = CGSERVER_HOSTNAME_MAXSIZE;
static char ps3_cgserver_hostname_buf[CGSERVER_HOSTNAME_MAXSIZE]
	= CGSERVER_HOSTNAME;
static char *ps3_cgserver_hostname = ps3_cgserver_hostname_buf;
static int ps3_cgserver_port = CGSERVER_PORT;

static void strip(char *s)
{
	char *p = s, *p_end = s + strlen(s);

	while (*p && isspace(*p)) ++p;
	if (p > s) { memmove(s, p, p_end - s + 1); p_end -= p - s; }
	for (p = p_end; p > s && isspace(p[-1]); --p);
	*p = 0;
}

#ifdef USE_DMALLOC
// Configuration string for the dmalloc memory debugger.  When not linking
// against dmalloc, then this buffer is ignored.
char ps3_dmalloc_options[1024] = "";
#define AVOID_FOPEN 1
#endif

static void ps3LoadLauncherConfig(void)
{
	char conf_filename[MAX_PATH];
	char line[1024], *eq = 0;
	int n = 0;
	static bool configLoaded = false;
	bool disableCgc = false;
	bool disableLog = false;
	bool enableCmdBufferCheck = false;

	if (configLoaded)
		return;

	strcpy(g_CmdLine, "ps3launcher -devmode");

#ifdef AVOID_FOPEN
	char configBuffer[32768];
	memset(configBuffer, 0, sizeof configBuffer);
	const char *configBufferP = configBuffer;
	{
		int fd = -1;
		int fsErr = cellFsOpen(
				PS3_LAUNCHER_CONF,
				CELL_FS_O_RDONLY,
				&fd,
				NULL,
				0);
		if (fsErr != CELL_FS_SUCCEEDED)
			return;
		uint64_t nRead = 0;
		fsErr = cellFsRead(
				fd,
				configBuffer,
				sizeof configBuffer,
				&nRead);
		if (fsErr != CELL_FS_SUCCEEDED)
			return;
		configBuffer[sizeof configBuffer - 1] = 0;
		if (nRead < sizeof configBuffer)
			configBuffer[nRead] = 0;
		cellFsClose(fd);
	}
#else
	FILE *fp = fopen(PS3_LAUNCHER_CONF, "r");
	if (!fp)
		return;
#endif
	while (true)
	{
		++n;
#ifdef AVOID_FOPEN
		{
			if (!*configBufferP)
				break;
			const char *lineEnd = strchr(configBufferP, '\n');
			if (lineEnd == NULL)
				lineEnd = configBufferP + strlen(configBufferP);
			size_t lineLen = std::min(
					static_cast<size_t>(lineEnd - configBufferP),
					static_cast<size_t>(sizeof line - 1));
			memcpy(line, configBufferP, lineLen);
			line[lineLen] = 0;
			configBufferP = lineEnd;
			while (*configBufferP && isspace(*configBufferP))
				++configBufferP;
		}
#else
		if (!fgets(line, sizeof line - 1, fp))
			break;
		line[sizeof line - 1] = 0;
#endif
		strip(line);
		if (!line[0] || line[0] == '#')
			continue;
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
		if (!strcasecmp(line, "cgserver_host"))
		{
			if (strlen(eq) >= ps3_cgserver_hostname_maxsize)
			{
				fprintf(stderr, "'%s', line %i: cgserver_host value too long\n",
						conf_filename, n);
				exit(EXIT_FAILURE);
			}
			strcpy(ps3_cgserver_hostname, eq);
		}
		else if (!strcasecmp(line, "cgserver_port"))
		{
			int port = atoi(eq);
			if (port < 1 || port > 65535)
			{
				fprintf(stderr, "'%s', line %i: invalid cgserver_port %i\n",
						conf_filename, n, port);
				exit(EXIT_FAILURE);
			}
			ps3_cgserver_port = port;
		}
		else if (!strcasecmp(line, "autoload"))
		{
			if (strlen(eq) > 0 && eq[0] != ' ')
			{
				strcat(g_CmdLine, " +map ");
				strcat(g_CmdLine, eq);
			}
		}
		else if (!strcasecmp(line, "disable_cgc"))
		{
			if(atoi(eq) != 0)
				disableCgc = true;
		}
		else if (!strcasecmp(line, "disable_log"))
		{
			if(atoi(eq) != 0)
				disableLog = true;
		}
		else if (!strcasecmp(line, "delete_shader_cache"))
		{
			if(atoi(eq) != 0)
				g_DeleteShaderCache = 1;
		}
		else if (!strcasecmp(line, "enable_cmdbuffer_check"))
		{
			if (atoi(eq) != 0)
      {
#ifndef GCM_DEBUG_CMDBUFFER
        fprintf(stderr,
            "'%s', line %i: warning: "
            "GCM command buffer verification disabled at compile time\n",
            conf_filename, n);
#endif
				enableCmdBufferCheck = true;
      }
		}
#ifdef USE_DMALLOC
		else if (!strcasecmp(line, "dmalloc_options"))
		{
			strncpy(ps3_dmalloc_options, eq, sizeof ps3_dmalloc_options);
			ps3_dmalloc_options[sizeof ps3_dmalloc_options - 1] = 0;
		}
#endif
	}
#ifndef AVOID_FOPEN
	fclose(fp);
#endif
	gPS3Env->pCgSrvHostname = ps3_cgserver_hostname;
	gPS3Env->nCgSrvPort = ps3_cgserver_port;
	gPS3Env->bDisableCgc = disableCgc;
	gPS3Env->bEnableCmdBufferCheck = enableCmdBufferCheck;
	gPS3Env->bDisableLog		= disableLog;
	configLoaded = true;
}

extern "C" const char *getenv(const char *name)
{
#ifdef USE_DMALLOC
	InitCellFs();
	if (!strcmp(name, "DMALLOC_OPTIONS"))
	{
		ps3LoadLauncherConfig();
		return ps3_dmalloc_options;
	}
#endif
	return 0;
}

int RunGame(const char * /*commandLine*/)
{
	int exitCode = 0;

	int ret = cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
	if(ret < 0) 
	{
		fprintf(stderr, "PS3 network initialization failed: cellSysmoduleLoadModule(), error %i\n", ret);
		RunGame_EXIT(1);
	}
	ret = sys_net_initialize_network();
	if(ret < 0) 
	{
		fprintf(stderr, "PS3 network initialization failed: sys_net_initialize_network(), error %i\n", ret);
		cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
		RunGame_EXIT(1);
	}

	static const size_t MAX_RESTART_LEVEL_NAME = 256;
	char fileName[MAX_RESTART_LEVEL_NAME];
	strcpy(fileName, "");
	
	static const char logFileName[] = SYS_APP_HOME"/mastercd/Game.log";
	if(!InitJobMan())
		return -1;
	if(!GetIJobManSPU()->InitSPUs(SYS_APP_HOME"/SPURepository.bin")) 
		return -1;

#if defined(SNTUNER)
	sys_ppu_thread_t profileThread = 0;
	int err;
	if(0 != (err=sys_ppu_thread_create(&profileThread, ProfileThread, (uint64)(UINT_PTR)"", 0, 4096,	SYS_PPU_THREAD_CREATE_JOINABLE,"ProfileThread")))
		printf("Cannot create ProfileThread, err=%d\n",err);
#endif

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hInstance = 0;
	startupParams.sLogFileName = logFileName;
	memcpy(startupParams.szUserPath, g_UserDirName, sizeof(g_UserDirName));

	strcpy(startupParams.szSystemCmdLine, g_CmdLine);

	// create the startup interface
	IGameStartup* pGameStartup = CreateGameStartup();
	const char *const szAutostartLevel = NULL;

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
	sys_net_finalize_network();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
	cellSysmoduleUnloadModule(CELL_SYSMODULE_FS);

	fprintf(stderr, "ERROR: Failed to initialize the GameStartup Interface!\n");
	RunGame_EXIT(exitCode);

	// Not reached.
	return 0;
}

PS3SystemEnvironment ps3Env;
PS3SystemEnvironment *gPS3Env = &ps3Env;

int PS3Break() { fputs("break\n", stderr); return 0; }

#if defined __GPROF__
// The checkpoint function is intended as a profiling helper for profiling
// startup and level-loading times.  Should remain empty in the main branch.
int PS3Checkpoint(const char *message)
{
  // Add local checkpoint code here!
}
#endif

#if defined PS3_PRX
extern "C"
{
	struct SCryRenderInterface;
	IRenderer *PackageRenderConstructor(int, char **, SCryRenderInterface *);
	INetwork *CreateNetwork(ISystem *, int);
	IEntitySystem *CreateEntitySystem(ISystem *);
	IInput *CreateInput(ISystem *, void *);
	ISoundSystem *CreateSoundSystem(ISystem *, void *);
	IPhysicalWorld *CreatePhysicalWorld(ISystem *);
	IMovieSystem *CreateMovieSystem(ISystem *);
	IAISystem *CreateAISystem(ISystem *);
	IScriptSystem *CreateScriptSystem(ISystem *, bool);
	ICryFont *CreateCryFontInterface(ISystem *);
	I3DEngine *CreateCry3DEngine(ISystem *, const char *);
	ICharacterManager *CreateCharManager(ISystem *, const char *);
	IGameFramework *CreateGameFramework();
}
namespace
{
  static PS3InitFnTable sInitFnTable =
  {
#if !defined PS3_PRX_Renderer
    PackageRenderConstructor,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryNetwork
    CreateNetwork,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryEntitySystem
    CreateEntitySystem,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryInput
    CreateInput,
#else
    NULL,
#endif
#if !defined PS3_PRX_CrySoundSystem
    CreateSoundSystem,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryPhysics
    CreatePhysicalWorld,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryMovie
    CreateMovieSystem,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryAISystem
    CreateAISystem,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryScriptSystem
    CreateScriptSystem,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryFont
    CreateCryFontInterface,
#else
    NULL,
#endif
#if !defined PS3_PRX_Cry3DEngine
    CreateCry3DEngine,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryAnimation
    CreateCharManager,
#else
    NULL,
#endif
#if !defined PS3_PRX_CryAction
		CreateGameFramework,
#else
		NULL,
#endif
  };
}
#endif // PS3_PRX

// This is the first function called by main().
static void InitPS3()
{
	gPS3Env->staticMemUsedKB = 0;
	sys_memory_info_t memInfo;
	if(sys_memory_get_user_memory_size(&memInfo) == CELL_OK)
		gPS3Env->staticMemUsedKB = (memInfo.total_user_memory - memInfo.available_user_memory) >> 10;//in KB
	gPS3Env->fnBreak = PS3Break;
#if defined __GPROF__
	gPS3Env->fnCheckpoint = PS3Checkpoint;
#endif
#if defined PS3_PRX
	gPS3Env->pInitFnTable = &sInitFnTable;
#endif

	// Note: If the entire application is linked statically, _LIB will be
	// defined, so we can use this as a test if PRX initialization code is
	// required.
#ifndef _LIB
	// Force initialization of the memory manager.
	_CryMemoryManagerPoolHelper::Init();
#endif // !_LIB

	// Initialize the fopen() wrapper buffers.
	InitFOpenWrapper();

	// Note: If the entire application is linked statically, _LIB will be
	// defined, so we can use this as a test if PRX initialization code is
	// required.
#ifndef _LIB
	// Register system libraries for use with loaded PRX modules.
	extern int sys_libc, sys_libm, sys_libstdcxx;
	sys_prx_register_library(&sys_libc);
	sys_prx_register_library(&sys_libm);
	sys_prx_register_library(&sys_libstdcxx);

	// Load all PRX modules.
	if (!LoadPRX("System", "CrySystem")
#if defined PS3_PRX_Renderer
			|| !LoadPRX("Renderer", "XRenderD3D10")
#endif
#if defined PS3_PRX_CryNetwork
			|| !LoadPRX("Network", "CryNetwork")
#endif
#if defined PS3_PRX_CryEntitySystem
			|| !LoadPRX("EntitySystem", "CryEntitySystem")
#endif
#if defined PS3_PRX_CryInput
			|| !LoadPRX("Input", "CryInput")
#endif
#if defined PS3_PRX_CrySoundSystem
			|| !LoadPRX("SoundSystem", "CrySoundSystem")
#endif
#if defined PS3_PRX_CryPhysics
			|| !LoadPRX("Physics", "CryPhysics")
#endif
#if defined PS3_PRX_CryMovie
			|| !LoadPRX("MovieSystem", "CryMovie")
#endif
#if defined PS3_PRX_CryAISystem
			|| !LoadPRX("AISystem", "CryAISystem")
#endif
#if defined PS3_PRX_CryScriptSystem
			|| !LoadPRX("ScriptSystem", "CryScriptSystem")
#endif
#if defined PS3_PRX_CryFont
			|| !LoadPRX("Font", "CryFont")
#endif
#if defined PS3_PRX_Cry3DEngine
			|| !LoadPRX("3DEngine", "Cry3DEngine")
#endif
#if defined PS3_PRX_CryAnimation
			|| !LoadPRX("Animation", "CryAnimation")
#endif
#if defined PS3_PRX_CryAction
			|| !LoadPRX("Action", "CryAction")
#endif
#if defined PS3_PRX_GameDll
			|| !LoadPRX("Game", "GameDll")
#endif
			)
	{
		fprintf(stderr, "PRX initialization failed!\n");
		exit(EXIT_FAILURE);
	}
#endif // !_LIB
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void main_func(void*)
{
	InitFileSystem();
	ps3LoadLauncherConfig();

	if(g_DeleteShaderCache)
	{
		if(gPS3Env->bDisableCgc)
		{
			printf("Cannot delete shader cache and having gPS3Env->bDisableCgc = true\n");
			gPS3Env->bDisableCgc = false;
		}
		DeleteShaderCache();
	}
	if(!gPS3Env->bDisableCgc)
		CopyShaderSources();
	RunGame("");
}

#if !defined(USE_DMALLOC)
	extern "C" void *mwprivate2_memalign(size_t alignment, size_t size)
	{
		return memalign(alignment, size);
	}
#else
	extern "C" void *mwprivate2_memalign(size_t alignment, size_t size);
#endif

//main needs to put the stack via asm to the heap to be able to DMA from/to the stack by the SPUs
int main()
{
	InitPS3();
#if defined(USE_HEAP_AS_STACK)
//	#if defined(_DEBUG)
		uint8 *pStack = (uint8*)mwprivate2_memalign(64, MAIN_THREAD_STACK_SIZE);
//	#else
//		uint8 *pStack = (uint8*)memalign(64, MAIN_THREAD_STACK_SIZE);
//	#endif
	gPS3Env->nMainStackSize = MAIN_THREAD_STACK_SIZE;
	gPS3Env->pMainStack  = pStack + MAIN_THREAD_STACK_SIZE;
	InvokeOnLinkedStack(main_func, NULL, (void*)pStack, MAIN_THREAD_STACK_SIZE);
	free(pStack);
#else
	main_func(NULL);
#endif

	GetIJobManSPU()->ShutDown();

	return 0; 
}

// vim:ts=2:sw=2:expandtab

