////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brush.cpp
//  Version:     v1.00
//  Created:     8/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History: Based on Andrey's Indoor editor.
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Brush.h"

#include "BrushPlane.h"
#include "BrushPoly.h"
#include "BrushFace.h"
#include "Settings.h"
#include "Grid.h"

#include "Objects\DisplayContext.h"
#include "Objects\SubObjSelection.h"
#include "Viewport.h"
#include "HitCOntext.h"
#include "ITransformManipulator.h"

#include <IIndexedMesh.h>
#include <IRenderer.h>
#include <I3Dengine.h>
#include <IRenderAuxGeom.h>

#define MIN_BOUNDS_SIZE 0.01f

#define SIDE_FRONT  0
#define SIDE_BACK   1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Undo object for Editable Mesh.
class CUndoBrush : public IUndoObject
{
public:
	CUndoBrush( SBrush *pBrush,const char *undoDescription )
	{
		// Stores the current state of this object.
		assert( pBrush != 0 );
		if (undoDescription)
			m_undoDescription = undoDescription;
		else
			m_undoDescription = "SolidBrushUndo";
		m_undo = *pBrush;
		m_pBrush = pBrush;
	}
protected:
	virtual int GetSize()
	{
		// sizeof(undoMesh) + sizeof(redoMesh);
		return sizeof(*this);
	}
	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_redo = *m_pBrush;
		}
		// Undo object state.
		*m_pBrush = m_undo;
		m_pBrush->BuildSolid(false);
	}
	virtual void Redo()
	{
		*m_pBrush = m_redo;
		m_pBrush->BuildSolid(false);
	}

private:
	CString m_undoDescription;
	_smart_ptr<SBrush> m_pBrush;
	SBrush m_undo;
	SBrush m_redo;
};

//////////////////////////////////////////////////////////////////////////
// SBrushSubSelection implementation.
//////////////////////////////////////////////////////////////////////////
bool SBrushSubSelection::AddPoint( Vec3 *pnt )
{
	if (std::find(points.begin(),points.end(),pnt) != points.end())
		return false;

	points.push_back(pnt);
	return true;
};

//////////////////////////////////////////////////////////////////////////
void SBrushSubSelection::Clear()
{
	points.clear();
}

SBrush::SBrush()
{
	m_flags = 0;
	m_pStatObj = 0;
	m_bounds.min = Vec3(0,0,0);
	m_bounds.max = Vec3(0,0,0);

	m_selectionType = 0;
	
//	m_pIndexedMesh = 0;
//	m_indoorGeom = GetIEditor()->Get3DEngine()->LoadStatObj();
}

//////////////////////////////////////////////////////////////////////////
SBrush::SBrush( const SBrush& b )
{
	m_flags = 0;
	m_pStatObj = 0;
	m_selectionType = 0;
	*this = b;
}

//////////////////////////////////////////////////////////////////////////
//
// Brush implementations.
//
//////////////////////////////////////////////////////////////////////////
SBrush::~SBrush()
{
	SAFE_RELEASE(m_pStatObj);
  ClearFaces();
}

