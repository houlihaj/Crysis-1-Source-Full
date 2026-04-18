#include "StdAfx.h"

#if defined(OPENGL)

#include "../GLBase.h"
#include <limits>
#undef min
#undef max

namespace NDXTCompression
{
	typedef struct Color888
	{
		uint8 r;		
		uint8 g;		
		uint8 b;		
	} Color888;

	inline const uint32 Distance(Color888 *pCol1, Color888 *pCol2)
	{
		return  (pCol1->r - pCol2->r) * (pCol1->r - pCol2->r) +
			(pCol1->g - pCol2->g) * (pCol1->g - pCol2->g) +
			(pCol1->b - pCol2->b) * (pCol1->b - pCol2->b);
	}

	inline void GetAlphaBlock(uint8 *pBlock, uint8 *pData, const uint32 cWidth, const uint32 cXPos, const uint32 cYPos)
	{
		uint32 i=0;
		for(int y=0; y<4; ++y) 
		{
			for(int x=0; x<4; ++x) 
			{
				const uint32 cOffset = (cYPos + y) * cWidth + (cXPos + x);
				pBlock[i++] = pData[cOffset];
			}
		}
	}

	inline void GetBlock(uint16 *pBlock, uint16 *pData, const uint32 cWidth, const uint32 cXPos, const uint32 cYPos)
	{
		uint32 i=0;
		for(int y=0; y<4; ++y) 
		{
			for(int x=0; x<4; ++x)
			{
				const uint32 cOffset = (cYPos + y) * cWidth + (cXPos + x);
				pBlock[i++] = pData[cOffset];
			}
		}
	}

	inline void CompressTo565(uint16 *pData, uint8 const* cpSrc, const uint32 cWidth, const uint32 cHeight)
	{
		for (int i=0, j=0; i<cWidth * cHeight * 4; i+=4, ++j) 
		{
			pData[j]  = ((cpSrc[i]	>> 3) << 11);
			pData[j] |= ((cpSrc[i+1] >> 2) << 5);
			pData[j] |= (cpSrc[i+2] >> 3);
		}
	}

	inline void GetAlpha(uint8 *pAlpha, uint8 const* cpSrc, const uint32 cWidth, const uint32 cHeight)
	{
		for (int i=3, j=0; i<cWidth * cHeight * 4; i+=4, ++j)
			pAlpha[j] = cpSrc[i];
	}

	inline void Saveuint16(uint8*& rpDest, const uint16& crVal)
	{
		uint16 *pDest = (uint16*)rpDest;
		*pDest++ = crVal;
		rpDest = (uint8*)pDest;
	}

	inline void Saveuint32(uint8*& rpDest, const uint32& crVal)
	{
		uint32 *pDest = (uint32*)rpDest;
		*pDest++ = crVal;
		rpDest = (uint8*)pDest;
	}

	inline void ShortToColor888(const uint16 cPixel, Color888 *pColour)
	{
		pColour->r = ((cPixel & 0xF800) >> 11) << 3;
		pColour->g = ((cPixel & 0x07E0) >> 5)  << 2;
		pColour->b = ((cPixel & 0x001F))       << 3;
	}

	uint32 GenBitMask(const uint16 cEx0, const uint16 cEx1, const uint32 cNumCols, const uint16 *cpIn, const uint8 *cpAlpha, Color888 *pOutCol)
	{
		uint32 closest, dist, bitMask = 0;
		uint8	mask[16];
		Color888 c, colours[4];

		ShortToColor888(cEx0, &colours[0]);
		ShortToColor888(cEx1, &colours[1]);
		if(cNumCols == 3) 
		{
			colours[2].r = (colours[0].r + colours[1].r) / 2;
			colours[2].g = (colours[0].g + colours[1].g) / 2;
			colours[2].b = (colours[0].b + colours[1].b) / 2;
			colours[3].r = (colours[0].r + colours[1].r) / 2;
			colours[3].g = (colours[0].g + colours[1].g) / 2;
			colours[3].b = (colours[0].b + colours[1].b) / 2;
		}
		else 
		{  // cNumCols == 4
			colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
			colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
			colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
			colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
			colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
			colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
		}

		for(int i=0; i<16; ++i) 
		{
			if(cpAlpha) 
			{  // Test to see if we have 1-bit transparency
				if(cpAlpha[i] < 128) 
				{
					mask[i] = 3;  // Transparent
					if(pOutCol) 
					{
						pOutCol[i].r = colours[3].r;
						pOutCol[i].g = colours[3].g;
						pOutCol[i].b = colours[3].b;
					}
					continue;
				}
			}

			// If no transparency, try to find which colour is the closest.
			closest = std::numeric_limits<uint32>::max();
			ShortToColor888(cpIn[i], &c);
			for(int j = 0; j < cNumCols; j++) 
			{
				dist = Distance(&c, &colours[j]);
				if(dist < closest) 
				{
					closest = dist;
					mask[i] = j;
					if (pOutCol) 
					{
						pOutCol[i].r = colours[j].r;
						pOutCol[i].g = colours[j].g;
						pOutCol[i].b = colours[j].b;
					}
				}
			}
		}

		for(int i=0; i<16; ++i)
			bitMask |= (mask[i] << (i*2));

		return bitMask;
	}

