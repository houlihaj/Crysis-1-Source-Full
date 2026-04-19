////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// (C) Gamespy Industries
// 
// Commonly shared network startup code
// WARNING: Please do not use this code as a basis for 
// Game Network startup code.  This is only for testing
// purposes.
//
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#include "../gsCommon.h"
#include <demo.h>
#include <string.h>
#include <revolution/so.h>
#include <revolution/ncd.h>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static NCDIfConfig      gIfConfig;
static NCDIpConfig      gIpConfig;
static OSMutex          gOsMemMutex;


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void SetDefaultIfConfig( NCDIfConfig* theIfConfig )
{
	(void)memset( theIfConfig, 0, sizeof( NCDIfConfig ) );

	theIfConfig->selectedMedia	= NCD_IF_SELECT_WIRED;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void SetDefaultIpConfig( NCDIpConfig* theIpConfig )
{

	(void)memset( theIpConfig, 0, sizeof( NCDIpConfig ) );

	theIpConfig->useDhcp  = TRUE;

	theIpConfig->adjust.maxTransferUnit       = 1300; // Value can be 1460 depending on network library 
	theIpConfig->adjust.tcpRetransTimeout     = 100;
	theIpConfig->adjust.dhcpRetransCount      = 4;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Memory functions used by the network library


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void* _Alloc(unsigned long theName, long theSize)
{
    void*       a_memory_p = NULL;
    
    (void)theName;     
        
    if (0 < theSize) 
	{
        OSLockMutex(&gOsMemMutex);
        a_memory_p = OSAlloc((u32)theSize);
        OSUnlockMutex(&gOsMemMutex);
    }
    
    return a_memory_p;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void _Free(unsigned long theName, void* theMemoryPointer, long theSize)
{
    (void)theName; 
    
    if (theMemoryPointer && 0 < theSize) 
	{
        OSLockMutex(&gOsMemMutex);
        OSFree(theMemoryPointer);
        OSUnlockMutex(&gOsMemMutex);
    }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// StartNetwork
//
BOOL StartNetwork()
{
    s32 rc;
    
    OSInitMutex(&gOsMemMutex);

    // Start up the network interface USB device
    SetDefaultIfConfig( &gIfConfig );
    OSReport( "NCDSetIfConfig() ..." );
    rc = NCDSetIfConfig( &gIfConfig );
    if( rc != NCD_RESULT_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    
    SetDefaultIpConfig( &gIpConfig );
    OSReport( "NCDSetIpConfig() ..." );
    rc = NCDSetIpConfig( &gIpConfig );
    if( rc != NCD_RESULT_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    
	// Waiting for the network interface to come with the 
	// configuration previously set.
    OSReport( "NCDIsInterfaceDecided() " );
    while( NCDIsInterfaceDecided() == FALSE )
    {
        NCDSleep( OSMillisecondsToTicks( 10 ) );
    }
    OSReport( "success\n" );

    
	// Finished bringing up network interface
	// Now initializing the socket library
    OSReport( "SOInit() ..." );
    {
        SOLibraryConfig soLibConfig;

        (void)memset(&soLibConfig, 0, sizeof(soLibConfig));
        soLibConfig.alloc = _Alloc;
        soLibConfig.free  = _Free;

        rc = SOInit(&soLibConfig);
        if( rc != SO_SUCCESS )
        {
            OSReport( "failed (%d)\n", rc );
            return FALSE;
        }
    }
    OSReport( "success\n" );
    

	//Now Starting the sockets library
    OSReport( "SOStartup() " );
    rc = SOStartup();
    if( rc != SO_SUCCESS )
    {
        OSReport( "failed (%d)\n", rc );
        return FALSE;
    }
    OSReport( "success\n" );

    // Getting the IP address of the local machine via DHCP
    while( SOGetHostID() == 0 )
    {
        NCDSleep( OSMillisecondsToTicks( 10 ) );
    }
    {
        SOInAddr addr;
        addr.addr = (u32)SOGetHostID();
        OSReport( "IP address = %s\n", SOInetNtoA( addr ) );
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// StopNetwork
//
void StopNetwork()
{
    (void)SOCleanup();
}


extern int test_main(int argc, char **argv);
void main(int argc, char **argv)
{
	DEMOInit(NULL);
	if (!StartNetwork())
		OSHalt("Network was not started\n");
	OSReport("=======================================================================\n");
	OSReport("Gamespy:// Starting App\n");
	test_main(argc, argv);
	OSReport("=======================================================================\n");
	OSReport("Gamespy:// Ending App\n");
	StopNetwork();
}