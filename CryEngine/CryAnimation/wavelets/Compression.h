#ifndef _COMPRESSION_WAVELET_
#define _COMPRESSION_WAVELET_


#include "Wavelet.h"
#include "DaubechiWavelet.h"

//int Compress(CWaveletData &, int* &, int=0, int=0,
//						 const double=1., const double=1., int=10, int=10);


typedef std::vector<int> TCompressetWaveletData;


namespace Wavelet
{

	int Compress(CWaveletData &in, TCompressetWaveletData &out, int Lwt, int Lbt,
		const TWavereal g1, const TWavereal g2, int np1, int np2);

	int UnCompress(TCompressetWaveletData& in, CWaveletData &out);

	// Compression uses DCT
	int CompressDCT(CWaveletData &in, TCompressetWaveletData &out, const TWavereal g);
	// decompression uses DCT
	int UnCompressDCT(TCompressetWaveletData &in, CWaveletData &out);
};

#endif