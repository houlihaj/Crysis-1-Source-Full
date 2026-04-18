#ifndef __NETLOG_H__
#define __NETLOG_H__

extern DWORD g_primaryThread;

ILINE bool IsPrimaryThread()
{
	return g_primaryThread == GetCurrentThreadId();
}

void FlushNetLog( bool final );
void InitPrimaryThread();
void NetLog( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryLog
void NetLogHUD( const char * fmt, ... ) PRINTF_PARAMS(1, 2);
// only in response to user input!
void NetLogAlways( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryLogAlways
void NetLogAlways_Secret( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryLogAlways
void NetError( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryError
void NetWarning( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryWarning
void NetQuickLog( bool toConsole, float timeout, const char * fmt, ... ) PRINTF_PARAMS(3, 4);
void NetPerformanceWarning( const char * fmt, ... ) PRINTF_PARAMS(1, 2);
void NetComment( const char * fmt, ... ) PRINTF_PARAMS(1, 2); // CryComment

string GetBacklogAsString();
void ParseBacklogString(const string& backlog, string& base, std::vector<string>& r);

#ifdef NDEBUG
# define ASSERT_PRIMARY_THREAD
#else
void AssertPrimaryThread();
# define ASSERT_PRIMARY_THREAD AssertPrimaryThread()
#endif

#endif
