//--------------------------------------------------------------------------------------
// AtgUtil.cpp
//
// Helper functions and typing shortcuts for samples
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "StdAfx.h"

#ifdef SOUNDSYSTEM_USE_XENON_XAUDIO

#include "AtgUtil.h"

namespace ATG
{

// Global access to the main D3D device
extern LPDIRECT3DDEVICE9 g_pd3dDevice;

// Static shaders used for helper functions
static D3DVertexDeclaration* g_pGradientVertexDecl   = NULL;
static D3DVertexShader*      g_pGradientVertexShader = NULL;
static D3DPixelShader*       g_pGradientPixelShader  = NULL;

// Flag that can be modified externally to control whether execution should continue
// even after a fatal error
BOOL g_bContinueOnError = FALSE;



//--------------------------------------------------------------------------------------
// Name: DebugSpewV()
// Desc: Internal helper function
//--------------------------------------------------------------------------------------
static VOID DebugSpewV( const CHAR* strFormat, const va_list pArgList )
{
    CHAR str[2048];
    // Use the secure CRT to avoid buffer overruns. Specify a count of
    // _TRUNCATE so that too long strings will be silently truncated
    // rather than triggering an error.
    vsnprintf_s( str, ARRAYSIZE(str), _TRUNCATE, strFormat, pArgList );
    OutputDebugStringA( str );
}


//--------------------------------------------------------------------------------------
// Name: DebugSpew()
// Desc: Prints formatted debug spew
//--------------------------------------------------------------------------------------
VOID CDECL DebugSpew( const CHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    DebugSpewV( strFormat, pArgList );
    va_end( pArgList );
}


//--------------------------------------------------------------------------------------
// Name: FatalError()
// Desc: Prints formatted debug spew and breaks into the debugger
//--------------------------------------------------------------------------------------
VOID CDECL FatalError( const CHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    DebugSpewV( strFormat, pArgList );
    va_end( pArgList );

    ATG_DebugBreak();

    /*
    // Optionally exit the app
    if( FALSE == g_bContinueOnError )
        exit(0);
        */
}



//--------------------------------------------------------------------------------------
// Name: LoadFile()
// Desc: Helper function to load a file
//--------------------------------------------------------------------------------------
HRESULT LoadFile( const CHAR* strFileName, VOID** ppFileData,
                          DWORD* pdwFileSize )
{
    assert( ppFileData );
    if( pdwFileSize )
        *pdwFileSize = 0L;

    // Open the file for reading
    HANDLE hFile = CreateFile( strFileName, GENERIC_READ, 0, NULL, 
                               OPEN_EXISTING, 0, NULL );
    
    if( INVALID_HANDLE_VALUE == hFile )
    {
        ATG_PrintError( "Unable to open file %s\n", strFileName );
        return E_FAIL;
    }

    DWORD dwFileSize = GetFileSize( hFile, NULL );
    VOID* pFileData = malloc( dwFileSize );

    if( NULL == pFileData )
    {
        CloseHandle( hFile );
        ATG_PrintError( "Unable to open allocate memory for file %s\n", strFileName );
        return E_OUTOFMEMORY;
    }

    DWORD dwBytesRead;
    ReadFile( hFile, pFileData, dwFileSize, &dwBytesRead, NULL );
    
    // Finished reading file
    CloseHandle( hFile ); 

    if( dwBytesRead != dwFileSize )
    {
        ATG_PrintError( "Unable to read file %s\n", strFileName );
        return E_FAIL;
    }

    if( pdwFileSize )
        *pdwFileSize = dwFileSize;
    *ppFileData = pFileData;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UnloadFile()
// Desc: Matching unload
//--------------------------------------------------------------------------------------
VOID UnloadFile( VOID* pFileData )
{
    assert( pFileData != NULL );
    free( pFileData );
}


//--------------------------------------------------------------------------------------
// Name: SaveFile()
// Desc: Helper function to save a file
//--------------------------------------------------------------------------------------
HRESULT SaveFile( const CHAR* strFileName, VOID* pFileData, DWORD dwFileSize )
{
    // Open the file for reading
    HANDLE hFile = CreateFile( strFileName, GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, 0, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
    {
        ATG_PrintError( "Unable to open file %s\n", strFileName );
        return E_FAIL;
    }

    DWORD dwBytesWritten;
    WriteFile( hFile, pFileData, dwFileSize, &dwBytesWritten, NULL );
    
    // Finished reading file
    CloseHandle( hFile ); 

    if( dwBytesWritten != dwFileSize )
    {
        ATG_PrintError( "Unable to write file %s\n", strFileName );
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetVertexElement()
// Desc: Helper function for creating vertex declarations
//--------------------------------------------------------------------------------------
inline D3DVERTEXELEMENT9 SetVertexElement( WORD &Offset, DWORD Type,
                                           BYTE Usage, BYTE UsageIndex )
{
    D3DVERTEXELEMENT9 Element;
    Element.Stream     = 0;
    Element.Offset     = Offset;
    Element.Type       = Type;
    Element.Method     = D3DDECLMETHOD_DEFAULT;
    Element.Usage      = Usage;
    Element.UsageIndex = UsageIndex;

    switch( Type )
    {
        case D3DDECLTYPE_FLOAT1:   Offset += 1*sizeof(FLOAT); break;
        case D3DDECLTYPE_FLOAT2:   Offset += 2*sizeof(FLOAT); break;
        case D3DDECLTYPE_FLOAT3:   Offset += 3*sizeof(FLOAT); break;
        case D3DDECLTYPE_FLOAT4:   Offset += 4*sizeof(FLOAT); break;
        case D3DDECLTYPE_D3DCOLOR: Offset += 1*sizeof(DWORD); break;
    }
    return Element;
}


//--------------------------------------------------------------------------------------
// Name: BuildVertexDeclFromFVF()
// Desc: Helper function to create vertex declarations
//--------------------------------------------------------------------------------------
VOID BuildVertexDeclFromFVF( DWORD dwFVF, D3DVERTEXELEMENT9* pDecl )
{
    WORD wOffset = 0;

    // Handle position
    switch( dwFVF & D3DFVF_POSITION_MASK )
    {
        case D3DFVF_XYZ:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0 ); break;
        case D3DFVF_XYZW:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0 ); break;
        case D3DFVF_XYZB1:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0 );
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_BLENDWEIGHT, 0 ); break;
        case D3DFVF_XYZB2:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0 );
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_BLENDWEIGHT, 0 ); break;
        case D3DFVF_XYZB3:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0 );
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_BLENDWEIGHT, 0 ); break;
        case D3DFVF_XYZB4:
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0 );
            *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_BLENDWEIGHT, 0 ); break;
    }

    // Handle normal, diffuse, and specular
    if( dwFVF & D3DFVF_NORMAL )    *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3,   D3DDECLUSAGE_NORMAL, 0 );
    if( dwFVF & D3DFVF_DIFFUSE )   *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0 );
    if( dwFVF & D3DFVF_SPECULAR )  *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1 );

    // Handle texture coordinates
    DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    for( DWORD i=0; i<dwNumTextures; i++ )
    {
        LONG lTexCoordSize = ( dwFVF & (0x00030000<<(i*2)) );

        if( lTexCoordSize == D3DFVF_TEXCOORDSIZE1(i) ) *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_TEXCOORD, (BYTE)i );
        if( lTexCoordSize == D3DFVF_TEXCOORDSIZE2(i) ) *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, (BYTE)i );
        if( lTexCoordSize == D3DFVF_TEXCOORDSIZE3(i) ) *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, (BYTE)i );
        if( lTexCoordSize == D3DFVF_TEXCOORDSIZE4(i) ) *pDecl++ = SetVertexElement( wOffset, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, (BYTE)i );
    }

    // End the declarator
    pDecl->Stream     = 0xff;
    pDecl->Offset     = 0;
    pDecl->Type       = (DWORD)D3DDECLTYPE_UNUSED;
    pDecl->Method     = 0;
    pDecl->Usage      = 0;
    pDecl->UsageIndex = 0;
}



