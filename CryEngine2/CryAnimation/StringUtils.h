/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	10 Feb. 2003 :- Created by Sergiy Migdalskiy
//
//  Contains:
//    Generic inline implementations of miscellaneous string manipulation utilities,
//    including path manipulation
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRY_ENGINE_STRING_UTILS_HDR_
#define _CRY_ENGINE_STRING_UTILS_HDR_

#include <CryString.h>
#include <algorithm>				// std::replace

namespace CryStringUtils
{

	// removes the extension from the file path
	inline char* StripFileExtension (char * szFilePath)
	{
		for (char* p = szFilePath + (int)strlen(szFilePath)-1; p >= szFilePath; --p)
		{
			switch(*p)
			{
			case ':':
			case '/':
			case '\\':
				// we've reached a path separator - it means there's no extension in this name
				return NULL;
			case '.':
				// there's an extension in this file name
				*p = '\0';
				return p+1;
			}
		}
		// it seems the file name is a pure name, without path or extension
		return NULL;
	}


	// returns the parent directory of the given file or directory.
	// the returned path is WITHOUT the trailing slash
	// if the input path has a trailing slash, it's ignored
	// nGeneration - is the number of parents to scan up
	template <class StringCls>
	StringCls GetParentDirectory (const StringCls& strFilePath, int nGeneration = 1)
	{
		for (const char* p = strFilePath.c_str() + strFilePath.length() - 2; // -2 is for the possible trailing slash: there always must be some trailing symbol which is the file/directory name for which we should get the parent
			p >= strFilePath.c_str();
			--p)
		{
			switch (*p)
			{
			case ':':
				return StringCls (strFilePath.c_str(), p);
				break;
			case '/':
			case '\\':
				// we've reached a path separator - return everything before it.
				if (!--nGeneration)
					return StringCls(strFilePath.c_str(), p);
				break;
			}
		};
		// it seems the file name is a pure name, without path or extension
		return StringCls();
	}

	// converts all chars to lower case
	inline string toLower (const string& str)
	{
		string temp = str;

#ifndef NOT_USE_CRY_STRING
		temp.MakeLower();
#else
		std::transform(temp.begin(),temp.end(),temp.begin(),tolower);			// STL MakeLower
#endif

		return temp;
	}

	// searches and returns the pointer to the extension of the given file
	inline const char* FindExtension (const char* szFileName)
	{
		const char* szEnd = szFileName + (int)strlen(szFileName);
		for (const char* p = szEnd-1; p >= szFileName; --p)
			if (*p == '.')
				return p+1;

		return szEnd;
	}

	// searches and returns the pointer to the file name in the given file path
	inline const char* FindFileNameInPath (const char* szFilePath)
	{
		for (const char* p = szFilePath + (int)strlen(szFilePath)-1; p >= szFilePath; --p)
			if (*p == '\\' || *p == '/')
				return p+1;
		return szFilePath;
	}

	// works like strstr, but is case-insensitive
	inline const char* stristr(const char* szString, const char* szSubstring)
	{
		int nSuperstringLength = (int)strlen(szString);
		int nSubstringLength = (int)strlen(szSubstring);

		for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
		{
			if (strnicmp(szString+nSubstringPos, szSubstring, nSubstringLength) == 0)
				return szString+nSubstringPos;
		}
		return NULL;
	}

	// replaces slashes
	inline void UnifyFilePath (string& strPath)
	{
#ifndef NOT_USE_CRY_STRING
		strPath.replace('\\','/' );
		strPath.MakeLower();
#else
		std::replace(strPath.begin(),strPath.end(), '\\','/' );											// STL replace
		std::transform(strPath.begin(),strPath.end(),strPath.begin(),tolower);			// STL MakeLower
#endif
	}

	// converts the number to a string
	inline string toString(unsigned nNumber)
	{
		char szNumber[16];
		sprintf (szNumber, "%u", nNumber);
		return szNumber;
	}