	void GenAlphaBitMask(const uint8 cA0, const uint8 cA1, const uint8 *cpIn, uint8 *pMask, uint8 *pOut)
	{
		uint8 alphas[8], m[16];
		uint32	closest, dist;

		alphas[0] = cA0;
		alphas[1] = cA1;

		// 6-alpha block.    
		// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
		alphas[2] = (4 * alphas[0] + 1 * alphas[1] + 2) / 5;	// Bit code 010
		alphas[3] = (3 * alphas[0] + 2 * alphas[1] + 2) / 5;	// Bit code 011
		alphas[4] = (2 * alphas[0] + 3 * alphas[1] + 2) / 5;	// Bit code 100
		alphas[5] = (1 * alphas[0] + 4 * alphas[1] + 2) / 5;	// Bit code 101
		alphas[6] = 0x00;										// Bit code 110
		alphas[7] = 0xFF;										// Bit code 111

		for(int i=0; i<16; ++i) 
		{
			closest = std::numeric_limits<uint32>::max();
			for (int j=0; j<8; ++j) 
			{
				dist = abs((int32)cpIn[i] - (int32)alphas[j]);
				if (dist < closest) 
				{
					closest = dist;
					m[i] = j;
				}
			}
		}

		for(int i=0; i<16; ++i)
			pOut[i] = alphas[m[i]];

		// First three bytes.
		pMask[0] = (m[0]) | (m[1] << 3) | ((m[2] & 0x03) << 6);
		pMask[1] = (m[2] & 0x04) | (m[3] << 1) | (m[4] << 4) | ((m[5] & 0x01) << 7);
		pMask[2] = (m[5] & 0x06) | (m[6] << 2) | (m[7] << 5);

		// Second three bytes.
		pMask[3] = (m[8]) | (m[9] << 3) | ((m[10] & 0x03) << 6);
		pMask[4] = (m[10] & 0x04) | (m[11] << 1) | (m[12] << 4) | ((m[13] & 0x01) << 7);
		pMask[5] = (m[13] & 0x06) | (m[14] << 2) | (m[15] << 5);
	}

	void ChooseEndpoints(const uint16 *cpBlock, uint16 *pEx0, uint16 *pEx1)
	{
		Color888 colours[16];
		int32 farthest = -1, d;

		for (int i=0; i<16; ++i)
			ShortToColor888(cpBlock[i], &colours[i]);
		for(int i=0; i<16; ++i)
		{
			for(int j=i+1; j<16; ++j) 
			{
				d = Distance(&colours[i], &colours[j]);
				if (d > farthest) 
				{
					farthest = d;
					*pEx0 = cpBlock[i];
					*pEx1 = cpBlock[j];
				}
			}
		}
	}

	void ChooseAlphaEndpoints(const uint8 *cpBlock, uint8 *pA0, uint8 *pA1)
	{
		uint32 lowest = 0xFF, highest = 0;
		for(uint32 i=0; i<16; ++i) 
		{
			if(cpBlock[i] < lowest) 
			{
				*pA1 = cpBlock[i];  // pA1 is the lower of the two.
				lowest = cpBlock[i];
			}
			else 
			if(cpBlock[i] > highest) 
			{
				*pA0 = cpBlock[i];  // pA0 is the higher of the two.
				highest = cpBlock[i];
			}
		}
	}

	void CorrectEndDXT(uint16 *pEx0, uint16 *pEx1, const bool cHasAlpha)
	{
		if(cHasAlpha)
		{
			if (*pEx0 > *pEx1) 
			{
				uint16 temp = *pEx0;
				*pEx0 = *pEx1;
				*pEx1 = temp;
			}
		}
		else 
		{
			if(*pEx0 < *pEx1) 
			{
				uint16 temp = *pEx0;
				*pEx0 = *pEx1;
				*pEx1 = temp;
			}
		}
	}

