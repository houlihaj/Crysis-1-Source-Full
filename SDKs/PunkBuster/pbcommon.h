// Copyright (C) 2001-2003 Even Balance, Inc.
//
//
// pbcommon.h
//
// EVEN BALANCE - T.RAY
//

#if !defined(LINUX)
#define __WITH_PB__
#endif

#ifdef __WITH_PB__



#ifndef __PBCOMMON__
#define __PBCOMMON__



#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>



//
// Ugly Platform dependency handling
//
#if defined (_WIN32)
#define __PBWIN32__
#elif defined (__linux__)
#define __PBLINUX__
#else
#define __PBMAC__
#endif
#ifdef __PBWIN32__
#include <direct.h>
#include <tchar.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef __PBDLL__
#define pbDLLEXT ".dll"
#define pbDIRSEP "\\"
#else
#define TCHAR char
#endif
#define getcwd _getcwd
#define itoa _itoa
#define stricmp _stricmp
#define strnicmp _strnicmp
#define mkdir _mkdir
#endif //#ifdef __PBWIN32__
#ifdef __PBLINUX__
#include <sys/stat.h>
#include <sys/types.h>
#ifndef __PBDLL__
#define pbDLLEXT ".so"
#define pbDIRSEP "/"
//#define stricmp strcasecmp
//#define strnicmp strncasecmp
#endif
#endif //#ifdef __PBLINUX__
#ifdef __PBMAC__
#ifndef __PBDLL__
#define pbDLLEXT ".mac"
#define pbDIRSEP "/"
//#define stricmp _stricmp
//#define strnicmp _strnicmp
#endif
#endif //#ifdef __PBMAC__



//
// Forward Function Declarations - PB functions called from inside game engine, defined in pbcl.cpp and pbsv.cpp
//

extern "C" {

extern void PbClientInitialize ( void *exeInst ) ;
extern void PbClAddEvent ( int event , int datalen , char *data ) ;
extern void PbClientProcessEvents ( void ) ;
extern int  PbTrapPreExecCmd ( char *cmdtext ) ;
extern void PbClientTrapConsole ( char *msg , int msglen ) ;
extern void PbClientForceProcess ( void ) ;
extern void PBClientConnecting ( int , char * , int * ) ;
extern void PbClientCompleteCommand ( char *buf , int buflen ) ;
extern int  PbAddDigest ( char *filename ) ;
extern int  isPbClEnabled ( void ) ;
extern int  getPbGuidAge ( void ) ;
extern void EnablePbCl ( void ) ;
extern void DisablePbCl ( void ) ;
extern void PbGetFileMD5( int index, char *fn, unsigned char *md5, long *sz ) ;
extern char *PbSetGuid ( char *nums , int len ) ;

extern void PbServerInitialize ( void ) ;
extern void PbSvAddEvent ( int event , int clientIndex , int datalen , char *data ) ;
extern void PbPassConnectString ( char *fromAddr , char *connectString ) ;
extern char *PbAuthClient ( char *fromAddr , int cl_pb , char *cl_guid ) ;
extern void PbServerProcessEvents ( void ) ;
extern void PbServerForceProcess ( void ) ;
extern void PbServerCompleteCommand ( char *buf , int buflen ) ;
extern int  isPbSvEnabled ( void ) ;
extern void EnablePbSv ( void ) ;
extern void DisablePbSv ( void ) ;
extern void PbCaptureConsoleOutput ( char *msg , int msglen ) ;

extern void PbShutdown( void );

extern int isPbInstalled( void );

} //extern "C"



//
// Typedefs
//

// game-side func typedefs
typedef char *(*tdPbGameCommand) ( char * , char * ) ;
typedef char *(*tdPbGameQuery) ( int , char * ) ;
typedef char *(*tdPbGameMsg) ( char * , int ) ;
typedef char *(*tdPbSendToServer) ( int , char * ) ;
typedef char *(*tdPbSendToClient) ( int , char * , int ) ;
typedef char *(*tdPbSendToAddrPort) ( char * , unsigned short , int , char * ) ;

