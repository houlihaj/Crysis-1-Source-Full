#ifndef __SHENDIAN_H__
#define __SHENDIAN_H__

#define SH_LITTLE_ENDIAN

#if !defined (_CPU_X86)
#  define PROC_NEEDS_STRICT_ALIGNMENT
#endif

struct swap_4
{
  unsigned char b1, b2, b3, b4;
};

#ifdef SH_BIG_ENDIAN
#  define big_endian_long(x)  x
#  define big_endian_short(x) x
#  define big_endian_float(x) x
#else

static inline unsigned int big_endian_long (unsigned int l)
{ return (l >> 24) | ((l >> 8) & 0xff00) | ((l << 8) & 0xff0000) | (l << 24); }

static inline ushort big_endian_short (ushort s)
{ return (s >> 8) | (s << 8); }

static inline float big_endian_float (float f)
{
  unsigned char tmp;
  swap_4 *pf = (swap_4 *)&f;
  tmp = pf->b1; pf->b1 = pf->b4; pf->b4 = tmp;
  tmp = pf->b2; pf->b2 = pf->b3; pf->b3 = tmp;
  return f;
}

#endif // SH_BIG_ENDIAN

#ifdef SH_LITTLE_ENDIAN
#  define little_endian_long(x)  x
#  define little_endian_short(x) x
#  define little_endian_float(x) x
#else

static inline unsigned int little_endian_long (unsigned int l)
{ return (l >> 24) | ((l >> 8) & 0xff00) | ((l << 8) & 0xff0000) | (l << 24); }

static inline ushort little_endian_short (ushort s)
{ return (s >> 8) | (s << 8); }

static inline float little_endian_float (float f)
{
  unsigned char tmp;
  swap_4 *pf = (swap_4 *)&f;
  tmp = pf->b1; pf->b1 = pf->b4; pf->b4 = tmp;
  tmp = pf->b2; pf->b2 = pf->b3; pf->b3 = tmp;
  return f;
}

#endif // SH_LITTLE_ENDIAN

static inline int float2long (float f)
{
  int exp;
  int mant = QRound ((float)frexp (f, &exp) * (float)0x1000000);
  int sign = mant & 0x80000000;
  if (mant < 0) mant = -mant;
  if (exp > 63) exp = 63; else if (exp < -64) exp = -64;
  return sign | ((exp & 0x7f) << 24) | (mant & 0xffffff);
}

static inline float long2float (int l)
{
  int exp = (l >> 24) & 0x7f;
  if (exp & 0x40) exp = exp | ~0x7f;
  float mant = float (l & 0x00ffffff) / 0x1000000;
  if (l & 0x80000000) mant = -mant;
  return (float)ldexp (mant, exp);
}

static inline short float2short (float f)
{
  int exp;
  int mant = QRound ((float)frexp (f, &exp) * (float)0x1000);
  int sign = mant & 0x8000;
  if (mant < 0) mant = -mant;
  if (exp > 7) mant = 0x7ff, exp = 7; else if (exp < -8) mant = 0, exp = -8;
  return (short)(sign | ((exp & 0xf) << 11) | (mant & 0x7ff));
}

static inline float short2float (short s)
{
  int exp = (s >> 11) & 0xf;
  if (exp & 0x8) exp = exp | ~0xf;
  float mant = float ((s & 0x07ff) | 0x0800) / 0x1000;
  if (s & 0x8000) mant = -mant;
  return (float)ldexp (mant, exp);
}

static inline unsigned int convert_endian (unsigned int l)
{ return little_endian_long (l); }

static inline int convert_endian (int l)
{ return little_endian_long (l); }

static inline int convert_endian (int i)
{ return little_endian_long (i); }

static inline ushort convert_endian (ushort s)
{ return little_endian_short (s); }

static inline float convert_endian (float f)
{ return little_endian_float (f); }

inline ushort get_le_short (void *buff)
{
#ifdef PROC_NEEDS_STRICT_ALIGNMENT
  ushort s; memcpy (&s, buff, sizeof (s));
  return little_endian_short (s);
#else
  return little_endian_short (*(ushort *)buff);
#endif
}

inline unsigned int get_le_long (void *buff)
{
#ifdef PROC_NEEDS_STRICT_ALIGNMENT
  unsigned int l; memcpy (&l, buff, sizeof (l));
  return little_endian_long (l);
#else
  return little_endian_long (*(unsigned int *)buff);
#endif
}

inline float get_le_float32 (void *buff)
{ unsigned int l = get_le_long (buff); return long2float (l); }

inline float get_le_float16 (void *buff)
{ ushort s = get_le_short (buff); return short2float (s); }

inline void set_le_short (void *buff, ushort s)
{
#ifdef PROC_NEEDS_STRICT_ALIGNMENT
  s = little_endian_short (s);
  memcpy (buff, &s, sizeof (s));
#else
  *((ushort *)buff) = little_endian_short (s);
#endif
}

inline void set_le_long (void *buff, unsigned int l)
{
#ifdef PROC_NEEDS_STRICT_ALIGNMENT
  l = little_endian_long (l);
  memcpy (buff, &l, sizeof (l));
#else
  *((unsigned int *)buff) = little_endian_long (l);
#endif
}

inline void set_le_float32 (void *buff, float f)
{ set_le_long (buff, float2long (f)); }

inline void set_le_float16 (void *buff, float f)
{ set_le_short (buff, float2short (f)); }

#endif // __SHENDIAN_H__
