// Client for the Cg compiler server.
// Sascha Demetrio, saschad@crytek.de
//
// The Cg compiler server is a server running on a PC compiling Cg shader programs
// on request. The Cg compiler server is a temporary solution until either all
// shared programs for PS3 are precompiled and/or the PSGL runtime library
// supports runtime compilation of shader programs.
//
// The Cg compiler server is implemented in the Python program 'cgserver.py'.

#if !defined __CRYCG__

#include "StdAfx.h"
#include <ISystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netex/errno.h>

#define CGSERVER_HOSTNAME "192.168.9.125"
#define CGSERVER_HOSTNAME_MAXSIZE (1024)
#define CGSERVER_PORT (4455)
#define CGSERVER2_PORT (4455)

#if 0
size_t ps3_cgserver_hostname_maxsize = CGSERVER_HOSTNAME_MAXSIZE;
static char ps3_cgserver_hostname_buf[CGSERVER_HOSTNAME_MAXSIZE]
	= CGSERVER_HOSTNAME;
char *ps3_cgserver_hostname = ps3_cgserver_hostname_buf;
int ps3_cgserver_port = CGSERVER_PORT;
#else
#define ps3_cgserver_hostname ((const char *)gPS3Env->pCgSrvHostname)
#define ps3_cgserver_port (gPS3Env->nCgSrvPort)
#endif


const uint32	COMPILER_VERSION	=	0xDeadFace;

struct SCompileData
{
	uint32		m_Version;
	uint8			m_VertexShader;//false==pixelshader
	char			m_Entry[256-1-8];
	uint32		m_ShaderSize;
};


static void SendAll(int s, const char *buffer, size_t size)
{
	ssize_t w;
	size_t wTotal = 0;

	while (wTotal < size)
	{
		w = send(s, buffer + wTotal, size - wTotal, 0);
		if (w < 0)
		{
			fprintf(stderr, "send() failed (%d, %d)\n",	(int)w, sys_net_errno);
			break;
		}
		wTotal += (size_t)w;
	}
}

static void SendCompileJob(int s,SCompileData *pData,const char* pShader)
{
	const uint32 Size	=	sizeof(SCompileData)+pData->m_ShaderSize;
	pData->m_Version	=	COMPILER_VERSION;
	SendAll(s,(const char*)&Size,4);
	SendAll(s,(const char*)pData,sizeof(SCompileData));
	SendAll(s,(const char*)pShader,pData->m_ShaderSize);
}

bool cgSrvEnable = true;

// Compile the specified shader program using the Cg compiler server.
// Parameters:
//  -	profile: The name of the profile. This is one of "vs_2_0", "vs_1_1",
//		"ps_2_0", "ps_1_1".
//  - program: The shader program text.
//  - entry: The entry function name, NULL for default ("main").
//  - options: NULL-terminated array of compiler options. NULL for no options.
//  - buffer: The buffer receiving the compiled program.
//  - bufferSize: The size of the specified buffer. The function reports an
//		error if the compiled program does not fit into the specified buffer.
// Return:
// On success, the function returns the size of the compiled program. In case
// of an error, the function returns (size_t)-1.


size_t cgSrvCompile2(const bool profile,
                    const char *program,
										const char *entry,
										const char *comment,
										const char **options,
										char *buffer,
										size_t bufferSize
										)
{
	if (!cgSrvEnable)
		return (size_t)-1;

	SCompileData CompileData;
	CompileData.m_VertexShader	=	profile;

	if(entry && strlen(entry)+1<sizeof(CompileData.m_Entry))
		strcpy(CompileData.m_Entry,entry);
	else
		CompileData.m_Entry[0]=0;
	int s = -1;
	int err;
	while (true)
	{
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
		{
			fprintf(stderr,
				"cgSrvCompile: can't create client socket: error %i\n",
				s);
			return (size_t)-1;
		}
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof addr);
		addr.sin_family = AF_INET;
#ifdef _DEBUG
		addr.sin_port = htons(CGSERVER2_PORT);
#else
		addr.sin_port = htons(ps3_cgserver_port);
#endif
		// FIXME: inet_addr() only works for dotted IP addresses.
		addr.sin_addr.s_addr = inet_addr(ps3_cgserver_hostname);
		err = connect(s, (struct sockaddr *)&addr, sizeof addr);
		if (err >= 0) break;
		if (err < 0)
		{
			printf("cgSrvCompile: can't connect to cgserver: error %i, sys_net_errno=%i\n", err, (int)sys_net_errno);
			//socketclose(s);
			//return (size_t)-1;
			struct timeval tv;
			struct fd_set emptySet;
			FD_ZERO(&emptySet);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			socketselect(1, &emptySet, &emptySet, &emptySet, &tv);
			fprintf(stderr, "cgSrvCompile: retrying...\n");
			socketclose(s);
			s = -1;
		}
	}
/*	size_t optionsLength = 0;
	for (int i = 0; options[i]; ++i)
		optionsLength += strlen(options[i]) + 1;
	for (int i = 0; i < localOptionCount; ++i)
		optionsLength += strlen(localOptions[i]) + 1;
	char *optionsBuffer = new char[optionsLength], *p = optionsBuffer;
	for (int i = 0; options[i]; ++i)
	{
		if (i) *p++ = ',';
		strcpy(p, options[i]);
		p += strlen(p);
	}
	for (int i = 0; i < localOptionCount; ++i)
	{
		if (i || options[0]) *p++ = ',';
		strcpy(p, localOptions[i]);
		p += strlen(p);
	}
	*p = 0;
	assert(p - optionsBuffer == optionsLength - 1);
	uint32_t hdr[3] = {
		optionsLength - 1,
		comment ? strlen(comment) : 0,
		strlen(program)
	};*/
