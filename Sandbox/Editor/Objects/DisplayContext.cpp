////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   DisplayContext.cpp
//  Version:     v1.00
//  Created:     4/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: DisplayContext
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "DisplayContext.h"
#include "IRenderAuxGeom.h"

#include "..\IconManager.h"
#include "..\Viewport.h"

#include <I3DEngine.h>
//#include <gl\gl.h>

#define FREEZE_COLOR RGB(100,100,100)


//////////////////////////////////////////////////////////////////////////
DisplayContext::DisplayContext()
{
	view = 0;
	renderer = 0;
	engine = 0;
	flags = 0;
	settings = 0;
	m_renderState = 0;

	m_currentMatrix = 0;
	m_matrixStack[m_currentMatrix].SetIdentity();
	pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	m_thickness = 0;

	m_textureLabels.reserve( 100 );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetView( CViewport *pView )
{
	view = pView;
	
	CRect rc;
	view->GetClientRect(rc);
	m_width = rc.Width();
	m_height = rc.Height();
	m_textureLabels.resize(0);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::InternalDrawLine( const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1 )
{
	pRenderAuxGeom->DrawLine( v0,colV0,v1,colV1,m_thickness );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawPoint( const Vec3 &p,int nSize )
{
	pRenderAuxGeom->DrawPoint( ToWS(p),m_color4b,nSize );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTri( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3 )
{
	pRenderAuxGeom->DrawTriangle( ToWS(p1),m_color4b,ToWS(p2),m_color4b,ToWS(p3),m_color4b );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawQuad( const Vec3 &p1,const Vec3 &p2,const Vec3 &p3,const Vec3 &p4 )
{
	Vec3 p[4] = { ToWS(p1),ToWS(p2),ToWS(p3),ToWS(p4) };
	pRenderAuxGeom->DrawTriangle( p[0],m_color4b,p[1],m_color4b,p[2],m_color4b );
	pRenderAuxGeom->DrawTriangle( p[2],m_color4b,p[3],m_color4b,p[0],m_color4b );
	//pRenderAuxGeom->DrawPolyline( poly,4,true,m_color4b );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireBox( const Vec3 &min,const Vec3 &max )
{
	pRenderAuxGeom->DrawAABB( AABB(min,max),m_matrixStack[m_currentMatrix],false,m_color4b, eBBD_Faceted );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawSolidBox( const Vec3 &min,const Vec3 &max )
{
	pRenderAuxGeom->DrawAABB( AABB(min,max),m_matrixStack[m_currentMatrix],true,m_color4b, eBBD_Faceted );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine( const Vec3 &p1,const Vec3 &p2 )
{
	InternalDrawLine( ToWS(p1),m_color4b,ToWS(p2),m_color4b );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawPolyLine( const Vec3 *pnts,int numPoints )
{
	assert( pnts != 0 );
	assert( numPoints > 1 );
	Vec3 p1,p2;
	for (int i = 0; i < numPoints; i++)
	{
		int j = (i+1)<numPoints?(i+1):0;
		if (i == 0) p1 = ToWS(pnts[i]);
		p2 = ToWS(pnts[j]);
		InternalDrawLine( p1,m_color4b,p2,m_color4b );
		p1 = p2;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainCircle( const Vec3 &worldPos,float radius,float height, bool bIncludeOutdoorVoxels )
{
	// Draw circle with default radius.
	Vec3 p0,p1;
	p0.x = worldPos.x + radius*sin(0.0f);
	p0.y = worldPos.y + radius*cos(0.0f);
	p0.z = engine->GetTerrainElevation( p0.x, p0.y, bIncludeOutdoorVoxels )+height;
	float step = 10.0f/180*gf_PI;
	for (float angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = worldPos.x + radius*sin(angle);
		p1.y = worldPos.y + radius*cos(angle);
		p1.z = engine->GetTerrainElevation( p1.x, p1.y, bIncludeOutdoorVoxels )+height;
		InternalDrawLine( ToWS(p0),m_color4b,ToWS(p1),m_color4b );
		p0 = p1;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawCircle( const Vec3 &pos,float radius )
{
	// Draw circle with default radius.
	Vec3 p0,p1;
	p0.x = pos.x + radius*sin(0.0f);
	p0.y = pos.y + radius*cos(0.0f);
	p0.z = pos.z;
	p0 = ToWS(p0);
	float step = 10.0f/180*gf_PI;
	for (float angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y + radius*cos(angle);
		p1.z = pos.z;
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireCircle2d( const CPoint &center,float radius,float z )
{
	Vec3 p0,p1,pos;
	pos.x = center.x;
	pos.y = center.y;
	pos.z = z;
	p0.x = pos.x + radius*sin(0.0f);
	p0.y = pos.y + radius*cos(0.0f);
	p0.z = z;
	float step = 10.0f/180*gf_PI;

	int prevState = GetState();
	//SetState( (prevState|e_Mode2D) & (~e_Mode3D) );
	for (float angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y + radius*cos(angle);
		p1.z = z;
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}
	SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireSphere( const Vec3 &pos,float radius )
{
	Vec3 p0,p1;
	float step = 10.0f/180*gf_PI;
	float angle;

	// Z Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius*sin(0.0f);
	p0.y += radius*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y + radius*cos(angle);
		p1.z = pos.z;
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}

	// X Axis
	p0 = pos;
	p1 = pos;
	p0.y += radius*sin(0.0f);
	p0.z += radius*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x;
		p1.y = pos.y + radius*sin(angle);
		p1.z = pos.z + radius*cos(angle);
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}
	
	// Y Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius*sin(0.0f);
	p0.z += radius*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y;
		p1.z = pos.z + radius*cos(angle);
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}
}


//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawWireSphere( const Vec3 &pos,const Vec3 radius )
{
	Vec3 p0,p1;
	float step = 10.0f/180*gf_PI;
	float angle;

	// Z Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius.x*sin(0.0f);
	p0.y += radius.y*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius.x*sin(angle);
		p1.y = pos.y + radius.y*cos(angle);
		p1.z = pos.z;
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}

	// X Axis
	p0 = pos;
	p1 = pos;
	p0.y += radius.y*sin(0.0f);
	p0.z += radius.z*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x;
		p1.y = pos.y + radius.y*sin(angle);
		p1.z = pos.z + radius.z*cos(angle);
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}

	// Y Axis
	p0 = pos;
	p1 = pos;
	p0.x += radius.x*sin(0.0f);
	p0.z += radius.z*cos(0.0f);
	p0 = ToWS(p0);
	for (angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius.x*sin(angle);
		p1.y = pos.y;
		p1.z = pos.z + radius.z*cos(angle);
		p1 = ToWS(p1);
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		p0 = p1;
	}
}

void DisplayContext::DrawWireQuad2d( const CPoint &pmin,const CPoint &pmax,float z )
{
	int prevState = GetState();
	SetState( (prevState|e_Mode2D) & (~e_Mode3D) );
	InternalDrawLine( Vec3(pmin.x,pmin.y,z),m_color4b,Vec3(pmax.x,pmin.y,z),m_color4b );
	InternalDrawLine( Vec3(pmax.x,pmin.y,z),m_color4b,Vec3(pmax.x,pmax.y,z),m_color4b );
	InternalDrawLine( Vec3(pmax.x,pmax.y,z),m_color4b,Vec3(pmin.x,pmax.y,z),m_color4b );
	InternalDrawLine( Vec3(pmin.x,pmax.y,z),m_color4b,Vec3(pmin.x,pmin.y,z),m_color4b );
	SetState(prevState);
}

void DisplayContext::DrawLine2d( const CPoint &p1,const CPoint &p2,float z )
{
	int prevState = GetState();
	
	SetState( (prevState|e_Mode2D) & (~e_Mode3D) );
	InternalDrawLine( Vec3(p1.x/m_width,p1.y/m_height,z),m_color4b,Vec3(p2.x/m_width,p2.y/m_height,z),m_color4b );
	SetState(prevState);
}

//////////////////////////////////////////////////////////////////////////
COLORREF DisplayContext::GetSelectedColor()
{
	float t = GetTickCount() / 1000.0f;
	float r1 = fabs(sin(t*8.0f));
	if (r1 > 255)
		r1 = 255;
	return RGB( 255,0,r1*255 );
//			float r2 = cos(t*3);
		//dc.renderer->SetMaterialColor( 1,0,r1,0.5f );
}

COLORREF DisplayContext::GetFreezeColor()
{
	return FREEZE_COLOR;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetSelectedColor( float fAlpha )
{
	SetColor( GetSelectedColor(),fAlpha );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetFreezeColor()
{
	SetColor( FREEZE_COLOR,0.5f );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine( const Vec3 &p1,const Vec3 &p2,const ColorF &col1,const ColorF &col2 )
{
	InternalDrawLine( ToWS(p1),ColorB(col1.r*255.0f,col1.g*255.0f,col1.b*255.0f,col1.a*255.0f),
										ToWS(p2),ColorB(col2.r*255.0f,col2.g*255.0f,col2.b*255.0f,col2.a*255.0f) );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawLine( const Vec3 &p1,const Vec3 &p2,COLORREF rgb1,COLORREF rgb2 )
{
	Vec3 c1 = Rgb2Vec(rgb1);
	Vec3 c2 = Rgb2Vec(rgb2);
	InternalDrawLine( ToWS(p1),ColorB(GetRValue(rgb1),GetGValue(rgb1),GetBValue(rgb1),255),ToWS(p2),ColorB(GetRValue(rgb2),GetGValue(rgb2),GetBValue(rgb2),255) );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::PushMatrix( const Matrix34 &tm )
{
	assert( m_currentMatrix < 32 );
	if (m_currentMatrix < 32)
	{
		m_currentMatrix++;
		m_matrixStack[m_currentMatrix] = m_matrixStack[m_currentMatrix-1] * tm;
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::PopMatrix()
{
	assert( m_currentMatrix > 0 );
	if (m_currentMatrix > 0)
		m_currentMatrix--;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& DisplayContext::GetMatrix()
{
	return m_matrixStack[m_currentMatrix];
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawBall( const Vec3 &pos,float radius )
{
	pRenderAuxGeom->DrawSphere( ToWS(pos),radius,m_color4b );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawArrow( const Vec3 &src,const Vec3 &trg,float fHeadScale,bool b2SidedArrow )
{
	float f2dScale = 1.0f;
	float arrowLen = 0.4f * fHeadScale;
	float arrowRadius = 0.16f * fHeadScale;
	if (flags & DISPLAY_2D)
	{
		f2dScale = 1.2f*m_matrixStack[m_currentMatrix].TransformVector(Vec3(1,0,0)).GetLength();
	}
	Vec3 dir = trg - src;
	dir = m_matrixStack[m_currentMatrix].TransformVector(dir.GetNormalized());
	Vec3 p0 = ToWS(src);
	Vec3 p1 = ToWS(trg);
	if (!b2SidedArrow)
	{
		p1 = p1 - dir*arrowLen;
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		pRenderAuxGeom->DrawCone( p1,dir,arrowRadius*f2dScale,arrowLen*f2dScale,m_color4b );
	}
	else
	{
		p0 = p0 + dir*arrowLen;
		p1 = p1 - dir*arrowLen;
		InternalDrawLine( p0,m_color4b,p1,m_color4b );
		pRenderAuxGeom->DrawCone( p0,-dir,arrowRadius*f2dScale,arrowLen*f2dScale,m_color4b );
		pRenderAuxGeom->DrawCone( p1,dir,arrowRadius*f2dScale,arrowLen*f2dScale,m_color4b );
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::RenderObject( int objectType,const Vec3 &pos,float scale )
{
	Matrix34 tm;
	tm.SetIdentity();

	tm = Matrix33::CreateScale( Vec3(scale,scale,scale) ) * tm;

	tm.SetTranslation(pos);
	RenderObject( objectType,tm );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::RenderObject( int objectType,const Matrix34 &tm )
{
	IStatObj *object = GetIEditor()->GetIconManager()->GetObject((EObject)objectType);
	if (object)
	{
		float color[4];
		color[0] = m_color4b.r * (1.0f/255.0f);
		color[1] = m_color4b.g * (1.0f/255.0f);
		color[2] = m_color4b.b * (1.0f/255.0f);
		color[3] = m_color4b.a * (1.0f/255.0f);

		Matrix34 xform = m_matrixStack[m_currentMatrix] * tm;
		SRendParams rp;
		rp.pMatrix = &xform;
		rp.AmbientColor = ColorF(color[0],color[1],color[2], 1);
		rp.fAlpha = color[3];
		rp.nDLightMask = 0xFFFF;
    rp.dwFObjFlags |= FOB_TRANS_MASK;
		//rp.nShaderTemplate = EFT_HELPER;
		object->Render( rp );
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawIcon( EIcon icon,const Vec3 &pos,float fScale )
{
	int texId = GetIEditor()->GetIconManager()->GetIcon(icon);
	renderer->DrawLabelImage( pos,fScale,texId );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTerrainRect( float x1,float y1,float x2,float y2,float height )
{
	Vec3 p1,p2;
	float x,y;
	
	float step = MAX( y2-y1,x2-x1 );
	if (step < 0.1)
		return;
	step = step / 100.0f;
	if (step > 10)
		step /= 10;

	for (y = y1; y < y2; y += step)
	{
		float ye = min(y+step,y2);

		p1.x = x1;
		p1.y = y;
		p1.z = engine->GetTerrainElevation( p1.x,p1.y ) + height;

		p2.x = x1;
		p2.y = ye;
		p2.z = engine->GetTerrainElevation( p2.x,p2.y ) + height;
		DrawLine( p1,p2 );

		p1.x = x2;
		p1.y = y;
		p1.z = engine->GetTerrainElevation( p1.x,p1.y ) + height;

		p2.x = x2;
		p2.y = ye;
		p2.z = engine->GetTerrainElevation( p2.x,p2.y ) + height;
		DrawLine( p1,p2 );
	}
	for (x = x1; x < x2; x += step)
	{
		float xe = min(x+step,x2);

		p1.x = x;
		p1.y = y1;
		p1.z = engine->GetTerrainElevation( p1.x,p1.y ) + height;

		p2.x = xe;
		p2.y = y1;
		p2.z = engine->GetTerrainElevation( p2.x,p2.y ) + height;
		DrawLine( p1,p2 );

		p1.x = x;
		p1.y = y2;
		p1.z = engine->GetTerrainElevation( p1.x,p1.y ) + height;

		p2.x = xe;
		p2.y = y2;
		p2.z = engine->GetTerrainElevation( p2.x,p2.y ) + height;
		DrawLine( p1,p2 );
	}
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTextLabel( const Vec3& pos,float size,const char *text, const bool bCenter )
{
	ColorF col(m_color4b.r*(1.0f/255.0f),m_color4b.g*(1.0f/255.0f),m_color4b.b*(1.0f/255.0f),m_color4b.a*(1.0f/255.0f) );
	view->DrawTextLabel( *this,ToWS(pos),size,col,text,bCenter );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::Draw2dTextLabel( float x,float y,float size,const char *text )
{
	float col[4] = { m_color4b.r*(1.0f/255.0f),m_color4b.g*(1.0f/255.0f),m_color4b.b*(1.0f/255.0f),m_color4b.a*(1.0f/255.0f) };
	renderer->Draw2dLabel( x,y,size,col,false,"%s",text );
  //view->DrawTextLabel( *this,pos,size,Vec3(m_color[0],m_color[1],m_color[2]),text );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::SetLineWidth( float width )
{
	m_thickness = width;
}

//////////////////////////////////////////////////////////////////////////
bool DisplayContext::IsVisible( const BBox &bounds )
{
	if (flags & DISPLAY_2D)
	{
		if (box.IsIntersectBox( bounds ))
		{
			return true;
		}
	}
	else
	{
		return camera->IsAABBVisible_F( AABB(bounds.min,bounds.max) );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
uint32 DisplayContext::GetState() const
{
	return m_renderState;
}

//! Set a new render state flags.
//! @param returns previous render state.
uint32 DisplayContext::SetState( uint32 state )
{
	uint32 old = m_renderState;
	m_renderState = state;
	pRenderAuxGeom->SetRenderFlags( m_renderState );
	return old;
}

//! Set a new render state flags.
//! @param returns previous render state.
uint32 DisplayContext::SetStateFlag( uint32 state )
{
	uint32 old = m_renderState;
	m_renderState |= state;
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( m_renderState );
	return old;
}

//! Clear specified flags in render state.
//! @param returns previous render state.
uint32 DisplayContext::ClearStateFlag( uint32 state )
{
	uint32 old = m_renderState;
	m_renderState &= ~state;
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( m_renderState );
	return old;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthTestOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_DepthTestOff) & (~e_DepthTestOn) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthTestOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_DepthTestOn) & (~e_DepthTestOff) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthWriteOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_DepthWriteOff) & (~e_DepthWriteOn) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DepthWriteOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_DepthWriteOn) & (~e_DepthWriteOff) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::CullOff()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_CullModeNone) & (~(e_CullModeBack|e_CullModeFront)) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::CullOn()
{
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	pRenderAuxGeom->SetRenderFlags( (m_renderState|e_CullModeBack) & (~(e_CullModeNone|e_CullModeFront)) );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
}

//////////////////////////////////////////////////////////////////////////
bool DisplayContext::SetDrawInFrontMode( bool bOn )
{
	int prevState = m_renderState;
	SAuxGeomRenderFlags renderFlags = m_renderState;
	renderFlags.SetDrawInFrontMode( (bOn) ? e_DrawInFrontOn : e_DrawInFrontOff );
	pRenderAuxGeom->SetRenderFlags( renderFlags );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	return (prevState & e_DrawInFrontOn) != 0;
}

int DisplayContext::SetFillMode( int nFillMode )
{
	int prevState = m_renderState;
	SAuxGeomRenderFlags renderFlags = m_renderState;
	renderFlags.SetFillMode( (EAuxGeomPublicRenderflags_FillMode)nFillMode );
	pRenderAuxGeom->SetRenderFlags( renderFlags );
	m_renderState = pRenderAuxGeom->GetRenderFlags().m_renderFlags;
	return prevState;
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::DrawTextureLabel( const Vec3& pos,int nWidth,int nHeight,int nTexId,int nTexIconFlags )
{
	Vec3 scrpos = view->WorldToView3D( pos );

	STextureLabel tl;
	tl.x = scrpos.x;
	tl.y = scrpos.y;
	if (nTexIconFlags & TEXICON_ALIGN_BOTTOM)
		tl.y -= nHeight/2;
	tl.z = scrpos.z - (1.0f-scrpos.z)*0.1f;
	tl.w = nWidth;
	tl.h = nHeight;
	tl.nTexId = nTexId;
	tl.flags = nTexIconFlags;
	tl.color[0] = m_color4b.r*(1.0f/255.0f);
	tl.color[1] = m_color4b.g*(1.0f/255.0f);
	tl.color[2] = m_color4b.b*(1.0f/255.0f);
	tl.color[3] = m_color4b.a*(1.0f/255.0f);

	// Try to not overflood memory with labels.
	if (m_textureLabels.size() < 100000)
		m_textureLabels.push_back( tl );
}

//////////////////////////////////////////////////////////////////////////
void DisplayContext::Flush2D()
{
	if (m_textureLabels.empty())
		return;

	CRect rc;
	view->GetClientRect(rc);

	int rcw = rc.right;
	int rch = rc.bottom;


	renderer->Set2DMode( true,rcw,rch,0,1 );

	renderer->SetState( GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA );
	//renderer->SetCullMode( R_CULL_NONE );

	float uvs[4],uvt[4];
 	uvs[0]=0; uvt[0]=1;
	uvs[1]=1; uvt[1]=1;
	uvs[2]=1; uvt[2]=0;
	uvs[3]=0; uvt[3]=0;

	int nLabels = m_textureLabels.size();
	for (int i = 0; i < nLabels; i++)
	{
		STextureLabel &t = m_textureLabels[i];
		float w2 = t.w * 0.5f;
		float h2 = t.h * 0.5f;
		if (t.flags & TEXICON_ADDITIVE)
			renderer->SetState( GS_BLSRC_ONE|GS_BLDST_ONE );

		renderer->DrawImageWithUV( t.x-w2,t.y+h2,t.z,t.w,-t.h,t.nTexId,uvs,uvt,t.color[0],t.color[1],t.color[2],t.color[3] );
		
		if (t.flags & TEXICON_ADDITIVE) // Restore state.
			renderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA );
	}
	//renderer->SetCullMode( R_CULL_BACK );
	renderer->Set2DMode( false,rcw,rch );
	
	m_textureLabels.clear();
}

//////////////////////////////////////////////////////////////////////////
uint8 BoxSides[0x40*8] = {
	  0,0,0,0, 0,0,0,0, //00
		0,4,6,2, 0,0,0,4, //01
		7,5,1,3, 0,0,0,4, //02
		0,0,0,0, 0,0,0,0, //03
		0,1,5,4, 0,0,0,4, //04
		0,1,5,4, 6,2,0,6, //05
		7,5,4,0, 1,3,0,6, //06
		0,0,0,0, 0,0,0,0, //07
		7,3,2,6, 0,0,0,4, //08
		0,4,6,7, 3,2,0,6, //09
		7,5,1,3, 2,6,0,6, //0a
		0,0,0,0, 0,0,0,0, //0b
		0,0,0,0, 0,0,0,0, //0c
		0,0,0,0, 0,0,0,0, //0d
		0,0,0,0, 0,0,0,0, //0e
		0,0,0,0, 0,0,0,0, //0f
		0,2,3,1, 0,0,0,4, //10
		0,4,6,2, 3,1,0,6, //11
		7,5,1,0, 2,3,0,6, //12
		0,0,0,0, 0,0,0,0, //13
		0,2,3,1, 5,4,0,6, //14
		1,5,4,6, 2,3,0,6, //15
		7,5,4,0, 2,3,0,6, //16
		0,0,0,0, 0,0,0,0, //17
		0,2,6,7, 3,1,0,6, //18
		0,4,6,7, 3,1,0,6, //19
		7,5,1,0, 2,6,0,6, //1a
		0,0,0,0, 0,0,0,0, //1b
		0,0,0,0, 0,0,0,0, //1c
		0,0,0,0, 0,0,0,0, //1d
		0,0,0,0, 0,0,0,0, //1e
		0,0,0,0, 0,0,0,0, //1f
		7,6,4,5, 0,0,0,4, //20
		0,4,5,7, 6,2,0,6, //21
		7,6,4,5, 1,3,0,6, //22
		0,0,0,0, 0,0,0,0, //23
		7,6,4,0, 1,5,0,6, //24
		0,1,5,7, 6,2,0,6, //25
		7,6,4,0, 1,3,0,6, //26
		0,0,0,0, 0,0,0,0, //27
		7,3,2,6, 4,5,0,6, //28
		0,4,5,7, 3,2,0,6, //29
		6,4,5,1, 3,2,0,6, //2a
		0,0,0,0, 0,0,0,0, //2b
		0,0,0,0, 0,0,0,0, //2c
		0,0,0,0, 0,0,0,0, //2d
		0,0,0,0, 0,0,0,0, //2e
		0,0,0,0, 0,0,0,0, //2f
		0,0,0,0, 0,0,0,0, //30
		0,0,0,0, 0,0,0,0, //31
		0,0,0,0, 0,0,0,0, //32
		0,0,0,0, 0,0,0,0, //33
		0,0,0,0, 0,0,0,0, //34
		0,0,0,0, 0,0,0,0, //35
		0,0,0,0, 0,0,0,0, //36
		0,0,0,0, 0,0,0,0, //37
		0,0,0,0, 0,0,0,0, //38
		0,0,0,0, 0,0,0,0, //39
		0,0,0,0, 0,0,0,0, //3a
		0,0,0,0, 0,0,0,0, //3b
		0,0,0,0, 0,0,0,0, //3c
		0,0,0,0, 0,0,0,0, //3d
		0,0,0,0, 0,0,0,0, //3e
		0,0,0,0, 0,0,0,0, //3f
};