	// does the same as strstr, but the szString is allowed to be no more than the specified size
	inline const char* strnstr (const char* szString, const char* szSubstring, int nSuperstringLength)
	{
		int nSubstringLength = (int)strlen(szSubstring);
		if (!nSubstringLength)
			return szString;

		for (int nSubstringPos = 0; szString[nSubstringPos] && nSubstringPos < nSuperstringLength - nSubstringLength; ++nSubstringPos)
		{
			if (strncmp(szString+nSubstringPos, szSubstring, nSubstringLength) == 0)
				return szString+nSubstringPos;
		}
		return NULL;
	}


	// finds the string in the array of strings
	// returns its 0-based index or -1 if not found
	// comparison is case-sensitive
	// The string array end is demarked by the NULL value
	inline int findString(const char* szString, const char* arrStringList[])
	{
		for (const char** p = arrStringList; *p; ++p)
		{
			if (0 == strcmp(*p, szString))
				return (int)(p - arrStringList);
		}
		return -1; // string was not found
	}


}




/**************************************************************************
*_splitpath() - split a path name into its individual components
*
*Purpose:
*       to split a path name into its individual components
*
*Entry:
*       path  - pointer to path name to be parsed
*       drive - pointer to buffer for drive component, if any
*       dir   - pointer to buffer for subdirectory component, if any
*       fname - pointer to buffer for file base name component, if any
*       ext   - pointer to buffer for file name extension component, if any
*
*Exit:
*       drive - pointer to drive string.  Includes ':' if a drive was given.
*       dir   - pointer to subdirectory string. Includes leading and trailing '/' or '\', if any.
*       fname - pointer to file base name
*       ext   - pointer to file extension, if any.  Includes leading '.'.
*
*******************************************************************************/


ILINE void portable_splitpath ( const char *path, char *drive,	char *dir, char *fname,	char *ext	)
{
	char *p;
	char *last_slash = NULL, *dot = NULL;
	unsigned len;

	/* we assume that the path argument has the following form, where any
	* or all of the components may be missing.
	*
	*  <drive><dir><fname><ext>
	*
	* and each of the components has the following expected form(s)
	*
	*  drive:
	*  0 to _MAX_DRIVE-1 characters, the last of which, if any, is a
	*  ':'
	*  dir:
	*  0 to _MAX_DIR-1 characters in the form of an absolute path
	*  (leading '/' or '\') or relative path, the last of which, if
	*  any, must be a '/' or '\'.  E.g -
	*  absolute path:
	*      \top\next\last\     ; or
	*      /top/next/last/
	*  relative path:
	*      top\next\last\  ; or
	*      top/next/last/
	*  Mixed use of '/' and '\' within a path is also tolerated
	*  fname:
	*  0 to _MAX_FNAME-1 characters not including the '.' character
	*  ext:
	*  0 to _MAX_EXT-1 characters where, if any, the first must be a
	*  '.'
	*
	*/

	/* extract drive letter and :, if any */

	if ((strlen(path) >= (_MAX_DRIVE - 2)) && (*(path + _MAX_DRIVE - 2) == (':'))) {
		if (drive) {
			strncpy(drive, path, _MAX_DRIVE - 1);
			*(drive + _MAX_DRIVE-1) = ('\0');
		}
		path += _MAX_DRIVE - 1;
	}
	else if (drive) {
		*drive = ('\0');
	}

	/* extract path string, if any.  Path now points to the first character
	* of the path, if any, or the filename or extension, if no path was
	* specified.  Scan ahead for the last occurence, if any, of a '/' or
	* '\' path separator character.  If none is found, there is no path.
	* We will also note the last '.' character found, if any, to aid in
	* handling the extension.
	*/

	for (last_slash = NULL, p = (char *)path; *p; p++) {
		if (*p == ('/') || *p == ('\\'))
			/* point to one beyond for later copy */
			last_slash = p + 1;
		else if (*p == ('.'))
			dot = p;
	}

	if (last_slash) {

		/* found a path - copy up through last_slash or max. characters
		* allowed, whichever is smaller
		*/

		if (dir) {
			len = min((unsigned int)(((char *)last_slash - (char *)path) / sizeof(char)),(unsigned int)(_MAX_DIR - 1));
			strncpy(dir, path, len);
			*(dir + len) = ('\0');
		}
		path = last_slash;
	}
	else if (dir) {

		/* no path found */

		*dir = ('\0');
	}

	/* extract file name and extension, if any.  Path now points to the
	* first character of the file name, if any, or the extension if no
	* file name was given.  Dot points to the '.' beginning the extension,
	* if any.
	*/

	if (dot && (dot >= path)) {
		/* found the marker for an extension - copy the file name up to
		* the '.'.
		*/
		if (fname) {
			len = min((unsigned int)(((char *)dot - (char *)path) / sizeof(char)),(unsigned int)(_MAX_FNAME - 1));
			strncpy(fname, path, len);
			*(fname + len) = ('\0');
		}
		/* now we can get the extension - remember that p still points
		* to the terminating nul character of path.
		*/
		if (ext) {
			len = min((unsigned int)(((char *)p - (char *)dot) / sizeof(char)),(unsigned int)(_MAX_EXT - 1));
			strncpy(ext, dot, len);
			*(ext + len) = ('\0');
		}
	}
	else {
		/* found no extension, give empty extension and copy rest of
		* string into fname.
		*/
		if (fname) {
			len = min((unsigned int)(((char *)p - (char *)path) / sizeof(char)),(unsigned int)(_MAX_FNAME - 1));
			strncpy(fname, path, len);
			*(fname + len) = ('\0');
		}
		if (ext) {
			*ext = ('\0');
		}
	}
}






