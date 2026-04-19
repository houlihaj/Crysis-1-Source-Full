/***********
Random Number Generation Code
***********/
#include "gsRandom.h"
#define RANa            16807         /* multiplier */


static long randomnum = 1;

/**************
** FUNCTIONS **
**************/
long nextlongrand(long seed)
{
	unsigned long lo, hi;
	
	lo = RANa * (long)(seed & 0xFFFF);
	hi = RANa * (long)((unsigned long)seed >> 16);
	lo += (hi & 0x7FFF) << 16;
	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	lo += hi >> 15;
	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	return (long)lo;
}

long longrand(void)                     /* return next random long */
{
	randomnum = nextlongrand(randomnum);
	return randomnum;
}

void Util_RandSeed(unsigned long seed)      /* to seed it */
{
	randomnum = seed ? (seed & LONGRAND_MAX) : 1;  /* nonzero seed */
}

int Util_RandInt(int low, int high)
{
	int  range = high-low;
	int  num = longrand() % range;

	return(num + low);
}
