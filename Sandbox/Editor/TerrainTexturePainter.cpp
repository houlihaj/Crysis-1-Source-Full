////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   terraintexturepainter.cpp
//  Version:     v1.00
//  Created:     8/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "TerrainTexturePainter.h"
#include "Viewport.h"
#include "Heightmap.h"
#include "TerrainGrid.h"
#include "VegetationMap.h"
#include "Objects\DisplayContext.h"
#include "Objects\ObjectManager.h"
#include "ResizeResolutionDialog.h"

#include "TerrainPainterPanel.h"

#include "CryEditDoc.h"
#include "Layer.h"

#include "Util\ImagePainter.h"

#include <I3DEngine.h>


struct CUndoTPSector
{
	CUndoTPSector()
	{
		m_pImg = 0;
		m_pImgRedo = 0;
	}

	CPoint tile;
	int x, y;
	int w;
	uint32 dwSize;
	CImage * m_pImg;
	CImage * m_pImgRedo;
};


struct CUndoTPLayerIdSector
{
	CUndoTPLayerIdSector()
	{
		m_pImg = 0;
		m_pImgRedo = 0;
	}

	int x, y;
	CByteImage * m_pImg;
	CByteImage * m_pImgRedo;
};

struct CUndoTPElement
{
	std::vector <CUndoTPSector> sects;
	std::vector <CUndoTPLayerIdSector> layerIds;

	~CUndoTPElement()
	{
		Clear();
	}

	void AddSector(float fpx, float fpy, float radius )
	{
		CHeightmap * heightmap = GetIEditor()->GetHeightmap();

		uint32 dwMaxRes = heightmap->m_TerrainRGBTexture.CalcMaxLocalResolution(0,0,1,1);

		uint32 dwTileCountX = heightmap->m_TerrainRGBTexture.GetTileCountX();
		uint32 dwTileCountY = heightmap->m_TerrainRGBTexture.GetTileCountY();

		float	gx1 = fpx-radius-2.0f/dwMaxRes;
		float	gx2 = fpx+radius+2.0f/dwMaxRes;
		float	gy1 = fpy-radius-2.0f/dwMaxRes;
		float gy2 = fpy+radius+2.0f/dwMaxRes;

		if(gx1<0.0f) gx1 = 0.0f;
		if(gy1<0.0f) gy1 = 0.0f;
		if(gx2>1.0f) gx2 = 1.0f;
		if(gy2>1.0f) gy2 = 1.0f;

		CRect recTiles=CRect(
			(uint32)floor(gx1 * dwTileCountX),
			(uint32)floor(gy1 * dwTileCountY),
			(uint32)ceil( gx2 * dwTileCountX),
			(uint32)ceil( gy2 * dwTileCountY));

		for(uint32 dwTileY=recTiles.top;dwTileY<recTiles.bottom;++dwTileY)
			for(uint32 dwTileX=recTiles.left;dwTileX<recTiles.right;++dwTileX)
			{
				uint32 dwLocalSize = heightmap->m_TerrainRGBTexture.GetTileResolution(dwTileX,dwTileY);
				uint32 dwSize = heightmap->m_TerrainRGBTexture.GetTileResolution(dwTileX,dwTileY) * heightmap->m_TerrainRGBTexture.GetTileCountX();

				uint32 gwid = dwSize * radius * 4;
				if(gwid<32)
					gwid = 32;
				else if(gwid<64)
					gwid = 64;
				else if(gwid<128)
					gwid = 128;
				else
					gwid = 256;

				float	x1 = gx1;
				float	x2 = gx2;
				float	y1 = gy1;
				float y2 = gy2;
				
				if( x1 < (float)dwTileX / dwTileCountX ) x1 = (float)dwTileX / dwTileCountX;
				if( y1 < (float)dwTileY / dwTileCountY ) y1 = (float)dwTileY / dwTileCountY;
				if( x2 > (float)(dwTileX + 1) / dwTileCountX ) x2 = (float)(dwTileX+1) / dwTileCountX;
				if( y2 > (float)(dwTileY + 1) / dwTileCountY ) y2 = (float)(dwTileY+1) / dwTileCountY;

				uint32 wid = gwid;

				if(wid>dwLocalSize)
					wid = dwLocalSize;

				CRect recSects=CRect(
					((int)floor(x1 * dwSize / wid)) * wid,
					((int)floor(y1 * dwSize / wid)) * wid,
					((int)ceil(x2 * dwSize / wid)) * wid,
					((int)ceil(y2 * dwSize / wid)) * wid);


				for(uint32 sy = recSects.top; sy < recSects.bottom; sy+=wid)
					for(uint32 sx = recSects.left; sx < recSects.right; sx+=wid)
					{
						bool bFind = false;
						for (int i=sects.size()-1; i>=0; i--)
						{
							CUndoTPSector * pSect = &sects[i];
							if(pSect->tile.x==dwTileX && pSect->tile.y==dwTileY)
							{
								if(pSect->x == sx &&  pSect->y == sy)
								{
									bFind = true;
									break;
								}
							}
						}
						if(!bFind)
						{
							CUndoTPSector newSect;
							newSect.x = sx;
							newSect.y = sy;
							newSect.w = wid;
							newSect.tile.x = dwTileX;
							newSect.tile.y = dwTileY;
							newSect.dwSize = dwSize;

							newSect.m_pImg = new CImage();
							newSect.m_pImg->Allocate(newSect.w, newSect.w);

							CUndoTPSector * pSect = &newSect;

							heightmap->m_TerrainRGBTexture.GetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize, 
								(float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *pSect->m_pImg);

							sects.push_back(newSect);
						}
					}
			}

		// Store LayerIDs
		const uint32 layerSize = 64;

		CRect reclids=CRect(
			(uint32)floor(gx1 * heightmap->GetWidth() / layerSize),
			(uint32)floor(gy1 * heightmap->GetHeight()/ layerSize),
			(uint32)ceil( gx2 * heightmap->GetWidth() / layerSize),
			(uint32)ceil( gy2 * heightmap->GetHeight()/ layerSize));

		for(uint32 ly = reclids.top; ly < reclids.bottom; ly++)
			for(uint32 lx = reclids.left; lx < reclids.right; lx++)
			{
				bool bFind = false;
				for (int i=layerIds.size()-1; i>=0; i--)
				{
					CUndoTPLayerIdSector * pSect = &layerIds[i];
					if(pSect->x == lx &&  pSect->y == ly)
					{
						bFind = true;
						break;
					}
				}
				if(!bFind)
				{
					CUndoTPLayerIdSector newSect;
					CUndoTPLayerIdSector * pSect = &newSect;

					pSect->m_pImg = new CByteImage;
					pSect->x = lx;
					pSect->y = ly;
					heightmap->GetLayerIdBlock(pSect->x*layerSize, pSect->y*layerSize, layerSize, layerSize, *pSect->m_pImg);

					layerIds.push_back(newSect);
				}
			}
	}
	
