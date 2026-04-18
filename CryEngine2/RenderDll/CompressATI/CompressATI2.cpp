#include "CompressorLib.h"
#include "ATI_Compress.h"

extern "C" __declspec(dllexport) void DeleteDataATI(void *pData);
extern "C" __declspec(dllexport) COMPRESSOR_ERROR CompressTextureATI(DWORD width,
                                                          DWORD height,
                                                          UNCOMPRESSED_FORMAT sourceFormat,
                                                          COMPRESSED_FORMAT destinationFormat,
                                                          void    *inputData,
                                                          void    **dataOut,
                                                          DWORD   *outDataSize);

extern "C" __declspec(dllexport) COMPRESSOR_ERROR DeCompressTextureATI(DWORD width,
                                                                     DWORD height,
                                                                     COMPRESSED_FORMAT sourceFormat,
                                                                     UNCOMPRESSED_FORMAT destinationFormat,
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
  // Init source texture
  ATI_TC_Texture srcTexture;
  srcTexture.dwSize = sizeof(srcTexture);
  srcTexture.dwWidth = width;
  srcTexture.dwHeight = height;
  srcTexture.dwPitch = width*4;
  srcTexture.format = ATI_TC_FORMAT_ARGB_8888;
  srcTexture.dwDataSize = ATI_TC_CalculateBufferSize(&srcTexture);
  srcTexture.pData = (ATI_TC_BYTE *)inputData;

  // Init dest texture
  ATI_TC_Texture destTexture;
  destTexture.dwSize = sizeof(destTexture);
  destTexture.dwWidth = width;
  destTexture.dwHeight = height;
  destTexture.dwPitch = 0;
	destTexture.format = destinationFormat == FORMAT_COMP_ATI2N_DXT5 ? ATI_TC_FORMAT_DXT5 : ATI_TC_FORMAT_ATI2N;
  destTexture.dwDataSize = ATI_TC_CalculateBufferSize(&destTexture);
  destTexture.pData = new byte [destTexture.dwDataSize];

  // Compress
  ATI_TC_ERROR err = ATI_TC_ConvertTexture(&srcTexture, &destTexture, NULL, NULL, NULL, NULL);
  COMPRESSOR_ERROR errOut = COMPRESSOR_ERROR_NONE;
  switch(err)
  {
    case ATI_TC_OK:
      break;
    case ATI_TC_ERR_INVALID_SOURCE_TEXTURE:
      errOut = COMPRESSOR_ERROR_NO_INPUT_DATA;
  	  break;
    case ATI_TC_ERR_INVALID_DEST_TEXTURE:
      errOut = COMPRESSOR_ERROR_NO_OUTPUT_POINTER;
      break;
    case ATI_TC_ERR_UNSUPPORTED_SOURCE_FORMAT:
      errOut = COMPRESSOR_ERROR_UNSUPPORTED_SOURCE_FORMAT;
      break;
    case ATI_TC_ERR_UNSUPPORTED_DEST_FORMAT:
      errOut = COMPRESSOR_ERROR_UNSUPPORTED_DESTINATION_FORMAT;
      break;
    case ATI_TC_ERR_UNABLE_TO_INIT_CODEC:
      errOut = COMPRESSOR_ERROR_UNABLE_TO_INIT_CODEC;
      break;
    default:
      errOut = COMPRESSOR_ERROR_GENERIC;
      break;
  }
  *dataOut = destTexture.pData;
  *outDataSize = destTexture.dwDataSize;

  return errOut;
}

COMPRESSOR_ERROR DeCompressTextureATI(DWORD width,
                                    DWORD height,
                                    COMPRESSED_FORMAT sourceFormat,
                                    UNCOMPRESSED_FORMAT destinationFormat,
                                    void    *inputData,
                                    void    **dataOut,
                                    DWORD   *outDataSize)
{
  // Init source texture
  ATI_TC_Texture srcTexture;
  srcTexture.dwSize = sizeof(srcTexture);
  srcTexture.dwWidth = width;
  srcTexture.dwHeight = height;
  srcTexture.dwPitch = width*4;
  srcTexture.format = ATI_TC_FORMAT_ATI2N;
  srcTexture.dwDataSize = ATI_TC_CalculateBufferSize(&srcTexture);
  srcTexture.pData = (ATI_TC_BYTE *)inputData;

  // Init dest texture
  ATI_TC_Texture destTexture;
  destTexture.dwSize = sizeof(destTexture);
  destTexture.dwWidth = width;
  destTexture.dwHeight = height;
  destTexture.dwPitch = 0;
  destTexture.format = ATI_TC_FORMAT_ARGB_8888;
  destTexture.dwDataSize = ATI_TC_CalculateBufferSize(&destTexture);
  destTexture.pData = new byte [destTexture.dwDataSize];

  // Compress
  ATI_TC_ERROR err = ATI_TC_ConvertTexture(&srcTexture, &destTexture, NULL, NULL, NULL, NULL);
  COMPRESSOR_ERROR errOut = COMPRESSOR_ERROR_NONE;
  switch(err)
  {
  case ATI_TC_OK:
    break;
  case ATI_TC_ERR_INVALID_SOURCE_TEXTURE:
    errOut = COMPRESSOR_ERROR_NO_INPUT_DATA;
    break;
  case ATI_TC_ERR_INVALID_DEST_TEXTURE:
    errOut = COMPRESSOR_ERROR_NO_OUTPUT_POINTER;
    break;
  case ATI_TC_ERR_UNSUPPORTED_SOURCE_FORMAT:
    errOut = COMPRESSOR_ERROR_UNSUPPORTED_SOURCE_FORMAT;
    break;
  case ATI_TC_ERR_UNSUPPORTED_DEST_FORMAT:
    errOut = COMPRESSOR_ERROR_UNSUPPORTED_DESTINATION_FORMAT;
    break;
  case ATI_TC_ERR_UNABLE_TO_INIT_CODEC:
    errOut = COMPRESSOR_ERROR_UNABLE_TO_INIT_CODEC;
    break;
  default:
    errOut = COMPRESSOR_ERROR_GENERIC;
    break;
  }
  *dataOut = destTexture.pData;
  *outDataSize = destTexture.dwDataSize;

  return errOut;
}

void DeleteDataATI(void *pData)
{
  delete [] pData;
}