//--------------------------------------------------------------------------------------
// Vertex and pixel shaders for gradient background rendering
//--------------------------------------------------------------------------------------
static const CHAR* g_strGradientShader =
"struct VS_IN                                              \n"
"{                                                         \n"
"   float4   Position     : POSITION;                      \n"
"   float4   Color        : COLOR0;                        \n"
"};                                                        \n"
"                                                          \n"
"struct VS_OUT                                             \n"
"{                                                         \n"
"   float4 Position       : POSITION;                      \n"
"   float4 Diffuse        : COLOR0;                        \n"
"};                                                        \n"
"                                                          \n"
"VS_OUT GradientVertexShader( VS_IN In )                   \n"
"{                                                         \n"
"   VS_OUT Out;                                            \n"
"   Out.Position = In.Position;                            \n"
"   Out.Diffuse  = In.Color;                               \n"
"   return Out;                                            \n"
"}                                                         \n"
"                                                          \n"
"                                                          \n"
"float4 GradientPixelShader( VS_OUT In ) : COLOR0          \n"
"{                                                         \n"
"   return In.Diffuse;                                     \n"
"}                                                         \n";


//--------------------------------------------------------------------------------------
// Name: LoadVertexShader()
// Desc: Loads pre-compiled vertex shader microcode from the specified file and
//       creates a vertex shader resource.
//--------------------------------------------------------------------------------------
//HRESULT LoadVertexShader( const CHAR* strFileName, LPDIRECT3DVERTEXSHADER9* ppVS )
//{
//    HRESULT hr;
//    VOID* pCode = NULL;
//
//    if( FAILED( hr = LoadFile( strFileName, &pCode ) ) )
//        return hr;
//    if( FAILED( hr = g_pd3dDevice->CreateVertexShader( (DWORD*)pCode, ppVS ) ) )
//    {
//        UnloadFile( pCode );
//        return hr;
//    }
//    UnloadFile( pCode );
//
//    return hr;
//}




