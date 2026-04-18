
#ifndef CIMAGE_H
#define CIMAGE_H

#define CHK(x) x

#define SH_LITTLE_ENDIAN

// The mask for extracting just R/G/B from an ulong or SRGBPixel
#ifdef SH_BIG_ENDIAN
#  define RGB_MASK 0xffffff00
#else
#  define RGB_MASK 0x00ffffff
#endif

/**
 * An RGB pixel.
 */
struct SRGBPixel
{
  uchar blue, green, red, alpha;
  SRGBPixel () /* : red(0), green(0), blue(0), alpha(255) {} */
  { *(unsigned int *)this = (unsigned int)~RGB_MASK; }
  SRGBPixel (int r, int g, int b) : red (r), green (g), blue (b), alpha (255) {}
  bool eq (const SRGBPixel& p) const { return ((*(unsigned int *)this) & RGB_MASK) == ((*(unsigned int *)&p) & RGB_MASK); }
  /// Get the pixel intensity
  int Intensity () { return (red + green + blue) / 3; }
};


/**
 * An RGB palette entry with statistics information.
 */
struct SRGBPalEntry
{
  uchar red, green, blue;
  uint count;
};

/**
 * Possible errors for CImageFile::mfGet_error.
 */
enum EImFileError { eIFE_OK = 0, eIFE_IOerror, eIFE_OutOfMemory, eIFE_BadFormat, eIFE_ResourceCompiler };

/// Red component sensivity
#define R_COEF		173
/// Green component sensivity
#define G_COEF		242
/// Blue component sensivity
#define B_COEF		107
/// Eye sensivity to different color components, squared
/// Red component sensivity, squared
#define R_COEF_SQ	299
/// Green component sensivity, squared
#define G_COEF_SQ	587
/// Blue component sensivity, squared
#define B_COEF_SQ	114

#define FIM_NORMALMAP						0x0001
#define FIM_DSDT								0x0002
#define FIM_NOTSUPPORTS_MIPS		0x0004
#define FIM_ALPHA								0x0008	// even if not requested when loading 3dc we store the info if ALPHA would be available so we know for later
#define FIM_DECAL								0x0010
#define FIM_GREYSCALE						0x0020	// hint this texture is greyscale (could be DXT1 with colored artefacts)
#define FIM_DONTSTREAM					0x0040	// to propaget to texture flag, FT_DONT_STREAM
#define FIM_LAST4MIPS						0x0080
#define FIM_ALLOWSTREAMING			0x0040	// allow streaming
#define FIM_LASTMIPS						0x0080
#define FIM_FILESINGLE					0x0100	// info from rc: no need to search for other files (e.g. DDNDIF)
#define FIM_BIG_ENDIANNESS			0x0400	// for textures converted to big endianness format
#define FIM_SPLITTED						0x0800	// for dds textures stored in splitted files
#define FIM_SRGB_READ						0x1000

class CImageFile
{
friend class CImageDDSFile;
friend class CImageCCTFile;
friend class CImageBmpFile;
friend class CImagePcxFile;
friend class CImageJpgFile;
friend class CTexMan;

private:
  /// Width of image.
  int m_Width;
  /// Height of image.
  int m_Height;
  /// Depth of image.
  int m_Depth;

  int m_Bps;
  int m_ImgSize;

  int m_NumMips;
  int m_Flags;					// e.g. FIM_GREYSCALE|FIM_ALPHA
  ColorF m_AvgColor;		// average texture color (r,g,b,a independently) loaded from image file
  int m_nStartSeek;

  /// The image data.
  union 
  {
    SRGBPixel* m_pPixImage[6];
    byte* m_pByteImage[6];
  };

  /// Last error code.
  static EImFileError m_eError;
  /// Last error detail information.
  static char m_Error_detail[256];

protected:

  ETEX_Format m_eFormat;
  ETEX_Format m_eSrcFormat;

  CImageFile ();

  static void mfSet_error (EImFileError error, char* detail = NULL);

  void mfSet_dimensions (int w, int h);

public:
  static char m_CurFileName[128];
  char m_FileName[128];

  ///
  virtual ~CImageFile ();

  ///
  int mfGet_width () { return m_Width; }
  ///
  int mfGet_height () { return m_Height; }
  ///
  int mfGet_depth () { return m_Depth; }
  ///
  byte* mfGet_image (int nSide)
  {
    if (!m_pByteImage[nSide])
    {
      if (m_ImgSize)
      {
        if (CTexture::m_pLoadData && CTexture::m_nLoadOffset + m_ImgSize < TEXPOOL_LOADSIZE)
        {
          m_pByteImage[nSide] = &CTexture::m_pLoadData[CTexture::m_nLoadOffset];
          CTexture::m_nLoadOffset += m_ImgSize;
        }
        else
          m_pByteImage[nSide] = new byte [m_ImgSize];
      }
    }
    return m_pByteImage[nSide];
  }
  bool mfIs_image (int nSide)
  {
    if (!m_pByteImage[nSide])
      return false;
    return true;
  }

  ///
  ColorF mfGet_AvgColor () { return m_AvgColor; }
  int mfGet_StartSeek () { return m_nStartSeek; }

  int mfGet_bps () { return m_Bps; }
  void mfSet_bps(int b) { m_Bps = b; }
  void mfSet_ImageSize (int Size) {m_ImgSize = Size;}
  int mfGet_ImageSize () {return m_ImgSize;}
  
  ETEX_Format mfGetFormat() { return m_eFormat; }
  ETEX_Format mfGetSrcFormat() { return m_eSrcFormat; }

  void mfSet_numMips (int num) { m_NumMips = num; }
  int  mfGet_numMips (void) { return m_NumMips; }
  void mfSet_Flags (int Flags) { m_Flags |= Flags; }
  int mfGet_Flags () { return m_Flags; }

  ///
  static EImFileError mfGet_error () { return m_eError; }
  ///
  static char* mfGet_error_detail () { return m_Error_detail ? m_Error_detail : (char *)""; }
  /// Write a message describing the error on screen.
  static void mfWrite_error (char* extra);

  static CImageFile* mfLoad_file( const char* filename, const bool bReload, uint nFlags);

  static CImageFile* mfLoad_file (FILE* fp, uint nFlags);

  static CImageFile* mfLoad_file (byte* buf, int size, uint nFlags);
};

#endif

