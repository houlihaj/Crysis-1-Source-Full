
#ifndef DDSIMAGE_H
#define DDSIMAGE_H

#include "ImageExtensionHelper.h"

/**
 * An ImageFile subclass for reading DDS files.
 */
class CImageDDSFile : public CImageFile
{
  ///
  friend class CImageFile;	// For constructor

private:

public:
  /// Read the DDS file from the buffer.
  CImageDDSFile (byte* buf, FILE *fp, int size, uint nFlags);
  int mfSizeWithMips(int filesize, int sx, int sy, int nDepth, int numMips, ETEX_Format eTF);
///
  virtual ~CImageDDSFile ();

private: // ------------------------------------------------------------------------------

	static uint32 GetMipCount( CImageExtensionHelper::DDS_HEADER *ddsh );
	static ETEX_Format GetTextureFormat( CImageExtensionHelper::DDS_HEADER *ddsh );
};

void WriteDDS(byte *dat, int wdt, int hgt, int dpth, int Size, char *name, ETEX_Format eF, int NumMips, ETEX_Type eTT);

#endif