// pb-side func typedefs
typedef char *(*tdPbAddClEvent) ( void * , int , int , char * , int ) ;
typedef char *(*tdPbAddSvEvent) ( void * , int , int , int , char * , int ) ;
typedef char *(*tdPbProcessPbEvents) ( void * , int ) ;
typedef char *(*tdPbGlQuery) ( int ) ;
typedef char *(*tdPbClientConnect) ( void * , int , char * , int * ) ;
typedef char *(*tdPbPassConnectString) ( void * , char * , char * ) ;
typedef char *(*tdPbAuthClient) ( void * , char * , int , char * ) ;
typedef int   (*tdPbTrapPreExecCmd) ( void * , char * ) ;
typedef void  (*tdPbTrapConsole) ( void * , char * , int ) ;



//
// External Functions used by Classes (definitions in pbcl.cpp and pbsv.cpp)
//
extern char *PbClGameCommand ( char * , char * ) ;
extern char *PbClGameQuery ( int , char * ) ;
extern char *PbClGameMsg ( char * , int ) ;
extern char *PbClSendToServer ( int , char * ) ;
extern char *PbClSendToAddrPort ( char * , unsigned short , int , char * ) ;

extern char *PbSvGameCommand ( char * , char * ) ;
extern char *PbSvGameQuery ( int , char * ) ;
extern char *PbSvGameMsg ( char * , int ) ;
extern char *PbSvSendToClient ( int , char * , int ) ;
extern char *PbSvSendToAddrPort ( char * , unsigned short , int , char * ) ;



//
// Defines (Error Messages)
//
#define PbsQueryFail "PB Error: Query Failed"
#define PbsClDllLoadFail "PB Error: Client DLL Load Failure"
#define PbsClDllProcFail "PB Error: Client DLL Get Procedure Failure"
#define PbsSvDllLoadFail "PB Error: Server DLL Load Failure"
#define PbsSvDllProcFail "PB Error: Server DLL Get Procedure Failure"



//
// Defines (Game Query-related)
//
#define PB_Q_MAXRESULTLEN	255
#define PB_Q_MAXCLIENTS		101
#define PB_Q_CLIENT			102
#define PB_Q_CVAR			103
#define PB_Q_SINFO			104
#define PB_Q_SADDR			105
#define PB_Q_SEARCHBINDINGS	106
#define PB_Q_GETBINDING     107
#define PB_Q_KEYNAME        108
#define PB_Q_SEARCHCVARS    109
#define PB_Q_CVARFLAGS		110
#define PB_Q_CVARDEFAULTS	111
#define PB_Q_EXEINSTANCE    112
#define PB_Q_DLLHANDLE      113
#define PB_Q_STATS			114
#define PB_Q_CVARVALID		115
#define PB_Q_FILEMD5		116
#define PB_Q_TEXTUREMD5		117
#define PB_Q_CANJOIN		118
#define PB_Q_ENOUGHPRIV		119



//
// Defines (Event-related)
//
#define PB_EV_PACKET		13
#define PB_EV_CMD			14
#define PB_EV_STAT			15
#define PB_EV_CONFIG		16

#define PB_EV_CMDCOMPL		51

// UI subset
#define PB_EV_UISUBSET		113
#define PB_EV_ISENABLED		113
#define PB_EV_ENABLE		117
#define PB_EV_DISABLE		118
#define PB_EV_GUIDAGE		119
#define PB_EV_CANJOIN		120
#define PB_EV_ENOUGHPRIV	121



//
// Defines (Message-related)
//
#define PB_MSG_CONSOLE 1
#define PB_MSG_SCREEN  2
#define PB_MSG_LOG     4



//
// Defines (Misc)
//
#define PB_MISCLEN	31
#define PB_NAMELEN	32
#define PB_GUIDLEN	32
#define PB_MAXPKTLEN 1100
//The following two ID values are provided by Even Balance and must be matched to the 
//values embedded in the Even Balance provided PunkBuster DLLs for the game
#define PB_SV_ID        0x357AFE2D
#define PB_CL_ID        0x264B8BB8



//
//PB Client OpenGL Query Facility
//
#define PB_GL_READPIXELS	101
#define PB_GL_WIDTH			102
#define PB_GL_HEIGHT		103
#define PB_GL_RGB			104
#define PB_GL_UB			105



//
// PB Server Integration Structs
//
typedef struct Pb_Sv_Client_s {
	char name[PB_NAMELEN+1] , guid[PB_GUIDLEN+1] , ip[PB_NAMELEN+1] ;
	int slotIndex ;
} stPb_Sv_Client ;



