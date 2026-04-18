extern char ps3_dmalloc_options[];

static const char *getenv_dmalloc(const char *name)
{
  static const char option_name[] = "DMALLOC_OPTIONS";
  const char *p = name, *q = option_name;

  while (*p && *p == *q) { ++p; ++q; }
  if (!*p && !*q)
    return ps3_dmalloc_options;

  return 0;
}

const char *getenv(const char *name)
{
  const char *env = 0;

  if ((env = getenv_dmalloc(name)))
    return env;

  return 0;
}

