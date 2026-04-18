#include "stdafx.h"
#include "FileTypeUtils.h"

// the supported file types
static const char *szSupportedExt[] = 
{
	"chr",
	"chr(c)",
	"cdf",
	"cdf(c)",
	"cgf",
	"cgf(c)",
	"cga",
	"cga(c)",
	"cid", 
	"caf"
};

// returns true if the given file path represents a file
// that can be previewed in Preview mode in Editor (e.g. cgf, cga etc.)
bool IsPreviewableFileType (const char *szPath)
{
	CString szExt = Path::GetExt(szPath);
	for (unsigned i = 0; i < sizeof(szSupportedExt)/sizeof(szSupportedExt[0]); ++i)
		if (stricmp(szExt, szSupportedExt[i])==0)
			return true;
	return false;
}