//	sendall(s, (const char *)hdr, sizeof hdr);
//	sendall(s, optionsBuffer, optionsLength - 1);
//	if (comment) sendall(s, comment, strlen(comment));
//	sendall(s, program, strlen(program));
	CompileData.m_ShaderSize	=	strlen(program);
	SendCompileJob(s,&CompileData,program);
//	delete[] optionsBuffer;
	size_t r, rTotal = 0;
	while (rTotal < bufferSize)
	{
		size_t recvCount = bufferSize - rTotal;
		if (recvCount > 4096) recvCount = 4096;
		r = recv(s, buffer + rTotal, recvCount, 0);
		if (r == 0) break;
		if (r == (size_t)-1 || r > recvCount)
		{
			fprintf(stderr,
					"cgSrvCompile: error in recv() from cgserver at offset %lu: "
					"error %li, sys_net_errno=%i\n",
					(unsigned long)rTotal,
					(long)r,
					(int)sys_net_errno);
			rTotal = (size_t)-1;
			break;
		}
		rTotal += r;
	}
	if (rTotal == bufferSize) { /* Overflow */ rTotal = (size_t)-1; }
	socketclose(s);
	return rTotal != 0 ? rTotal : (size_t)-1;
}

size_t cgSrvCompile(const bool profile,
                    const char *program,
										const char *entry,
										const char *comment,
										const char **options,
										char *buffer,
										size_t bufferSize
										)
{
	if (!cgSrvEnable)
		return (size_t)-1;

	const char *localOptions[16];
	char entryBuffer[512];
	int localOptionCount = 0;
	if(profile)
	{
		localOptions[localOptionCount++] = "-profile sce_vp_rsx";
	}
	else
	{
		localOptions[localOptionCount++] = "-profile sce_fp_rsx";
	}
	if (entry)
	{
		snprintf(entryBuffer, sizeof entryBuffer - 1, "-entry %s", entry);
		entryBuffer[sizeof entryBuffer - 1] = 0;
		localOptions[localOptionCount++] = entryBuffer;
	}
	int s = -1;
	int err;
	while (true)
	{
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
		{
			fprintf(stderr,
				"cgSrvCompile: can't create client socket: error %i\n",
				s);
			return (size_t)-1;
		}
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(ps3_cgserver_port);
		// FIXME: inet_addr() only works for dotted IP addresses.
		addr.sin_addr.s_addr = inet_addr(ps3_cgserver_hostname);
		err = connect(s, (struct sockaddr *)&addr, sizeof addr);
		if (err >= 0) break;
		if (err < 0)
		{
			printf("cgSrvCompile: can't connect to cgserver: error %i, sys_net_errno=%i\n", err, (int)sys_net_errno);
			//socketclose(s);
			//return (size_t)-1;
			struct timeval tv;
			struct fd_set emptySet;
			FD_ZERO(&emptySet);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			socketselect(1, &emptySet, &emptySet, &emptySet, &tv);
			fprintf(stderr, "cgSrvCompile: retrying...\n");
			socketclose(s);
			s = -1;
		}
	}
	size_t optionsLength = 0;
	for (int i = 0; options[i]; ++i)
		optionsLength += strlen(options[i]) + 1;
	for (int i = 0; i < localOptionCount; ++i)
		optionsLength += strlen(localOptions[i]) + 1;
	char *optionsBuffer = new char[optionsLength], *p = optionsBuffer;
	for (int i = 0; options[i]; ++i)
	{
		if (i) *p++ = ',';
		strcpy(p, options[i]);
		p += strlen(p);
	}
	for (int i = 0; i < localOptionCount; ++i)
	{
		if (i || options[0]) *p++ = ',';
		strcpy(p, localOptions[i]);
		p += strlen(p);
	}
	*p = 0;
	assert(p - optionsBuffer == optionsLength - 1);
	uint32_t hdr[3] = {
		optionsLength - 1,
		comment ? strlen(comment) : 0,
		strlen(program)
	};
	SendAll(s, (const char *)hdr, sizeof hdr);
	SendAll(s, optionsBuffer, optionsLength - 1);
	if (comment) SendAll(s, comment, strlen(comment));
	SendAll(s, program, strlen(program));
	delete[] optionsBuffer;
	size_t r, rTotal = 0;
	while (rTotal < bufferSize)
	{
		size_t recvCount = bufferSize - rTotal;
		if (recvCount > 4096) recvCount = 4096;
		r = recv(s, buffer + rTotal, recvCount, 0);
		if (r == 0) break;
		if (r == (size_t)-1 || r > recvCount)
		{
			fprintf(stderr,
					"cgSrvCompile: error in recv() from cgserver at offset %lu: "
					"error %li, sys_net_errno=%i\n",
					(unsigned long)rTotal,
					(long)r,
					(int)sys_net_errno);
			rTotal = (size_t)-1;
			break;
		}
		rTotal += r;
	}
	if (rTotal == bufferSize) { /* Overflow */ rTotal = (size_t)-1; }
	socketclose(s);
	return rTotal != 0 ? rTotal : (size_t)-1;
}

#endif // !defined __CRYCG__

// vim:ts=2:sw=2:si

