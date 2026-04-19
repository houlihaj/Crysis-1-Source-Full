#ifndef _RANDOM_H_
#define _RANDOM_H_


/***********
Random Number Generation Code
***********/
#define LONGRAND_MAX    2147483647L   /* 2**31 - 1 */

/**************
** FUNCTIONS **
**************/

extern long	nextlongrand	(long seed);
//extern long	longrand		();							/* return next random long */
extern void	Util_RandSeed	(unsigned long seed);		/* to seed it */
extern int	Util_RandInt	(int low, int high);

#endif