	void CompressToDXT(uint8 const* cpSrc, void* pDest, const D3DFORMAT cFormat, const uint32 cWidth, const uint32 cHeight)
	{
		uint16	Block[16], ex0, ex1;
		uint32	BitMask;
		uint8		AlphaBlock[16], a0, a1;
		
		uint16 *pData;
		uint8 *pAlpha;
		bool allocatedOnHeap = ((cWidth * cHeight) <= 64 * 64)? false : true;
		if(allocatedOnHeap)
		{
			pData		= new uint16[cWidth * cHeight * 2];
			pAlpha	= new uint8[cWidth * cHeight];
		}
		else
		{
			pData		= (uint16*)alloca(cWidth * cHeight * 2 * 2);
			pAlpha	= (uint8*)alloca(cWidth * cHeight);
		}
		CompressTo565(pData, cpSrc, cWidth, cHeight);
		GetAlpha(pAlpha, cpSrc, cWidth, cHeight);

		uint8 *pCurDest = (uint8*)pDest;

		switch (cFormat)
		{
		case D3DFMT_DXT1:
			{
				bool hasAlpha = false;
				for (int y=0; y<cHeight; y+=4) 
				{
					for (int x=0; x<cWidth; x+=4) 
					{
						GetAlphaBlock(AlphaBlock, pAlpha, cWidth, x, y);
						for (int i=0; i<16; ++i) 
						{
							if (AlphaBlock[i] < 128) 
							{
								hasAlpha = true;
								break;
							}
						}
						GetBlock(Block, pData, cWidth, x, y);
						ChooseEndpoints(Block, &ex0, &ex1);
						CorrectEndDXT(&ex0, &ex1, hasAlpha);
						Saveuint16(pCurDest, ex0);
						Saveuint16(pCurDest, ex1);
						if (hasAlpha)
							BitMask = GenBitMask(ex0, ex1, 3, Block, AlphaBlock, NULL);
						else
							BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						Saveuint32(pCurDest, BitMask);
					}		
				}
			}
			break;

		case D3DFMT_DXT3:
			{
				for (int y=0; y<cHeight; y+=4) 
				{
					for (int x=0; x<cWidth; x+=4) 
					{
						GetAlphaBlock(AlphaBlock, pAlpha, cWidth, x, y);
						for (int i=0; i<16; i+=2)
							*pCurDest++ = ((uint8)(((AlphaBlock[i] >> 4) << 4) | (AlphaBlock[i+1] >> 4)));

						GetBlock(Block, pData, cWidth, x, y);
						ChooseEndpoints(Block, &ex0, &ex1);
						if(ex0 < ex1) 
						{
							uint16 temp = ex0;
							ex0 = ex1;
							ex1 = temp;
						}
						Saveuint16(pCurDest, ex0);
						Saveuint16(pCurDest, ex1);
						BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						Saveuint32(pCurDest, BitMask);
					}		
				}
			}
			break;

		case D3DFMT_DXT5:
			{
				uint8 AlphaOut[16], AlphaBitMask[6];
				for (int y=0; y<cHeight; y+=4) 
				{
					for (int x=0; x<cWidth; x+=4) 
					{
						GetAlphaBlock(AlphaBlock, pAlpha, cWidth, x, y);
						ChooseAlphaEndpoints(AlphaBlock, &a0, &a1);
						GenAlphaBitMask(a0, a1, AlphaBlock, AlphaBitMask, AlphaOut);
						*pCurDest++ = a0;
						*pCurDest++ = a1;
						*pCurDest++ = AlphaBitMask[0];
						*pCurDest++ = AlphaBitMask[1];
						*pCurDest++ = AlphaBitMask[2];
						*pCurDest++ = AlphaBitMask[3];
						*pCurDest++ = AlphaBitMask[4];
						*pCurDest++ = AlphaBitMask[5];

						GetBlock(Block, pData, cWidth, x, y);
						ChooseEndpoints(Block, &ex0, &ex1);
						if(ex0 < ex1) 
						{
							uint16 temp = ex0;
							ex0 = ex1;
							ex1 = temp;
						}
						Saveuint16(pCurDest, ex0);
						Saveuint16(pCurDest, ex1);
						BitMask = GenBitMask(ex0, ex1, 4, Block, NULL, NULL);
						Saveuint32(pCurDest, BitMask);
					}
				}
			}
			break;
		}
		if(allocatedOnHeap)
		{
			delete [] pData;
			delete [] pAlpha;
		}
	}
} //NDXTCompression

#endif//OPENGL