// Geometric Tools, Inc.
// http://www.geometrictools.com
// Copyright (c) 1998-2006.  All Rights Reserved
//
// The Wild Magic Library (WM3) source code is supplied under the terms of
// the license agreement
//     http://www.geometrictools.com/License/WildMagic3License.pdf
// and may not be copied or disclosed except in accordance with the terms
// of that agreement.

// Added/modified by Danny June 2006

#ifndef EDGE2_H
#define EDGE2_H

class Edge2
{
public:
  Edge2 (int j0 = -1, int j1 = -1) {i0 = j0; i1 = j1;}

  bool operator< (const Edge2& rkE) const {
    if (i1 < rkE.i1) return true;
    if (i1 == rkE.i1) return i0 < rkE.i0;
    return false;
  }
  bool operator== (const Edge2& rkE) const {return i0 == rkE.i0 && i1 == rkE.i1;}

  int i0, i1;
};

#endif

