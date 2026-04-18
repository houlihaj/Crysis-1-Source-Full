////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   CopyProtection.h
//  Version:     v1.00
//  Created:     21/8/2007 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#if defined(SECUROM) && defined(WIN64)
#define SECUROM_64
#endif

#if defined(SECUROM) && defined(WIN32) && !defined(WIN64)
#define SECUROM_32
#endif

#ifdef SECUROM_32
#include <C:\\SecuRom\\API\\securom_api.h>
#endif

//#define SECUROM_INCLUDE_EXE_FUNCTIONS

#if defined(SECUROM_32) || defined(SECUROM_64)

#define SECUROM_PAUL_CRC32 0x859557FA

#ifdef WIN64

#include <windows.h>

typedef enum
{
	b64_ok = 0,					//everything is ok, original detected...
	b64_err_reg,				//could not access registry (key does not exist or too less access rights)
	b64_err_reg_id_missing,		//id entry which holds encrypted hw id from launcher is missing
	b64_err_hw_id,				//could not read current hw id
	b64_err_mismatch			//stored hw id mismatches current hw id
}
b64_ret_val;

typedef b64_ret_val (__stdcall *chk_p)(int); //function pointer to entry point of b64.dll

inline int TestSecurom64()
{
	char sdll[32];
	strcpy( sdll,"b6" ); strcat( sdll,"4" ); strcat( sdll,"." ); strcat( sdll,"d" ); strcat( sdll,"l" ); strcat( sdll,"l" ); // b64.dll
	// Copy protection.
	HMODULE h_dll = LoadLibrary(sdll);
	if(!h_dll)
	{
		return -1;
	}
	char sname[32];
	strcpy( sname,"c" ); strcat( sname,"h" ); strcat( sname,"k" );
	chk_p was_securom_ok = (chk_p)GetProcAddress(h_dll,sname);
	if(!was_securom_ok)
	{
		return -1;
	}
	int ret = was_securom_ok(1);
	FreeLibrary(h_dll);
	return ret;
}

#ifdef SECUROM_INCLUDE_EXE_FUNCTIONS
//////////////////////////////////////////////////////////////////////////
// Timur.
// This is FarCry.exe authentication function, this code is not for public release!!
//////////////////////////////////////////////////////////////////////////
void AuthCheckFunction( void *data )
{
	// src and trg can be the same pointer (in place encryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit:  int key[4] = {n1,n2,n3,n4};
	// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	register unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

	// src and trg can be the same pointer (in place decryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit: int key[4] = {n1,n2,n3,n4};
	// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	register unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

	// Data assumed to be 32 bytes.
	int key1[4] = {1873613783,235688123,812763783,1745863682};
	TEA_DECODE( (unsigned int*)data,(unsigned int*)data,32,(unsigned int*)key1 );
	int key2[4] = {1897178562,734896899,156436554,902793442};
	TEA_ENCODE( (unsigned int*)data,(unsigned int*)data,32,(unsigned int*)key2 );
}

#endif //SECUROM_INCLUDE_EXE_FUNCTIONS

#else // WIN64

#ifdef SECUROM_INCLUDE_EXE_FUNCTIONS
//////////////////////////////////////////////////////////////////////////
// Timur.
// This is FarCry.exe authentication function, this code is not for public release!!
//////////////////////////////////////////////////////////////////////////
void AuthCheckFunction( void *data )
{
	//	SECUROM_MARKER_HIGH_SECURITY_ON(1)

	// src and trg can be the same pointer (in place encryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit:  int key[4] = {n1,n2,n3,n4};
	// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	register unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

	// src and trg can be the same pointer (in place decryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit: int key[4] = {n1,n2,n3,n4};
	// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	register unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

	// Data assumed to be 32 bytes.
	int key1[4] = {1873613783,235688123,812763783,1745863682};
	TEA_DECODE( (unsigned int*)data,(unsigned int*)data,32,(unsigned int*)key1 );
	int key2[4] = {1897178562,734896899,156436554,902793442};
	TEA_ENCODE( (unsigned int*)data,(unsigned int*)data,32,(unsigned int*)key2 );

	//SECUROM_MARKER_HIGH_SECURITY_OFF(1)
}

/*
//////////////////////////////////////////////////////////////////////////
void* TestProtectedFunction( void *param1,void *param2 )
{
//SECUROM_MARKER_HIGH_SECURITY_ON(2)

BOOL bTest = SecuROM_Tripwire();

if (bTest == TRUE)
{
CryLogAlways( "*SECURITY Tripwire OK" );
} else
{
CryLogAlways( "*SECURITY Tripwire FAIL" );
}


static VirtualMachine_t Sony_VM;

DWORD trigger1 = 0xFF;
Sony_VM.code_ptr = SPOT3;
Sony_VM.std.Param1 = (DWORD)&trigger1;
SECUROM_MARKER_VM_CALL(10,SPOT3,Sony_VM);

if (trigger1 == TRIGGER_FAILED)
{
CryLogAlways( "*SECURITY TRIGGER FAILED (SPOT3)" );
}
else
{
CryLogAlways( "*SECURITY TRIGGER OK (SPOT3)" );
}

DWORD trigger6 = TRIGGER_OK;
Sony_VM.code_ptr = SPOT6;
Sony_VM.std.Param1 = (DWORD)&trigger6;
SECUROM_MARKER_VM_CALL(11,SPOT6,Sony_VM);

if (trigger6 != TRIGGER_OK)
{
CryLogAlways( "*SECURITY TRIGGER FAILED (SPOT6)" );
}
else
{
CryLogAlways( "*SECURITY TRIGGER OK (SPOT6)" );
}

//SECUROM_MARKER_HIGH_SECURITY_OFF(2)

return 0;
}
*/

void* ProtectedFunction_Load( void *param1,void *param2 )
{
	IEntity *pEntity = (IEntity*)param1;
	TSerialize *pSer = (TSerialize*)param2;

	DWORD trigger6 = TRIGGER_OK;

	static VirtualMachine_t Sony_VM;
	Sony_VM.code_ptr = SPOT6;
	Sony_VM.std.Param1 = (DWORD)&trigger6;
	SECUROM_MARKER_VM_CALL(2,SPOT6,Sony_VM);

	SECUROM_MARKER_HIGH_SECURITY_ON(1)
		if (trigger6 != TRIGGER_OK)
		{
			//CryLogAlways( "*SECURITY TRIGGER FAILED (SPOT6)" );
		}
		else
		{
			pEntity->Serialize( *pSer, ENTITY_SERIALIZE_PROXIES );
			//CryLogAlways( "*SECURITY TRIGGER OK (SPOT6)" );
		}

		SECUROM_MARKER_HIGH_SECURITY_OFF(1)

			return 0;
}

#endif //SECUROM_INCLUDE_EXE_FUNCTIONS

#endif //WIN64

#endif // #if defined(SECUROM_32) || defined(SECUROM_64)