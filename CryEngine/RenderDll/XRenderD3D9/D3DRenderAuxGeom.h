#ifndef D3D_RENDER_AUX_GEOM_H
#define D3D_RENDER_AUX_GEOM_H


#include "../Common/RenderAuxGeom.h"


class CD3D9Renderer;
class ICrySizer;


class CD3DRenderAuxGeom : public CRenderAuxGeom
{
public:
	// interface
	virtual void Flush( bool discardGeometry = true );
	virtual void GetMemoryUsage( ICrySizer* pSizer );
	
public:
	static CD3DRenderAuxGeom* Create( CD3D9Renderer& renderer )
	{
		return( new CD3DRenderAuxGeom( renderer ) );
	}

public:
	~CD3DRenderAuxGeom();

	void ReleaseDeviceObjects();
	HRESULT RestoreDeviceObjects();
	void SetOrthoMode( bool enable, D3DXMATRIX* pMatrix = 0 );	
  void Discard();
	void DiscardTransMatrices();

protected:
	virtual int GetTransMatrixIndex();

private:
	struct SStreamBufferManager
	{
	public:
		SStreamBufferManager();
		void Reset();
		void DiscardVB();
		void DiscardIB();

	public:
		bool m_discardVB;
		uint32 m_curVBIndex;
		bool m_discardIB;
		uint32 m_curIBIndex;
	};

	struct SDrawObjMesh
	{
		SDrawObjMesh()
			: m_numVertices( 0 )
			, m_numFaces( 0 )
			, m_pVB( 0 )
			, m_pIB( 0 )
		{
		}

		~SDrawObjMesh()
		{
			Release();
		}

		void Release()
		{
			SAFE_RELEASE( m_pVB );
			SAFE_RELEASE( m_pIB );

			m_numVertices = 0;
			m_numFaces = 0;
		}

		uint32 m_numVertices;
		uint32 m_numFaces;
#if defined (DIRECT3D9) || defined(OPENGL)
		IDirect3DVertexBuffer9* m_pVB;
		IDirect3DIndexBuffer9* m_pIB;
#elif defined (DIRECT3D10)
    ID3D10Buffer* m_pVB;
    ID3D10Buffer* m_pIB;
#endif
	};

	enum EAuxObjNumLOD
	{
		e_auxObjNumLOD = 5
	};

	struct SMatrices
	{
		SMatrices()
		: m_pCurTransMat( 0 )
		{
			m_matTrans2D._11 =  2; m_matTrans2D._12 =  0; m_matTrans2D._13 = 0; m_matTrans2D._14 = 0;
			m_matTrans2D._21 =  0; m_matTrans2D._22 = -2; m_matTrans2D._23 = 0; m_matTrans2D._24 = 0;
			m_matTrans2D._31 =  0; m_matTrans2D._32 =  0; m_matTrans2D._33 = 0; m_matTrans2D._34 = 0;
			m_matTrans2D._41 = -1; m_matTrans2D._42 =  1; m_matTrans2D._43 = 0; m_matTrans2D._44 = 1;
		}

		void UpdateMatrices();

		D3DXMATRIX m_matView;
		D3DXMATRIX m_matViewInv;
		D3DXMATRIX m_matProj;
		D3DXMATRIX m_matTrans3D;
		D3DXMATRIX m_matTrans2D;
		const D3DXMATRIX* m_pCurTransMat;
	};

private:
	CD3DRenderAuxGeom( CD3D9Renderer& renderer );
#if defined (DIRECT3D9) || defined(OPENGL)
	void DetermineAuxPrimitveFlags( uint32& d3dNumPrimDivider, D3DPRIMITIVETYPE& d3dPrim, CRenderAuxGeom::EPrimType primType ) const;
#elif defined (DIRECT3D10)
  void DetermineAuxPrimitveFlags( uint32& d3dNumPrimDivider, D3D10_PRIMITIVE_TOPOLOGY& d3dPrim, CRenderAuxGeom::EPrimType primType ) const;
#endif	
	void DrawAuxPrimitives( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd, const EPrimType& primType );
	void DrawAuxIndexedPrimitives( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd, const EPrimType& primType );
	void DrawAuxObjects( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd );

	void PrepareThickLines2D( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd );
	void PrepareThickLines3D( AuxSortedPushBuffer::const_iterator itBegin, AuxSortedPushBuffer::const_iterator itEnd );

	void PrepareRendering();
	void SetShader( const SAuxGeomRenderFlags& renderFlags );
	void AdjustRenderStates( const SAuxGeomRenderFlags& renderFlags );	
#if defined (DIRECT3D9) || defined(OPENGL)
	void BindStreams( eVertexFormat newVertexFormat, IDirect3DVertexBuffer9* pNewVB, IDirect3DIndexBuffer9* pNewIB );
#elif defined (DIRECT3D10)
  void BindStreams( eVertexFormat newVertexFormat, ID3D10Buffer* pNewVB, ID3D10Buffer* pNewIB );
#endif

	template< typename TMeshFunc >
		HRESULT CreateMesh( SDrawObjMesh& mesh, TMeshFunc meshFunc );

