#ifndef _COMPRESSED_DATA_WAVELET
#define _COMPRESSED_DATA_WAVELET


#include "Wavelet.h"

class CWaveRDC : public CWaveletData
{

public:


	CWaveRDC();

	virtual ~CWaveRDC();


	virtual int DumpZ(const char*, int = 0);

	void ResizeZ(int n);

	int Compress(const CWaveletData &, bool fast=false);
	//{
	//	return 1;
	//}

	int UnCompress(CWaveletData &, int level = 1);
	//{
	//	return 1;
	//}

	void Dir(int v = 1);

private:

	// push data to compress stream
	int Push(int *, int, unsigned int *, int &, int, int);
	void Push(unsigned int &, unsigned int *, int &, int);
	int  Pop(int *, int, int &, int, int);
	void Pop(unsigned int &, int &, int);
	inline int wabs(int &i){return i>0 ? i : -i;} 

public:
	int m_nLayers;		// number of layers in compressed array dataz
	int m_Options;		// current layer compression options
	std::vector<unsigned int> m_DataZ; // compressed data array

private:
	// number of 32-bits layer service words (lsw)
	int m_nLSW;             
	// free bits in the last word of current block
	int m_nFreeBits;				 
	// current layer encoding bit length for large integers
	int m_nLargeInts;               
	// current layer encoding bit length for small integers
	int m_nSmallInts;          
	// length of the Block Service Word
	int m_nBSW;								 
	// current layer shift subtracted from original data
	short int m_iShift;             
	// number that encodes 0;
	short int m_iZero;              

};



#endif