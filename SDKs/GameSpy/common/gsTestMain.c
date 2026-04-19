#include "gsCommon.h"
#include "gsMemory.h"
#include "gsDebug.h"



// sample common entry point
extern int test_main(int argc, char ** argp); 


int gsTestMain(int argc, char ** argp);

int gsTestMain(int argc, char ** argp)
{
	int r;

	// pre

	r = test_main(argc,argp);

	// post

	return r;
}
