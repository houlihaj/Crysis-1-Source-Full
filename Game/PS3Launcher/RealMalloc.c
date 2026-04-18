#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MALLOCWRAPPER_USE_DMALLOC
#include <dmalloc.h>
#endif

void *mallocwrapper_realMalloc(size_t size)
{
  return malloc(size);
}

void *mallocwrapper_realRealloc(void *ptr, size_t size)
{
  return realloc(ptr, size);
}

void mallocwrapper_realFree(void *ptr)
{
  free(ptr);
}

void *mallocwrapper_realMemalign(size_t alignment, size_t size)
{
  return memalign(alignment, size);
}

void *mallocwrapper_realReallocAlign(void *ptr, size_t size, size_t alignment)
{
  return reallocalign(ptr, size, alignment);
}

void *mallocwrapper_realValloc(size_t size)
{
  return valloc(size);
}

char *mallocwrapper_realStrdup(const char *str)
{
	return strdup(str);
}

void *mallocwrapper_realMemcpy(void *dst, const void *src, size_t count)
{
	return memcpy(dst, src, count);
}

/* vim:ts=2:sw=2
 */

