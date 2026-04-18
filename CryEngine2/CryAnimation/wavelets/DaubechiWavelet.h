////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   DaubechiWavelet.h
//  Version:     v1.00
//  Created:     27/7/2006 by Alexey Medvedev.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _DAUBECHI_WAVELET
#define _DAUBECHI_WAVELET


#include "Wavelet.h"


// Daubechi wavelet realization
class CDaubechiDWT : public CDWT
{
public:

	//ctors
	CDaubechiDWT(int ndw, bool tree);
	// num - number of samples, ndw - type of wavelet, tree - tree partitioning, border - border type
	CDaubechiDWT(int num, int ndw, bool tree);
	// num - number of samples, ndw - type of wavelet, tree - tree partitioning, border - border type
	CDaubechiDWT(int num ,int ndw,bool tree,EBorderBehavior border);

	// One step of decomposition
	virtual void Decompose(int,int);  
	// One step of reconstruction
	virtual void Reconstruct(int,int);

//private:

	void	Init(int);

//private:
	TWavereal	* m_Filter;
};


#endif