//
// Forward (External) Declaration for PBsdk_SetPointers() - must be defined in each game module
//
extern "C" void PBsdk_getPbSdkPointer ( char *fn , unsigned long Flag ) ;
extern "C" void PBsdk_SetPointers ( void ) ;



#ifndef __PBDLL__
#ifndef __TRSTR__
//
// stristr
//
// case insensitive variation of strstr() function
//
inline char *stristr ( char *haystack , char *needle )
{
	char l[2] = "x" , u[2] = "X" , *cp , *lcp = NULL ;
	int nsl ;

	if ( haystack == NULL || needle == NULL || !(*needle) ) return haystack ;
	nsl = strlen ( needle ) ;
	*l = (char) tolower ( *needle ) ;
	for ( cp = haystack ; lcp == NULL ; ++cp ) {
		cp = strstr ( cp , l ) ;
		if ( cp == NULL ) break ;
		if ( !_strnicmp( cp , needle , nsl ) ) lcp = cp ;
	}
	*u = (char) toupper ( *needle ) ;
	if ( *l == *u ) return lcp ;
	for ( cp = haystack ; ; ++cp ) {
		cp = strstr ( cp , u ) ;
		if ( cp == NULL ) break ;
		if ( !_strnicmp ( cp , needle , nsl ) ) {
			if ( lcp == NULL ) return cp ;
			if ( cp < lcp ) return cp ;
			return lcp ;
		}
	}
	return lcp ;
}
#endif
#endif



//
// PbCopyFile
//
// returns 1 if successful, 0 if failed
//
inline int PbCopyFile ( char *sfn , char *tfn , long sizeLimit = 0 )
{
	FILE *fs = fopen ( sfn , "rb" ) ;
	int success = 0 ;
	if ( fs != NULL ) {
		FILE *ft = fopen ( tfn , "wb" ) ;
		if ( ft != NULL ) {
			fseek ( fs , 0 , SEEK_END ) ;
			long siz = ftell ( fs ) ;
			if ( siz > 0 ) {
				if ( sizeLimit == 0 || siz < sizeLimit ) {
					char *buf = new char [ siz ] ;
					if ( buf != NULL ) {
						fseek ( fs , 0 , SEEK_SET ) ;
						long rb = fread ( buf , 1 , siz , fs ) ;
						long wb = fwrite ( buf , 1 , rb , ft ) ;
						delete buf ;
						if ( wb == siz ) success = 1 ;
					}
				}
			}
			fclose ( ft ) ;
		}
		fclose ( fs ) ;
	}
	return success ;
}



//
// setRW
//
inline void setRW ( char *fn )
{
#ifdef __PBWIN32__
	_chmod ( fn , _S_IREAD | _S_IWRITE ) ;
#endif
#ifdef __PBLINUX__
	chmod ( fn , S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH ) ;
#endif
#ifdef __PBMAC__

#endif
}


#if !defined( __PBDLL__ ) && defined( __PBWIN32__ )
//
// TCHARchar
//
// provided to convert from unicode and other wide-byte strings to standard zero-delimted ascii char string arrays used by PB
//
inline char *TCHARchar ( const TCHAR *t , char *cs , int maxlenminus1 )
{
	memset ( cs , 0 , maxlenminus1 + 1 ) ;
	int i ;
	for ( i = 0 ; t[i] && i < maxlenminus1 ; i++ ) cs[i] = (char) t[i] ;
	return cs ;
}
#endif



//
// dbLog
//
inline void dbLog ( char *fn , char *fmtstr , ... )
{
	FILE *f = fopen ( fn , "abc" ) ;
	if ( f == NULL ) return ;

	char buf[4150] ;
	va_list va ;

	va_start ( va , fmtstr ) ;
	if ( strlen ( fmtstr ) > 2048 ) {
		strncpy ( buf , fmtstr , 4096 ) ;
		buf[4096] = 0 ;
	} else vsprintf ( buf , fmtstr , va ) ;

	fprintf ( f , "%s\r\n" , buf ) ;
	fflush ( f ) ;
	fclose ( f ) ;
}



#endif //#ifndef __PBCOMMON__
#endif //#ifdef __WITH_PB__
