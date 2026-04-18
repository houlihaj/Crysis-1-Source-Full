#ifndef AILOG_H
#define AILOG_H

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef PS3
#ifndef _DEBUG
// comment this out to remove asserts at compile time
#define ENABLE_AI_ASSERT
#else
#define ENABLE_AI_ASSERT
#endif
#endif

/// Sets up console variables etc
void AIInitLog(struct ISystem * system);

/// Return the verbosity for the console output
int AIGetLogConsoleVerbosity();

/// Return the verbosity for the file output
int AIGetLogFileVerbosity();

/// Indicates if AI asserts are enabled at runtime (meaningless if they're
/// disabled at compile time)
bool AIGetAssertEnabled();

/// Indicates if AI warnings/errors are enabled (e.g. for validation code you
/// don't want to run if there would be no output)
bool AIGetWarningErrorsEnabled();

/// Reports an AI Warning. This should be used when AI gets passed data from outside that
/// is "bad", but we can handle it and it shouldn't cause serious problems.
void AIWarning(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Reports an AI Error. This should be used when AI either gets passed data that is so
/// bad that we can't handle it, or we want to detect/check against a logical/programming
/// error and be sure this check isn't ever compiled out (like an assert might be). Unless
/// in performance critical code, prefer a manual test with AIError to using AIAssert.
/// Note that in the editor this will return (so problems can be fixed). In game it will cause 
/// execution to halt. 
void AIError(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Asserts that a condition is true. Can be enabled/disabled at compile time (will be enabled
/// for testing). Can also be enabled/disabled at run time (if enabled at compile time), so 
/// code below this should handle the condition = false case. It should be used to trap 
/// logical/programming errors, NOT data/script errors - i.e. once the code has been tested and 
/// debugged, it should never get hit, whatever changes are made to data/script
#ifdef ENABLE_AI_ASSERT
# ifdef _MSC_VER
# define AIAssert(exp) (void)( (exp) || !AIGetAssertEnabled() || (AIAssertHit(#exp, __FILE__, __LINE__), 0) )
# elif defined(__GNUC__)
// GCC does not like void expressions (like assert()) in || expressions.
# define AIAssert(exp) (void)( { if (AIGetAssertEnabled()) { assert(exp); } } )
# else
# define AIAssert(exp) (void)( !AIGetAssertEnabled() || assert(exp) )
# endif
#else
# define AIAssert(exp) ((void)0)
#endif

/// Used to log progress points - e.g. startup/shutdown/loading of AI. With the default (for testers etc)
/// verbosity settings this will write to the log file and the console.
void AILogProgress(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Used to log significant events like AI state changes. With the default (for testers etc)
/// verbosity settings this will write to the log file, but not the console.
void AILogEvent(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Used to log events like that AI people might be interested in during their private testing.
/// With the default (for testers etc) verbosity settings this will not write to the log file
/// or the console.
void AILogComment(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Should probably never be used since it bypasses verbosity checks
void AILogAlways(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Displays messages during loading - also gives an opportunity for system/display to update
void AILogLoading(const char *format, ...) PRINTF_PARAMS(1, 2);

/// Overlays the saved messages - called from CAISystem::DebugDraw
void AILogDisplaySavedMsgs(struct IRenderer *pRenderer);

//=============== Don't look below here ======================
void AIAssertHit(const char * exp, const char * file, unsigned line);

#endif // file included
