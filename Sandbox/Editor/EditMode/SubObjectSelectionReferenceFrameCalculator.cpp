////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   SubObjectSelectionReferenceFrameCalculator.cpp
//  Version:     v1.00
//  Created:     9/3/2006 Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: Calculate the reference frame for sub-object selections.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SubObjectSelectioNReferenceFrameCalculator.h"
#include "Brush/Brush.h"

SubObjectSelectionReferenceFrameCalculator::SubObjectSelectionReferenceFrameCalculator(ESubObjElementType selectionType)
:	bAnySelected(false),
	pos(0.0f, 0.0f, 0.0f),
	normal(0.0f, 0.0f, 0.0f),
	nNormals(0),
	selectionType(selectionType),
	bUseExplicitFrame(false),
	bExplicitAnySelected(false)
{
}

void SubObjectSelectionReferenceFrameCalculator::AddBrush(const Matrix34& worldTM, SBrush* pBrush)
{
	if (this->selectionType == SO_ELEM_VERTEX)
	{
		for (int i = 0; i < pBrush->m_Faces.size(); i++)
		{
			SBrushFace &face = *pBrush->m_Faces[i];
			SBrushPoly *poly = face.m_Poly;
			if (!poly)
				continue;
			for (int j = 0; j < poly->m_Pts.size(); j++)
			{
				SBrushVert &vert = poly->m_Pts[j];
				if (vert.bSelected)
				{
					Vec3 worldPosition = worldTM * vert.xyz;
					if (std::find(this->positions.begin(),this->positions.end(),worldPosition) == this->positions.end())
					{
						this->positions.push_back(worldPosition);
						// This position is not selected yet.
						this->bAnySelected = true;
						this->normal = this->normal - face.m_Plane.normal;
						this->nNormals++;
						this->pos = this->pos + worldPosition;
					}
				}
			}
		}
	}

	if (this->selectionType == SO_ELEM_FACE || this->selectionType == SO_ELEM_POLYGON)
	{
		// Average all face normals.
		for (int i = 0; i < pBrush->m_Faces.size(); i++)
		{
			SBrushFace &face = *pBrush->m_Faces[i];
			if (face.m_bSelected)
			{
				this->bAnySelected = true;
				this->normal = this->normal - face.m_Plane.normal;
				this->nNormals++;
				face.CalcCenter();
				this->pos = this->pos + worldTM * face.m_vCenter;
			}
		}
	}
}

void SubObjectSelectionReferenceFrameCalculator::SetExplicitFrame(bool bAnySelected, const Matrix34& refFrame)
{
	this->refFrame = refFrame;
	this->bUseExplicitFrame = true;
	this->bExplicitAnySelected = bAnySelected;
}

bool SubObjectSelectionReferenceFrameCalculator::GetFrame(Matrix34& refFrame)
{
	if (this->bUseExplicitFrame)
	{
		refFrame = this->refFrame;
		return this->bExplicitAnySelected;
	}
	else
	{
		refFrame.SetIdentity();

		if (this->nNormals > 0)
		{
			this->normal = this->normal / this->nNormals;
			if (!this->normal.IsZero())
				this->normal.Normalize();

			// Average position.
			this->pos = this->pos / this->nNormals;
			refFrame.SetTranslation(this->pos);
		}

		if (this->bAnySelected)
		{
			if (!this->normal.IsZero())
			{
				Vec3 xAxis(1,0,0),yAxis(0,1,0),zAxis(0,0,1);
				if (this->normal.IsEquivalent(zAxis) || normal.IsEquivalent(-zAxis))
					zAxis = xAxis;
				xAxis = this->normal.Cross(zAxis).GetNormalized();
				yAxis = xAxis.Cross(this->normal).GetNormalized();
				refFrame.SetFromVectors( xAxis,yAxis,normal,pos );
			}
		}

		return bAnySelected;
	}
}
