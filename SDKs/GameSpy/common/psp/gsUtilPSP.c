#if defined(_PSP)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#include "../gsCommon.h"


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if !defined(GSI_NO_THREADS)

static void gsiResolveHostnameThread(void * arg)
{
	int result = 0;
	int resolverID = 0;
	char buf[1024]; // PSP documentation recommends 1024
	in_addr addr;
	GSIResolveHostnameHandle * info = (GSIResolveHostnameHandle)arg;

	result = sceNetResolverCreate(&resolverID, buf, sizeof(buf));
	if (result < 0)
	{
		// failed to create resolver, did you call sceNetResolverInit() ?
		info->ip = GSI_ERROR_RESOLVING_HOSTNAME;
		return -1;
	}
	else
	{
		// this will block until completed
		result = sceNetResolverStartNtoA(resolverID, info->hostname, &addr, &info->ip, GSI_RESOLVER_TIMEOUT, GSI_RESOLVER_RETRY);
		if (result < 0)
			info->ip = GSI_ERROR_RESOLVING_HOSTNAME;
		sceNetResolverDelete(resolverID);
	}
}

int gsiStartResolvingHostname(const char * hostname, GSIResolveHostnameHandle * handle)
{
	GSIResolveHostnameInfo * info;

	// allocate a handle
	info = (GSIResolveHostnameInfo *)gsimalloc(sizeof(GSIResolveHostnameInfo));
	if(!info)
		return -1;

	// make a copy of the hostname so the thread has access to it
	info->hostname = goastrdup(hostname);
	if(!info->hostname)
	{
		gsifree(info);
		return -1;
	}

	// not resolved yet
	info->finishedResolving = 0;

	// start the thread
	if(gsiStartThread(gsiResolveHostnameThread, (0x1000), info, &info->threadID) == -1)
	{
		gsifree(info->hostname);
		gsifree(info);
		return -1;
	}

	// set the handle to the info
	*handle = info;

	return 0;
}

void gsiCancelResolvingHostname(GSIResolveHostnameHandle handle)
{
	if (0 == handle->finishedResolving)
	{
		sceNetResolverStop(handle->resolverID); // safe to call from separate thread
		gsiCancelThread(handle->threadID);
	}
}

unsigned int gsiGetResolvedIP(GSIResolveHostnameHandle handle)
{
	return handle->ip;
}

#endif // (GSI_NO_THREADS)


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if defined(UNIQUEID)

static const char * GetMAC(void)
{
	static struct SceNetEtherAddr mac;
	int result = 0;

	result = sceNetGetLocalEtherAddr(&mac);
	if (result == 0)
		return (char *)&mac.data;
	else
		return NULL;
}

const char * GOAGetUniqueID_Internal(void)
{
	static char keyval[17];
	const char * MAC;

	// check if we already have the Unique ID
	if(keyval[0])
		return keyval;

	// get the MAC
	MAC = GetMAC();
	if(!MAC)
	{
		// error getting the MAC
		static char errorMAC[6] = { 1, 2, 3, 4, 5, 6 };
		MAC = errorMAC;
	}

	// format it
	sprintf(keyval, "%02X%02X%02X%02X%02X%02X0000",
		MAC[0] & 0xFF,
		MAC[1] & 0xFF,
		MAC[2] & 0xFF,
		MAC[3] & 0xFF,
		MAC[4] & 0xFF,
		MAC[5] & 0xFF);

	return keyval;
}

#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#endif // _PSP only