SBrush& SBrush::operator = (const SBrush& b)
{
	int i;

	m_bounds.min = b.m_bounds.min;
	m_bounds.max = b.m_bounds.max;

	for (i=0; i<m_Faces.size(); i++)
	{
		SBrushFace *f = m_Faces[i];
		delete f;
	}
	m_Faces.clear();
	for (i=0; i<b.m_Faces.size(); i++)
	{
		SBrushFace *f = new SBrushFace;
		m_Faces.push_back(f);
		*f = *b.m_Faces[i];
	}
	m_flags = b.m_flags;

	return *this;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::MakeFacePlanes()
{
  for (int i = 0; i < m_Faces.size(); i++)
  {
    m_Faces[i]->MakePlane();
  }
}

//////////////////////////////////////////////////////////////////////////
void SBrush::ClearFaces()
{
  for (int i = 0; i < m_Faces.size(); i++)
  {
    delete m_Faces[i];
  }
  m_Faces.clear();
}

//////////////////////////////////////////////////////////////////////////
float SBrush::GetVolume()
{
  int     i;
  SBrushPoly   *p;
  Vec3   corner;
  float   d, area, volume;
  SBrushPlane  *plane;

  p = NULL;
  for (i=0; i<m_Faces.size(); i++)
  {
    p = m_Faces[i]->m_Poly;
    if (p)
      break;
  }
  if (!p)
    return 0;
  corner = p->m_Pts[0].xyz;

  volume = 0;
  for ( ; i<m_Faces.size(); i++)
  {
    p = m_Faces[i]->m_Poly;
    if (!p)
      continue;
    plane = &m_Faces[i]->m_Plane;
    d = -((corner | plane->normal) - plane->dist);
    area = p->Area();
    volume += d*area;
  }

  volume /= 3.0f;
  return volume;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::ClipPrefabModels(bool bClip)
{
	//@ASK Andrey.
	/*
  SBrushFace *f;
  int i, j;
  TPrefabItem *pi;

  for (i=0; i<m_Faces.size(); i++)
  {
    f = m_Faces[i];
    for (pi=f->m_Prefabs; pi; pi=pi->Next)
    {
      if (!bClip)
      {
        if (pi->m_Model)
          delete pi->m_Model;
        pi->m_Model = NULL;
        continue;
      }
      if (!pi->m_Geom || !pi->m_Geom->mModel)
        continue;
      for (j=0; j<m_Faces.size(); j++)
      {
        if (j == i)
          continue;
        m_Faces[j]->ClipPrefab(pi);
      }
    }
  }
	*/
}

//////////////////////////////////////////////////////////////////////////
SBrushPoly*	SBrush::CreateFacePoly( SBrushFace *face )
{
  SBrushPoly *p;
  SBrushFace *clip;
  SBrushPlane plane;
  bool past;

  p = face->m_Plane.CreatePoly();

	int numFaces = m_Faces.size();

  past = false;
  for (int i = 0; i < numFaces; i++)
  {
    clip = m_Faces[i];
    if (!p)
      return p;
    if (clip == face)
    {
      past = true;
      continue;
    }
    if ((face->m_Plane.normal.Dot(clip->m_Plane.normal) > 0.999) &&
				(fabs(face->m_Plane.dist - clip->m_Plane.dist) < DIST_EPSILON))
    {
      if (past)
      {
        delete p;
        return NULL;
      }
      continue;
    }
    plane.Invert(&clip->m_Plane);
    p = p->ClipByPlane(&plane, false);
  }
  if (!p)
    return NULL;

  if (p->m_Pts.size() < 3)
  {
    delete p;
    p = NULL;
  }

	//@ASK Andrey.
  //p->m_Pts.Shrink();
  return p;
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::BuildSolid( bool bRemoveEmptyFaces )
{
	BBox box;
	box.Reset();

	int i,j;

	// Clear render element valid flag.
	Invalidate();

	MakeFacePlanes();

  for (i = 0; i < m_Faces.size(); i++)
  {
    SBrushFace *f = m_Faces[i];
    
		if (f->m_Poly)
      delete f->m_Poly;
    
    f->m_Poly = CreateFacePoly(f);

		//@ASK Andrey
		/*
    f->m_Shader = gcMapEf.mfForName( f->m_TexInfo.name );
		*/

    if (!f->m_Poly)
      continue;

    // Build geometry tiles if needed
    f->BuildPrefabs();

		// Add this polygon to bounding box.
		int numPts = f->m_Poly->m_Pts.size();
    for (j=0; j < numPts; j++)
    {
			box.Add( f->m_Poly->m_Pts[j].xyz );
    }

    for (j=0; j < numPts; j++)
    {
			//@ASK Andrey.
      f->CalcTexCoords( f->m_Poly->m_Pts[j] );
    }

		// Select points in polygon that are near plane point.
		if (f->m_nSelectedPoints)
		{
			for (int k = 0; k < 3; k++)
			{
				if (f->m_nSelectedPoints & (1<<k))
				{
					for (j=0; j < numPts; j++)
					{
						if (IsEquivalent(f->m_Poly->m_Pts[j].xyz,f->m_PlanePts[k]))
							f->m_Poly->m_Pts[j].bSelected = true;
					}
				}
			}
		}
  }
  if (bRemoveEmptyFaces)
    RemoveEmptyFaces();

	//@ASK Andrey
	/*
	ClipPrefabModels(gcpCryIndEd->m_wndTextureBar.m_bClipPrefab);
	*/
	m_bounds.min = box.min;
	m_bounds.max = box.max;

	// Check if valid.
	if (m_bounds.min.x > m_bounds.max.x ||
			m_bounds.min.y > m_bounds.max.y ||
			m_bounds.min.z > m_bounds.max.z)
	{
		m_bounds.min = m_bounds.max = Vec3(0,0,0);
		return false;
	}
	if (m_Faces.empty())
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::RecalcTexCoords()
{
	// Clear render element valid flag.
	Invalidate();
	for (int i = 0; i < m_Faces.size(); i++)
	{
		SBrushFace *f = m_Faces[i];
		int numPts = f->m_Poly->m_Pts.size();
		for (int j = 0; j < numPts; j++)
		{
			f->CalcTexCoords( f->m_Poly->m_Pts[j] );
		}
	}
}

void SBrush::GenerateIndexMesh(IIndexedMesh *pMesh)
//////////////////////////////////////////////////////////////////////////
{
	int i,j;
	int numVerts = 0;
	int numFaces = 0;

	for (i = 0; i < m_Faces.size(); i++)
	{
		SBrushFace *f = m_Faces[i];
		if (!f->m_Poly)
			continue;
		int numPts = f->m_Poly->m_Pts.size();
		numVerts += numPts;
		numFaces += numPts-2;
	}

	pMesh->SetVertexCount(numVerts);
	pMesh->SetFacesCount(numFaces);
	pMesh->SetTexCoordsCount(numVerts);

	IIndexedMesh::SMeshDesc md;
	pMesh->GetMesh(md);

	int nCurrVert = 0;
	int nCurrFace = 0;

	int nMaxMatId = 0;

	int baseIndex = 0;
	for (i = 0; i < m_Faces.size(); i++)
  {
		SBrushFace *f = m_Faces[i];
		if (!f->m_Poly)
			continue;

		int numPts = f->m_Poly->m_Pts.size();
		for (j = 0; j < numPts; j++)
		{
			SBrushVert *v = &f->m_Poly->m_Pts[numPts-j-1];
			md.m_pVerts[nCurrVert] = v->xyz;
			md.m_pNorms[nCurrVert] = f->m_Plane.normal;
			md.m_pTexCoord[nCurrVert].s = v->st[0];
			md.m_pTexCoord[nCurrVert].t = v->st[1];
			nCurrVert++;
		}
		for (j = 0; j < numPts-2; j++)
		{
			SMeshFace *pFace = &md.m_pFaces[nCurrFace];
			pFace->v[0] = baseIndex;
			pFace->v[1] = baseIndex+j+1;
			pFace->v[2] = baseIndex+j+2;
			pFace->t[0] = pFace->v[0];
			pFace->t[1] = pFace->v[1];
			pFace->t[2] = pFace->v[2];
			pFace->nSubset = f->m_nMatID;
			if (f->m_nMatID > nMaxMatId)
				nMaxMatId = f->m_nMatID;
			pFace->dwFlags = 0;
			nCurrFace++;
		}
		baseIndex += numPts;
  }
	pMesh->SetSubSetCount(nMaxMatId+1);
	for (i = 0; i < nMaxMatId+1; i++)
	{
		SMeshSubset &subset = pMesh->GetSubSet(i);
		subset.nMatID = i;
	}
	pMesh->CalcBBox();
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::UpdateMesh()
{
	// Clear render element valid flag.
	Invalidate();

	if (m_Faces.empty())
		return false;

	if (!m_pStatObj)
	{
		m_pStatObj = GetIEditor()->Get3DEngine()->CreateStatObj();
		m_pStatObj->AddRef();
		m_pStatObj->SetMaterial( gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("editor/objects/grid") );
	}
	IIndexedMesh *pMesh = m_pStatObj->GetIndexedMesh();

	GenerateIndexMesh(pMesh);

	pMesh->Optimize();
	m_pStatObj->Invalidate(true);

	m_flags |= BRF_MESH_VALID;
	return m_flags & BRF_MESH_VALID;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::RemoveEmptyFaces()
{
  int i;
	std::vector<SBrushFace*> fc;

	// delete faces that doesnt contain polygon.
  for (i=0; i < m_Faces.size(); i++)
  {
    SBrushFace *f = m_Faces[i];
    if (f->m_Poly)
      fc.push_back(f);
    else
      delete f;
  }

	m_Faces.clear();

	// If less then 3 faces left, remove all faces.
  if (fc.size() < 3)
  {
    for (i = 0; i < fc.size(); i++)
      delete fc[i];
    return;
  }
	// Copy left brushes to brush face.
  m_Faces = fc;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::FitTexture(int nX, int nY)
{
  int i;

  for (i=0; i<m_Faces.size(); i++)
  {
    m_Faces[i]->FitTexture(nX, nY);
  }
}

////////////////////////////////////////////////////////////////////
bool SBrush::IsIntersect(SBrush *Vol)
{    

  /*
  for (int k=0;k<m_Faces.size();k++)
  {
    SBrushFace *f = &m_Faces[k];
    if (f->IsIntersecting(Vol))
      return (true);          
  } //k

  return (false);
  */

  //return (true);
  
	int i;
  
  Vec3 Cent = (m_bounds.min + m_bounds.max) * 0.5f;

  for (i=0; i<Vol->m_Faces.size(); i++)
  {
    SBrushFace *f = Vol->m_Faces[i];
    if ((f->m_Plane.normal | Cent) > f->m_Plane.dist)
      break;
  }
  if (i == Vol->m_Faces.size())
    return true;

  return false;
  
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::IsInside(const Vec3 &vPoint)
{
	for (int i=0;i<m_Faces.size();i++)
	{
    SBrushFace *f = m_Faces[i];
    if ((f->m_Plane.normal.Dot(vPoint)) > f->m_Plane.dist)
      return(false);
	} //i

	return(true);
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::Intersect(SBrush *Vol, std::vector<SBrush*>& List)
{
  SBrush  *front, *back;
  int n = 0;
  int i;

  for (i=0 ; i<3 ; i++)
  {
    if (m_bounds.min[i] >= Vol->m_bounds.max[i] - DIST_EPSILON || m_bounds.max[i] <= Vol->m_bounds.min[i] + DIST_EPSILON)
      break;
  }
  if (i != 3)
    return false;


  SBrush *b = Vol;
  SBrush *Die = this;
  float vol;

	int j,k;
  for (j=0; j<b->m_Faces.size(); j++)
  {
    SBrushFace *f = b->m_Faces[j];
    SBrushFace *fc = new SBrushFace;
    *fc = *f;

    fc->m_PlanePts[0] += f->m_Plane.normal * 16;
    fc->m_PlanePts[1] += f->m_Plane.normal * 16;
    fc->m_PlanePts[2] += f->m_Plane.normal * 16;
    fc->MakePlane();
    for (i=0; i<Die->m_Faces.size(); i++)
    {
      for (k=0; k<Die->m_Faces[i]->m_Poly->m_Pts.size(); k++)
      {
        if ((fc->m_Plane.normal | Die->m_Faces[i]->m_Poly->m_Pts[k].xyz) > fc->m_Plane.dist)
          break;
      }
      if (k!=Die->m_Faces[i]->m_Poly->m_Pts.size())
        break;
    }
    delete fc;
    if (i!=Die->m_Faces.size())
      break;
  }
  if (j == b->m_Faces.size())
  {
    List.push_back( Die );
    vol = Die->GetVolume();
    return true;
  }

  Vec3 Mins = m_bounds.min;
  Vec3 Maxs = m_bounds.max;

  for (j=0; j<b->m_Faces.size() && Die; j++)
  {
    SBrushFace *f = b->m_Faces[j];

    Die->SplitByFace(f, front, back);
    if (back)
      back->m_flags |= BRF_TEMP;
    if (front)
      delete front;

    if (Die->m_flags & BRF_TEMP)
      delete Die;
    Die = back;
  }
  if (Die)
  {
    SBrush *b = Die;
    List.push_back(b);
    vol = b->GetVolume();
    //gCurMap->m_ChoppedBrushes++;
    n++;
  }

  if (n)
    return true;
  return false;
}

//////////////////////////////////////////////////////////////////////////
int SBrush::GetMemorySize()
{
  int Size = sizeof(*this);
  for (int i=0; i<m_Faces.size(); i++)
  {
    Size += sizeof(SBrushFace);
    Size += sizeof(SBrushPoly);
    Size += sizeof(SBrushVert) * m_Faces[i]->m_Poly->m_Pts.size();
  }
  
  return Size;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::SnapToGrid( bool bOnlySelected )
{
  SBrushFace *f;
  int i;

  for (int nf=0; nf<m_Faces.size(); nf++)
  {
    f = m_Faces[nf];
    for (i=0; i<3; i++)
    {
			if (bOnlySelected && !(f->m_nSelectedPoints & (1<<i)))
				continue;
			f->m_PlanePts[i] = gSettings.pGrid->Snap( f->m_PlanePts[i] );
    }
  }
  BuildSolid();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::SplitByFace (SBrushFace *f, SBrush* &front, SBrush* &back)
{
  SBrush  *b;
  SBrushFace   *nf;
  Vec3   temp;
  SBrush *sb;

  b = new SBrush();

  sb = b;
  *sb = *this;
  nf = new SBrushFace;
  *nf = *f;

  nf->m_TexInfo = b->m_Faces[0]->m_TexInfo;
  b->m_Faces.push_back(nf);

  b->BuildSolid();
  b->RemoveEmptyFaces();
  if ( !b->m_Faces.size() )
  {
    delete b;
    back = NULL;
  }
  else
  {
		//@ASK Andrey.
		/*
    b->LinkToObj(m_Owner);
		*/
    back = b;
  }

  b = new SBrush;
  sb = b;
  *sb = *this;
  nf = new SBrushFace;
  *nf = *f;

  temp = nf->m_PlanePts[0];
  nf->m_PlanePts[0] = nf->m_PlanePts[1];
  nf->m_PlanePts[1] = temp;

  nf->m_TexInfo = b->m_Faces[0]->m_TexInfo;
  b->m_Faces.push_back(nf);

  b->BuildSolid();
  b->RemoveEmptyFaces ();
  if ( !b->m_Faces.size() )
  {
    delete b;
    front = NULL;
  }
  else
  {
		//@ASK Andrey.
		/*
    b->LinkToObj(m_Owner);
		*/
    front = b;
  }
}

int SBrush::OnSide (SBrushPlane *plane)
{
  int     i, j;
  SBrushPoly *p;
  float   d, max;
  int     side;

  max = 0;
  side = SIDE_FRONT;
  for (i=0;i<m_Faces.size(); i++)
  {
    p = m_Faces[i]->m_Poly;
    if (!p)
      continue;
    for (j=0; j<p->m_Pts.size(); j++)
    {
      d = (p->m_Pts[j].xyz | plane->normal) - plane->dist;
      if (d > max)
      {
        max = d;
        side = SIDE_FRONT;
      }
      if (-d > max)
      {
        max = -d;
        side = SIDE_BACK;
      }
    }
  }
  return side;
}

SBrush* SBrush::Clone( bool bBuildSoilid )
{
  SBrush *sb = new SBrush;
  *sb = *this;

  if (bBuildSoilid)
    sb->BuildSolid();

  return sb;
}

void SBrush::SplitByPlane (SBrushPlane *plane, SBrush* &front, SBrush* &back)
{
  SBrushPoly *tp = plane->CreatePoly();
  SBrushFace fc;

	if (!tp)
		return;

  fc.m_PlanePts[0] = tp->m_Pts[0].xyz;
  fc.m_PlanePts[1] = tp->m_Pts[1].xyz;
  fc.m_PlanePts[2] = tp->m_Pts[2].xyz;
  SplitByFace(&fc, front, back);
  delete tp;
  return;
  
  SBrush *b[2];
  int     i, j;
  SBrushPoly *p, *cp[2], *midpoly;
  SBrushPlane   plane2;
  SBrushFace    *f;
  float   d, f_front, f_back;
  
  front = back = NULL;
  
  f_front = f_back = 0;
  for (i=0; i<m_Faces.size(); i++)
  {
    p = m_Faces[i]->m_Poly;
    if (!p)
      continue;
    for (j=0; j<p->m_Pts.size(); j++)
    {
      d = (p->m_Pts[j].xyz | plane->normal) - plane->dist;
      if (d > 0 && d > f_front)
        f_front = d;
      if (d < 0 && d < f_back)
        f_back = d;
    }
  }
  if (f_front < 0.1)
  {
    SBrush *nb = Clone(true);
    back = nb;
    return;
  }
  if (f_back > -0.1)
  { 
    SBrush *nb = Clone(true);
    front = nb;
    return;
  }
    
  p = plane->CreatePoly();
  for (i=0; i<m_Faces.size() && p; i++)
  {
    SBrushPlane *pp = &m_Faces[i]->m_Plane;
    plane2.Invert(pp);
    p = p->ClipByPlane(&plane2, true);
  }
  
  if (!p)
  { 
    int   side;
    
    side = OnSide (plane);
    if (side == SIDE_FRONT)
    {
      SBrush *nb = Clone(true);
      front = nb;
    }
    if (side == SIDE_BACK)
    {
      SBrush *nb = Clone(true);
      back = nb;
    }
    return;
  }
  
  midpoly = p;
  
  for (i=0; i<2; i++)
  {
    b[i] = Clone(false);
    for (j=0; j<b[i]->m_Faces.size(); j++)
    {
      delete b[i]->m_Faces[j];
    }
    b[i]->m_Faces.clear();
  }
    
  for (i=0; i<m_Faces.size(); i++)
  {
    f = m_Faces[i];
    p = f->m_Poly;
    if (!p)
      continue;
    p->ClipByPlane(plane, 0, &cp[0], &cp[1]);
    if (cp[0] == p)
    {
      cp[0] = new SBrushPoly;
      *(cp[0]) = *p;
    }
    if (cp[1] == p)
    {
      cp[1] = new SBrushPoly;
      *(cp[1]) = *p;
    }
    for (j=0; j<2; j++)
    {
      if (!cp[j])
        continue;
      SBrushFace *ff = new SBrushFace;
      *ff = *f;
      ff->m_Poly = cp[j];
      b[j]->m_Faces.push_back(ff);
    }
  }
    
  for (i=0; i<2; i++)
  {
    if (b[i]->m_Faces.size() < 3)
    {
      delete b[i];
      b[i] = NULL;
    }
  }
  
  if ( !(b[0] && b[1]) )
  {
    if (!b[0] && !b[1])
			CLogFile::WriteLine("Split removed brush\n");
    else
			CLogFile::WriteLine("Split not on both sides\n");
    if (b[0])
    {
      delete b[0];
      SBrush *nb = Clone(true);
      front = nb;
    }
    if (b[1])
    {
      delete b[1];
      SBrush *nb = Clone(true);
      back = nb;
    }
    return;
  }
  
  for (i=0; i<2; i++)
  {
    SBrushFace *ff = new SBrushFace;
    if (!i)
      ff->m_Plane = *plane;
    else
    {
      plane2.Invert(plane);
      ff->m_Plane = plane2;
    }
		// @FIXME ??
    //b[i]->CreateFacePoly( ff );
    ff->m_TexInfo = b[i]->m_Faces[0]->m_TexInfo;
    b[i]->m_Faces.push_back(ff);
    if (i==0)
    {
      SBrushPoly *pl = new SBrushPoly;
      *pl = *midpoly;
      ff->m_Poly = pl;
    }
    else
      ff->m_Poly = midpoly;
  }
    
  front = b[0];
  back = b[1];
}

/*
TBrush *SBrush::Parse()
{
  TBrush *b;
  SBrushFace  *f;
  int     i,j;
  
  gCurMap->m_ParsedBrushes++;
  b = new TBrush;

  do
  {
    if (!GetToken (true))
      break;
    if (!strcmp (token, "}") )
      break;
    
    if (strcmpi(token, "patchDef2") == 0 || strcmpi(token, "patchDef3") == 0)
    {
      delete b;
      
      b = SPatch::Parse(strcmpi(token, "patchDef2") == 0);
      if (b == NULL)
      {
        CLogFile::WriteLine ("Warning: parsing patch/brush\n");
        return NULL;
      }
      else
        continue;
    }
    else
    {      
      f = new SBrushFace;
      memset(f, 0, sizeof(SBrushFace));
      
      for (i=0; i<3; i++)
      {
        if (i != 0)
          GetToken (true);
        if (strcmp (token, "(") )
        {
          CLogFile::WriteLine("Warning: Parsing brush error\n");
          delete b;
          return NULL;
        }
        
        for (j=0 ; j<3 ; j++)
        {
          GetToken (false);
          f->m_PlanePts[i][j] = atof(token);
        }
        
        GetToken (false);
        if (strcmp (token, ")") )
        {
          CLogFile::WriteLine("Warning: Parsing brush error\n");
          delete b;
          return NULL;
        }
      }
    }

    {
      // read the TextureInfo
      GetToken (false);
      f->m_TexInfo.SetName(token);
      GetToken (false);
      f->m_TexInfo.shift[0] = atof(token);
      GetToken (false);
      f->m_TexInfo.shift[1] = atof(token);
      GetToken (false);
      f->m_TexInfo.rotate = atof(token); 
      GetToken (false);
      f->m_TexInfo.scale[0] = atof(token);
      GetToken (false);
      f->m_TexInfo.scale[1] = atof(token);
            
      // the flags and value field aren't necessarily present
      f->m_Shader = gcMapEf.mfForName( f->m_TexInfo.name );
      if (f->m_Shader->m_SM)
      {
        f->m_TexInfo.contents = f->m_Shader->m_SM->m_Contents;
        f->m_TexInfo.flags = f->m_Shader->m_SM->m_SurfFlags;
      }
      else
      {
        f->m_TexInfo.contents = 0;
        f->m_TexInfo.flags = 0;
      }
      
      if (TokenAvailable ())
      {
        GetToken (false);
        f->m_TexInfo.contents = atoi(token);
        GetToken (false);
        f->m_TexInfo.flags = atoi(token);
        GetToken (false);
        f->m_TexInfo.value = atoi(token);
      }
      if (!f->m_TexInfo.value)
        f->m_TexInfo.value = gcMapEf.mfGetVal(f->m_Shader, NULL);
      b->m_Faces.push_back(f);
      
    }
  } while (1);
  
  return b;
}

bool SBrush::ParseFromInd()
{
  gCurMap->m_ParsedBrushes++;
  GetToken(true);
  GetToken(true);
  if (token[0] != '[')
  {
    CLogFile::WriteLine("Error: Couldn't parsing IND file\n");
    return false;
  }
  while (true)
  {
    GetToken(true);
    if (token[0] == ']')
      break;
    if (!stricmp(token, "Face"))
    {
      SBrushFace *f = new SBrushFace;
      memset(f, 0, sizeof(SBrushFace));
      m_Faces.push_back(f);
      f->m_Poly = new SBrushPoly;
      GetToken(true);
      int nv = atoi(token);
      for (int i=0; i<nv; i++)
      {
        SBrushVert v;
        GetToken(true);
        for (int j=0; j<3; j++)
        {
          GetToken(true);
          v.xyz[j]  = atof(token);
        }
        for (j=0; j<2; j++)
        {
          GetToken(true);
          v.st[j]  = atof(token);
        }
        GetToken(true);

        f->m_Poly->m_Pts.push_back(v);

      }
      f->m_PlanePts[0] = f->m_Poly->m_Pts[0].xyz;
      f->m_PlanePts[1] = f->m_Poly->m_Pts[1].xyz;
      f->m_PlanePts[2] = f->m_Poly->m_Pts[2].xyz;

      // Parse TexInfo
      GetToken(true);
      GetToken(true);
      strcpy(f->m_TexInfo.name, token);
      GetToken(true);
      f->m_TexInfo.shift[0] = atof(token);
      GetToken(true);
      f->m_TexInfo.shift[1] = atof(token);
      GetToken(true);
      f->m_TexInfo.rotate = atof(token);
      GetToken(true);
      f->m_TexInfo.scale[0] = atof(token);
      GetToken(true);
      f->m_TexInfo.scale[1] = atof(token);
      GetToken(true);
      f->m_TexInfo.flags = atoi(token);
      GetToken(true);
    }
    else
    {
      CLogFile::WriteLine("Error: Couldn't parsing IND file\n");
      return false;
    }
  }

  return true;
}
*/

//////////////////////////////////////////////////////////////////////////
void SBrush::Create( const Vec3& mins, const Vec3& maxs, int numSides )
{
	if (numSides == 4)
	{
		CreateBox( mins,maxs );
		return;
	}
	int i;
	SBrushFace *f;

	ClearFaces();

	for (i=0 ; i<3 ; i++)
	{
		if (maxs[i] < mins[i])
			CLogFile::WriteLine ("Error: SBrush::Create: backwards");
	}
	// find center of brush
	int axis = 2;
	float mid[3];
	float width = 0.01f;
	for (i = 0; i < 3; i++)
	{
		mid[i] = (maxs[i] + mins[i]) * 0.5;
		if (i == axis) continue;
		if ((maxs[i] - mins[i]) * 0.5 > width)
			width = (maxs[i] - mins[i]) * 0.5;
	}

	// create top face
	f = new SBrushFace;
	f->m_PlanePts[2][(axis+1)%3] = mins[(axis+1)%3]; f->m_PlanePts[2][(axis+2)%3] = mins[(axis+2)%3]; f->m_PlanePts[2][axis] = maxs[axis];
	f->m_PlanePts[1][(axis+1)%3] = maxs[(axis+1)%3]; f->m_PlanePts[1][(axis+2)%3] = mins[(axis+2)%3]; f->m_PlanePts[1][axis] = maxs[axis];
	f->m_PlanePts[0][(axis+1)%3] = maxs[(axis+1)%3]; f->m_PlanePts[0][(axis+2)%3] = maxs[(axis+2)%3]; f->m_PlanePts[0][axis] = maxs[axis];
	m_Faces.push_back(f);

	// create bottom face
	f = new SBrushFace;
	f->m_PlanePts[0][(axis+1)%3] = mins[(axis+1)%3]; f->m_PlanePts[0][(axis+2)%3] = mins[(axis+2)%3]; f->m_PlanePts[0][axis] = mins[axis];
	f->m_PlanePts[1][(axis+1)%3] = maxs[(axis+1)%3]; f->m_PlanePts[1][(axis+2)%3] = mins[(axis+2)%3]; f->m_PlanePts[1][axis] = mins[axis];
	f->m_PlanePts[2][(axis+1)%3] = maxs[(axis+1)%3]; f->m_PlanePts[2][(axis+2)%3] = maxs[(axis+2)%3]; f->m_PlanePts[2][axis] = mins[axis];
	m_Faces.push_back(f);

	for (i = 0 ;i < numSides ;i++)
	{
		f = new SBrushFace;

		float sv = sin (i*3.14159265f*2/numSides);
		float cv = cos (i*3.14159265f*2/numSides);

		f->m_PlanePts[0][(axis+1)%3] = floor(mid[(axis+1)%3]+width*cv+0.5);
		f->m_PlanePts[0][(axis+2)%3] = floor(mid[(axis+2)%3]+width*sv+0.5);
		f->m_PlanePts[0][axis] = mins[axis];

		f->m_PlanePts[1][(axis+1)%3] = f->m_PlanePts[0][(axis+1)%3];
		f->m_PlanePts[1][(axis+2)%3] = f->m_PlanePts[0][(axis+2)%3];
		f->m_PlanePts[1][axis] = maxs[axis];

		f->m_PlanePts[2][(axis+1)%3] = floor(f->m_PlanePts[0][(axis+1)%3] - width*sv + 0.5);
		f->m_PlanePts[2][(axis+2)%3] = floor(f->m_PlanePts[0][(axis+2)%3] + width*cv + 0.5);
		f->m_PlanePts[2][axis] = maxs[axis];

		m_Faces.push_back(f);
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::CreateBox( const Vec3& mins, const Vec3& maxs )
{
  int   i, j;
  Vec3 pts[4][2];
  SBrushFace *f;

	ClearFaces();

  for (i=0 ; i<3 ; i++)
  {
    if (maxs[i] < mins[i])
      CLogFile::WriteLine ("Error: SBrush::Create: backwards");
  }

  pts[0][0][0] = mins[0];
  pts[0][0][1] = mins[1];
  
  pts[1][0][0] = mins[0];
  pts[1][0][1] = maxs[1];
  
  pts[2][0][0] = maxs[0];
  pts[2][0][1] = maxs[1];
  
  pts[3][0][0] = maxs[0];
  pts[3][0][1] = mins[1];
  
  for (i=0; i<4; i++)
  {
    pts[i][0][2] = mins[2];
    pts[i][1][0] = pts[i][0][0];
    pts[i][1][1] = pts[i][0][1];
    pts[i][1][2] = maxs[2];
  }

  for (i=0; i<4; i++)
  {
    f = new SBrushFace;
    
    j = (i+1)%4;

    f->m_PlanePts[0] = pts[j][1];
    f->m_PlanePts[1] = pts[i][1];
    f->m_PlanePts[2] = pts[i][0];
    m_Faces.push_back(f);
  }
  
  f = new SBrushFace;
  f->m_PlanePts[0] = pts[0][1];
  f->m_PlanePts[1] = pts[1][1];
  f->m_PlanePts[2] = pts[2][1];
  m_Faces.push_back(f);

  f = new SBrushFace;
  f->m_PlanePts[0] = pts[2][0];
  f->m_PlanePts[1] = pts[1][0];
  f->m_PlanePts[2] = pts[0][0];
  m_Faces.push_back(f);
}

//===================================================================

void SBrush::MakeSelFace (SBrushFace *f)
{
	//@ASK Andrey.
	/*
  SBrushPoly *p;
  int   i;
  int   pnum[128];

  p = f->CreatePoly(this);
  if (!p)
    return;
  for (i=0; i<p->m_Pts.size(); i++)
  {
    pnum[i] = gCurMap->FindPoint(&p->m_Pts[i]);
  }
  for (i=0; i<p->m_Pts.size(); i++)
  {
    gCurMap->FindEdge (pnum[i], pnum[(i+1)%p->m_Pts.size()], f);
  }
  delete p;
	*/
}

SBrushFace* SBrush::RayHit( Vec3 Origin, Vec3 dir, float *dist )
{
  SBrushFace *f, *FirstFace=0;
  Vec3 p1, p2;
  float frac, d1, d2;
  int   i, j;

  p1 = Origin;
  for (i=0 ; i<3 ; i++)
  {
    p2[i] = p1[i] + dir[i]*32768;
  }

  for (j=0; j<m_Faces.size(); j++)
  {
    f = m_Faces[j];

    d1 = (p1 | f->m_Plane.normal) - f->m_Plane.dist;
    d2 = (p2 | f->m_Plane.normal) - f->m_Plane.dist;
    if (d1 >= 0 && d2 >= 0)
    {
      *dist = 0;
      return NULL;
    }
    if (d1 <=0 && d2 <= 0)
      continue;
    frac = d1 / (d1 - d2);
    if (d1 > 0)
    {
      FirstFace = f;
      p1 = p1 + frac * (p2 - p1);
    }
    else
      p2 = p1 + frac * (p2 - p1);
  }

  p1 -= Origin;
  d1 = p1 | dir;

  *dist = d1;

  return FirstFace;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::SelectSide( Vec3 Origin,Vec3 Dir,bool shear,SBrushSubSelection &subSelection )
{
  SBrushFace *f;
  Vec3 p1, p2;

	Vec3 Target = Origin + Dir * 10000.0f; 

  for (int j=0; j < m_Faces.size(); j++)
  {
    f = m_Faces[j];
    p1 = Origin;
    p2 = Target;

		// Clip ray by every face of brush, except tested one.
    for (int i=0; i < m_Faces.size(); i++)
    {
      if (m_Faces[i] != f)
				m_Faces[i]->ClipLine(p1, p2);
    }

    //if (i != m_Faces.size())
      //continue;

    if (IsEquivalent(p1,Origin,0))
      continue;

    if (f->ClipLine(p1, p2))
      continue;

    SelectFace( f,shear,subSelection );
  }
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Move(Vec3& Delta)
{
  int   i;
  SBrushFace *f;

  for (int j=0; j<m_Faces.size(); j++)
  {
    f = m_Faces[j];
    for (i=0; i<3; i++)
    {
      f->m_PlanePts[i] += Delta;
    }
  }
  BuildSolid();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Transform( Matrix34 &tm,bool bBuild )
{
	int   i;
  SBrushFace *f;

  for (int j=0; j<m_Faces.size(); j++)
  {
    f = m_Faces[j];
    for (i=0; i<3; i++)
    {
      f->m_PlanePts[i] = tm.TransformPoint(f->m_PlanePts[i]);
    }
  }
	if (bBuild)
		BuildSolid();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Invalidate()
{
	m_flags &= ~BRF_MESH_VALID;
}

void SBrush::SelectFace( SBrushFace *f,bool shear,SBrushSubSelection &subSelection )
{
  int   i;
  SBrushFace *f2;
  SBrushPoly *p;
  float d;
  int   c;

  c = 0;
  for (i = 0; i<3; i++)
  {
    if (subSelection.AddPoint(&f->m_PlanePts[i]))
			c++;
  }
  if (c == 0)
    return;

	/*
	 TBrush *b2;
  for (b2=gCurMap->m_SelectedBrushes; b2; b2=b2->Next)
  {
    if (b2 == this)
      continue;
    for (int j=0; j<b2->m_Faces.size(); j++)
    {
      f2 = b2->m_Faces[j];
      for (i=0; i<3; i++)
      {
        if (fabs(f2->m_PlanePts[i] | f->m_Plane.normal) - f->m_Plane.dist > DIST_EPSILON)
          break;
      }
      if (i == 3)
      {
        b2->SelectFaceForDragging (f2, shear);
        break;
      }
    }
  }
	*/

  if (!shear)
    return;

  for (int j=0; j<m_Faces.size(); j++)
  {
    f2 = m_Faces[j];
    if (f2 == f)
      continue;
    
		p = CreateFacePoly(f2);
    if (!p)
      continue;

    for (i=0; i<p->m_Pts.size(); i++)
    {
      d = (p->m_Pts[i].xyz | f->m_Plane.normal) - f->m_Plane.dist;
      if (d > -DIST_EPSILON && d < DIST_EPSILON)
			{
        break;
			}
    }

    if (i != p->m_Pts.size())
    {
      if (i == 0)
      {
        d = p->m_Pts[p->m_Pts.size()-1].xyz.Dot(f->m_Plane.normal) - f->m_Plane.dist;
        if (d > -DIST_EPSILON && d < DIST_EPSILON)
          i = p->m_Pts.size() - 1;
      }

      subSelection.AddPoint( &f2->m_PlanePts[0] );
      f2->m_PlanePts[0] = p->m_Pts[i].xyz;

      if (++i == p->m_Pts.size())
        i = 0;

      d = p->m_Pts[i].xyz.Dot(f->m_Plane.normal) - f->m_Plane.dist;
      if (d > -DIST_EPSILON && d < DIST_EPSILON)
			{
        subSelection.AddPoint( &f2->m_PlanePts[1] );
			}
      f2->m_PlanePts[1] = p->m_Pts[i].xyz;
      if (++i == p->m_Pts.size())
        i = 0;

      f2->m_PlanePts[2] = p->m_Pts[i].xyz;
    }

    delete p;
  }
}

//void gDrawLightFrustum(CMapObject *o, Vec3 org);

bool SBrush::DrawModel(bool b)
{
	//@ASK Andrey.
	/*
  CComModel *m = (CComModel *)m_Owner->m_Class->m_Model;
  if (!m)
    return false;
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glEnable (GL_TEXTURE_2D);
//  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();

  Vec3 org = m_Owner->m_Brushes.m_NextInObj->m_bounds.min - m_Owner->m_Class->m_bounds.min;

  glTranslatef(org[0], org[1], org[2]);

  float a = m_Owner->FloatForKey("angle");
  Vec3 angs;
  angs[YAW] = a;
  if (!a)
    m_Owner->VectorForKey("angles", angs);
  glRotatef(angs[YAW], 0, 0, 1);
  glRotatef(angs[PITCH], 0, 1, 0);
  glRotatef(angs[ROLL], 1, 0, 0);

  glColor4f(1,1,1,1);
  bool bRes = m->mfDraw(NULL, true);

  glPopMatrix();
  if (!strnicmp(m_Owner->m_Class->m_Name, "light", 5))
  {
    Vec3 dir;
    m_Owner->VectorForKey("direction", dir);
    if (dir != Vec3(0,0,0))
      gDrawLightFrustum(m_Owner, org);
  }
  glPopAttrib();

  return bRes;
	*/
	return false;
}

void SBrush::Draw()
{
	//@ASK Andrey.
	/*
  int i, j;
  SShader   *prev = 0;
  SBrushPoly *p;

  if (m_flags & BRF_HIDE)
    return;

  if (m_Patch)
  {
    m_Patch->Draw(1);
    return;
  }
  
  EDrawMode dm = gcpMapPersp->m_eDrawMode;
  
  if (m_Owner->m_Class->m_flags & OCF_NORESIZE)
  {
    bool bp = (m_flags & BRF_FAILMODEL) ? false : DrawModel(true);
    
    if (bp)
      return;
  }
  
  prev = NULL;
  for (i=0; i<m_Faces.size(); i++)
  {
    SBrushFace *f = m_Faces[i];
    p = f->m_Poly;
    if (!p)
      continue;
        
    if (f->m_Prefabs)
    {
      for (TPrefabItem *pi=f->m_Prefabs; pi; pi=pi->Next)
      {
        glPushMatrix();
        glMultMatrixf(&pi->m_Matrix.m_values[0][0]);
        if (pi->m_Model)
          pi->m_Model->mfDraw(f->m_Shader, NULL);
        else
          pi->m_Geom->mfDraw(f->m_Shader, NULL);
        glPopMatrix();
      }
      continue;
    }
    if ((dm == eDM_Textured || dm == eDM_Light) && f->m_Shader != prev)
    {
      prev = f->m_Shader;
      glBindTexture(GL_TEXTURE_2D, gNE[f->m_Shader->m_Id].m_Tex->m_Bind);
    }
    
    if (!m_Patch)
      glColor4fv(&f->m_Color[0]);
    else
      glColor4f (f->m_Color[0], f->m_Color[1], f->m_Color[2], 0.13 );

    float a;
    if ((a=gcMapEf.mfGetAlpha(f->m_Shader)) != 1.0f)
      glColor4f (f->m_Color[0], f->m_Color[1], f->m_Color[2], a);
    
    glBegin(GL_POLYGON);
    
    for (j=0; j<p->m_Pts.size(); j++)
    {
      if (dm == eDM_Textured || dm == eDM_Light)
        glTexCoord2fv( &p->m_Pts[j].st[0] );
      glVertex3fv(&p->m_Pts[j].xyz[0]);
    }
    glEnd();
  }
    
  if ((m_Owner->m_Class->m_flags & OCF_NORESIZE) && (dm == eDM_Textured || dm == eDM_Light))
    glEnable (GL_TEXTURE_2D);
  
  glBindTexture( GL_TEXTURE_2D, 0 );
	*/
}

bool SBrush::AddToListModel(bool b)
{
	bool bRes = false;
	//@ASK Andrey.
	/*
  if (!strnicmp(m_Owner->m_Class->m_Name, "light", 5))
    return true;
  
  Vec3 org = m_Owner->m_Brushes.m_NextInObj->m_bounds.min - m_Owner->m_Class->m_bounds.min;
  
  CComModel *m = (CComModel *)m_Owner->m_Class->m_Model;
  if (!m)
    return false;
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glEnable (GL_TEXTURE_2D);
//  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();

  glTranslatef(org[0], org[1], org[2]);

  float a = m_Owner->FloatForKey("angle");
  Vec3 angs;
  angs[YAW] = a;
  if (!a)
    m_Owner->VectorForKey("angles", angs);
  glRotatef(angs[YAW], 0, 0, 1);
  glRotatef(angs[PITCH], 0, 1, 0);
  glRotatef(angs[ROLL], 1, 0, 0);

  glColor4f(1,1,1,1);
  bRes = m->mfDraw(NULL, true);

  glPopMatrix();
  glPopAttrib();
	*/

  return bRes;
}

void SBrush::AddToList(CRenderObject *obj)
{
	//@ASK Andrey.
	/*
  int i;
  SShader   *prev = 0;
  SBrushPoly *p;

  if (m_flags & BRF_HIDE)
    return;

  if (m_Patch)
  {
    m_Patch->Draw(1);
    return;
  }
  
  EDrawMode dm = gcpMapPersp->m_eDrawMode;
  
  if (m_Owner->m_Class->m_flags & OCF_NORESIZE)
  {
    bool bp = (m_flags & BRF_FAILMODEL) ? false : AddToListModel(true);
    
    if (bp)
      return;
  }
  
  prev = NULL;
  for (i=0; i<m_Faces.size(); i++)
  {
    SBrushFace *f = m_Faces[i];
    p = f->m_Poly;
    if (!p)
      continue;

    ECull cl = f->m_Shader->m_eCull;
    if (cl != eCULL_None)
    {
      float d = f->m_Plane.normal * g_Globals.m_Origin;
      if (cl == eCULL_Back)
      {
        if (d < f->m_Plane.dist-8.0f)
          continue;
      }
      else
      {
        if (d > f->m_Plane.dist+8.0f)
          continue;
      }
    }
    
    if (f->m_Prefabs)
    {
      for (TPrefabItem *pi=f->m_Prefabs; pi; pi=pi->Next)
      {
        glPushMatrix();
        glMultMatrixf(&pi->m_Matrix.m_values[0][0]);
        if (pi->m_Model)
          pi->m_Model->mfDraw(f->m_Shader, NULL);
        else
          pi->m_Geom->mfDraw(f->m_Shader, NULL);
        glPopMatrix();
      }
      continue;
    }

    if (g_Globals.m_TemplateId < EFT_USER_FIRST)
      f->m_Shader->AddTemplate(g_Globals.m_TemplateId);
    else
      f->m_Shader->AddTemplate(g_Globals.m_TemplateId, g_Globals.m_CustomTemplate.GetBuffer(0));
    SShader *e = f->m_Shader->mfGetTemplate(g_Globals.m_TemplateId);
    if ((gNE[e->m_Id].m_TessSize && !f->m_bTesselated) || (!gNE[e->m_Id].m_TessSize && f->m_bTesselated))
      f->BuildRE();
    
    if (obj)
      gRenDev->EF_AddEf(0, f->m_RE, f->m_Shader, obj, g_Globals.m_TemplateId, NULL);
    else
      gRenDev->EF_AddEf(0, f->m_RE, f->m_Shader, g_Globals.m_TemplateId, NULL);
  }
	*/
}

//============================================================

/*
void SBrush::Write(CCryMemFile* pMemFile)
{
  SBrushFace *fa;
  const char *pname;
  int   i, j;
  
  if (m_Patch)
  {
    m_Patch->Write(pMemFile);
    return;
  }
  {
    gCurMap->FPrintfToMem (pMemFile, "{\n");
    for (j=0; j<m_Faces.size(); j++)
    {
      fa = m_Faces[j];
      for (i=0; i<3; i++)
      {
        gCurMap->FPrintfToMem(pMemFile, "( ");
        for (int j = 0; j < 3; j++)
        {
          if (fa->m_PlanePts[i][j] == static_cast<int>(fa->m_PlanePts[i][j]))
            gCurMap->FPrintfToMem(pMemFile, "%i ", static_cast<int>(fa->m_PlanePts[i][j]));
          else
            gCurMap->FPrintfToMem(pMemFile, "%f ", fa->m_PlanePts[i][j]);
        }
        gCurMap->FPrintfToMem(pMemFile, ") ");
      }
      
      {
        pname = fa->m_TexInfo.name;
        if (pname[0] == 0)
          pname = "unnamed";
        
        gCurMap->FPrintfToMem (pMemFile, "%s %i %i %i ", pname, (int)fa->m_TexInfo.shift[0], (int)fa->m_TexInfo.shift[1], (int)fa->m_TexInfo.rotate);
        
        if (fa->m_TexInfo.scale[0] == (int)fa->m_TexInfo.scale[0])
          gCurMap->FPrintfToMem (pMemFile, "%i ", (int)fa->m_TexInfo.scale[0]);
        else
          gCurMap->FPrintfToMem (pMemFile, "%f ", (float)fa->m_TexInfo.scale[0]);
        if (fa->m_TexInfo.scale[1] == (int)fa->m_TexInfo.scale[1])
          gCurMap->FPrintfToMem (pMemFile, "%i", (int)fa->m_TexInfo.scale[1]);
        else
          gCurMap->FPrintfToMem (pMemFile, "%f", (float)fa->m_TexInfo.scale[1]);
        
        gCurMap->FPrintfToMem (pMemFile, " %i %i %i", fa->m_TexInfo.contents, fa->m_TexInfo.flags, fa->m_TexInfo.value);
      }
      gCurMap->FPrintfToMem (pMemFile, "\n");
    }
    gCurMap->FPrintfToMem (pMemFile, "}\n");
  }
}
*/

//////////////////////////////////////////////////////////////////////////
void SBrush::Serialize( XmlNodeRef &xmlNode,bool bLoading )
{
	if (bLoading)
	{
		// Loading.
		// Load all faces.
		int numFaces = xmlNode->getChildCount();
		m_Faces.resize( numFaces );
		for (int i = 0; i < numFaces; i++)
		{
			if (!m_Faces[i])
				m_Faces[i] = new SBrushFace;
			SBrushFace *fc = m_Faces[i];

			XmlNodeRef faceNode = xmlNode->getChild(i);
			faceNode->getAttr( "p1",fc->m_PlanePts[0] );
			faceNode->getAttr( "p2",fc->m_PlanePts[1] );
			faceNode->getAttr( "p3",fc->m_PlanePts[2] );
			int nMatId = fc->m_nMatID;
			faceNode->getAttr( "MatId",nMatId );
			fc->m_nMatID = nMatId;

			Vec3 texScale( 1,1,1 );
			Vec3 texShift( 0,0,0 );
			faceNode->getAttr( "TexScale",texScale );
			faceNode->getAttr( "TexShift",texShift );
			faceNode->getAttr( "TexRotate",fc->m_TexInfo.rotate );
			fc->m_TexInfo.scale[0] = texScale.x;
			fc->m_TexInfo.scale[1] = texScale.y;
			fc->m_TexInfo.shift[0] = texShift.x;
			fc->m_TexInfo.shift[1] = texShift.y;
		}
		BuildSolid(false);
	}
	else
	{
		// Saving.
		//char str[1024];
		// Save all faces.
		for (int i = 0; i < m_Faces.size(); i++)
    {
			SBrushFace *fc = m_Faces[i];
			XmlNodeRef faceNode = xmlNode->newChild( "Face" );
			faceNode->setAttr( "p1",fc->m_PlanePts[0] );
			faceNode->setAttr( "p2",fc->m_PlanePts[1] );
			faceNode->setAttr( "p3",fc->m_PlanePts[2] );
			faceNode->setAttr( "MatId",fc->m_nMatID );

			Vec3 texScale( fc->m_TexInfo.scale[0],fc->m_TexInfo.scale[1],0 );
			Vec3 texShift( fc->m_TexInfo.shift[0],fc->m_TexInfo.shift[1],0 );
			faceNode->setAttr( "TexScale",texScale );
			faceNode->setAttr( "TexShift",texShift );
			faceNode->setAttr( "TexRotate",fc->m_TexInfo.rotate );

			//sprintf( 
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::RecordUndo( const char *sUndoDescription )
{
	if (CUndo::IsRecording())
	{
		CUndo::Record( new CUndoBrush(this,sUndoDescription) );
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::SetMatrix( const Matrix34 &tm )
{
	m_matrix = tm;
	//Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Render( CViewport *view,const Vec3 &objectSpaceCamSrc )
{
	if (!(m_flags&BRF_MESH_VALID))
	{
		if (!UpdateMesh())
			return;
	}
}


//////////////////////////////////////////////////////////////////////////
IStatObj* SBrush::GetIStatObj()
{
	if (!m_pStatObj || !(m_flags&BRF_MESH_VALID))
	{
		UpdateMesh();
	}
	return m_pStatObj;
};

//////////////////////////////////////////////////////////////////////////
// CEdGeometry implementation.
//////////////////////////////////////////////////////////////////////////
void SBrush::GetBounds( AABB &box )
{
	box = m_bounds;
}

//////////////////////////////////////////////////////////////////////////
CEdGeometry* SBrush::Clone()
{
	return Clone(true);
};

//////////////////////////////////////////////////////////////////////////
IIndexedMesh* SBrush::GetIndexedMesh()
{
	IStatObj *pObj = GetIStatObj();
	if (pObj)
		return pObj->GetIndexedMesh();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::SetModified( bool bModified )
{
	if (bModified)
		m_flags |= BRF_MODIFIED;
	else
		m_flags &= ~BRF_MODIFIED;
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::IsModified() const
{
	return m_flags & BRF_MODIFIED;
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::StartSubObjSelection( const Matrix34 &nodeWorldTM,int elemType,int nFlags )
{
	m_flags |= BRF_SUB_OBJ_SEL;
	m_selectionType = elemType;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::EndSubObjSelection()
{
	m_flags &= ~BRF_SUB_OBJ_SEL;

	if (m_pStatObj)
	{
		// Remove geometry hide flag.
		int nStatObjFlags = m_pStatObj->GetFlags();
		nStatObjFlags &= ~STATIC_OBJECT_HIDDEN;
		m_pStatObj->SetFlags( nStatObjFlags );
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Display( DisplayContext &dc )
{
	bool b2D = false;
	if (dc.flags & DISPLAY_2D)
	{
		b2D = true;
		int cullAxis = 0;
		EViewportType viewType = dc.view->GetType();
		switch (viewType) {
		case ET_ViewportXY:
			cullAxis = 2;
			break;
		case ET_ViewportXZ:
			cullAxis = 1;
			break;
		case ET_ViewportYZ:
			cullAxis = 0;
			break;
		}
		dc.PushMatrix( m_matrix );

		// if selected.
		bool bSelected = (m_flags & BRF_SELECTED) != 0;

		if (bSelected)
			dc.SetLineWidth(2);

		int i, j;
		for (i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace *f = m_Faces[i];

			Vec3 norm = m_matrix.TransformVector( f->m_Plane.normal );

			if (norm[cullAxis] <= 0)
				continue;

			SBrushPoly *poly = f->m_Poly;
			if (!poly)
				continue;
			int numv = poly->m_Pts.size();
			for (j=0; j < numv; j++)
			{
				int k = ((j+1) < numv) ? j+1 : 0;
				SBrushVert &vert = poly->m_Pts[j];
				SBrushVert &vert1 = poly->m_Pts[k];
				dc.DrawLine( vert.xyz,vert1.xyz );
			}
		}
		if (bSelected)
			dc.SetLineWidth(0);

		dc.PopMatrix();
	}
	else if (!m_pStatObj && !m_Faces.empty())
	{
		dc.PushMatrix(m_matrix);
		// if selected.
		bool bSelected = (m_flags & BRF_SELECTED) != 0;
		if (bSelected)
			dc.SetLineWidth(2);
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (!face.m_bSelected)
			{
				SBrushPoly *poly = face.m_Poly;
				if (!poly)
					continue;
				int numv = poly->m_Pts.size();
				for (int j = 0; j < numv; j++)
				{
					int k = ((j+1) < numv) ? j+1 : 0;
					SBrushVert &p0 = poly->m_Pts[j];
					SBrushVert &p1 = poly->m_Pts[k];
					dc.DrawLine( p0.xyz,p1.xyz );
				}
			}
		}
		if (bSelected)
			dc.SetLineWidth(0);
		dc.PopMatrix();
	}

	if (!(m_flags & BRF_SUB_OBJ_SEL))
	{

		return;
	}

	// Remember render states.
	uint32 nPrevState = dc.GetState();

	//////////////////////////////////////////////////////////////////////////
	// Sub object selection.
	if (m_pStatObj)
	{
		int nStatObjFlags = m_pStatObj->GetFlags();
		if (g_SubObjSelOptions.displayType == SO_DISPLAY_GEOMETRY)
			nStatObjFlags &= ~STATIC_OBJECT_HIDDEN;
		else
			nStatObjFlags |= STATIC_OBJECT_HIDDEN;
		m_pStatObj->SetFlags( nStatObjFlags );
	}

	const Matrix34 &worldTM = m_matrix;
	Matrix34 invWorldTM = worldTM.GetInverted();

	// World space camera vector.
	Vec3 vWSCameraVector = worldTM.GetTranslation() - dc.view->GetViewTM().GetTranslation();
	// Object space camera vector.
	Vec3 vOSCameraVector = invWorldTM.TransformVector(vWSCameraVector).GetNormalized();
	Vec3 vOSCameraPos = invWorldTM.TransformPoint(dc.view->GetViewTM().GetTranslation());

	dc.PushMatrix( worldTM );

	//////////////////////////////////////////////////////////////////////////
	// Display flat shaded object.
	//////////////////////////////////////////////////////////////////////////
	if (g_SubObjSelOptions.displayType == SO_DISPLAY_FLAT)
	{
		ColorB faceColor(0,250,250,255);
		ColorB col = faceColor;
		if (!b2D)
			dc.SetDrawInFrontMode(false);
		dc.SetFillMode( e_FillModeSolid );
		dc.CullOn();
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if ((m_selectionType != SO_ELEM_FACE && m_selectionType != SO_ELEM_POLYGON) || !face.m_bSelected)
			{
				ColorB col = faceColor;
				float dt = -face.m_Plane.normal.Dot(vOSCameraVector);
				dt = max(0.4f,dt);
				dt = min(1.0f,dt);
				col.r = ftoi(faceColor.r*dt);
				col.g = ftoi(faceColor.g*dt);
				col.b = ftoi(faceColor.b*dt);
				col.a = faceColor.a;
				dc.SetColor(col);

				SBrushPoly *poly = face.m_Poly;
				if (!poly)
					continue;
				for (int j = 1; j < poly->m_Pts.size()-1; j++)
				{
					dc.DrawTri( poly->m_Pts[0].xyz,poly->m_Pts[j+1].xyz,poly->m_Pts[j].xyz );
				}
			}
		}
	}

	ColorB edgeColor(255,255,255,155);
	if (m_selectionType != SO_ELEM_EDGE)
	{
		if (g_SubObjSelOptions.bDisplayBackfacing)
			dc.CullOff();
		else
			dc.CullOn();
		if (!b2D)
			dc.SetDrawInFrontMode(true);
		dc.SetFillMode( e_FillModeWireframe );
		// Draw triangles.
		//dc.pRenderAuxGeom->DrawTriangles( triMesh.pVertices,triMesh.GetVertexCount(),	mesh.m_pIndices,mesh.GetIndexCount(),edgeColor );
		for (int i = 0; i < m_Faces.size(); i++)
		{
			dc.SetColor( edgeColor );
			SBrushFace &face = *m_Faces[i];
			if (!face.m_bSelected)
			{
				SBrushPoly *poly = face.m_Poly;
				if (!poly)
					continue;
				int numv = poly->m_Pts.size();
				for (int j = 0; j < numv; j++)
				{
					int k = ((j+1) < numv) ? j+1 : 0;
					SBrushVert &p0 = poly->m_Pts[j];
					SBrushVert &p1 = poly->m_Pts[k];
					dc.DrawLine( p0.xyz,p1.xyz );
				}
			}
		}
	}

	if (g_SubObjSelOptions.bDisplayNormals)
	{
		dc.SetColor(edgeColor);
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			face.CalcCenter();
			dc.DrawLine(face.m_vCenter,face.m_vCenter+face.m_Plane.normal*g_SubObjSelOptions.fNormalsLength,RGB(0,255,255),RGB(255,255,255) );
		}
	}

	if (m_selectionType == SO_ELEM_VERTEX)
	{
		ColorB pointColor(0,255,255,255);
		ColorB pointColorSelected(255,0,0,255);
		dc.SetColor(pointColor);
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			SBrushPoly *poly = face.m_Poly;
			if (!poly)
				continue;

			for (int j = 0; j < poly->m_Pts.size(); j++)
			{
				SBrushVert &vert = poly->m_Pts[j];
				if (!g_SubObjSelOptions.bDisplayBackfacing && (vert.xyz-vOSCameraPos).Dot(face.m_Plane.normal) > 0)
					continue; // Backfacing.

				if (vert.bSelected)
					dc.SetColor(pointColorSelected);
				dc.DrawPoint( vert.xyz,4 );
				if (vert.bSelected)
					dc.SetColor(pointColor);
			}
		}
	}
	else if (m_selectionType == SO_ELEM_EDGE)
	{
		ColorB edgeColor(200,255,200,255);
		ColorB selEdgeColor(255,0,0,255);

		/*
		// Draw selected edges.
		for (int i = 0; i < triMesh.GetEdgeCount(); i++)
		{
			CTriEdge &edge = triMesh.pEdges[i];
			if (!g_SubObjSelOptions.bDisplayBackfacing)
			{
				// Check if edge is backfacing.
				bool backface0 = edge.face[0] >= 0 && vOSCameraVector.Dot(triMesh.pFaces[edge.face[0]].normal) > 0;
				bool backface1 = edge.face[1] >= 0 && vOSCameraVector.Dot(triMesh.pFaces[edge.face[1]].normal) > 0;
				if (backface0 && backface1)
					continue;
				if (backface0 && edge.face[1] < 0) // If only one face, and it is backfacing.
					continue;
				if (backface1 && edge.face[0] < 0) // If only one face, and it is backfacing.
					continue;
			}
			const Vec3 &p1 = triMesh.pWSVertices[edge.v[0]];
			const Vec3 &p2 = triMesh.pWSVertices[edge.v[1]];
			if (triMesh.edgeSel[i])
			{
				dc.pRenderAuxGeom->DrawLine( p1,selEdgeColor,p2,selEdgeColor );
			}
			else
			{
				dc.pRenderAuxGeom->DrawLine( p1,edgeColor,p2,edgeColor );
			}
		}
		*/
	}
	else if (m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON)
	{
		ColorB pointColor(0,255,255,255);
		ColorB selFaceColor(255,0,0,255);
		if (g_SubObjSelOptions.displayType != SO_DISPLAY_FLAT)
		{
			// For Non flat shading display selected face transparent.
			selFaceColor.a = 100;
		}

		// Draw selected faces and face points.
		dc.CullOff();
		dc.SetFillMode( e_FillModeSolid );
		dc.SetColor(selFaceColor);

		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (face.m_bSelected)
			{
				dc.SetColor(selFaceColor);
				SBrushPoly *poly = face.m_Poly;
				if (!poly)
					continue;
				for (int j = 1; j < poly->m_Pts.size()-1; j++)
				{
					dc.DrawTri( poly->m_Pts[0].xyz,poly->m_Pts[j+1].xyz,poly->m_Pts[j].xyz );
				}
			}

			if (!g_SubObjSelOptions.bDisplayBackfacing && vOSCameraVector.Dot(face.m_Plane.normal) > 0)
				continue; // Backfacing.

			dc.SetColor(pointColor);
			face.CalcCenter();
			dc.DrawPoint( face.m_vCenter,4 );
		}
	}

	dc.SetDrawInFrontMode(false);
	dc.PopMatrix();

	// Restore render states.
	dc.SetState(nPrevState);
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::HitTest( HitContext &hit )
{
	if (!(m_flags & BRF_SUB_OBJ_SEL))
	{
		// If Hit Test in normal mode.
		float dist = 0;
		SBrushFace *face = RayHit( hit.raySrc,hit.rayDir,&dist );
		if (face)
		{
			hit.dist = dist;
			return true;
		}
		return false;
	}

	// If Hit Testing in sub object editing mode.

	const Matrix34 &worldTM = m_matrix;
	Matrix34 invWorldTM = worldTM.GetInverted();

	// Object space camera position.
	Vec3 vOSCameraPos = invWorldTM.TransformPoint( hit.view->GetViewTM().GetTranslation() );
	// World space camera vector.
	Vec3 vWSCameraVector = worldTM.GetTranslation() - hit.view->GetViewTM().GetTranslation();
	// Object space camera vector.
	Vec3 vOSCameraVector = invWorldTM.TransformVector(vWSCameraVector).GetNormalized();

	bool bFirstHitOnly = hit.nSubObjFlags & SO_HIT_POINT;
	bool bHitTestSelected = hit.nSubObjFlags & SO_HIT_TEST_SELECTED;
	bool bSelectOnHit = hit.nSubObjFlags & SO_HIT_SELECT;
	bool bAdd = hit.nSubObjFlags & SO_HIT_SELECT_ADD;
	bool bRemove = hit.nSubObjFlags & SO_HIT_SELECT_REMOVE;
	bool bSelectValue = !bRemove;

	IUndoObject *pUndoObj = NULL;
	bool bSelChanged = false;
	if (bSelectOnHit)
	{
		if (CUndo::IsRecording())
		{
			pUndoObj = new CUndoBrush(this,"SubObj Select");
			/*
			switch (m_selectionType)
			{
			case SO_ELEM_VERTEX:
				pUndoObj = new CUndoEdMesh(this,CTriMesh::COPY_VERT_SEL|CTriMesh::COPY_WEIGHTS,"Select Vertex(s)");
				break;
			case SO_ELEM_EDGE:
				pUndoObj = new CUndoEdMesh(this,CTriMesh::COPY_EDGE_SEL|CTriMesh::COPY_WEIGHTS,"Select Edge(s)");
				break;
			case SO_ELEM_FACE:
				pUndoObj = new CUndoEdMesh(this,CTriMesh::COPY_FACE_SEL|CTriMesh::COPY_WEIGHTS,"Select Face(s)");
				break;
			}
			*/
		}
	}

	if (bSelectOnHit && !bAdd && !bRemove)
	{
		bSelChanged = ClearSelection();
	}

	float minDist = FLT_MAX;
	int closestElem = -1;

	if (m_selectionType == SO_ELEM_VERTEX)
	{
		int i;
		for (i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			SBrushPoly *poly = face.m_Poly;
			if (!poly)
				continue;
			for (int j = 0; j < poly->m_Pts.size(); j++)
			{
				SBrushVert &vert = poly->m_Pts[j];

				// Check backfacing.
				if (g_SubObjSelOptions.bIgnoreBackfacing && (vert.xyz-vOSCameraPos).Dot(face.m_Plane.normal) > 0)
					continue;

				CPoint p = hit.view->WorldToView( worldTM.TransformPoint(vert.xyz) );
				if (p.x >= hit.rect.left && p.x <= hit.rect.right &&
						p.y >= hit.rect.top && p.y <= hit.rect.bottom)
				{
					if (!bSelectOnHit)
					{
						if (!bHitTestSelected)
							return true;
						else if (vert.bSelected)
							return true;
					}
					else
					{
						/*
						if (bFirstHitOnly)
						{
							float dist = vOSCameraPos.GetDistance(vert.xyz);
							if (dist < minDist)
							{
								closestElem = i;
								minDist = dist;
							}
						}
						else*/ if ((bool)vert.bSelected != bSelectValue)
						{
							bSelChanged = true;
							vert.bSelected = bSelectValue;
						}
					}
				}
			}
			face.SelectPointsFromPoly();
		}
		
		/*
		//////////////////////////////////////////////////////////////////////////
		if (closestElem >= 0)
		{
			if (triMesh.vertSel[closestElem] != bSelectValue)
				bSelChanged = true;
			triMesh.vertSel[closestElem] = bSelectValue;
		}
		*/

		/*
		// Display backface culled vertices.
		std::vector<char> hitTestVtx;
		hitTestVtx.resize(triMesh.GetVertexCount());
		memset( &hitTestVtx[0],0,sizeof(char)*triMesh.GetVertexCount() );

		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (g_SubObjSelOptions.bIgnoreBackfacing && vOSCameraVector.Dot(face.m_Plane.normal) > 0)
				continue; // Backfacing.
			hitTestVtx[face.v[0]] = true;
			hitTestVtx[face.v[1]] = true;
			hitTestVtx[face.v[2]] = true;
		}
/*
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			if (!hitTestVtx[i])
				continue;
			CPoint p = hit.view->WorldToView( triMesh.pWSVertices[i] );
			if (p.x >= hit.rect.left && p.x <= hit.rect.right &&
				p.y >= hit.rect.top && p.y <= hit.rect.bottom)
			{
				if (!bSelectOnHit)
				{
					if (!bHitTestSelected)
						return true;
					else if (triMesh.vertSel[i])
						return true;
				}
				else
				{
					if (bFirstHitOnly)
					{
						float dist = vWSCameraPos.GetDistance(triMesh.pWSVertices[i]);
						if (dist < minDist)
						{
							closestElem = i;
							minDist = dist;
						}
					}
					else if (triMesh.vertSel[i] != bSelectValue)
					{
						bSelChanged = true;
						triMesh.vertSel[i] = bSelectValue;
					}
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
		if (closestElem >= 0)
		{
			if (triMesh.vertSel[closestElem] != bSelectValue)
				bSelChanged = true;
			triMesh.vertSel[closestElem] = bSelectValue;
		}
		*/
	}
	/*
	if (m_selectionType == SO_ELEM_EDGE)
	{
		for (int i = 0; i < triMesh.GetEdgeCount(); i++)
		{
			CTriEdge &edge = triMesh.pEdges[i];
			if (g_SubObjSelOptions.bIgnoreBackfacing)
			{
				// Check if edge is backfacing.
				bool backface0 = edge.face[0] >= 0 && vOSCameraVector.Dot(triMesh.pFaces[edge.face[0]].normal) > 0;
				bool backface1 = edge.face[1] >= 0 && vOSCameraVector.Dot(triMesh.pFaces[edge.face[1]].normal) > 0;
				if (backface0 && backface1)
					continue;
				if (backface0 && edge.face[1] < 0) // If only one face, and it is backfacing.
					continue;
				if (backface1 && edge.face[0] < 0) // If only one face, and it is backfacing.
					continue;
			}

			if (hit.view->HitTestLine( triMesh.pWSVertices[edge.v[0]],triMesh.pWSVertices[edge.v[1]],hit.point2d,5 ))
			{
				if (!bSelectOnHit)
				{
					if (!bHitTestSelected)
						return true;
					else if (triMesh.edgeSel[i])
						return true;
				}
				else if (bFirstHitOnly)
				{
					float dist = vWSCameraPos.GetDistance(triMesh.pWSVertices[edge.v[0]]);
					if (dist < minDist)
					{
						closestElem = i;
						minDist = dist;
					}
				}
				else if (triMesh.edgeSel[i] != bSelectValue)
				{
					bSelChanged = true;
					triMesh.edgeSel[i] = bSelectValue;
				}
			}
		}
		//////////////////////////////////////////////////////////////////////////
		if (closestElem >= 0)
		{
			if (triMesh.edgeSel[closestElem] != bSelectValue)
				bSelChanged = true;
			triMesh.edgeSel[closestElem] = bSelectValue;
		}
	}
	else*/ if (m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON)
	{
		// If Hit Test in normal mode.
		float dist = 0;
		SBrushFace *pHitFace = RayHit( hit.raySrc,hit.rayDir,&dist );
		hit.dist = dist;

		if (!bSelectOnHit)
		{
			if (pHitFace)
			{
				if (!bHitTestSelected)
					return true;
				else if (pHitFace->m_bSelected)
					return true;
			}
		}
		else if (bFirstHitOnly)
		{
			if (pHitFace && pHitFace->m_bSelected != bSelectValue)
			{
				bSelChanged = true;
				pHitFace->m_bSelected = bSelectValue;
			}
		}
		else {
			// Rectangular hit.
			for (int i = 0; i < m_Faces.size(); i++)
			{
				SBrushFace &face = *m_Faces[i];

				if (g_SubObjSelOptions.bIgnoreBackfacing && vOSCameraVector.Dot(face.m_Plane.normal) > 0)
					continue; // Backfacing.

				face.CalcCenter();
				CPoint p = hit.view->WorldToView( worldTM.TransformPoint(face.m_vCenter) );
				if (p.x >= hit.rect.left && p.x <= hit.rect.right &&
					p.y >= hit.rect.top && p.y <= hit.rect.bottom)
				{
					if (face.m_bSelected != bSelectValue)
					{
						bSelChanged = true;
						face.m_bSelected = bSelectValue;
					}
				}
			}
		}
	}

	bool bSelectionNotEmpty = false;
	if (bSelectOnHit && bSelChanged)
	{
		if (CUndo::IsRecording() && pUndoObj)
			CUndo::Record( pUndoObj );

		//bSelectionNotEmpty = triMesh.UpdateSelection();
		//if (g_SubObjSelOptions.bSoftSelection)
			//triMesh.SoftSelection( g_SubObjSelOptions );

		//OnSelectionChange();
	}
	else
	{
		if (pUndoObj)
			pUndoObj->Release();
	}

	//return bSelectionNotEmpty;
	return bSelChanged;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::ModifySelection( SSubObjSelectionModifyContext &modCtx )
{
	if (modCtx.type == SO_MODIFY_UNSELECT)
	{
		IUndoObject *pUndoObj = NULL;
		if (CUndo::IsRecording())
			pUndoObj = new CUndoBrush(this,"SubObj Unselect");
		bool bChanged = ClearSelection();
		//if (bChanged)
		//	OnSelectionChange();
		if (CUndo::IsRecording() && bChanged)
			CUndo::Record( pUndoObj );
		else if (pUndoObj)
			pUndoObj->Release();
		return;
	}

	Matrix34 worldTM = m_matrix;
	Matrix34 invTM = worldTM.GetInverted();

	// Change modify reference frame to object space.
	Matrix34 modRefFrame = invTM * modCtx.worldRefFrame;
	Matrix34 modRefFrameInverse = modCtx.worldRefFrame.GetInverted() * worldTM;

	if (modCtx.type == SO_MODIFY_MOVE)
	{
		if (CUndo::IsRecording())
			CUndo::Record( new CUndoBrush(this,"Brush SubObj Move") );

		Vec3 vOffset = modCtx.vValue;
		vOffset = modCtx.worldRefFrame.GetInverted().TransformVector(vOffset); // Offset in local space.

		Matrix34 tm = modRefFrame * Matrix34::CreateTranslationMat(vOffset) * modRefFrameInverse;

		if (m_selectionType == SO_ELEM_VERTEX)
		{
			for (int i = 0; i < m_Faces.size(); i++)
			{
				SBrushFace &face = *m_Faces[i];
				for (int j = 0; j < 3; j++)
				{
					if (face.m_nSelectedPoints & (1<<j))
					{
						face.m_PlanePts[j] = tm.TransformPoint( face.m_PlanePts[j] );
						face.m_PlanePts[j] = invTM*modCtx.view->SnapToGrid( worldTM*face.m_PlanePts[j] );
					}
				}
			}
		}
		if (m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON)
		{
			for (int i = 0; i < m_Faces.size(); i++)
			{
				SBrushFace &face = *m_Faces[i];
				if (face.m_bSelected)
				{
					for (int j = 0; j < 3; j++)
						//if (triMesh.pWeights[i] != 0)
					{
						//Matrix34 tm = modRefFrame * Matrix34::CreateTranslationMat(vOffset*triMesh.pWeights[i]) * modRefFrameInverse;
						face.m_PlanePts[j] = tm.TransformPoint( face.m_PlanePts[j] );
					}
				}
			}
		}
		if (!BuildSolid())
			GetIEditor()->RestoreUndo();

		//OnSelectionChange();
	}
	else if (modCtx.type == SO_MODIFY_ROTATE)
	{
		if (CUndo::IsRecording())
			CUndo::Record( new CUndoBrush(this,"Brush SubObj Rotate") );

/*
		Ang3 angles = Ang3(modCtx.vValue);
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			CTriVertex &vtx = triMesh.pVertices[i];
			if (triMesh.pWeights[i] != 0)
			{
				Matrix34 tm = modRefFrame * Matrix33::CreateRotationXYZ(angles*triMesh.pWeights[i]) * modRefFrameInverse;
				vtx.pos = tm.TransformPoint( vtx.pos );
			}
		}
		*/
		Ang3 angles = Ang3(modCtx.vValue);
		Matrix34 tm = modRefFrame * Matrix33::CreateRotationXYZ(angles) * modRefFrameInverse;
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (face.m_bSelected)
				for (int j = 0; j < 3; j++)
					face.m_PlanePts[j] = tm.TransformPoint( face.m_PlanePts[j] );
		}
		BuildSolid();
		//OnSelectionChange();
	}
	/*
	else if (modCtx.type == SO_MODIFY_SCALE)
	{
		if (CUndo::IsRecording())
			CUndo::Record( new CUndoBrush(this,,"Brush SubObj Scale") );

		Vec3 vScale = modCtx.vValue;
		for (int i = 0; i < triMesh.GetVertexCount(); i++)
		{
			CTriVertex &vtx = triMesh.pVertices[i];
			if (triMesh.pWeights[i] != 0)
			{
				Vec3 scl = Vec3(1.0f,1.0f,1.0f)*(1.0f - triMesh.pWeights[i]) + vScale*triMesh.pWeights[i];
				Matrix34 tm = modRefFrame * Matrix33::CreateScale(scl) * modRefFrameInverse;
				vtx.pos = tm.TransformPoint( vtx.pos );
			}
		}
	}
	*/

	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::AcceptModifySelection()
{
	BuildSolid(false);
}

//////////////////////////////////////////////////////////////////////////
bool SBrush::ClearSelection()
{
	bool bSelChanged = false;
	int i;

	// Clear face selection.
	for (i = 0; i < m_Faces.size(); i++)
	{
		SBrushFace &face = *m_Faces[i];
		if (face.m_bSelected)
		{
			bSelChanged = true;
			face.m_bSelected = false;
		}
		if (face.m_nSelectedPoints)
			bSelChanged = true;
		face.m_nSelectedPoints = 0;

		if (face.m_Poly)
		{
			for (int j = 0; j < face.m_Poly->m_Pts.size(); j++)
			{
				if (face.m_Poly->m_Pts[j].bSelected)
				{
					bSelChanged = true;
					face.m_Poly->m_Pts[j].bSelected = false;
				}
			}
		}
	}

	return bSelChanged;
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_SnapToGrid()
{
	RecordUndo();
	SnapToGrid(true);
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_DeleteVertex()
{
	if (m_selectionType != SO_ELEM_VERTEX)
		return;

	RecordUndo();
	bool bAnySelectedFaces = false;
	// Delete all faces that hold selected vertices.
	for (int i = 0; i < m_Faces.size(); i++)
	{
		SBrushFace &face = *m_Faces[i];
		face.m_bSelected = false;
		if (face.m_Poly)
		{
			for (int j = 0; j < face.m_Poly->m_Pts.size(); j++)
			{
				if (face.m_Poly->m_Pts[j].bSelected)
					face.m_bSelected = true;
			}
		}
	}
	Selection_DeleteFace();
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_SplitFace()
{
	if (!(m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON))
		return;

	RecordUndo();

	SBrush temp = *this;

	int i;

	int nFaces = m_Faces.size();
	for (i = 0; i < nFaces; i++)
	{
		SBrushFace &face = *m_Faces[i];
		if (!face.m_bSelected)
			continue;
		if (!face.m_Poly)
			continue;

		int j;
		Vec3 midp(0,0,0);
		for (j = 0; j < face.m_Poly->m_Pts.size(); j++)
		{
			midp += face.m_Poly->m_Pts[j].xyz;
		}
		midp /= face.m_Poly->m_Pts.size();
		Vec3 newp = midp + face.m_Plane.normal;

		for (j = 0; j < face.m_Poly->m_Pts.size(); j++)
		{
			int nextj = (j < face.m_Poly->m_Pts.size()-1) ? j+1 : 0;
			SBrushFace *f = new SBrushFace;
			f->m_nMatID = face.m_nMatID;
			f->m_TexInfo = face.m_TexInfo;
			f->m_PlanePts[0] = newp;
			f->m_PlanePts[1] = face.m_Poly->m_Pts[j].xyz;
			f->m_PlanePts[2] = face.m_Poly->m_Pts[nextj].xyz;
			m_Faces.push_back(f);
		}
	}
	// Delete selected faces.
	i=0;
	while (i < m_Faces.size())
	{
		if (m_Faces[i]->m_bSelected)
		{
			delete m_Faces[i];
			m_Faces.erase( m_Faces.begin()+i );
		}
		else
			i++;
	}
	if (!BuildSolid(true))
	{
		// If after face removal brush is not valid, restore the original.
		*this = temp;
		GetIEditor()->RestoreUndo();
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_DeleteFace()
{
	RecordUndo();

	SBrush temp = *this;
	int i = 0;
	while (i < m_Faces.size())
	{
		if (m_Faces[i]->m_bSelected)
		{
			delete m_Faces[i];
			m_Faces.erase( m_Faces.begin()+i );
		}
		else
			i++;
	}
	if (!BuildSolid(true))
	{
		// If after face removal brush is not valid, restore the original.
		*this = temp;
		GetIEditor()->RestoreUndo();
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_SetMatId( int nMatId )
{
	if (!(m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON))
		return;

	RecordUndo();
	// Clear face selection.
	for (int i = 0; i < m_Faces.size(); i++)
	{
		SBrushFace &face = *m_Faces[i];
		if (face.m_bSelected)
		{
			face.m_nMatID = nMatId;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void SBrush::Selection_SelectMatId( int nMatId )
{
	if (m_selectionType == SO_ELEM_VERTEX)
	{
		// Select all vertices of the faces with the specified matid.
		RecordUndo();
		ClearSelection();
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (face.m_nMatID == nMatId)
			{
				if (face.m_Poly)
				{
					for (int j = 0; j < face.m_Poly->m_Pts.size(); j++)
					{
						face.m_Poly->m_Pts[j].bSelected = true;
					}
				}
			}
			//OnSelectionChange();
		}
	}
	else if (m_selectionType == SO_ELEM_FACE || m_selectionType == SO_ELEM_POLYGON)
	{
		RecordUndo();
		ClearSelection();
		for (int i = 0; i < m_Faces.size(); i++)
		{
			SBrushFace &face = *m_Faces[i];
			if (face.m_nMatID == nMatId)
			{
				face.m_bSelected = true;
			}
		}
		//OnSelectionChange();
	}
}