	const D3DXMATRIX& GetCurrentView() const;
	const D3DXMATRIX& GetCurrentViewInv() const;
	const D3DXMATRIX& GetCurrentProj() const;
	const D3DXMATRIX& GetCurrentTrans3D() const;
	const D3DXMATRIX& GetCurrentTrans2D() const;

	bool IsOrthoMode() const;

private:
	CD3D9Renderer& m_renderer;

#if defined (DIRECT3D9) || defined(OPENGL)
	IDirect3DVertexBuffer9* m_pAuxGeomVB;
	IDirect3DIndexBuffer9* m_pAuxGeomIB;

	IDirect3DVertexBuffer9* m_pCurVB;
	IDirect3DIndexBuffer9* m_pCurIB;
#elif defined (DIRECT3D10)
  ID3D10Buffer* m_pAuxGeomVB;
  ID3D10Buffer* m_pAuxGeomIB;

  ID3D10Buffer* m_pCurVB;
  ID3D10Buffer* m_pCurIB;
#endif

	SStreamBufferManager m_auxGeomSBM;

	uint32 m_wndXRes;
	uint32 m_wndYRes;
	float m_aspect;
	float m_aspectInv;

	SMatrices m_matrices;

	EPrimType m_curPrimType;

	uint8 m_curPointSize;

	int m_transMatrixIdx;

	CShader* m_pAuxGeomShader;
	EAuxGeomPublicRenderflags_DrawInFrontMode m_curDrawInFrontMode;

	std::vector< D3DXMATRIX > m_colOrthoMatrices;

	SDrawObjMesh m_sphereObj[ e_auxObjNumLOD ];
	SDrawObjMesh m_coneObj[ e_auxObjNumLOD ];
	SDrawObjMesh m_cylinderObj[ e_auxObjNumLOD ];
};


inline
CD3DRenderAuxGeom::SStreamBufferManager::SStreamBufferManager()
: m_discardVB( true )
, m_curVBIndex( 0 )
, m_discardIB( true )
, m_curIBIndex( 0 )
{
}


inline void
CD3DRenderAuxGeom::SStreamBufferManager::Reset()
{
	m_discardVB = true;
	m_curVBIndex = 0;
	m_discardIB = true;
	m_curIBIndex = 0;
}


inline void 
CD3DRenderAuxGeom::SStreamBufferManager::DiscardVB()
{
	m_discardVB = true;
	m_curVBIndex = 0;
}


inline void 
CD3DRenderAuxGeom::SStreamBufferManager::DiscardIB()
{
	m_discardIB = true;
	m_curIBIndex = 0;
}


#if defined (DIRECT3D9) || defined(OPENGL)
inline void CD3DRenderAuxGeom::DetermineAuxPrimitveFlags( uint32& d3dNumPrimDivider, D3DPRIMITIVETYPE& d3dPrim, CRenderAuxGeom::EPrimType primType ) const
#elif defined (DIRECT3D10)
inline void CD3DRenderAuxGeom::DetermineAuxPrimitveFlags( uint32& d3dNumPrimDivider, D3D10_PRIMITIVE_TOPOLOGY& d3dPrim, CRenderAuxGeom::EPrimType primType ) const
#endif
{
#if defined (DIRECT3D9) || defined(OPENGL)
	switch( primType )
	{
	case CRenderAuxGeom::e_PtList:
		{
			d3dNumPrimDivider = 1;
			d3dPrim = D3DPT_POINTLIST;
			break;
		}
	case CRenderAuxGeom::e_LineList:
		{
			d3dNumPrimDivider = 2;
			d3dPrim = D3DPT_LINELIST;
			break;
		}
	case CRenderAuxGeom::e_LineListInd:
		{
			d3dNumPrimDivider = 2;
			d3dPrim = D3DPT_LINELIST;
			break;
		}
	case CRenderAuxGeom::e_TriList:
		{
			d3dNumPrimDivider = 3;
			d3dPrim = D3DPT_TRIANGLELIST;
			break;
		}
	case CRenderAuxGeom::e_TriListInd:
	default:
		{
			d3dNumPrimDivider = 3;
			d3dPrim = D3DPT_TRIANGLELIST;
			break;
		}
	}
#elif defined (DIRECT3D10)
  switch( primType )
  {
  case CRenderAuxGeom::e_PtList:
    {
      d3dNumPrimDivider = 1;
      d3dPrim = D3D10_PRIMITIVE_TOPOLOGY_POINTLIST;
      break;
    }
  case CRenderAuxGeom::e_LineList:
    {
      d3dNumPrimDivider = 2;
      d3dPrim = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
      break;
    }
  case CRenderAuxGeom::e_LineListInd:
    {
      d3dNumPrimDivider = 2;
      d3dPrim = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
      break;
    }
  case CRenderAuxGeom::e_TriList:
    {
      d3dNumPrimDivider = 3;
      d3dPrim = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;
    }
  case CRenderAuxGeom::e_TriListInd:
  default:
    {
      d3dNumPrimDivider = 3;
      d3dPrim = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;
    }
  }
#endif
}


#endif D3D_RENDER_AUX_GEOM_H
