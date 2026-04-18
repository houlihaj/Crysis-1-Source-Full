#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_CATCH_ALLOC		(32)

extern void *mallocwrapper_realMalloc(size_t);
extern void *mallocwrapper_realRealloc(void *, size_t);
extern void mallocwrapper_realFree(void *);
extern void *mallocwrapper_realMemalign(size_t, size_t);
extern void *mallocwrapper_realReallocAlign(void *, size_t, size_t);
extern void *mallocwrapper_realValloc(size_t);
extern char *mallocwrapper_realStrdup(const char *);
extern void *mallocwrapper_realMemcpy(void *, const void *, size_t);

int mallocwrapper_enableTrace = 1;
void *mallocwrapper_catchAlloc[MAX_CATCH_ALLOC] = { NULL };
int mallocwrapper_catchAllocMax = MAX_CATCH_ALLOC;

void mallocwrapper_catch(const char *id, const void *ptr, int index)
{
  printf("mallocwrapper_catch(id = \"%s\", ptr = 0x%lx, index = %i)\n",
      id, (unsigned long)ptr, index);
}

void mallocwrapper_error(const char *message)
{
	printf("mallocwrapper_error(message = \"%s\")\n", message);
}

static void mallocwrapper_checkAddr(const void *ptr, const char *id)
{
  int i;

  if (ptr == NULL)
    return;
  for (i = 0; i < MAX_CATCH_ALLOC; ++i)
  {
    if (!mallocwrapper_catchAlloc[i])
      continue;
    if (mallocwrapper_catchAlloc[i] == ptr)
    {
      mallocwrapper_catch(id, ptr, i);
      break;
    }
  }
}

void *malloc(size_t size)
{
  void *ptr = mallocwrapper_realMalloc(size);

  if (mallocwrapper_enableTrace)
    mallocwrapper_checkAddr(ptr, "malloc");
  return ptr;
}

void *calloc(size_t nmemb, size_t size)
{
  void *ptr;

  size *= nmemb;
  ptr = malloc(size);
  memset(ptr, 0, size);
  return ptr;
}

void *realloc(void *ptr, size_t size)
{
  if (mallocwrapper_enableTrace)
    mallocwrapper_checkAddr(ptr, "realloc-in");
  ptr = mallocwrapper_realRealloc(ptr, size);
  if (mallocwrapper_enableTrace)
    mallocwrapper_checkAddr(ptr, "realloc");
  return ptr;
}

void free(void *ptr)
{
	if(ptr != NULL)
	{
	  if (mallocwrapper_enableTrace)
		  mallocwrapper_checkAddr(ptr, "free");
		mallocwrapper_realFree(ptr);
	}
}

void *memalign(size_t alignment, size_t size)
{
  void *ptr = mallocwrapper_realMemalign(alignment, size);

  if (mallocwrapper_enableTrace)
    mallocwrapper_checkAddr(ptr, "memalign");
  return ptr;
}

void *reallocalign(void *ptr, size_t size, size_t alignment)
{
	if (mallocwrapper_enableTrace)
		mallocwrapper_checkAddr(ptr, "reallocalign-in");
	ptr = mallocwrapper_realReallocAlign(ptr, size, alignment);
	if (mallocwrapper_enableTrace)
		mallocwrapper_checkAddr(ptr, "reallocalign");
	return ptr;
}

void *valloc(size_t size)
{
  void *ptr = mallocwrapper_realValloc(size);

  if (mallocwrapper_enableTrace)
    mallocwrapper_checkAddr(ptr, "valloc");
  return ptr;
}

char *strdup(const char *str)
{
	char *dup;

	if (mallocwrapper_enableTrace)
		mallocwrapper_checkAddr(str, "strdup-in");
	dup = mallocwrapper_realStrdup(str);
	if (mallocwrapper_enableTrace)
		mallocwrapper_checkAddr(dup, "strdup");
	return dup;
}

void *memcpy(void *dst, const void *src, size_t count)
{
	if (mallocwrapper_enableTrace)
	{
		mallocwrapper_checkAddr(src, "memcpy-src");
		mallocwrapper_checkAddr(dst, "memcpy-dst");
	}

	/* Check for overlaps.  Note: we don't check for overlaps when src == dst,
	 * because the libc runtime library of the Cell SDK sometimes calls memcpy
	 * with src == dst. */
	const uint8_t *const dst8 = dst;
	const uint8_t *const dst8_end = dst8 + count;
	const uint8_t *const src8 = src;
	const uint8_t *const src8_end = src8 + count;
	if (dst8 > src8 && dst8 < src8_end)
	{
		fprintf(stderr, "mallocwrapper: overlap in memcpy(), dst > src\n");
		mallocwrapper_error("memcpy overlap, dst > src");
	}
	if (dst8 < src8 && dst8_end > src8)
	{
		fprintf(stderr, "mallocwrapper: overlap in memcpy(), dst < src\n");
		mallocwrapper_error("memcpy overlap, dst < src");
	}

	return mallocwrapper_realMemcpy(dst, src, count);
}

/* vim:ts=2:sw=2
 */

