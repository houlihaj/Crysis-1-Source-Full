char *memcpy(char *dst, char *src, int size)
{
  char *src_end = src + size;
  char *dst_p = dst;

  while (src < src_end)
    *dst_p++ = *src++;
  return dst;
}