/**************************************************************************
*void _makepath() - build path name from components
*
*Purpose:
*       create a path name from its individual components
*
*Entry:
*       char *path  - pointer to buffer for constructed path
*       char *drive - pointer to drive component, may or may not contain trailing ':'
*       char *dir   - pointer to subdirectory component, may or may not include leading and/or trailing '/' or '\' characters
*       char *fname - pointer to file base name component
*       char *ext   - pointer to extension component, may or may not contain a leading '.'.
*
*Exit:
*       path - pointer to constructed path name
*
*******************************************************************************/
ILINE void portable_makepath ( char *path, const char *drive, const char *dir, const char *fname, const char *ext )
{
	const char *p;

	/* we assume that the arguments are in the following form (although we
	* do not diagnose invalid arguments or illegal filenames (such as
	* names longer than 8.3 or with illegal characters in them)
	*
	*  drive:
	*      A           ; or
	*      A:
	*  dir:
	*      \top\next\last\     ; or
	*      /top/next/last/     ; or
	*      either of the above forms with either/both the leading
	*      and trailing / or \ removed.  Mixed use of '/' and '\' is
	*      also tolerated
	*  fname:
	*      any valid file name
	*  ext:
	*      any valid extension (none if empty or null )
	*/

	/* copy drive */

	if (drive && *drive) {
		*path++ = *drive;
		*path++ = (':');
	}

	/* copy dir */

	if ((p = dir) && *p) {
		do {
			*path++ = *p++;
		}
		while (*p);
		if (*(p-1) != '/' && *(p-1) != ('\\')) {
			*path++ = ('\\');
		}
	}

	/* copy fname */

	if (p = fname) {
		while (*p) {
			*path++ = *p++;
		}
	}

	/* copy ext, including 0-terminator - check to see if a '.' needs
	* to be inserted.
	*/

	if (p = ext) {
		if (*p && *p != ('.')) {
			*path++ = ('.');
		}
		while (*path++ = *p++)
			;
	}
	else {
		/* better add the 0-terminator */
		*path = ('\0');
	}
}



#endif