	void Paste(bool bIsRedo = false)
	{
		CHeightmap * heightmap = GetIEditor()->GetHeightmap();

		bool bFirst = true;
		CPoint gminp;
		CPoint gmaxp;

		AABB aabb(AABB::RESET);

		for (int i=sects.size()-1; i>=0; i--)
		{
			CUndoTPSector * pSect = &sects[i];
			heightmap->m_TerrainRGBTexture.SetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize, 
				(float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *(bIsRedo ? pSect->m_pImgRedo : pSect->m_pImg) );

			aabb.Add(	Vec3((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize, 0));
			aabb.Add(	Vec3((float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, 0));
		}

		// LayerIDs
		for (int i=layerIds.size()-1; i>=0; i--)
		{
			CUndoTPLayerIdSector * pSect = &layerIds[i];
			heightmap->SetLayerIdBlock(pSect->x*pSect->m_pImg->GetWidth(), pSect->y*pSect->m_pImg->GetHeight(), *(bIsRedo ? pSect->m_pImgRedo : pSect->m_pImg) );
			heightmap->UpdateEngineTerrain( pSect->x*pSect->m_pImg->GetWidth(), pSect->y*pSect->m_pImg->GetHeight(), pSect->m_pImg->GetWidth(),pSect->m_pImg->GetHeight(),true, false );
		}

		if(!aabb.IsReset())
		{
			RECT rc = {	aabb.min.x * heightmap->GetWidth(), aabb.min.y * heightmap->GetHeight(), 
									aabb.max.x * heightmap->GetWidth(), aabb.max.y * heightmap->GetHeight()};
			heightmap->UpdateLayerTexture(rc);
		}
	}

	void StoreRedo()
	{
		CHeightmap * heightmap = GetIEditor()->GetHeightmap();

		for (int i=sects.size()-1; i>=0; i--)
		{
			CUndoTPSector * pSect = &sects[i];
			if(!pSect->m_pImgRedo)
			{
				pSect->m_pImgRedo = new CImage();
				pSect->m_pImgRedo->Allocate(pSect->w, pSect->w);

				heightmap->m_TerrainRGBTexture.GetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize, 
							(float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *pSect->m_pImgRedo);
			}
		}

		// LayerIds
		for (int i=layerIds.size()-1; i>=0; i--)
		{
			CUndoTPLayerIdSector * pSect = &layerIds[i];
			if(!pSect->m_pImgRedo)
			{
				pSect->m_pImgRedo = new CByteImage();
				heightmap->GetLayerIdBlock(pSect->x*pSect->m_pImg->GetWidth(), pSect->y*pSect->m_pImg->GetHeight(), pSect->m_pImg->GetWidth(), pSect->m_pImg->GetHeight(), *pSect->m_pImgRedo);
			}
		}
	}

	void Clear()
	{
		for (int i=0; i<sects.size(); i++)
		{
			CUndoTPSector * pSect = &sects[i];
			delete pSect->m_pImg;
			pSect->m_pImg = 0;

			delete pSect->m_pImgRedo;
			pSect->m_pImgRedo = 0;
		}

		for (int i=0; i<layerIds.size(); i++)
		{
			CUndoTPLayerIdSector * pSect = &layerIds[i];
			delete pSect->m_pImg;
			pSect->m_pImg = 0;

			delete pSect->m_pImgRedo;
			pSect->m_pImgRedo = 0;
		}
	}

	int GetSize()
	{
		int size = 0;
		for (int i=0; i<sects.size(); i++)
		{
			CUndoTPSector * pSect = &sects[i];
			if(pSect->m_pImg)
				size +=pSect->m_pImg->GetSize();
			if(pSect->m_pImgRedo)
				size +=pSect->m_pImgRedo->GetSize();
		}
		for (int i=0; i<layerIds.size(); i++)
		{
			CUndoTPLayerIdSector * pSect = &layerIds[i];
			if(pSect->m_pImg)
				size +=pSect->m_pImg->GetSize();
			if(pSect->m_pImgRedo)
				size +=pSect->m_pImgRedo->GetSize();
		}
		return size;
	}
};

//////////////////////////////////////////////////////////////////////////
//! Undo object.
class CUndoTexturePainter : public IUndoObject
{
public:
	CUndoTexturePainter( CTerrainTexturePainter * pTool)
	{
		m_pUndo = pTool->m_pTPElem;
		pTool->m_pTPElem = 0;
	}

protected:
	virtual void Release() 
	{ 
		delete m_pUndo;
		delete this; 
	};

	virtual int GetSize() {	return sizeof(*this) + (m_pUndo ? m_pUndo->GetSize() : 0) ; };
	virtual const char* GetDescription() { return "Terrain Painter Modify"; };

	virtual void Undo( bool bUndo )
	{
		if (bUndo)
			m_pUndo->StoreRedo();

		if(m_pUndo)
			m_pUndo->Paste();
	}
	virtual void Redo()
	{
		if(m_pUndo)
			m_pUndo->Paste(true);
	}

private:
	CUndoTPElement * m_pUndo;
};


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainTexturePainter,CEditTool)

CTextureBrush CTerrainTexturePainter::m_brush;

namespace {
	int s_toolPanelId = 0;
	CTerrainPainterPanel *s_toolPanel = 0;
};

//////////////////////////////////////////////////////////////////////////
CTerrainTexturePainter::CTerrainTexturePainter()
{
	m_brush.maxRadius=1000.0f;

	SetStatusText( _T("Paint Texture Layers") );

	m_heightmap = GetIEditor()->GetHeightmap();
	assert( m_heightmap );

	m_3DEngine = GetIEditor()->Get3DEngine();
	assert( m_3DEngine );

	m_renderer = GetIEditor()->GetRenderer();
	assert( m_renderer );

	m_terrainGrid = m_heightmap->GetTerrainGrid();
	assert(m_terrainGrid);
	
	m_pointerPos(0,0,0);
	GetIEditor()->ClearSelection();

	//////////////////////////////////////////////////////////////////////////
	// Initialize sectors.
	//////////////////////////////////////////////////////////////////////////
	SSectorInfo sectorInfo;
	m_heightmap->GetSectorsInfo( sectorInfo );
	m_numSectors = sectorInfo.numSectors;
	m_sectorSize = sectorInfo.sectorSize;
	m_sectorTexSize = sectorInfo.sectorTexSize;
	m_surfaceTextureSize = sectorInfo.surfaceTextureSize;

//	m_pixelsPerMeter = ((float)sectorInfo.sectorTexSize) / sectorInfo.sectorSize;

	// Initialize terrain texture generator.
	//m_terrTexGen.PrepareHeightmap( m_surfaceTextureSize,m_surfaceTextureSize );
	//m_terrTexGen.PrepareLayers( m_surfaceTextureSize,m_surfaceTextureSize );

	m_bIsPainting = false;
	m_pTPElem = 0;
}

//////////////////////////////////////////////////////////////////////////
CTerrainTexturePainter::~CTerrainTexturePainter()
{
	m_pointerPos(0,0,0);

	if (GetIEditor()->IsUndoRecording())
		GetIEditor()->CancelUndo();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::BeginEditParams( IEditor *ie,int flags )
{
	if (!s_toolPanelId)
	{
		s_toolPanel = new CTerrainPainterPanel(this,AfxGetMainWnd());
		s_toolPanelId = GetIEditor()->AddRollUpPage( ROLLUP_TERRAIN,_T("Layer Painter"),s_toolPanel );
		AfxGetMainWnd()->SetFocus();

		s_toolPanel->SetBrush(m_brush);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::EndEditParams()
{
	if (s_toolPanelId)
	{
		GetIEditor()->RemoveRollUpPage(ROLLUP_TERRAIN,s_toolPanelId);
		s_toolPanel = 0;
		s_toolPanelId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTexturePainter::MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	bool bPainting = false;
	m_pointerPos = view->ViewToWorld( point,0,true );

	m_brush.bErase = false;

	if(m_bIsPainting)
	{
		if(event == eMouseLDown || event == eMouseLUp)
		{
			Action_StopUndo();
		}
	}

	if(m_brush.type == ET_BRUSH_CHANGERESOLUTION)
	{
		// ChangeResolution
		if(event == eMouseLDown)
			Action_ChangeResolution();

		GetIEditor()->SetStatusText( _T("L-Mouse: Change Resolution") );
	}
	else
	{
		if((flags & MK_CONTROL)!=0)															// pick layerid
		{
			if(event == eMouseLDown)
				Action_PickLayerId();
		}
		else if(event == eMouseLDown || (event == eMouseMove && (flags&MK_LBUTTON)) )		// paint
			Action_Paint();
	
		GetIEditor()->SetStatusText( _T("L-Mouse: Paint   +/-: Change Brush Radius   CTRL L-Mouse: Pick LayerId") );
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Display( DisplayContext &dc )
{
	dc.SetColor( 0,1,0,1 );

	if(m_pointerPos.x==0 && m_pointerPos.y==0 && m_pointerPos.z==0)
		return;		// mouse cursor not in window

	if(m_brush.type == ET_BRUSH_CHANGERESOLUTION)
	{
		uint32 dwCountX=m_heightmap->m_TerrainRGBTexture.GetTileCountX(), dwCountY=m_heightmap->m_TerrainRGBTexture.GetTileCountY();

		float fTerrainSize = (float)m_3DEngine->GetTerrainSize();																			// in m

		float _fX=m_pointerPos.x/fTerrainSize;										// 0..1
		float _fY=m_pointerPos.y/fTerrainSize;										// 0..1

		if(_fX<0 || _fX>1 || _fY<0 || _fY>1)
			return;

		float fX0=fTerrainSize*floor(_fX*dwCountX)/dwCountX;
		float fY0=fTerrainSize*floor(_fY*dwCountY)/dwCountY;

		float fX1=fX0+fTerrainSize/dwCountX, fY1=fY0+fTerrainSize/dwCountY;

		float fXMid=(fX0+fX1)*0.5f, fYMid=(fY0+fY1)*0.5f;

			// flipped X and Y because of Editor and Engine difference
		int iX=(int)(_fY*dwCountX);
  	int iY=(int)(_fX*dwCountY);
	
		uint32 dwLocalSize = m_heightmap->m_TerrainRGBTexture.GetTileResolution(iX,iY);

		char strInfo[256];

		sprintf(strInfo,"%d x %d\n(%.2f pix/m)",dwLocalSize,dwLocalSize,(float)dwLocalSize/(fTerrainSize/dwCountX));

		dc.DrawTextLabel(Vec3(fXMid,fYMid,m_3DEngine->GetTerrainElevation(fXMid,fYMid)-20.0f),2.0f,strInfo,true);

		dc.DrawTerrainRect(fX0,fY0,fX1,fY1,1.0f);
		dc.DrawTerrainRect(fX0*0.9f+fX1*0.1f,fY0*0.9f+fY1*0.1f,fX1*0.9f+fX0*0.1f,fY1*0.9f+fY0*0.1f,1.0f);
	}
	else
	{
		dc.DepthTestOff();
		dc.DrawTerrainCircle( m_pointerPos,m_brush.radius,0.2f );
		dc.DepthTestOn();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTexturePainter::OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags )
{
	bool bProcessed = false;

	if(m_brush.type == ET_BRUSH_CHANGERESOLUTION)
		return false;		// no key processing

	if (nChar == VK_ADD)
	{
		m_brush.radius += 1;
		bProcessed = true;
	}
	if (nChar == VK_SUBTRACT)
	{
		if (m_brush.radius > 1)
			m_brush.radius -= 1;
		bProcessed = true;
	}
	if (bProcessed && s_toolPanel)
	{
		//s_toolPanel->SetBrush(m_brush);
	}
	if (m_brush.radius < m_brush.minRadius)
		m_brush.radius = m_brush.minRadius;
	if (m_brush.radius > m_brush.maxRadius)
		m_brush.radius = m_brush.maxRadius;

	if (bProcessed)
	{
		if (s_toolPanel)
			s_toolPanel->SetBrush(m_brush);
	}

	return bProcessed;
}


//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::UpdateSectorTexture( CPoint texsector,
	const float fGlobalMinX, const float fGlobalMinY, const float fGlobalMaxX, const float fGlobalMaxY )
{
	int iTexSectorsNum = m_3DEngine->GetTerrainSize()/m_3DEngine->GetTerrainTextureNodeSizeMeters();

	CCryEditDoc *pDoc = GetIEditor()->GetDocument();

	float fInvSectorCnt = 1.0f/(float)iTexSectorsNum;
	float fMinX = texsector.x*fInvSectorCnt;
	float fMinY = texsector.y*fInvSectorCnt;

	uint32 dwFullResolution = pDoc->m_cHeightmap.m_TerrainRGBTexture.CalcMaxLocalResolution(fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt);

	if(dwFullResolution)
	{
		uint32 dwNeededResolution = dwFullResolution/iTexSectorsNum;

		CTerrainSector *st = m_terrainGrid->GetSector(texsector);

		bool bFullRefreshRequired = (st->textureId)==0;

		int iLocalOutMinX=0,iLocalOutMinY=0,iLocalOutMaxX=dwNeededResolution,iLocalOutMaxY=dwNeededResolution;		// in pixel

		bool bRecreated;

		int texId = m_terrainGrid->LockSectorTexture( texsector,dwNeededResolution,bRecreated);

		if(bRecreated)
			bFullRefreshRequired=true;

		if(!bFullRefreshRequired)
		{
			iLocalOutMinX=floor((fGlobalMinX-fMinX)*dwFullResolution);
			iLocalOutMinY=floor((fGlobalMinY-fMinY)*dwFullResolution);
			iLocalOutMaxX=ceil((fGlobalMaxX-fMinX)*dwFullResolution);
			iLocalOutMaxY=ceil((fGlobalMaxY-fMinY)*dwFullResolution);

			iLocalOutMinX=CLAMP(iLocalOutMinX,0,dwNeededResolution);
			iLocalOutMinY=CLAMP(iLocalOutMinY,0,dwNeededResolution);
			iLocalOutMaxX=CLAMP(iLocalOutMaxX,0,dwNeededResolution);
			iLocalOutMaxY=CLAMP(iLocalOutMaxY,0,dwNeededResolution);
		}

		if(iLocalOutMaxX!=iLocalOutMinX && iLocalOutMaxY!=iLocalOutMinY) 
		{
			CImage image;
	
			image.Allocate(iLocalOutMaxX-iLocalOutMinX,iLocalOutMaxY-iLocalOutMinY);

			pDoc->m_cHeightmap.m_TerrainRGBTexture.GetSubImageStretched(
				fMinX+fInvSectorCnt/dwNeededResolution*iLocalOutMinX,
				fMinY+fInvSectorCnt/dwNeededResolution*iLocalOutMinY,
				fMinX+fInvSectorCnt/dwNeededResolution*iLocalOutMaxX,
				fMinY+fInvSectorCnt/dwNeededResolution*iLocalOutMaxY,
				image);

			// convert RGB colour into format that has less compression artefacts for brightness variations
			{
				bool bDX10 = m_renderer->GetRenderType()==eRT_DX10;			// i would be cleaner if renderer would ensure the behave the same but that can be slower 

				uint32 dwWidth = image.GetWidth();
				uint32 dwHeight = image.GetHeight();

				uint32 *pMem = &image.ValueAt(0,0);

				for(uint32 dwY=0;dwY<dwHeight;++dwY)
					for(uint32 dwX=0;dwX<dwWidth;++dwX)
					{
#ifdef TERRAIN_USE_CIE_COLORSPACE
						float fR = ((*pMem>>16)&0xff)*(1.0f/255.0f);
						float fG = ((*pMem>>8)&0xff)*(1.0f/255.0f);
						float fB = ((*pMem)&0xff)*(1.0f/255.0f);

						ColorF cCIE = ColorF(fR,fG,fB).RGB2mCIE();

						uint32 dwR = (uint32)(cCIE.r*255.0f+0.5f);
						uint32 dwG = (uint32)(cCIE.g*255.0f+0.5f);
						uint32 dwB = (uint32)(cCIE.b*255.0f+0.5f);
#else
						uint32 dwR = (*pMem>>16)&0xff;
						uint32 dwG = (*pMem>>8)&0xff;
						uint32 dwB = (*pMem)&0xff;
#endif

						if(bDX10)
							*pMem++ = 0xff000000 | (dwB<<16) | (dwG<<8) | (dwR);		// shader requires alpha channel = 1
						 else
							*pMem++ = 0xff000000 | (dwR<<16) | (dwG<<8) | (dwB);		// shader requires alpha channel = 1
					}
			}
/*
			// debug borders
			for(int i=0;i<dwNeededResolution;++i)
			{
				image.ValueAt(i,0)=0xff;
				image.ValueAt(0,i)=0x00ff;
				image.ValueAt(i,dwNeededResolution-1)=0xff00;
				image.ValueAt(dwNeededResolution-1,i)=0xffff00;
			}
*/
			m_renderer->UpdateTextureInVideoMemory( texId,(unsigned char*)image.GetData(),iLocalOutMinX,iLocalOutMinY,iLocalOutMaxX-iLocalOutMinX,iLocalOutMaxY-iLocalOutMinY,eTF_A8R8G8B8 );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_ChangeResolution()
{
	assert(m_brush.type == ET_BRUSH_CHANGERESOLUTION);

	if(m_pointerPos.x==0 && m_pointerPos.y==0 && m_pointerPos==0)
		return;

	CCryEditDoc *pDoc = GetIEditor()->GetDocument();

	uint32 dwCountX=m_heightmap->m_TerrainRGBTexture.GetTileCountX(), dwCountY=m_heightmap->m_TerrainRGBTexture.GetTileCountY();

	float fTerrainSize = (float)m_3DEngine->GetTerrainSize();																			// in m

	float _fX=m_pointerPos.x/fTerrainSize;										// 0..1
	float _fY=m_pointerPos.y/fTerrainSize;										// 0..1

	// flipped X and Y because of Editor and Engine difference
	int iX=(int)(_fY*dwCountX);
	int iY=(int)(_fX*dwCountY);

	if(iX<0 || iY<0 || iX>=dwCountX || iY>=dwCountY)
		return;		// out of range

	uint32 dwOldSize = pDoc->GetHeightmap().m_TerrainRGBTexture.GetTileResolution((uint32)iX,(uint32)iY);

	CResizeResolutionDialog dlg;
	dlg.SetSize(dwOldSize);
	if(dlg.DoModal()!=IDOK)
		return;

	uint32 dwNewSize = dlg.GetSize();

	if(dwOldSize == dwNewSize || dwNewSize>2048 || dwNewSize<64)
		return;

	pDoc->GetHeightmap().m_TerrainRGBTexture.ChangeTileResolution((uint32)iX,(uint32)iY, dwNewSize);

	GetIEditor()->GetDocument()->SetModifiedFlag();

	// update 3dEngine display
	{
		float fX0=fTerrainSize*floor(_fX*dwCountX)/dwCountX;
		float fY0=fTerrainSize*floor(_fY*dwCountY)/dwCountY;

		float fX1=fX0+fTerrainSize/dwCountX, fY1=fY0+fTerrainSize/dwCountY;

		int iTexSectorSize = m_3DEngine->GetTerrainTextureNodeSizeMeters();			// in meters

		if(!iTexSectorSize)
		{
			assert(0);						// maybe we should visualized this to the user
			return;								// you need to calculated the surface texture first
		}

		int iMinSecX = (int)floor(fX0/(float)iTexSectorSize);
		int iMinSecY = (int)floor(fY0/(float)iTexSectorSize);
		int iMaxSecX = (int)ceil (fX1/(float)iTexSectorSize);
		int iMaxSecY = (int)ceil (fY1/(float)iTexSectorSize);

/*
		int iTerrainSize = m_3DEngine->GetTerrainSize();												// in meters
		iMinSecX = max(iMinSecX,0);
		iMinSecY = max(iMinSecY,0);
		iMaxSecX = min(iMaxSecX,iTerrainSize/iTexSectorSize);
		iMaxSecY = min(iMaxSecY,iTerrainSize/iTexSectorSize);
*/
		for(int iY=iMinSecY; iY<iMaxSecY; ++iY)
		for(int iX=iMinSecX; iX<iMaxSecX; ++iX)
			UpdateSectorTexture(CPoint(iY,iX),0,0,1,1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_PickLayerId()
{
	int iTerrainSize = m_3DEngine->GetTerrainSize();																			// in m
	int hmapWidth = m_heightmap->GetWidth();
	int hmapHeight = m_heightmap->GetHeight();

	int iX = (int)((m_pointerPos.y*hmapWidth)/iTerrainSize);			// maybe +0.5f is needed
	int iY = (int)((m_pointerPos.x*hmapHeight)/iTerrainSize);			// maybe +0.5f is needed

	if(iX>=0 && iX<iTerrainSize)
	if(iY>=0 && iY<iTerrainSize)
	{	
		uint32 dwLayerid = m_heightmap->GetLayerIdAt(iX,iY);

		CCryEditDoc *pDoc = GetIEditor()->GetDocument();
		CLayer *pLayer = pDoc->FindLayerByLayerId(dwLayerid);

		s_toolPanel->SelectLayer(pLayer);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_Paint()
{
	CCryEditDoc *pDoc = GetIEditor()->GetDocument();

	//////////////////////////////////////////////////////////////////////////
	// Paint spot on selected layer.
	//////////////////////////////////////////////////////////////////////////
	CLayer *pLayer = GetSelectedLayer();
	if (!pLayer)
		return;

	if(!pDoc->IsNewTerranTextureSystem())
		if(pLayer->IsAutoGen())								// old system was not using some layers
			return;

	static bool bPaintLock = false;
	if (bPaintLock)
		return;

	bPaintLock = true;
	
	float fTerrainSize = (float)m_3DEngine->GetTerrainSize();																			// in m

	SEditorPaintBrush br(pDoc->m_cHeightmap,*pLayer,m_brush.bMaskByLayerSettings,m_brush.m_dwMaskLayerId);

	br.m_cFilterColor = m_brush.m_cFilterColor*m_brush.m_fBrightness;
	br.fRadius = m_brush.radius/fTerrainSize;
	br.hardness = m_brush.hardness;
	br.color = m_brush.value;
	if (m_brush.bErase)
		br.color = 0;

	// Paint spot on layer mask.
	{
		float fX=m_pointerPos.y/fTerrainSize;							// 0..1
		float fY=m_pointerPos.x/fTerrainSize;							// 0..1

		// change terrain texture
		{
			CRGBLayer &TerrainRGBLayer = pDoc->m_cHeightmap.m_TerrainRGBTexture;

			assert(TerrainRGBLayer.CalcMinRequiredTextureExtend());

			// load m_texture is needed/possible
			pLayer->PrecacheTexture();

			if(pLayer->m_texture.IsValid())
			{
				Action_CollectUndo(fX, fY, br.fRadius);
				TerrainRGBLayer.PaintBrushWithPatternTiled(fX,fY,br,pLayer->m_texture);
			}
		}

		if(m_brush.bDetailLayer)		// we also want to change the detail layer data
		{
			// get unique layerId
			uint32 dwLayerId = pLayer->GetOrRequestLayerId();					assert(dwLayerId!=0xffffffff);	

			m_heightmap->PaintLayerId(fX,fY,br,dwLayerId);
		}
	}


	Vec3 vMin = m_pointerPos-Vec3(m_brush.radius,m_brush.radius,0);
	Vec3 vMax = m_pointerPos+Vec3(m_brush.radius,m_brush.radius,0);

	//////////////////////////////////////////////////////////////////////////
	// Update Terrain textures.
	//////////////////////////////////////////////////////////////////////////
	{
		int iTerrainSize = m_3DEngine->GetTerrainSize();												// in meters
		int iTexSectorSize = m_3DEngine->GetTerrainTextureNodeSizeMeters();			// in meters

		if(!iTexSectorSize)
		{
			assert(0);						// maybe we should visualized this to the user
			return;								// you need to calculated the surface texture first
		}

		int iMinSecX = (int)floor(vMin.x/(float)iTexSectorSize);
		int iMinSecY = (int)floor(vMin.y/(float)iTexSectorSize);
		int iMaxSecX = (int)ceil (vMax.x/(float)iTexSectorSize);
		int iMaxSecY = (int)ceil (vMax.y/(float)iTexSectorSize);

		iMinSecX = max(iMinSecX,0);
		iMinSecY = max(iMinSecY,0);
		iMaxSecX = min(iMaxSecX,iTerrainSize/iTexSectorSize);
		iMaxSecY = min(iMaxSecY,iTerrainSize/iTexSectorSize);

		float fTerrainSize = (float)m_3DEngine->GetTerrainSize();																			// in m

		// update rectangle in 0..1 range
		float fGlobalMinX=vMin.x/fTerrainSize, fGlobalMinY=vMin.y/fTerrainSize;
		float fGlobalMaxX=vMax.x/fTerrainSize, fGlobalMaxY=vMax.y/fTerrainSize;

		for(int iY=iMinSecY; iY<iMaxSecY; ++iY)
		for(int iX=iMinSecX; iX<iMaxSecX; ++iX)
		{
			if(pDoc->IsNewTerranTextureSystem())
				UpdateSectorTexture( CPoint(iY,iX),fGlobalMinY,fGlobalMinX,fGlobalMaxY,fGlobalMaxX );
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Update surface types.
	//////////////////////////////////////////////////////////////////////////
	// Build rectangle in heightmap coordinates.
	{
		float fTerrainSize = (float)m_3DEngine->GetTerrainSize();																			// in m
		int hmapWidth = m_heightmap->GetWidth();
		int hmapHeight = m_heightmap->GetHeight();

		float fX=m_pointerPos.y*hmapWidth/fTerrainSize;
		float fY=m_pointerPos.x*hmapHeight/fTerrainSize;

		int unitSize = m_heightmap->GetUnitSize();
		float fHMRadius = m_brush.radius/unitSize;
		CRect hmaprc;

		// clip against heightmap borders
		hmaprc.left = max((int)floor(fX-fHMRadius),0);
		hmaprc.top = max((int)floor(fY-fHMRadius),0);
		hmaprc.right = min((int)ceil(fX+fHMRadius),hmapWidth);
		hmaprc.bottom = min((int)ceil(fY+fHMRadius),hmapHeight);

		// Calculate surface type for this block.
		if(!pDoc->IsNewTerranTextureSystem())
			m_heightmap->CalcSurfaceTypes( &hmaprc );

		// Update surface types at 3d engine terrain.
		m_heightmap->UpdateEngineTerrain( hmaprc.left,hmaprc.top,hmaprc.Width(),hmaprc.Height(),false,true );
	}

	GetIEditor()->GetDocument()->SetModifiedFlag();
	GetIEditor()->UpdateViews(eUpdateHeightmap);

	bPaintLock = false;
}

//////////////////////////////////////////////////////////////////////////
CLayer* CTerrainTexturePainter::GetSelectedLayer() const
{
	CString selLayer = s_toolPanel->GetSelectedLayer();
	CCryEditDoc *pDoc = GetIEditor()->GetDocument();
	for (int i = 0; i < pDoc->GetLayerCount(); i++)
	{
		CLayer *pLayer = pDoc->GetLayer(i);
		if (selLayer == pLayer->GetLayerName())
		{
			return pLayer;
		}
	}
	return 0;
}

/*
//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::ImportExport(bool bIsImport, bool bIsClipboard)
{
	if(!bIsClipboard)
	{
		if(!strlen(gSettings.terrainTextureExport))
			gSettings.BrowseTerrainTexture();

		if(!strlen(gSettings.terrainTextureExport))
		{
			AfxMessageBox("Error: Need to specify file name.");
			return;
		}
	}

	BBox box;
	GetIEditor()->GetSelectedRegion( box );

	CPoint minp;
	CPoint maxp;

	if(box.IsEmpty())
	{
		minp = CPoint(0, 0);
		maxp = CPoint(m_heightmap->GetWidth(), m_heightmap->GetHeight());
	}
	else
	{
		minp = m_heightmap->WorldToHmap( box.min );
		maxp = m_heightmap->WorldToHmap( box.max );
	}


	m_heightmap->m_TerrainRGBTexture.ImportExportBlock( bIsClipboard ? 0 : gSettings.terrainTextureExport,
		(float)minp.x/m_heightmap->GetWidth(), (float)minp.y/m_heightmap->GetHeight(), 
		(float)maxp.x/m_heightmap->GetWidth(), (float)maxp.y/m_heightmap->GetHeight(), bIsImport);

	if(bIsImport)
	{
		RECT rc = {minp.x, minp.y, maxp.x, maxp.y};
		m_heightmap->UpdateLayerTexture(rc);
	}
}
*/

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_StartUndo()
{
	if(m_bIsPainting )
		return;

	m_pTPElem = new CUndoTPElement;

	m_bIsPainting = true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_CollectUndo(float x, float y, float radius)
{
	if(!m_bIsPainting )
		Action_StartUndo();

	m_pTPElem->AddSector(x, y, radius);
}



//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_StopUndo()
{
	if(!m_bIsPainting)
		return;

	GetIEditor()->BeginUndo();

	if (CUndo::IsRecording())
		CUndo::Record( new CUndoTexturePainter(this) );

	GetIEditor()->AcceptUndo("Texture Layer Painting");

	m_bIsPainting = false;
}