//-----------------------------------------------------------------------------
// Endian-swapping functions
//-----------------------------------------------------------------------------
inline WORD ReverseEndianness( WORD in )
{
    WORD out;
    ((BYTE*)&out)[0] = ((BYTE*)&in)[1];
    ((BYTE*)&out)[1] = ((BYTE*)&in)[0];
    return out;
}

inline DWORD ReverseEndianness( DWORD in )
{
    DWORD out;
    ((BYTE*)&out)[0] = ((BYTE*)&in)[3];
    ((BYTE*)&out)[1] = ((BYTE*)&in)[2];
    ((BYTE*)&out)[2] = ((BYTE*)&in)[1];
    ((BYTE*)&out)[3] = ((BYTE*)&in)[0];
    return out;
}


//-----------------------------------------------------------------------------
// Name: DumpTexture()
// Desc: Writes the contents of a 32-bit texture to a .tga file.
//-----------------------------------------------------------------------------
/*
HRESULT DumpTexture( LPDIRECT3DTEXTURE9 pTexture, const CHAR* strFileName,
                             BOOL bLittleEndian )
{
    // Get the surface description. Make sure it's a 32-bit format
    XGTEXTURE_DESC desc;
    XGGetTextureDesc( pTexture, 0, &desc );

    // Lock the surface
    D3DLOCKED_RECT lock;
    pSurface->LockRect( &lock, 0, 0 );

    // Allocate memory for storing the surface bits
    VOID* pUntiledBits = (VOID*)new BYTE[desc.SlicePitch];

    // Unswizzle the bits, if necessary
    if( XGIsTiledFormat( desc.Format ) )
    {
        RECT rect = { 0, 0, desc.WidthInBlocks, desc.HeightInBlocks };
        XGUntileSurface( pUntiledBits, desc.RowPitch, NULL, lock.pBits,
                         desc.WidthInBlocks, desc.HeightInBlocks, &rect,
                         desc.BytesPerBlock );
    }
    else
        memcpy( pUntiledBits, lock.pBits, desc.SlicePitch );

    DWORD* pBits = new DWORD[desc.Width*desc.Height];
    XGCopySurface( pBits, sizeof(DWORD)*desc.Width, desc.Width, desc.Height,
                   D3DFMT_LIN_A8R8G8B8, NULL, pUntiledBits, desc.RowPitch,
                   (D3DFORMAT)MAKELINFMT(desc.Format), NULL, 0L, 0.0f );
    delete[] pUntiledBits;

    // Unlock the surface
    pSurface->UnlockRect();

    // Setup the TGA file header
    struct TargaHeader
    {
        BYTE IDLength;
        BYTE ColormapType;
        BYTE ImageType;
        BYTE ColormapSpecification[5];
        WORD XOrigin;
        WORD YOrigin;
        WORD ImageWidth;
        WORD ImageHeight;
        BYTE PixelDepth;
        BYTE ImageDescriptor;
    } tgaHeader;

    ZeroMemory( &tgaHeader, sizeof(tgaHeader) );
    tgaHeader.IDLength        = 0;
    tgaHeader.ImageType       = 2;
    tgaHeader.ImageWidth      = (WORD)desc.Width;
    tgaHeader.ImageHeight     = (WORD)desc.Height;
    tgaHeader.PixelDepth      = 32;
    tgaHeader.ImageDescriptor = 0x28;

    if( bLittleEndian )
    {
        tgaHeader.ImageWidth  = ReverseEndianness( tgaHeader.ImageWidth );
        tgaHeader.ImageHeight = ReverseEndianness( tgaHeader.ImageHeight );
        DWORD* pData = (DWORD*)pBits;
        for( DWORD i=0; i<desc.Width*desc.Height; i++ )
            pData[i] = ReverseEndianness( pData[i] );
    }

    // Create a new file
    FILE* file = fopen( strFileName, "wb" );
    if( NULL == file )
    {
        delete[] pBits;
        return E_FAIL;
    }

    // Write the Targa header and the surface pixels to the file
    fwrite( &tgaHeader, sizeof(TargaHeader), 1, file );
    fwrite( pBits, sizeof(BYTE), sizeof(DWORD)*desc.Width*desc.Height, file );
    fclose( file );

    // Cleanup and return
    delete[] pBits;
    return S_OK;
}
*/

} // namespace ATG

#endif
