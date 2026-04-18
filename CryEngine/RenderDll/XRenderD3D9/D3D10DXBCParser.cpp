/*=============================================================================
  D3D10DXBCParser.cpp : D3D10 DXBC binary shader parser.
  Copyright (c) 2001-2005 Crytek Studios. All Rights Reserved.

  Revision history:
    * Created by Honich Andrey

=============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"

#if defined(DIRECT3D10)


_inline uint rol(uint value, uint places) 
{ 
  return (value<<places) | (value>>(32-places)); 
} 

_inline static void FF(UINT& CRC0, UINT CRC1, UINT CRC2, UINT CRC3, UINT nVal, UINT nShift, UINT nH)
{
  CRC0 = rol(CRC0 + ((~CRC1 & CRC3) | (CRC1 & CRC2)) + nVal + nH, nShift) + CRC1;
}

_inline static void GG(UINT& CRC0, UINT CRC1, UINT CRC2, UINT CRC3, UINT nVal, UINT nShift, UINT nH)
{
  CRC0  = rol(CRC0 + ((~CRC3 & CRC2) | (CRC3 & CRC1)) + nVal + nH, nShift) + CRC1;
}

_inline static void HH(UINT& CRC0, UINT CRC1, UINT CRC2, UINT CRC3, UINT nVal, UINT nShift, UINT nH)
{
  CRC0 = rol((CRC1 ^ CRC2 ^ CRC3) + nVal + CRC0 + nH, nShift) + CRC1;
}

_inline static void II(UINT& CRC0, UINT CRC1, UINT CRC2, UINT CRC3, UINT nVal, UINT nShift, UINT nH)
{
  CRC0 = rol(((~CRC3 | CRC1) ^ CRC2) + nVal + CRC0 + nH, nShift) + CRC1;
}

static void sComputeCRC(byte *pCode, uint nSize, uint nCRC[])
{
  UINT *pData;
  byte EmptyBlock[16*4*2];
  byte TempBlock[16*4];
  memset(EmptyBlock, 0, sizeof(EmptyBlock));
  EmptyBlock[0] = 0x80;
  UINT i;
  int nSizeLeft = 56;
  bool bTwoBlocksLeft = 0;
  if ((nSize & 0x3f) >= nSizeLeft)
  {
    bTwoBlocksLeft = 1;
    nSizeLeft = 64 + 56;
  }
  nSizeLeft -= (nSize & 0x3f);
  UINT CRC[4];
  CRC[0] = 0x67452301;
  CRC[1] = 0xEFCDAB89;
  CRC[2] = 0x98BADCFE;
  CRC[3] = 0x10325476;

  UINT nBlocks = (nSizeLeft + nSize + 8) >> 6;
  UINT nCurBlockOffset = 0;
  UINT nBound = bTwoBlocksLeft ? nBlocks-2 : nBlocks-1;
  UINT *pOrigData = (UINT *)pCode;
  for (i=0; i<nBlocks; i++)
  {
    if (i == nBound)
    {
      if (bTwoBlocksLeft == 0)
      {
        if (nBlocks-1 == i)
        {
          *(UINT *)TempBlock = nSize << 3;
          memcpy(&TempBlock[4], pOrigData, nSize-nCurBlockOffset);
          memcpy(&TempBlock[4+(nSize-nCurBlockOffset)], &EmptyBlock[0], nSizeLeft);
          *(UINT *)(&TempBlock[15*sizeof(UINT)]) = (nSize * 2) | 1;
        }
      }
      else
      {
        if (nBlocks-2 == i)
        {
          memcpy(&TempBlock, pOrigData, nSize-nCurBlockOffset);
          memcpy(&TempBlock[nSize-nCurBlockOffset], &EmptyBlock[0], nSizeLeft - 56);
          nBound = nBlocks-1;
        }
        else
          if (nBlocks-1 == i)
          {
            *(UINT *)TempBlock = nSize << 3;
            memcpy(&TempBlock[4], &EmptyBlock[nSizeLeft-56], 56);
            *(UINT *)(&TempBlock[15*sizeof(UINT)]) = (nSize * 2) | 1;
          }
      }
      pData = (UINT *)TempBlock;
    }
    else
      pData = pOrigData;
    UINT nC0 = CRC[0];
    UINT nC1 = CRC[1];
    UINT nC2 = CRC[2];
    UINT nC3 = CRC[3];

    FF(nC0, CRC[1], CRC[2], CRC[3], pData[0], 7, 0xD76AA478);
    FF(nC3, nC0, CRC[1], CRC[2], pData[1], 12, 0xE8C7B756);
    FF(nC2, nC3, nC0, CRC[1], pData[2], 17, 0x242070DB);
    FF(nC1, nC2, nC3, nC0, pData[3], 22, 0xC1BDCEEE);

    FF(nC0, nC1, nC2, nC3, pData[4], 7, 0xF57C0FAF);
    FF(nC3, nC0, nC1, nC2, pData[5], 12, 0x4787C62A);
    FF(nC2, nC3, nC0, nC1, pData[6], 17, 0xA8304613);
    FF(nC1, nC2, nC3, nC0, pData[7], 22, 0xFD469501);

    FF(nC0, nC1, nC2, nC3, pData[8], 7, 0x698098D8);
    FF(nC3, nC0, nC1, nC2, pData[9], 12, 0x8B44F7AF);
    FF(nC2, nC3, nC0, nC1, pData[10], 17, 0xFFFF5BB1);
    FF(nC1, nC2, nC3, nC0, pData[11], 22, 0x895CD7BE);

    FF(nC0, nC1, nC2, nC3, pData[12], 7, 0x6B901122);
    FF(nC3, nC0, nC1, nC2, pData[13], 12, 0xFD987193);
    FF(nC2, nC3, nC0, nC1, pData[14], 17, 0xA679438E);
    FF(nC1, nC2, nC3, nC0, pData[15], 22, 0x49B40821);


    GG(nC0, nC1, nC2, nC3, pData[1], 5, 0xF61E2562);
    GG(nC3, nC0, nC1, nC2, pData[6], 9, 0xC040B340);
    GG(nC2, nC3, nC0, nC1, pData[11], 14, 0x265E5A51);
    GG(nC1, nC2, nC3, nC0, pData[0], 20, 0xE9B6C7AA);

    GG(nC0, nC1, nC2, nC3, pData[5], 5, 0xD62F105D);
    GG(nC3, nC0, nC1, nC2, pData[10], 9, 0x2441453);
    GG(nC2, nC3, nC0, nC1, pData[15], 14, 0xD8A1E681);
    GG(nC1, nC2, nC3, nC0, pData[4], 20, 0xE7D3FBC8);

    GG(nC0, nC1, nC2, nC3, pData[9], 5, 0x21E1CDE6);
    GG(nC3, nC0, nC1, nC2, pData[14], 9, 0xC33707D6);
    GG(nC2, nC3, nC0, nC1, pData[3], 14, 0xF4D50D87);
    GG(nC1, nC2, nC3, nC0, pData[8], 20, 0x455A14ED);

    GG(nC0, nC1, nC2, nC3, pData[13], 5, 0xA9E3E905);
    GG(nC3, nC0, nC1, nC2, pData[2], 9, 0xFCEFA3F8);
    GG(nC2, nC3, nC0, nC1, pData[7], 14, 0x676F02D9);
    GG(nC1, nC2, nC3, nC0, pData[12], 20, 0x8D2A4C8A);


    HH(nC0, nC1, nC2, nC3, pData[5], 4, -0x0005C6BE);
    HH(nC3, nC0, nC1, nC2, pData[8], 11, -0x788E097F);
    HH(nC2, nC3, nC0, nC1, pData[11], 16, 0x6D9D6122);
    HH(nC1, nC2, nC3, nC0, pData[14], 23, -0x021AC7F4);

    HH(nC0, nC1, nC2, nC3, pData[1], 4, -0x5B4115BC);
    HH(nC3, nC0, nC1, nC2, pData[4], 11, 0x4BDECFA9);
    HH(nC2, nC3, nC0, nC1, pData[7], 16, -0x0944B4A0);
    HH(nC1, nC2, nC3, nC0, pData[10], 23, -0x41404390);

    HH(nC0, nC1, nC2, nC3, pData[13], 4, 0x289B7EC6);
    HH(nC3, nC0, nC1, nC2, pData[0], 11, -0x155ED806);
    HH(nC2, nC3, nC0, nC1, pData[3], 16, -0x2B10CF7B);
    HH(nC1, nC2, nC3, nC0, pData[6], 23, 0x04881D05);

    HH(nC0, nC1, nC2, nC3, pData[9], 4, -0x262B2FC7);
    HH(nC3, nC0, nC1, nC2, pData[12], 11, -0x1924661B);
    HH(nC2, nC3, nC0, nC1, pData[15], 16, 0x1FA27CF8);
    HH(nC1, nC2, nC3, nC0, pData[2], 23, -0x3B53A99B);


    II(nC0, nC1, nC2, nC3, pData[0], 6, -0x0BD6DDBC);
    II(nC3, nC0, nC1, nC2, pData[7], 10, 0x432AFF97);
    II(nC2, nC3, nC0, nC1, pData[14], 15, -0x546BDC59);
    II(nC1, nC2, nC3, nC0, pData[5], 21, -0x036C5FC7);

    II(nC0, nC1, nC2, nC3, pData[12], 6, 0x655B59C3);
    II(nC3, nC0, nC1, nC2, pData[3], 10, -0x70F3336E);
    II(nC2, nC3, nC0, nC1, pData[10], 15, -0x00100B83);
    II(nC1, nC2, nC3, nC0, pData[1], 21, -0x7A7BA22F);

    II(nC0, nC1, nC2, nC3, pData[8], 6, 0x6FA87E4F);
    II(nC3, nC0, nC1, nC2, pData[15], 10, -0x01D31920);
    II(nC2, nC3, nC0, nC1, pData[6], 15, -0x5CFEBCEC);
    II(nC1, nC2, nC3, nC0, pData[13], 21, 0x4E0811A1);

    II(nC0, nC1, nC2, nC3, pData[4], 6, -0x08AC817E);
    II(nC3, nC0, nC1, nC2, pData[11], 10, -0x42C50DCB);
    II(nC2, nC3, nC0, nC1, pData[2], 15, 0x2AD7D2BB);
    II(nC1, nC2, nC3, nC0, pData[9], 21, -0x14792C6F);


    CRC[0] += nC0;
    CRC[1] += nC1;
    CRC[2] += nC2;
    CRC[3] += nC3;

    nCurBlockOffset += 16*sizeof(UINT);
    pOrigData += 16;
  }
  nCRC[0] = CRC[0];
  nCRC[1] = CRC[1];
  nCRC[2] = CRC[2];
  nCRC[3] = CRC[3];
}


#pragma pack(push,1)
struct SDXBCHeader
{
  FOURCC Id;
  uint CRC[4];
  uint16 VersionLow;
  uint16 VersionHigh;
  uint32 nCodeSize;
  uint32 nFOURCCSections;
};

enum EDXBCOpClass
{
  eDCL_FloatArithmetic = 0,
  eDCL_IntArithmetic = 1,
  eDCL_UnsignedArithmetic = 2,
  eDCL_Logic = 3,
  eDCL_Flow = 4,
  eDCL_Load = 5,
  eDCL_Decl = 6,
};

enum EDXBCOp
{
  eDOP_add = 0,
  eDOP_and = 1,
  eDOP_break = 2,
  eDOP_breakc = 3,
  eDOP_call = 4,
  eDOP_callc = 5,
  eDOP_case = 6,
  eDOP_continue = 7,
  eDOP_continuec = 8,
  eDOP_cut = 9,
  eDOP_default = 10,
  eDOP_rtx = 11,
  eDOP_rty = 12,
  eDOP_discard = 13,
  eDOP_div = 14,
  eDOP_dp2 = 15,
  eDOP_dp3 = 16,
  eDOP_dp4 = 17,
  eDOP_else = 18,
  eDOP_emit = 19,
  eDOP_emit_then_cut = 20,
  eDOP_endif = 21,
  eDOP_endloop = 22,
  eDOP_endswitch = 23,
  eDOP_eq = 24,
  eDOP_exp = 25,
  eDOP_frc = 26,
  eDOP_ftoi = 27,
  eDOP_ftou = 28,
  eDOP_ge = 29,
  eDOP_iadd = 30,
  eDOP_if = 31,
  eDOP_ieq = 32,
  eDOP_ige = 33,
  eDOP_ilt = 34,
  eDOP_imad = 35,
  eDOP_imax = 36,
  eDOP_imin = 37,
  eDOP_imul = 38,
  eDOP_ine = 39,
  eDOP_ineg = 40,
  eDOP_ishl = 41,
  eDOP_ishr = 42,
  eDOP_itof = 43,
  eDOP_label = 44,
  eDOP_ld = 45,
  eDOP_ldms = 46,
  eDOP_log = 47,
  eDOP_loop = 48,
  eDOP_lt = 49,
  eDOP_mad = 50,
  eDOP_min = 51,
  eDOP_max = 52,
  eDOP_mem = 53,
  eDOP_mov = 54,
  eDOP_movc = 55,
  eDOP_mul = 56,
  eDOP_ne = 57,
  eDOP_nop = 58,
  eDOP_not = 59,
  eDOP_or = 60,
  eDOP_resinfo = 61,
  eDOP_ret = 62,
  eDOP_retc = 63,
  eDOP_round_ne = 64,
  eDOP_round_ni = 65,
  eDOP_round_pi = 66,
  eDOP_round_z = 67,
  eDOP_rsq = 68,
  eDOP_sample = 69,
  eDOP_sample_c = 70,
  eDOP_sample_c_lz = 71,
  eDOP_sample_l = 72,
  eDOP_sample_d = 73,
  eDOP_sample_b = 74,
  eDOP_sqrt = 75,
  eDOP_switch = 76,
  eDOP_sincos = 77,
  eDOP_udiv = 78,
  eDOP_ult = 79,
  eDOP_uge = 80,
  eDOP_umul = 81,
  eDOP_umad = 82,
  eDOP_umax = 83,
  eDOP_umin = 84,
  eDOP_ushr = 85,
  eDOP_utof = 86,
  eDOP_xor = 87,
  eDOP_dcl_resource = 88,
  eDOP_dcl_constantbuffer = 89,
  eDOP_dcl_sampler = 90,
  eDOP_dcl_indexrange = 91,
  eDOP_dcl_outputtopology = 92,
  eDOP_dcl_inputprimitive = 93,
  eDOP_dcl_maxout = 94,
  eDOP_dcl_input = 95,
  eDOP_dcl_input_sgv = 96,
  eDOP_dcl_input_siv = 97,
  eDOP_dcl_input_2 = 98,
  eDOP_dcl_input_sgv_2 = 99,
  eDOP_dcl_input_siv_2 = 100,
  eDOP_dcl_output = 101,
  eDOP_dcl_output_sgv = 102,
  eDOP_dcl_output_siv = 103,
  eDOP_dcl_temps = 104,
  eDOP_dcl_indexableTemp = 105,
  eDOP_jmp = 106,

  eDOP_Max,
};

#pragma pack(pop)

struct SDXBCOperation
{
  byte nOperands;
  const char *szInstr;
  EDXBCOpClass Class;

  void Init(const char *szOp, int InOperands, EDXBCOpClass eClass)
  {
    nOperands = InOperands;
    szInstr = szOp;
    Class = eClass;
  }
};


#define DXBC_OP(a, b, c) s_Instructions[eDOP_##a].Init(#a, b, c)
#define DXBC_OP_2(a, a2, b, c) s_Instructions[eDOP_##a2].Init(#a, b, c)

SDXBCOperation s_Instructions[eDOP_Max];
static void sInitInstructions()
{
  DXBC_OP(add, 3, eDCL_FloatArithmetic);
  DXBC_OP(and, 3, eDCL_Logic);
  DXBC_OP(break, 0, eDCL_Flow);
  DXBC_OP(breakc, 1, eDCL_Flow);
  DXBC_OP(call, 1, eDCL_Flow);
  DXBC_OP(callc, 2, eDCL_Flow);
  DXBC_OP(continue, 0, eDCL_Flow);
  DXBC_OP(continuec, 1, eDCL_Flow);
  DXBC_OP(case, 1, eDCL_Flow);
  DXBC_OP(cut, 0, eDCL_Flow);
  DXBC_OP(default, 0, eDCL_Flow);
  DXBC_OP(discard, 1, eDCL_Flow);
  DXBC_OP(div, 3, eDCL_FloatArithmetic);
  DXBC_OP(dp2, 3, eDCL_FloatArithmetic);
  DXBC_OP(dp3, 3, eDCL_FloatArithmetic);
  DXBC_OP(dp4, 3, eDCL_FloatArithmetic);
  DXBC_OP(else, 0, eDCL_Flow);
  DXBC_OP(emit, 0, eDCL_Flow);
  DXBC_OP(emit_then_cut, 0, eDCL_Flow);
  DXBC_OP(endif, 0, eDCL_Flow);
  DXBC_OP(endloop, 0, eDCL_Flow);
  DXBC_OP(endswitch, 0, eDCL_Flow);
  DXBC_OP(eq, 3, eDCL_FloatArithmetic);
  DXBC_OP(exp, 2, eDCL_FloatArithmetic);
  DXBC_OP(frc, 2, eDCL_FloatArithmetic);
  DXBC_OP(ftoi, 2, eDCL_FloatArithmetic);
  DXBC_OP(ftou, 2, eDCL_FloatArithmetic);
  DXBC_OP(ge, 3, eDCL_FloatArithmetic);
  DXBC_OP(rtx, 2, eDCL_FloatArithmetic);
  DXBC_OP(rty, 2, eDCL_FloatArithmetic);
  DXBC_OP(iadd, 3, eDCL_IntArithmetic);
  DXBC_OP(if, 1, eDCL_Flow);
  DXBC_OP(ieq, 3, eDCL_IntArithmetic);
  DXBC_OP(ige, 3, eDCL_IntArithmetic);
  DXBC_OP(ilt, 3, eDCL_IntArithmetic);
  DXBC_OP(imad, 4, eDCL_IntArithmetic);
  DXBC_OP(imax, 3, eDCL_IntArithmetic);
  DXBC_OP(imin, 3, eDCL_IntArithmetic);
  DXBC_OP(imul, 4, eDCL_IntArithmetic);
  DXBC_OP(ine, 3, eDCL_IntArithmetic);
  DXBC_OP(ineg, 2, eDCL_IntArithmetic);
  DXBC_OP(ishl, 3, eDCL_IntArithmetic);
  DXBC_OP(ishr, 3, eDCL_IntArithmetic);
  DXBC_OP(itof, 2, eDCL_IntArithmetic);
  DXBC_OP(label, 1, eDCL_Flow);
  DXBC_OP(ld, 3, eDCL_Load);
  DXBC_OP(ldms, 4, eDCL_Load);
  DXBC_OP(log, 2, eDCL_FloatArithmetic);
  DXBC_OP(loop, 0, eDCL_Flow);
  DXBC_OP(lt, 3, eDCL_FloatArithmetic);
  DXBC_OP(mad, 4, eDCL_FloatArithmetic);
  DXBC_OP(max, 3, eDCL_FloatArithmetic);
  DXBC_OP(min, 3, eDCL_FloatArithmetic);
  DXBC_OP(mov, 2, eDCL_FloatArithmetic);
  DXBC_OP(movc, 4, eDCL_FloatArithmetic);
  DXBC_OP(mul, 3, eDCL_FloatArithmetic);
  DXBC_OP(ne, 3, eDCL_FloatArithmetic);
  DXBC_OP(nop, 0, eDCL_Flow);
  DXBC_OP(not, 2, eDCL_Logic);
  DXBC_OP(or, 3, eDCL_Logic);
  DXBC_OP(resinfo, 3, eDCL_Flow);
  DXBC_OP(ret, 0, eDCL_Flow);
  DXBC_OP(retc, 1, eDCL_Flow);
  DXBC_OP(round_ne, 2, eDCL_FloatArithmetic);
  DXBC_OP(round_ni, 2, eDCL_FloatArithmetic);
  DXBC_OP(round_pi, 2, eDCL_FloatArithmetic);
  DXBC_OP(round_z, 2, eDCL_FloatArithmetic);
  DXBC_OP(rsq, 2, eDCL_FloatArithmetic);
  DXBC_OP(sample, 4, eDCL_Load);
  DXBC_OP(sample_l, 4, eDCL_Load);
  DXBC_OP(sample_d, 6, eDCL_Load);
  DXBC_OP(sample_c, 5, eDCL_Load);
  DXBC_OP(sample_c_lz, 5, eDCL_Load);
  DXBC_OP(sample_c_lz, 5, eDCL_Load);
  DXBC_OP(sqrt, 2, eDCL_FloatArithmetic);
  DXBC_OP(switch, 1, eDCL_Flow);
  DXBC_OP(sincos, 3, eDCL_FloatArithmetic);
  DXBC_OP(sincos, 3, eDCL_FloatArithmetic);
  DXBC_OP(udiv, 4, eDCL_UnsignedArithmetic);
  DXBC_OP(ult, 3, eDCL_UnsignedArithmetic);
  DXBC_OP(uge, 3, eDCL_UnsignedArithmetic);
  DXBC_OP(umax, 3, eDCL_UnsignedArithmetic);
  DXBC_OP(umin, 3, eDCL_UnsignedArithmetic);
  DXBC_OP(umul, 4, eDCL_UnsignedArithmetic);
  DXBC_OP(umad, 4, eDCL_UnsignedArithmetic);
  DXBC_OP(ushr, 3, eDCL_UnsignedArithmetic);
  DXBC_OP(utof, 2, eDCL_UnsignedArithmetic);
  DXBC_OP(xor, 3, eDCL_Logic);
  DXBC_OP(jmp, 0, eDCL_Flow);
  DXBC_OP(dcl_input, 1, eDCL_Decl);
  DXBC_OP_2(dcl_input, dcl_input_2, 1, eDCL_Decl);
  DXBC_OP(dcl_input_sgv, 1, eDCL_Decl);
  DXBC_OP_2(dcl_input_sgv, dcl_input_sgv_2, 1, eDCL_Decl);
  DXBC_OP(dcl_input_siv, 1, eDCL_Decl);
  DXBC_OP_2(dcl_input_siv, dcl_input_siv_2, 1, eDCL_Decl);
  DXBC_OP(dcl_output, 1, eDCL_Decl);
  DXBC_OP(dcl_inputprimitive, 0, eDCL_Decl);
  DXBC_OP(dcl_outputtopology, 0, eDCL_Decl);
  DXBC_OP(dcl_maxout, 0, eDCL_Decl);
  DXBC_OP(dcl_constantbuffer, 1, eDCL_Decl);
  DXBC_OP(dcl_sampler, 1, eDCL_Decl);
  DXBC_OP(dcl_resource, 1, eDCL_Decl);
  DXBC_OP(dcl_output_siv, 1, eDCL_Decl);
  DXBC_OP(dcl_output_sgv, 1, eDCL_Decl);
  DXBC_OP(dcl_temps, 0, eDCL_Decl);
  DXBC_OP(dcl_indexableTemp, 0, eDCL_Decl);
  DXBC_OP(dcl_indexrange, 2, eDCL_Decl);
}

enum D3D10_SB_OPERAND_TYPE
{
  D3D10_SB_OPERAND_TYPE_TEMP = 0,
  D3D10_SB_OPERAND_TYPE_INPUT = 1,
  D3D10_SB_OPERAND_TYPE_OUTPUT = 2,
  D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP = 3,
  D3D10_SB_OPERAND_TYPE_IMMEDIATE32 = 4,
  D3D10_SB_OPERAND_TYPE_IMMEDIATE64 = 5,
  D3D10_SB_OPERAND_TYPE_SAMPLER = 6,
  D3D10_SB_OPERAND_TYPE_RESOURCE = 7,
  D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER = 8,
  D3D10_SB_OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER = 9,
  D3D10_SB_OPERAND_TYPE_LABEL = 10,
  D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID = 11,
  D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH = 12,
  D3D10_SB_OPERAND_TYPE_NULL = 13,
};

enum D3D10_SB_4_COMPONENT_NAME
{
  D3D10_SB_4_COMPONENT_X = 0,
  D3D10_SB_4_COMPONENT_Y = 1,
  D3D10_SB_4_COMPONENT_Z = 2,
  D3D10_SB_4_COMPONENT_W = 3,
};

enum D3D10_SB_OPERAND_INDEX_DIMENSION
{
  D3D10_SB_OPERAND_INDEX_0D = 0,
  D3D10_SB_OPERAND_INDEX_1D = 1,
  D3D10_SB_OPERAND_INDEX_2D = 2,
  D3D10_SB_OPERAND_INDEX_3D = 3,
};

enum D3D10_SB_OPERAND_NUM_COMPONENTS
{
  D3D10_SB_OPERAND_0_COMPONENT = 0,
  D3D10_SB_OPERAND_1_COMPONENT = 1,
  D3D10_SB_OPERAND_4_COMPONENT = 2,
  D3D10_SB_OPERAND_N_COMPONENT = 3,
};

enum D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE
{
  D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE = 0,
  D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE = 1,
  D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE = 2,
};

enum D3D10_SB_OPERAND_MODIFIER
{
  D3D10_SB_OPERAND_MODIFIER_NONE = 0,
  D3D10_SB_OPERAND_MODIFIER_NEG = 1,
  D3D10_SB_OPERAND_MODIFIER_ABS = 2,
  D3D10_SB_OPERAND_MODIFIER_ABSNEG = 3,
};

enum D3D10_SB_EXTENDED_OPERAND_TYPE
{
  D3D10_SB_EXTENDED_OPERAND_EMPTY = 0,
  D3D10_SB_EXTENDED_OPERAND_MODIFIER = 1,
};

enum D3D10_SB_OPERAND_INDEX_REPRESENTATION
{
  D3D10_SB_OPERAND_INDEX_IMMEDIATE32 = 0,
  D3D10_SB_OPERAND_INDEX_IMMEDIATE64 = 1,
  D3D10_SB_OPERAND_INDEX_RELATIVE = 2,
  D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE = 3,
  D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE = 4,
};

enum D3D10_SB_EXTENDED_OPCODE_TYPE
{
  D3D10_SB_EXTENDED_OPCODE_EMPTY = 0,
  D3D10_SB_EXTENDED_OPCODE_SAMPLE_CONTROLS = 1,
};

enum D3D10_SB_INSTRUCTION_TEST_BOOLEAN
{
  D3D10_SB_INSTRUCTION_TEST_ZERO = 0,
  D3D10_SB_INSTRUCTION_TEST_NONZERO = 1
};

enum D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE
{
  D3D10_SB_RESINFO_INSTRUCTION_RETURN_FLOAT = 0,
  D3D10_SB_RESINFO_INSTRUCTION_RETURN_RCPFLOAT = 1,
  D3D10_SB_RESINFO_INSTRUCTION_RETURN_UINT = 2,
};

enum D3D10_SB_NAME
{
  D3D10_SB_NAME_UNDEFINED = 0,
  D3D10_SB_NAME_POSITION = 1,
  D3D10_SB_NAME_CLIP_DISTANCE = 2,
  D3D10_SB_NAME_CULL_DISTANCE = 3,
  D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX = 4,
  D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX = 5,
  D3D10_SB_NAME_VERTEX_ID = 6,
  D3D10_SB_NAME_PRIMITIVE_ID = 7,
  D3D10_SB_NAME_INSTANCE_ID = 8,
  D3D10_SB_NAME_IS_FRONT_FACE = 9,
};

enum D3D10_SB_SAMPLER_MODE
{
  D3D10_SB_SAMPLER_MODE_DEFAULT = 0,
  D3D10_SB_SAMPLER_MODE_COMPARISON = 1,
  D3D10_SB_SAMPLER_MODE_MONO = 2,
};

enum D3D10_SB_INTERPOLATION_MODE
{
  D3D10_SB_INTERPOLATION_UNDEFINED = 0,
  D3D10_SB_INTERPOLATION_CONSTANT = 1,
  D3D10_SB_INTERPOLATION_LINEAR = 2,
  D3D10_SB_INTERPOLATION_LINEAR_CENTROID = 3,
  D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE = 4,
  D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID = 5,
};

enum D3D10_SB_RESOURCE_DIMENSION
{
  D3D10_SB_RESOURCE_DIMENSION_UNKNOWN = 0,
  D3D10_SB_RESOURCE_DIMENSION_BUFFER = 1,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D = 2,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D = 3,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D = 4,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE = 5,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY = 6,
  D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY = 7,
};

enum D3D10_SB_RESOURCE_RETURN_TYPE
{
  D3D10_SB_RETURN_TYPE_UNORM = 1,
  D3D10_SB_RETURN_TYPE_SNORM = 2,
  D3D10_SB_RETURN_TYPE_SINT = 3,
  D3D10_SB_RETURN_TYPE_UINT = 4,
  D3D10_SB_RETURN_TYPE_FLOAT = 5,
  D3D10_SB_RETURN_TYPE_MIXED = 6,  
};

enum D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN
{
  D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED = 0,
  D3D10_SB_CONSTANT_BUFFER_DYNAMIC_INDEXED = 1,
};

enum D3D10_SB_PRIMITIVE
{
  D3D10_SB_PRIMITIVE_UNDEFINED = 0,
  D3D10_SB_PRIMITIVE_POINT = 1,
  D3D10_SB_PRIMITIVE_LINE = 2,
  D3D10_SB_PRIMITIVE_TRIANGLE = 3,
};

enum D3D10_SB_PRIMITIVE_TOPOLOGY
{
  D3D10_SB_PRIMITIVE_TOPOLOGY_UNDEFINED = 0,
  D3D10_SB_PRIMITIVE_TOPOLOGY_POINTLIST = 1,
  D3D10_SB_PRIMITIVE_TOPOLOGY_LINELIST = 2,
  D3D10_SB_PRIMITIVE_TOPOLOGY_LINESTRIP = 3,
  D3D10_SB_PRIMITIVE_TOPOLOGY_TRIANGLELIST = 4,
  D3D10_SB_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5,
};

enum D3D10_SB_CUSTOMDATA_CLASS
{
  D3D10_SB_CUSTOMDATA_COMMENT = 0,
  D3D10_SB_CUSTOMDATA_DEBUGINFO = 1,
  D3D10_SB_CUSTOMDATA_OPAQUE = 2,
  D3D10_SB_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER = 3,  
};

struct SDXBCOperandIndex
{
  int m_CodeOffset;
  union
  {
    uint m_RegIndex;
    uint m_RegIndexA[2];
    int64 m_RegIndex64;
  };
  D3D10_SB_OPERAND_TYPE m_RelRegType;
  D3D10_SB_4_COMPONENT_NAME m_ComponentName;
  D3D10_SB_OPERAND_INDEX_DIMENSION m_IndexDimension;
  union
  {
    uint m_RelIndex;
    uint m_RelIndexA[2];
    int64 m_RelIndex64;
  };
  union
  {
    uint m_RelIndex1;
    uint m_RelIndexA1[2];
    int64 m_RelIndex641;
  };
};

struct SDXBCOperand
{
  int m_CodeOffset;
  D3D10_SB_OPERAND_TYPE m_Type;
  SDXBCOperandIndex m_Index[3];
  D3D10_SB_OPERAND_NUM_COMPONENTS m_NumComponents;
  D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE m_ComponentSelection;
  BOOL m_bExtendedOperand;
  D3D10_SB_OPERAND_MODIFIER m_Modifier;
  D3D10_SB_EXTENDED_OPERAND_TYPE m_ExtendedOperandType;
  union
  {
    uint m_WriteMask;
    byte m_Swizzle[4];
    D3D10_SB_4_COMPONENT_NAME m_ComponentName;
  };
  union
  {
    uint m_Value[4];
    float m_Valuef[4];
    uint m_ValueA[4][2];
    int64 m_Value64[4];
  };
  D3D10_SB_OPERAND_INDEX_REPRESENTATION m_IndexType[3];
  D3D10_SB_OPERAND_INDEX_DIMENSION m_IndexDimension;
};

struct SDXBCInputSystemInterpretedValueDecl
{
  D3D10_SB_NAME m_Name;
};
struct SDXBCInputSystemGeneratedValueDecl
{
  D3D10_SB_NAME m_Name;
};
struct SDXBCInputPSDecl
{
  D3D10_SB_INTERPOLATION_MODE m_InterpolationMode;
};
struct SDXBCInputPSSystemInterpretedValueDecl
{
  D3D10_SB_NAME m_Name;
  D3D10_SB_INTERPOLATION_MODE m_InterpolationMode;
};
struct SDXBCInputPSSystemGeneratedValueDecl
{
  D3D10_SB_NAME m_Name;
};
struct SDXBCOutputSystemInterpretedValueDecl
{
  D3D10_SB_NAME m_Name;
};
struct SDXBCOutputSystemGeneratedValueDecl
{
  D3D10_SB_NAME m_Name;
};
struct SDXBCIndexRangeDecl
{
  uint m_RegCount;
};
struct SDXBCResourceDecl
{
  D3D10_SB_RESOURCE_DIMENSION m_Dimension;
  D3D10_SB_NAME m_Name;
  D3D10_SB_RESOURCE_RETURN_TYPE m_ReturnType[4];
  uint m_SampleCount;
};
struct SDXBCConstantBufferDecl
{
  D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN m_AccessPattern;
};
struct SDXBCInputPrimitiveDecl
{
  D3D10_SB_PRIMITIVE m_Primitive;
};
struct SDXBCOutputTopologyDecl
{
  D3D10_SB_PRIMITIVE_TOPOLOGY m_Topology;
};
struct SDXBCGSMaxOutputVertexCountDecl
{
  uint m_MaxOutputVertexCount;
};
struct SDXBCSamplerDecl
{
  D3D10_SB_SAMPLER_MODE m_SamplerMode;
};
struct SDXBCTempsDecl
{
  uint m_NumTemps;
};
struct SDXBCIndexableTempDecl
{
  uint m_StartRegister;
  uint m_NumRegisters;
};
struct SDXBCCustomData
{
  D3D10_SB_CUSTOMDATA_CLASS m_Type;
  uint m_DataSizeInBytes;
  void *m_pData;
};

struct SDXBCInstruction
{
  int m_CodeOffset;
  EDXBCOp m_OpCode;
  SDXBCOperand m_Operands[5];
  uint m_NumOperands;
  BOOL m_bExtended;
  D3D10_SB_EXTENDED_OPCODE_TYPE m_OpCodeEx;
  char m_TexelOffset[3];
  uint m_PrivateData[2];
  BOOL m_bSaturate;
  union
  {
    SDXBCInputSystemInterpretedValueDecl m_InputDeclSIV;
    SDXBCInputSystemGeneratedValueDecl m_InputDeclSGV;
    SDXBCInputPSDecl m_InputPSDecl;
    SDXBCInputPSSystemInterpretedValueDecl m_InputPSDeclSIV;
    SDXBCInputPSSystemGeneratedValueDecl m_InputPSDeclSGV;
    SDXBCOutputSystemInterpretedValueDecl m_OutputDeclSIV;
    SDXBCOutputSystemGeneratedValueDecl m_OutputDeclSGV;
    SDXBCIndexRangeDecl m_IndexRangeDecl;
    SDXBCResourceDecl m_ResourceDecl;
    SDXBCConstantBufferDecl m_ConstantBufferDecl;
    SDXBCInputPrimitiveDecl m_InputPrimitiveDecl;
    SDXBCOutputTopologyDecl m_OutputTopologyDecl;
    SDXBCGSMaxOutputVertexCountDecl m_GSMaxOutputVertexCountDec;
    SDXBCSamplerDecl m_SamplerDecl;
    SDXBCTempsDecl m_TempsDecl;
    SDXBCIndexableTempDecl m_IndexableTempDecl;
    SDXBCCustomData m_CustomData;
    D3D10_SB_INSTRUCTION_TEST_BOOLEAN m_Test;
    D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE m_ResInfoReturnType;
  };

  void Clear()
  {
    ZeroStruct(*this);
  }
};

//================================================================================================

struct SDXBCShaderCB
{
  int OffsName;
  int Variables;
  int OffsetVars;
  int Size;
  int uFlags;
  D3D10_CBUFFER_TYPE Type;
};

struct SDXBCShaderRDEF
{
  uint m_CBuffers;
  uint m_OffsetCBuffers;
  uint m_NumBindResources;
  uint m_OffsBindResources;
  uint m_Version;
  uint m_Flags;
  uint m_OffsCreator;
};

struct SDXBCShaderReflectionVariableType
{
  D3D10_SHADER_TYPE_DESC Desc;
};


struct SDXBCShaderReflectionVariable
{
  D3D10_SHADER_VARIABLE_DESC VarDesc;
  SDXBCShaderReflectionVariableType Type;
  int nType;
};


struct SDXBCShaderReflectionConstantBuffer
{
  D3D10_SHADER_BUFFER_DESC Desc;
  TArray<SDXBCShaderReflectionVariable> m_Vars;
};

struct SDXBCShaderReflectionResourceBinding
{
  D3D10_SHADER_INPUT_BIND_DESC Desc;
};

struct SDXBCShaderReflection
{
  D3D10_SHADER_DESC Desc;
  TArray<SDXBCShaderReflectionConstantBuffer> m_ConstantBuffers;
  TArray<SDXBCShaderReflectionResourceBinding> m_ResourceBindings;
};

struct SDXBCShaderVariable
{
  int OffsName;
  int StartOffset;
  int Size;	
  int uFlags;	
  int OffsType;
  int OffsDefault;
};

struct SDXBCShaderVariableType
{
  WORD Class;
  WORD Type;
  WORD Rows;
  WORD Columns;
  WORD Elements;
  WORD Members;
};

struct SDXBCShaderResourceBinding
{
  int OffsName;
  D3D10_SHADER_INPUT_TYPE     Type;           // Type of resource (e.g. texture, cbuffer, etc.)
  D3D10_RESOURCE_RETURN_TYPE  ReturnType;     // Return type (if texture)
  D3D10_SRV_DIMENSION         Dimension;      // Dimension (if texture)
  UINT                        NumSamples;     // Number of samples (0 if not MS texture)

  UINT                        BindPoint;      // Starting bind point
  UINT                        BindCount;      // Number of contiguous bind points (for arrays)
  UINT                        uFlags;         // Input binding flags
};

//================================================================================================

static void *sFindDXBCSection(FOURCC SectID, byte *pCode, SDXBCHeader *pHeader, uint32 *pOffsets)
{
  uint i;

  for (i=0; i<pHeader->nFOURCCSections; i++)
  {
    byte *pData = &pCode[pOffsets[i]];
    if (*(FOURCC *)pData == SectID)
      return pData + 8;
  }
  return NULL;
}

static void sCopyStringFromDXBC(LPCSTR *szDst, byte *szSrc)
{
  int nLen = strlen((const char *)szSrc)+1;
  *szDst = new char[nLen];
  memcpy((void *)*szDst, szSrc, nLen);
}

static int sGetNumInstructionOperands(int nCommand)
{
  assert(nCommand < eDOP_Max);
  if (nCommand < eDOP_Max)
    return s_Instructions[nCommand].nOperands;
  return 0;
}

void sParseOperand(DWORD*& pTokens, SDXBCOperand& Operand, DWORD *pStart)
{
  int i;
  Operand.m_CodeOffset = pTokens - pStart;
  DWORD dwToken = *pTokens++;
  Operand.m_NumComponents = (D3D10_SB_OPERAND_NUM_COMPONENTS)(dwToken & 3);
  Operand.m_bExtendedOperand = dwToken >> 31;
  Operand.m_Type = (D3D10_SB_OPERAND_TYPE)((dwToken >> 12) & 0xff);
  uint nOperands = 0;
  switch (Operand.m_NumComponents)
  {
    case D3D10_SB_OPERAND_1_COMPONENT:
      nOperands = 1;
      break;
    case D3D10_SB_OPERAND_4_COMPONENT:
      nOperands = 4;
      break;
  }
  if (Operand.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32)
  {
    for (i=0; i<nOperands; i++)
    {
      Operand.m_Value[i] = *pTokens++;
    }
  }
  else
  if (Operand.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64)
  {
    for (i=0; i<nOperands; i++)
    {
      Operand.m_Value64[i] = *(uint64 *)pTokens;
      pTokens++;
      pTokens++;
    }
  }
  else
  {
    if (Operand.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT)
    {
      Operand.m_ComponentSelection = (D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE)((dwToken >> 2) & 3);
      if (Operand.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)
        Operand.m_WriteMask = (dwToken & 0xf0);
      else
      if (Operand.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
      {
        Operand.m_Swizzle[0] = (dwToken >> 4) & 3;
        Operand.m_Swizzle[1] = (dwToken >> 6) & 3;
        Operand.m_Swizzle[2] = (dwToken >> 8) & 3;
        Operand.m_Swizzle[3] = (dwToken >> 10) & 3;
      }
      else
      if (Operand.m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)
      {
        byte b = (dwToken >> 4) & 3;
        Operand.m_Swizzle[0] = b;
        Operand.m_Swizzle[1] = b;
        Operand.m_Swizzle[2] = b;
        Operand.m_Swizzle[3] = b;
      }
      else
      {
        assert(0);
      }
      Operand.m_IndexDimension = (D3D10_SB_OPERAND_INDEX_DIMENSION)((dwToken >> 20) & 3);
      for (i=0; i<Operand.m_IndexDimension; i++)
      {
        int tShift = ((i&3)*3)+22;
        Operand.m_IndexType[i] = (D3D10_SB_OPERAND_INDEX_REPRESENTATION)((dwToken & (3 << tShift)) >> tShift);
      }
    }
  }
  if (Operand.m_bExtendedOperand)
  {
    DWORD dwToken = *pTokens++;
    Operand.m_ExtendedOperandType = (D3D10_SB_EXTENDED_OPERAND_TYPE)(dwToken & 0x3f);
    if (Operand.m_ExtendedOperandType == D3D10_SB_EXTENDED_OPERAND_MODIFIER)
    {
      Operand.m_Modifier = (D3D10_SB_OPERAND_MODIFIER)((dwToken >> 6) & 0xff);
    }
  }
  for (i=0; i<Operand.m_IndexDimension; i++)
  {
    switch (Operand.m_IndexType[i])
    {
      case D3D10_SB_OPERAND_INDEX_IMMEDIATE32:
        Operand.m_Index[i].m_CodeOffset = pTokens - pStart;
        Operand.m_Index[i].m_RegIndexA[1] = *pTokens++;
        Operand.m_Index[i].m_ComponentName = Operand.m_ComponentName;
        Operand.m_Index[i].m_RelRegType = Operand.m_Type;
        break;
      case D3D10_SB_OPERAND_INDEX_IMMEDIATE64:
        Operand.m_Index[i].m_CodeOffset = pTokens - pStart;
        Operand.m_Index[i].m_RegIndex64 = *(uint64 *)pTokens;
        pTokens++; pTokens++;
        Operand.m_Index[i].m_ComponentName = Operand.m_ComponentName;
        Operand.m_Index[i].m_RelRegType = Operand.m_Type;
        break;
      case D3D10_SB_OPERAND_INDEX_RELATIVE:
        {
          Operand.m_Index[i].m_CodeOffset = pTokens - pStart;
          SDXBCOperand operand;
          ZeroStruct(operand);
          sParseOperand(pTokens, operand, pStart);
          Operand.m_Index[i].m_RelIndexA[1] = operand.m_Index[0].m_RegIndexA[1];
          Operand.m_Index[i].m_RelIndexA1[1] = operand.m_Index[1].m_RegIndexA[1];
          Operand.m_Index[i].m_RelRegType = operand.m_Type;
          Operand.m_Index[i].m_IndexDimension = operand.m_IndexDimension;
          Operand.m_Index[i].m_ComponentName = operand.m_Index[0].m_ComponentName;
        }
        break;
      case D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE:
        {
          Operand.m_Index[i].m_CodeOffset = pTokens - pStart;
          Operand.m_Index[i].m_RegIndexA[1] = *pTokens++;
          SDXBCOperand operand;
          ZeroStruct(operand);
          sParseOperand(pTokens, operand, pStart);
          Operand.m_Index[i].m_RelIndexA[1] = operand.m_Index[0].m_RegIndexA[1];
          Operand.m_Index[i].m_RelIndexA1[1] = operand.m_Index[1].m_RegIndexA[1];
          Operand.m_Index[i].m_RelRegType = operand.m_Index[0].m_RelRegType;
          Operand.m_Index[i].m_IndexDimension = operand.m_IndexDimension;
          Operand.m_Index[i].m_ComponentName = operand.m_Index[0].m_ComponentName;
        }
        break;
    }
  }
}

void sParseInstruction(SDXBCInstruction& Inst, DWORD*& pTokens, DWORD *pStart)
{
  Inst.Clear();

  Inst.m_CodeOffset = pTokens - pStart;
  DWORD *pSt = pTokens;
  DWORD dwToken = *pTokens++;
  int i;
  Inst.m_bSaturate = dwToken & 0x2000;
  Inst.m_bExtended = dwToken >> 31;
  uint nInstructionLength = (dwToken >> 24) & 0x7f;
  Inst.m_OpCode = (EDXBCOp)(dwToken & 0x7ff);
  Inst.m_NumOperands = sGetNumInstructionOperands(Inst.m_OpCode);
  if (Inst.m_bExtended)
  {
    DWORD dwExt = *pTokens++;
    if (Inst.m_OpCode>=eDOP_ld && Inst.m_OpCode<=eDOP_ldms || (Inst.m_OpCode>=eDOP_sample && Inst.m_OpCode<=eDOP_sample_d))
    {
      Inst.m_OpCodeEx = (D3D10_SB_EXTENDED_OPCODE_TYPE)(dwExt & 0x3f);
      Inst.m_TexelOffset[0] = (dwExt >> 9) & 0xf;
      Inst.m_TexelOffset[1] = (dwExt >> 13) & 0xf;
      Inst.m_TexelOffset[2] = (dwExt >> 17) & 0xf;
      if (Inst.m_TexelOffset[0] & 8)
        Inst.m_TexelOffset[0] |= 0xf0;
      if (Inst.m_TexelOffset[1] & 8)
        Inst.m_TexelOffset[1] |= 0xf0;
      if (Inst.m_TexelOffset[2] & 8)
        Inst.m_TexelOffset[2] |= 0xf0;
    }
  }
  else
  {
    switch (Inst.m_OpCode)
    {
      case eDOP_dcl_input_siv:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        Inst.m_InputDeclSIV.m_Name = (D3D10_SB_NAME)(*pTokens++ & 0xffff); 
        break;
      case eDOP_dcl_input_sgv:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        Inst.m_InputDeclSGV.m_Name = (D3D10_SB_NAME)(*pTokens++ & 0xffff); 
        break;
      case eDOP_dcl_input:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_dcl_input_2:
        Inst.m_InputDeclSGV.m_Name = (D3D10_SB_NAME)((dwToken>>11) & 0xf); 
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_dcl_output:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_dcl_output_siv:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        Inst.m_OutputDeclSIV.m_Name = (D3D10_SB_NAME)(*pTokens++ & 0xffff); 
        break;
      case eDOP_dcl_temps:
        Inst.m_TempsDecl.m_NumTemps = *pTokens++;
        break;
      case eDOP_dcl_sampler:
        Inst.m_SamplerDecl.m_SamplerMode = (D3D10_SB_SAMPLER_MODE)((dwToken>>11)&0xf);
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_dcl_indexrange:
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        Inst.m_IndexRangeDecl.m_RegCount = *pTokens++;
        break;
      case eDOP_dcl_outputtopology:
        Inst.m_OutputTopologyDecl.m_Topology = (D3D10_SB_PRIMITIVE_TOPOLOGY)((dwToken>>11)&0x3f);
        break;
      case eDOP_dcl_inputprimitive:
        Inst.m_InputPrimitiveDecl.m_Primitive = (D3D10_SB_PRIMITIVE)((dwToken>>11)&0x3f);
        break;
      case eDOP_resinfo:
        Inst.m_ResInfoReturnType = (D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE)((dwToken>>11)&3);
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        sParseOperand(pTokens, Inst.m_Operands[1], pStart);
        sParseOperand(pTokens, Inst.m_Operands[2], pStart);
        break;
      case eDOP_dcl_resource:
        {
          Inst.m_ResourceDecl.m_Dimension = (D3D10_SB_RESOURCE_DIMENSION)((dwToken>>11)&0xf);
          sParseOperand(pTokens, Inst.m_Operands[0], pStart);
          DWORD dwRet = *pTokens++;
          Inst.m_ResourceDecl.m_ReturnType[0] = (D3D10_SB_RESOURCE_RETURN_TYPE)(dwRet & 0xf);
          Inst.m_ResourceDecl.m_ReturnType[1] = (D3D10_SB_RESOURCE_RETURN_TYPE)((dwRet>>4) & 0xf);
          Inst.m_ResourceDecl.m_ReturnType[2] = (D3D10_SB_RESOURCE_RETURN_TYPE)((dwRet>>8) & 0xf);
          Inst.m_ResourceDecl.m_ReturnType[3] = (D3D10_SB_RESOURCE_RETURN_TYPE)((dwRet>>12) & 0xf);
          Inst.m_ResourceDecl.m_SampleCount = (dwToken>>18) & 0x3f;
        }
        break;
      case eDOP_sample:
        Inst.m_SamplerDecl.m_SamplerMode = (D3D10_SB_SAMPLER_MODE)((dwToken>>11) & 0xf); 
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_dcl_constantbuffer:
        Inst.m_ConstantBufferDecl.m_AccessPattern = (D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN)((dwToken>>11)&1);
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_callc:
        Inst.m_Test = (D3D10_SB_INSTRUCTION_TEST_BOOLEAN)((dwToken>>12)&1);
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        sParseOperand(pTokens, Inst.m_Operands[1], pStart);
        break;
      case eDOP_continuec:
      case eDOP_discard:
      case eDOP_if:
      case eDOP_breakc:
      case eDOP_retc:
        Inst.m_Test = (D3D10_SB_INSTRUCTION_TEST_BOOLEAN)((dwToken>>12)&1);
        sParseOperand(pTokens, Inst.m_Operands[0], pStart);
        break;
      case eDOP_mem:
        Inst.m_CustomData.m_Type = (D3D10_SB_CUSTOMDATA_CLASS)(dwToken>>12);
        if (*pTokens < nInstructionLength)
        {
          Inst.m_CustomData.m_DataSizeInBytes = 0;
          Inst.m_CustomData.m_pData = NULL;
        }
        else
        {
          nInstructionLength = *pTokens++;
          Inst.m_CustomData.m_DataSizeInBytes = nInstructionLength*4-8;
          Inst.m_CustomData.m_pData = new byte[Inst.m_CustomData.m_DataSizeInBytes];
          memcpy(Inst.m_CustomData.m_pData, pTokens, Inst.m_CustomData.m_DataSizeInBytes);
        }
        break;
      default:
        {
          for (i=0; i<Inst.m_NumOperands; i++)
          {
            sParseOperand(pTokens, Inst.m_Operands[i], pStart);
          }
        }
        break;
    }
  }

  //assert(pTokens == &pSt[nInstructionLength]);
  pTokens = &pSt[nInstructionLength];
}

void sPrintRegister(D3D10_SB_OPERAND_TYPE oType, string& op)
{
  switch (oType)
  {
    case D3D10_SB_OPERAND_TYPE_TEMP:
      op += "r";
      break;
    case D3D10_SB_OPERAND_TYPE_INPUT:
      op += "v";
      break;
    case D3D10_SB_OPERAND_TYPE_OUTPUT:
      op += "o";
      break;
    case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP:
      op += "x";
      break;
    case D3D10_SB_OPERAND_TYPE_IMMEDIATE32:
      op += "l";
      break;
    case D3D10_SB_OPERAND_TYPE_IMMEDIATE64:
      op += "d";
      break;
    case D3D10_SB_OPERAND_TYPE_SAMPLER:
      op += "s";
      break;
    case D3D10_SB_OPERAND_TYPE_RESOURCE:
      op += "t";
      break;
    case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER:
      op += "cb";
      break;
    case D3D10_SB_OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER:
      op += "icb";
      break;
    case D3D10_SB_OPERAND_TYPE_LABEL:
      op += "label";
      break;
    case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID:
      op += "primID";
      break;
    case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
      op += "oDepth";
      break;
    case D3D10_SB_OPERAND_TYPE_NULL:
      op += "null";
      break;
  }
}

void sPrintComponent(uint nComponent, string& op)
{
  switch (nComponent)
  {
    case 0:
      op += "x";
      break;
    case 1:
      op += "y";
      break;
    case 2:
      op += "z";
      break;
    case 3:
      op += "w";
      break;
  }
}

void sPrintOperand(SDXBCOperand* pOper)
{
  string op;
  string sf;
  int i;
  if (pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_NEG || pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_ABSNEG)
    op += "-";
  if (pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_ABS || pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_ABSNEG)
    op += "|";
  sPrintRegister(pOper->m_Type, op);
  if (pOper->m_Type != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 && pOper->m_Type != D3D10_SB_OPERAND_TYPE_IMMEDIATE64)
  {
    if (pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D && pOper->m_Index[0].m_RelRegType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32)
    {
      op += sf.Format("%d", pOper->m_Index[0].m_RegIndexA[1]);
    }
    else
    if (pOper->m_IndexDimension > D3D10_SB_OPERAND_INDEX_0D)
    {
      for (i=0; i<pOper->m_IndexDimension; i++)
      {
        if (pOper->m_IndexType[i] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32)
        {
          if (!i)
          {
            if (pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D)
              op += sf.Format("%d", pOper->m_Index[i].m_RegIndexA[1]);
            else
            {
              if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_INPUT)
                op += sf.Format("[%d]", pOper->m_Index[i].m_RegIndexA[1]);
              else
              if (pOper->m_Type != D3D10_SB_OPERAND_TYPE_OUTPUT)
                op += sf.Format("%d", pOper->m_Index[i].m_RegIndexA[1]);
              else
                goto st;
            }
          }
          else
          {
  st:
            if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_INPUT ||
                pOper->m_Type == D3D10_SB_OPERAND_TYPE_OUTPUT ||
                pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER ||
                pOper->m_Type == D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP)
              op += sf.Format("[%d]", pOper->m_Index[i].m_RegIndexA[1]);
            else
              op += sf.Format("%d", pOper->m_Index[i].m_RegIndexA[1]);
          }
        }
        if (pOper->m_IndexType[i] == D3D10_SB_OPERAND_INDEX_RELATIVE || pOper->m_IndexType[i] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE)
        {
          op += "[";
          sPrintRegister(pOper->m_Index[i].m_RelRegType, op);
          if (pOper->m_Index[i].m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D)
          {
            if (pOper->m_Index[i].m_RelRegType != D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
            {
              op += sf.Format("_%d_%d.", pOper->m_Index[i].m_RelIndexA[1], pOper->m_Index[i].m_RelIndexA1[1]);
            }
          }
          else
            op += sf.Format("%d.", pOper->m_Index[i].m_RelIndexA[1]);
          sPrintComponent(pOper->m_Index[i].m_ComponentName, op);
          op += sf.Format(" + %d]", pOper->m_Index[i].m_RegIndexA[1]);
        }
      }
    }
    if (pOper->m_NumComponents != D3D10_SB_OPERAND_0_COMPONENT && pOper->m_NumComponents != D3D10_SB_OPERAND_1_COMPONENT)
    {
      if (pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)
      {
        op += ".";
        if (pOper->m_WriteMask & 0x10)
          op += "x";
        if (pOper->m_WriteMask & 0x20)
          op += "y";
        if (pOper->m_WriteMask & 0x40)
          op += "z";
        if (pOper->m_WriteMask & 0x80)
          op += "w";
      }
      else
      if (pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
      {
        op += ".";
        for (i=0; i<4; i++)
        {
          sPrintComponent(pOper->m_Swizzle[i], op);
        }
      }
      else
      if (pOper->m_Type != D3D10_SB_OPERAND_TYPE_SAMPLER && pOper->m_Type != D3D10_SB_OPERAND_TYPE_RESOURCE)
      {
        op += ".";
        sPrintComponent(pOper->m_ComponentName, op);
      }
    }
  }
  else
  {
    assert(0);
  }
  if (pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_ABS || pOper->m_Modifier == D3D10_SB_OPERAND_MODIFIER_ABSNEG)
    op += "|";
}

struct SCBRemap
{
  byte bTouched;
  int nCB;
  int nR;
};

struct SRegMap
{
  int nByteOffs;
  int nByteSize;
  int nOrigVar;
};

static DWORD sEncodeOperandOp(SDXBCOperand *pOper)
{
  DWORD dwOper = (pOper->m_NumComponents & 3) |
    ((pOper->m_Type & 0xff) << 12);
  if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
  {
    assert(pOper->m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT);
    dwOper |= ((pOper->m_ComponentSelection & 3) << 2);
    if (pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
    {
      dwOper |= ((pOper->m_Swizzle[0] & 3) << 4) |
        ((pOper->m_Swizzle[1] & 3) << 6) |
        ((pOper->m_Swizzle[2] & 3) << 8) |
        ((pOper->m_Swizzle[3] & 3) << 10);
    }
    else
    if (pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)
    {
      dwOper |= ((pOper->m_Swizzle[0] & 3) << 4);
    }
    else
    {
      assert(0);
    }
    assert(pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
    dwOper |= ((pOper->m_IndexDimension & 3) << 20) |
      ((pOper->m_IndexType[0] & 3) << 22) |
      ((pOper->m_IndexType[1] & 3) << 25);
  }
  else
  {
    assert(0);
  }

  return dwOper;
}

bool PatchDXBCShaderCode(LPD3D10BLOB& pShader, CHWShader_D3D *pSh)
{
  int i, j, n, m;
  if (!pShader)
    return false;
  byte *pCode = (byte *)pShader->GetBufferPointer();
  if (!pCode)
    return false;

  UINT nSize = pShader->GetBufferSize();
  std::vector<SRegMap> RMap[CB_MAX];

  SDXBCHeader *pHeader = (SDXBCHeader *)pCode;
  if (pHeader->Id != MAKEFOURCC('D','X','B','C'))
    return false;

  bool bRes = true;
  uint32 *pOffsets = (uint32 *)(pCode + sizeof(SDXBCHeader));

  // Read resource definitions
  SDXBCShaderReflection Refl;
  {
    byte *pByteCodeRDEF = (byte *)sFindDXBCSection(MAKEFOURCC('R','D','E','F'), pCode, pHeader, pOffsets);
    SDXBCShaderRDEF *pRDEF = (SDXBCShaderRDEF *)pByteCodeRDEF;

    Refl.Desc.Version = pRDEF->m_Version;
    Refl.Desc.Flags = pRDEF->m_Flags;
    Refl.Desc.ConstantBuffers = pRDEF->m_CBuffers;
    Refl.Desc.BoundResources = pRDEF->m_NumBindResources;
    sCopyStringFromDXBC(const_cast<const char**>(&Refl.Desc.Creator), &pByteCodeRDEF[pRDEF->m_OffsCreator]);
    for (i=0; i<Refl.Desc.ConstantBuffers; i++)
    {
      byte *pByteCodeCB = pByteCodeRDEF + pRDEF->m_OffsetCBuffers;
      SDXBCShaderCB *pDXCB = (SDXBCShaderCB *)pByteCodeCB;
      pDXCB += i;

      SDXBCShaderReflectionConstantBuffer *pRCB = Refl.m_ConstantBuffers.AddIndex(1);
      ZeroStruct(*pRCB);
      sCopyStringFromDXBC(const_cast<const char**>(&pRCB->Desc.Name), &pByteCodeRDEF[pDXCB->OffsName]);
      pRCB->Desc.Type = pDXCB->Type;
      pRCB->Desc.Variables = pDXCB->Variables;
      pRCB->Desc.Size = pDXCB->Size;
      pRCB->Desc.uFlags = (pDXCB->uFlags & D3D10_CBF_USERPACKED) ? D3D10_CBF_USERPACKED : 0;
      for (j=0; j<pRCB->Desc.Variables; j++)
      {
        byte *pByteCodeCBVar = pByteCodeRDEF + pDXCB->OffsetVars;
        SDXBCShaderVariable *pDXCBVar = (SDXBCShaderVariable *)pByteCodeCBVar;
        pDXCBVar += j;
        SDXBCShaderReflectionVariable *pVar = pRCB->m_Vars.AddIndex(1);
        ZeroStruct(*pVar);
        sCopyStringFromDXBC(const_cast<const char**>(&pVar->VarDesc.Name), &pByteCodeRDEF[pDXCBVar->OffsName]);
        pVar->VarDesc.StartOffset = pDXCBVar->StartOffset;
        pVar->VarDesc.Size = pDXCBVar->Size;
        pVar->VarDesc.uFlags = (pDXCBVar->uFlags & D3D10_SVF_USERPACKED) ? D3D10_SVF_USERPACKED : 0;
        if (pVar->VarDesc.Size>0 && pDXCBVar->OffsDefault)
        {
          pVar->VarDesc.DefaultValue = new byte[pVar->VarDesc.Size];
          byte *pByteCodeCBVarDef = pByteCodeRDEF + pDXCBVar->OffsDefault;
          memcpy(pVar->VarDesc.DefaultValue, pByteCodeCBVarDef, pVar->VarDesc.Size);
        }
        byte *pByteCodeCBVarType = pByteCodeRDEF + pDXCBVar->OffsType;
        SDXBCShaderVariableType *pDXCBVarType = (SDXBCShaderVariableType *)pByteCodeCBVarType;
        pVar->Type.Desc.Class = (D3D10_SHADER_VARIABLE_CLASS)pDXCBVarType->Class;
        pVar->Type.Desc.Type = (D3D10_SHADER_VARIABLE_TYPE)pDXCBVarType->Type;
        pVar->Type.Desc.Rows = pDXCBVarType->Rows;
        pVar->Type.Desc.Columns = pDXCBVarType->Columns;
        pVar->Type.Desc.Elements = pDXCBVarType->Elements;
        pVar->Type.Desc.Members = pDXCBVarType->Members;
        assert(pVar->Type.Desc.Members == 0);
      }
    }
    for (i=0; i<Refl.Desc.BoundResources; i++)
    {
      byte *pByteCodeBR = pByteCodeRDEF + pRDEF->m_OffsBindResources;
      SDXBCShaderResourceBinding *pDXBR = (SDXBCShaderResourceBinding *)pByteCodeBR;
      pDXBR += i;
      SDXBCShaderReflectionResourceBinding *pBR = Refl.m_ResourceBindings.AddIndex(1);
      ZeroStruct(*pBR);
      sCopyStringFromDXBC(const_cast<const char**>(&pBR->Desc.Name), &pByteCodeRDEF[pDXBR->OffsName]);
      pBR->Desc.Type = pDXBR->Type;
      pBR->Desc.BindPoint = pDXBR->BindPoint;
      pBR->Desc.BindCount = pDXBR->BindCount;
      pBR->Desc.uFlags = (pDXBR->uFlags & D3D10_SIF_USERPACKED) ? D3D10_SIF_USERPACKED : 0;
      pBR->Desc.ReturnType = pDXBR->ReturnType;
      pBR->Desc.Dimension = pDXBR->Dimension;
      pBR->Desc.NumSamples = pDXBR->NumSamples;
    }
  }
  assert(Refl.Desc.ConstantBuffers <= 1);
  if (Refl.Desc.ConstantBuffers < 1)
    return true;

  static bool bInitInstructions = false;
  if (!bInitInstructions)
  {
    bInitInstructions = true;
    sInitInstructions();
  }

  // Parse shader byte-code
  byte *pByteCode = (byte *)sFindDXBCSection(MAKEFOURCC('S','H','D','R'), pCode, pHeader, pOffsets);
  int nTokens = *(DWORD *)&pByteCode[4];
  int nType = *(WORD *)&pByteCode[2];
  SDXBCInstruction Inst;
  TArray<SDXBCInstruction> Insts;
  DWORD *pStart = (DWORD *)&pByteCode[8];
  DWORD *pTokens = pStart;
  DWORD *pEnd = pStart + nTokens - 2;
  while (pTokens != pEnd)
  {
    sParseInstruction(Inst, pTokens, pStart);
    Insts.Add(Inst);
  }

  SDXBCShaderReflectionConstantBuffer *pRCB = &Refl.m_ConstantBuffers[0];

  // Patch shader byte-code
  SCBRemap *pCBuf = new SCBRemap[4096];
  memset(pCBuf, 0, sizeof(SCBRemap)*4096);
  int nRegCounts[CB_MAX];
  memset(nRegCounts, 0, sizeof(nRegCounts));
  for (i=0; i<Insts.Num(); i++)
  {
    SDXBCInstruction *pInst = &Insts[i];
    for (j=0; j<pInst->m_NumOperands; j++)
    {
      SDXBCOperand *pOper = &pInst->m_Operands[j];
      if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
      {
        if (pInst->m_OpCode == eDOP_dcl_constantbuffer)
          continue;
        assert(pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
        if (pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D)
        {
          assert(pOper->m_Index[0].m_RegIndexA[1] == 0);
          if (pOper->m_Index[0].m_RegIndexA[1] == 0)
          {
            int nReg = pOper->m_Index[1].m_RegIndexA[1];
            if (pCBuf[nReg].bTouched)
              continue;
            for (n=0; n<pRCB->Desc.Variables; n++)
            {
              SDXBCShaderReflectionVariable* pCV = &pRCB->m_Vars[n];
              SDXBCShaderReflectionVariableType* pVT = &pCV->Type;
              if (pVT->Desc.Class==D3D10_SVC_VECTOR || pVT->Desc.Class==D3D10_SVC_SCALAR || pVT->Desc.Class==D3D10_SVC_MATRIX_COLUMNS || pVT->Desc.Class==D3D10_SVC_MATRIX_ROWS)
              {
                //assert(!(CDesc.StartOffset & 0xf));
                uint nV = (pCV->VarDesc.StartOffset>>4);
                uint nVectors = ((pCV->VarDesc.Size+15)>>4);
                assert(nV<4096 && nV+nVectors<=4096);
                if (nReg<nV || nReg>=nV+nVectors)
                  continue;
                SFXParam *pr = gRenDev->m_cEF.mfGetFXParameter(pSh->m_Params, pCV->VarDesc.Name);
                if (!pr)
                {
                  if (!strncmp(pCV->VarDesc.Name, "_g_", 3))
                  {
                    SFXParam spr;
                    pr = &spr;
                  }
                }
                assert(pr);
                if (pr)
                {
                  int nCB = -1;
                  if (pr->m_Assign.size())
                  {
                    SParamDB *pDB = gRenDev->m_cEF.mfGetShaderParamDB(pr->m_Assign.c_str());
                    assert(pDB);
                    /*if (pDB)
                    {
                      if (pDB->nFlags & PD_USAGE_PERBATCH)
                        nCB = CB_PER_BATCH;
                      else
                      if (pDB->nFlags & PD_USAGE_PERINSTANCE)
                        nCB = CB_PER_INSTANCE;
                      else
                      if (pDB->nFlags & PD_USAGE_PERMATERIAL)
                        nCB = CB_PER_MATERIAL;
                      else
                      if (pDB->nFlags & PD_USAGE_GLOBAL)
                        nCB = CB_PER_FRAME;
                      else
                      if (pDB->nFlags & PD_USAGE_SKINDATA)
                        nCB = CB_SKIN_DATA;
                      else
                      if (pDB->nFlags & PD_USAGE_SHAPEDATA)
                        nCB = CB_SHAPE_DATA;
                      assert(nCB >= 0);
                    }*/
                  }
                  if (nCB < 0)
                    nCB = CB_PER_BATCH;
                  for (int nn = 0; nn<nVectors; nn++)
                  {
                    pCBuf[nV+nn].nCB = nCB;
                    pCBuf[nV+nn].bTouched = true;
                    pCBuf[nV+nn].nR = nRegCounts[nCB]++;
                  }
                }
              }
            }
          }
        }
      }
      //sPrintOperand(pOper);
    }
  }

  // Encode changes to bytecode
  TArray<byte> NewCode;
  NewCode.Copy(pCode, nSize);
  int nOffsStart = pStart - (DWORD *)pCode;
  for (i=0; i<Insts.Num(); i++)
  {
    SDXBCInstruction *pInst = &Insts[i];
    for (j=0; j<pInst->m_NumOperands; j++)
    {
      SDXBCOperand *pOper = &pInst->m_Operands[j];
      if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
      {
        if (pInst->m_OpCode == eDOP_dcl_constantbuffer)
          continue;
        assert(pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
        if (pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D)
        {
          assert(pOper->m_Index[0].m_RegIndexA[1] == 0);
          if (pOper->m_Index[0].m_RegIndexA[1] == 0)
          {
            if (pOper->m_IndexType[1] == D3D10_SB_OPERAND_INDEX_RELATIVE)
            {
              // Change Relative index to Immediate_Plus_Relative index type
              pOper->m_IndexType[1] = D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE;
              pOper->m_Index[1].m_RegIndexA[1] = 0;
              int nOffset = (pOper->m_Index[1].m_CodeOffset+nOffsStart)*sizeof(DWORD);

              // Update CB offset
              NewCode.Insert(nOffset, sizeof(DWORD));
              *(DWORD *)(&NewCode[nOffset]) = 0;
              pOper->m_Index[0].m_CodeOffset--;
              pOper->m_Index[1].m_CodeOffset--;

              // Encode instruction DWORD
              DWORD dwNewOp = sEncodeOperandOp(pOper);
              *(DWORD *)(&NewCode[(pOper->m_CodeOffset+nOffsStart)*sizeof(DWORD)]) = dwNewOp;

              // Update instruction length;
              DWORD dwToken = *(DWORD *)(&NewCode[(pInst->m_CodeOffset+nOffsStart)*sizeof(DWORD)]);
              uint nInstructionLength = (dwToken >> 24) & 0x7f;
              nInstructionLength++;
              dwToken &= ~(0x7F << 24);
              dwToken |= (nInstructionLength << 24);
              *(DWORD *)(&NewCode[(pInst->m_CodeOffset+nOffsStart)*sizeof(DWORD)]) = dwToken;
              nOffsStart++;
            }
            if (pOper->m_IndexType[1] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE)
            {
              int nnn = 0;
            }
            int nReg = pOper->m_Index[1].m_RegIndexA[1];
            SCBRemap *pCBRemap = &pCBuf[nReg];
            assert (pCBRemap->bTouched);
            if (pCBRemap->bTouched)
            {
              // Search for original variable in resource for the CB register operand
              SDXBCShaderReflectionVariable *pV;
              for (n=0; n<pRCB->m_Vars.Num(); n++)
              {
                pV = &pRCB->m_Vars[n];
                int nRegV = pV->VarDesc.StartOffset >> 4;
                int nRegsV = (pV->VarDesc.Size+15) >> 4;
                if (nReg>=nRegV && nReg<nRegV+nRegsV)
                {
                  if (pV->VarDesc.Size & 0xf)
                  {
                    assert(pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE || pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE);
                    if (pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE || pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)
                    {
                      assert(pV->VarDesc.Size <= 12);
                      int nComp = pOper->m_Swizzle[0];
                      int nCompV = (pV->VarDesc.StartOffset & 0xf) >> 2;
                      int nCompsV = (pV->VarDesc.Size & 0xf) >> 2;
                      if (nComp<nCompV || nComp>=nCompV+nCompsV)
                        continue;
                    }
                  }
                  break;
                }
              }
              assert(n != pRCB->m_Vars.Num());
              int nCB = pCBRemap->nCB;
              for (m=0; m<RMap[nCB].size(); m++)
              {
                SRegMap *pRM = &RMap[nCB][m];
                if (pRM->nOrigVar == n)
                  break;
              }
              // Add modified CB variable to the list
              if (m == RMap[nCB].size())
              {
                SRegMap rm;
                rm.nOrigVar = n;
                rm.nByteOffs = (pCBRemap->nR << 4) | (pV->VarDesc.StartOffset & 0xf);
                rm.nByteSize = pV->VarDesc.Size;
                RMap[nCB].push_back(rm);
              }
              DWORD *pIndexCode0 = (DWORD *)&NewCode[(nOffsStart+pOper->m_Index[0].m_CodeOffset)*sizeof(DWORD)];
              DWORD *pIndexCode1 = (DWORD *)&NewCode[(nOffsStart+pOper->m_Index[1].m_CodeOffset)*sizeof(DWORD)];
              *pIndexCode0 = pCBRemap->nCB;
              *pIndexCode1 = pCBRemap->nR;
            }
          }
        }
      }
    }
  }

  // Add CB declarations to bytecode
  nOffsStart = pStart - (DWORD *)pCode;
  for (i=0; i<Insts.Num(); i++)
  {
    SDXBCInstruction *pInst = &Insts[i];
    if (pInst->m_OpCode != eDOP_dcl_constantbuffer)
      continue;
    assert (pInst->m_NumOperands == 1);
    SDXBCOperand *pOper = &pInst->m_Operands[0];
    assert (pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER);
    if (pOper->m_Type == D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER)
    {
      assert(pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
      assert(pOper->m_ComponentSelection == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE);
      if (pOper->m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D)
      {
        assert(pOper->m_Index[0].m_RegIndexA[1] == 0);
        if (pOper->m_Index[0].m_RegIndexA[1] == 0)
        {
          int nNextOffset = (nOffsStart+pInst->m_CodeOffset)*sizeof(DWORD);
          bool bFirst = true;
          for (j=0; j<CB_MAX; j++)
          {
            if (!nRegCounts[j])
              continue;
            DWORD cbDecl[4];
            cbDecl[0] = eDOP_dcl_constantbuffer | (4<<24);
            cbDecl[1] = sEncodeOperandOp(pOper);
            cbDecl[2] = j;
            cbDecl[3] = nRegCounts[j];
            if (!bFirst)
              NewCode.Insert(nNextOffset, 4*sizeof(DWORD));
            else
              bFirst = false;
            memcpy(&NewCode[nNextOffset], cbDecl, 4*sizeof(DWORD));
            nNextOffset += 4*sizeof(DWORD);
          }
        }
      }
    }
    break;
  }

  // Add new updated resource definitions
  {
    TArray <byte> RDEF;
    byte *pCode = &NewCode[0];
    SDXBCHeader *pHeader = (SDXBCHeader *)pCode;
    uint32 *pOffsets = (uint32 *)(pCode + sizeof(SDXBCHeader));
    byte *pByteCodeRDEF = (byte *)sFindDXBCSection(MAKEFOURCC('R','D','E','F'), pCode, pHeader, pOffsets);
    SDXBCShaderRDEF *pRDEF = (SDXBCShaderRDEF *)pByteCodeRDEF;
    int nMax = -1;
    for (i=0; i<CB_MAX; i++)
    {
      if (RMap[i].size())
        nMax = i;
    }
    assert(nMax >= 0);
    nMax++;
    pRDEF->m_CBuffers = nMax;
    RDEF.Copy(pByteCodeRDEF, sizeof(SDXBCShaderRDEF));
    char *szNamesCB[CB_MAX] = {"$PER_BATCH", "$PER_INSTANCE", "$PER_FRAME", "$PER_MATERIAL", "$SKIN_DATA", "$SHAPE_DATA"};
    int nOffs = sizeof(SDXBCShaderRDEF);
    pRDEF = (SDXBCShaderRDEF *)&RDEF[0];
    pRDEF->m_OffsCreator = nOffs;
    uint nLen = strlen(Refl.Desc.Creator)+1;
    RDEF.Align4Copy((byte *)Refl.Desc.Creator, nLen);
    nOffs += nLen;

    int nStringOffset[CB_MAX+1];
    for (i=0; i<nMax; i++)
    {
      nStringOffset[i] = nOffs;
      uint nLen = strlen(szNamesCB[i])+1;
      RDEF.Align4Copy((byte *)szNamesCB[i], nLen);
      nOffs += nLen;
    }
    pRDEF = (SDXBCShaderRDEF *)&RDEF[0];
    pRDEF->m_OffsetCBuffers = nOffs;
    nOffs += nMax*sizeof(SDXBCShaderCB);
    for (i=0; i<nMax; i++)
    {
      SDXBCShaderCB DXCB;
      DXCB.OffsName = nStringOffset[i];
      DXCB.Type = D3D10_CT_CBUFFER;
      DXCB.Variables = RMap[i].size();
      DXCB.uFlags = 0;
      DXCB.Size = 0;
      DXCB.uFlags = 0;
      DXCB.OffsetVars = nOffs;
      RDEF.Copy((byte *)&DXCB, sizeof(DXCB));
      nOffs += DXCB.Variables*sizeof(SDXBCShaderVariable);
    }
    std::vector<SDXBCShaderVariableType> DXBCTypes;
    for (i=0; i<nMax; i++)
    {
      pRDEF = (SDXBCShaderRDEF *)&RDEF[0];
      SDXBCShaderCB *pSCB = (SDXBCShaderCB *)&RDEF[pRDEF->m_OffsetCBuffers];
      pSCB += i;
      for (j=0; j<pSCB->Variables; j++)
      {
        SRegMap *pRM = &RMap[i][j];
        SDXBCShaderReflectionVariable *pV = &pRCB->m_Vars[pRM->nOrigVar];
        for (n=0; n<DXBCTypes.size(); n++)
        {
          SDXBCShaderVariableType *pType = &DXBCTypes[n];
          if (pType->Class == pV->Type.Desc.Class &&
              pType->Columns == pV->Type.Desc.Columns &&
              pType->Rows == pV->Type.Desc.Rows &&
              pType->Type == pV->Type.Desc.Type &&
              pType->Members == pV->Type.Desc.Members &&
              pType->Elements == pV->Type.Desc.Elements)
            break;
        }
        if (n == DXBCTypes.size())
        {
          SDXBCShaderVariableType Type;
          Type.Class = pV->Type.Desc.Class;
          Type.Columns = pV->Type.Desc.Columns;
          Type.Rows = pV->Type.Desc.Rows;
          Type.Type = pV->Type.Desc.Type;
          Type.Members = pV->Type.Desc.Members;
          Type.Elements = pV->Type.Desc.Elements;
          DXBCTypes.push_back(Type);
        }
        pV->nType = n;
      }
    }
    int nOffsNames = nOffs + DXBCTypes.size()*sizeof(SDXBCShaderVariableType);
    TArray<char> Names;
    for (i=0; i<nMax; i++)
    {
      pRDEF = (SDXBCShaderRDEF *)&RDEF[0];
      SDXBCShaderCB *pSCB = (SDXBCShaderCB *)&RDEF[pRDEF->m_OffsetCBuffers];
      pSCB += i;
      int nVars = pSCB->Variables;
      for (j=0; j<nVars; j++)
      {
        SRegMap *pRM = &RMap[i][j];
        SDXBCShaderReflectionVariable *pV = &pRCB->m_Vars[pRM->nOrigVar];
        SDXBCShaderVariable DXV;
        DXV.Size = pRM->nByteSize;
        DXV.StartOffset = pRM->nByteOffs;
        DXV.OffsDefault = 0;
        DXV.uFlags = pV->VarDesc.uFlags;
        DXV.OffsType = nOffs + pV->nType*sizeof(SDXBCShaderVariableType);
        uint nLen = strlen(pV->VarDesc.Name)+1;
        Names.Align4Copy(pV->VarDesc.Name, nLen);
        DXV.OffsName = nOffsNames;
        nOffsNames += nLen;
        RDEF.Copy((byte *)&DXV, sizeof(DXV));
      }
    }
    RDEF.Copy((byte *)&DXBCTypes[0], DXBCTypes.size()*sizeof(SDXBCShaderVariableType));
    RDEF.Copy((byte *)&Names[0], Names.Num());
    pRDEF = (SDXBCShaderRDEF *)&RDEF[0];
    int nBindRes = pRDEF->m_NumBindResources;

    int nBindings = 0;
    // CB resources bindings
    for (i=0; i<nMax; i++)
    {
      if (RMap[i].size())
        nBindings++;
    }
    for (i=0; i<nBindRes; i++)
    {
      SDXBCShaderReflectionResourceBinding *pRB = &Refl.m_ResourceBindings[i];
      // Ignore constant buffers
      if (pRB->Desc.Type == D3D10_SIT_CBUFFER)
        continue;
      nBindings++;
    }
    pRDEF->m_NumBindResources = nBindings;

    if (nBindings)
    {
      pRDEF->m_OffsBindResources = RDEF.Num();
      TArray<char> Names;
      int nOffsNames = pRDEF->m_OffsBindResources + (nBindings * sizeof(SDXBCShaderResourceBinding));
      // CB resources bindings
      for (i=0; i<nMax; i++)
      {
        if (!RMap[i].size())
          continue;
        SDXBCShaderResourceBinding RB;
        RB.OffsName = nOffsNames;
        uint nLen = strlen(szNamesCB[i])+1;
        Names.Align4Copy(szNamesCB[i], nLen);
        nOffsNames += nLen;
        RB.Type = D3D10_SIT_CBUFFER;
        RB.BindPoint = i;
        RB.BindCount = 1;
        RB.uFlags = 0;
        RB.ReturnType = (D3D10_RESOURCE_RETURN_TYPE)0;
        RB.Dimension = D3D10_SRV_DIMENSION_UNKNOWN;
        RB.NumSamples = 0;
        RDEF.Copy((byte *)&RB, sizeof(RB));
      }
      // Other bindings
      for (i=0; i<nBindRes; i++)
      {
        SDXBCShaderResourceBinding RB;
        SDXBCShaderReflectionResourceBinding *pRB = &Refl.m_ResourceBindings[i];
        // Ignore constant buffers
        if (pRB->Desc.Type == D3D10_SIT_CBUFFER)
          continue;
        RB.OffsName = nOffsNames;
        uint nLen = strlen(pRB->Desc.Name)+1;
        Names.Align4Copy(pRB->Desc.Name, nLen);
        nOffsNames += nLen;
        RB.Type = pRB->Desc.Type;
        RB.BindPoint = pRB->Desc.BindPoint;
        RB.BindCount = pRB->Desc.BindCount;
        RB.uFlags = pRB->Desc.uFlags;
        RB.ReturnType = pRB->Desc.ReturnType;
        RB.Dimension = pRB->Desc.Dimension;
        RB.NumSamples = pRB->Desc.NumSamples;
        RDEF.Copy((byte *)&RB, sizeof(RB));
      }
      RDEF.Copy((byte *)&Names[0], Names.Num());
    }
    
    // Update the final code
    int nOffsRDEF = pByteCodeRDEF - pCode;
    int nSizeOld = *(DWORD *)&NewCode[nOffsRDEF-4];
    int nDif = RDEF.Num() - nSizeOld;
    NewCode.Remove(nOffsRDEF, nSizeOld);
    *(DWORD *)&NewCode[nOffsRDEF-4] = RDEF.Num();
    NewCode.Insert(nOffsRDEF, RDEF.Num());
    memcpy(&NewCode[nOffsRDEF], &RDEF[0], RDEF.Num());

    // Update sections offsets
    pHeader = (SDXBCHeader *)&NewCode[0];
    pOffsets = (uint32 *)(&pHeader[1]);
    for (i=0; i<pHeader->nFOURCCSections; i++)
    {
      if (pOffsets[i] > nOffsRDEF)
        pOffsets[i] += nDif;
    }
  }

  int nSizeHash = NewCode.Num() - 20;
  SDXBCHeader *pHeaderNew = (SDXBCHeader *)&NewCode[0];
  pHeaderNew->nCodeSize = NewCode.Num();
  sComputeCRC((byte *)&pHeaderNew->VersionLow, nSizeHash, pHeaderNew->CRC);

  SAFE_RELEASE(pShader);
  D3D10CreateBlob(NewCode.Num(), &pShader);
  void *pBuf = pShader->GetBufferPointer();
  memcpy(pBuf, &NewCode[0], NewCode.Num());

  SAFE_DELETE_ARRAY(pCBuf);

  return bRes;
}
#endif
