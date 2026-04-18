//#define NOT_USE_CRY_MEMORY_MANAGER

#include <CryModuleDefs.h>
#define eCryModule eCryM_Animation
#define CRYANIMATION_EXPORTS

#pragma once

//! Include standard headers.
#include <platform.h>

#pragma warning(disable : 4996)
#pragma warning(default:4018)	// signed/unsigned mismatch
#pragma warning(default:4244)	// conversion from 'int' to 'float', possible loss of data
#pragma warning(default:4996)	// 'stricmp' was declared deprecated


//! Include main interfaces.
#include <Cry_Math.h>

#include <ISystem.h>
#include <ITimer.h>
#include <ILog.h>
#include <IConsole.h>
#include <ICryPak.h>
#include <StlUtils.h>

#include <ICryAnimation.h>
#include "AnimationBase.h"



#define SIZEOF_ARRAY(arr) (sizeof(arr)/sizeof((arr)[0]))

// maximum number of LODs per one geometric model (CryGeometry)
enum {g_nMaxGeomLodLevels = 6};

#include "CharacterManager.h"


// Check if file resource is locked.
extern bool IsResourceLocked( const char *filename );

//#define VTUNE_PROFILE
//#define MEMORY_REPORT



#ifdef MEMORY_REPORT

#define NOT_USE_CRY_MEMORY_MANAGER
// ---------------------------------------------------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------------------------------------------------

typedef	struct tag_au
{
	size_t		actualSize;
	size_t		reportedSize;
	void		*actualAddress;
	void		*reportedAddress;
	char		sourceFile[128];
	char		sourceFunc[128];
	unsigned int	sourceLine;
	unsigned int	allocationType;
	bool		breakOnDealloc;
	bool		breakOnRealloc;
	unsigned int	allocationNumber;
	struct tag_au	*next;
	struct tag_au	*prev;
} sAllocUnit;

typedef	struct
{
	unsigned int	totalReportedMemory;
	unsigned int	totalActualMemory;
	unsigned int	peakReportedMemory;
	unsigned int	peakActualMemory;
	unsigned int	accumulatedReportedMemory;
	unsigned int	accumulatedActualMemory;
	unsigned int	accumulatedAllocUnitCount;
	unsigned int	totalAllocUnitCount;
	unsigned int	peakAllocUnitCount;
} sMStats;

// ---------------------------------------------------------------------------------------------------------------------------------
// External constants
// ---------------------------------------------------------------------------------------------------------------------------------

extern	const	unsigned int	m_alloc_unknown;
extern	const	unsigned int	m_alloc_new;
extern	const	unsigned int	m_alloc_new_array;
extern	const	unsigned int	m_alloc_malloc;
extern	const	unsigned int	m_alloc_calloc;
extern	const	unsigned int	m_alloc_realloc;
extern	const	unsigned int	m_alloc_delete;
extern	const	unsigned int	m_alloc_delete_array;
extern	const	unsigned int	m_alloc_free;

// ---------------------------------------------------------------------------------------------------------------------------------
// Used by the macros
// ---------------------------------------------------------------------------------------------------------------------------------

void		m_setOwner(const char *file, const unsigned int line, const char *func);

// ---------------------------------------------------------------------------------------------------------------------------------
// Allocation breakpoints
// ---------------------------------------------------------------------------------------------------------------------------------

bool &m_breakOnRealloc(void *reportedAddress);
bool &m_breakOnDealloc(void *reportedAddress);
void m_breakOnAllocation(unsigned int count);

void setLogAllocations(bool);

// ---------------------------------------------------------------------------------------------------------------------------------
// The meat of the memory tracking software
// ---------------------------------------------------------------------------------------------------------------------------------

void		*m_allocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc,
										 const unsigned int allocationType, const size_t reportedSize);
void		*m_reallocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc,
											 const unsigned int reallocationType, const size_t reportedSize, void *reportedAddress);
void		m_deallocator(const char *sourceFile, const unsigned int sourceLine, const char *sourceFunc,
											const unsigned int deallocationType, const void *reportedAddress);

// ---------------------------------------------------------------------------------------------------------------------------------
// Utilitarian functions
// ---------------------------------------------------------------------------------------------------------------------------------

bool		m_validateAddress(const void *reportedAddress);
bool		m_validateAllocUnit(const sAllocUnit *allocUnit);
bool		m_validateAllAllocUnits();

// ---------------------------------------------------------------------------------------------------------------------------------
// Unused RAM calculations
// ---------------------------------------------------------------------------------------------------------------------------------

unsigned int	m_calcUnused(const sAllocUnit *allocUnit);
unsigned int	m_calcAllUnused();

// ---------------------------------------------------------------------------------------------------------------------------------
// Logging and reporting
// ---------------------------------------------------------------------------------------------------------------------------------

void		m_dumpAllocUnit(const sAllocUnit *allocUnit, const char *prefix = "");
void		m_dumpMemoryReport(const char *filename = "memreport.log", const bool overwrite = true);
sMStats		m_getMemoryStatistics();

// ---------------------------------------------------------------------------------------------------------------------------------
// Variations of global operators new & delete
// ---------------------------------------------------------------------------------------------------------------------------------

void	*operator new(size_t reportedSize);
void	*operator new[](size_t reportedSize);
void	*operator new(size_t reportedSize, const char *sourceFile, int sourceLine);
void	*operator new[](size_t reportedSize, const char *sourceFile, int sourceLine);
void	operator delete(void *reportedAddress);
void	operator delete[](void *reportedAddress);


#ifdef	new
#undef	new
#endif

#ifdef	delete
#undef	delete
#endif

#ifdef	malloc
#undef	malloc
#endif

#ifdef	calloc
#undef	calloc
#endif

#ifdef	realloc
#undef	realloc
#endif

#ifdef	free
#undef	free
#endif

#define	new		(m_setOwner  (__FILE__,__LINE__,__FUNCTION__),false) ? 0 : new
#define	delete		(m_setOwner  (__FILE__,__LINE__,__FUNCTION__),false) ? m_setOwner("",0,"") : delete
#define	malloc(sz)	m_allocator  (__FILE__,__LINE__,__FUNCTION__,m_alloc_malloc,sz)
#define	calloc(sz)	m_allocator  (__FILE__,__LINE__,__FUNCTION__,m_alloc_calloc,sz)
#define	realloc(ptr,sz)	m_reallocator(__FILE__,__LINE__,__FUNCTION__,m_alloc_realloc,sz,ptr)
#define	free(ptr)	m_deallocator(__FILE__,__LINE__,__FUNCTION__,m_alloc_free,ptr)


#endif

//////////////////////////////////////////////////////////////////////////
inline void AnimWarning( const char *format,... ) PRINTF_PARAMS(1, 2);
inline void AnimWarning( const char *format,... )
{
	if (g_pCharacterManager->m_IsDedicatedServer || !format)
		return;

	va_list args;
	va_start(args, format);
	GetISystem()->WarningV( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,0,0,format ,args );
	va_end(args);
}

inline void AnimFileWarning( const char *file,const char *format,... ) PRINTF_PARAMS(2, 3);
inline void AnimFileWarning( const char *file,const char *format,... )
{
	if (g_pCharacterManager->m_IsDedicatedServer || !format)
		return;

	va_list args;
	va_start(args, format);
	GetISystem()->WarningV( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,0,0,format ,args );
	va_end(args);
}
