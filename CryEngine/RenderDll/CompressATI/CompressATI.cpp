#include "CompressorLib.h"

extern "C" __declspec(dllexport) void DeleteDataATI(void *pData);
extern "C" __declspec(dllexport) COMPRESSOR_ERROR CompressTextureATI(DWORD width,
                                                          DWORD height,
                                                          UNCOMPRESSED_FORMAT sourceFormat,
                                                          COMPRESSED_FORMAT destinationFormat,
                                                          void    *inputData,
                                                          void    **dataOut,
                                                          DWORD   *outDataSize);

COMPRESSOR_ERROR CompressTextureATI(DWORD width,
                                          DWORD height,
                                          UNCOMPRESSED_FORMAT sourceFormat,
                                          COMPRESSED_FORMAT destinationFormat,
                                          void    *inputData,
                                          void    **dataOut,
                                          DWORD   *outDataSize)
{
  return CompressTexture(width, height, sourceFormat, destinationFormat, inputData, dataOut, outDataSize);
}

void DeleteDataATI(void *pData)
{
  delete [] pData;
}