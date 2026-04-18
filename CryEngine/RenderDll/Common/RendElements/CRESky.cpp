#include "StdAfx.h"
#include "RendElement.h"
#include "CRESky.h"
#include "Stars.h"
#include "I3DEngine.h"


void CRESky::mfPrepare()
{
  gRenDev->EF_CheckOverflow(0, 0, this);

  gRenDev->m_RP.m_pRE = this;
  gRenDev->m_RP.m_RendNumIndices = 0;
  gRenDev->m_RP.m_RendNumVerts = 0;
}

float CRESky::mfDistanceToCameraSquared(Matrix34& matInst)
{
  return 999999.0f;
}

bool CRESky::DrawFogLayer()
{
  return true;
}

CRESky::~CRESky()
{
  delete m_parrFogLayer;
  delete m_parrFogLayer2;
}

// render black occlusion volumes mostly to hide seams in indoors
bool CRESky::DrawBlackPortal()
{
  if(!m_arrvPortalVerts[0][0].xyz.x)
    return true;

  gRenDev->ResetToDefault();

  for(int i=0; i<MAX_SKY_OCCLAREAS_NUM; i++)
  {
    if(!m_arrvPortalVerts[i][0].xyz.x)
      return true;

    gRenDev->EF_SetState(GS_DEPTHWRITE);
    gRenDev->SetCullMode(R_CULL_NONE);
    gRenDev->SelectTMU(0);
    gRenDev->EnableTMU(false);
		CVertexBuffer pVertexBuffer(m_arrvPortalVerts[i],VERTEX_FORMAT_P3F_COL4UB);
    gRenDev->DrawTriStrip(&pVertexBuffer,4);
  }
  return true;
}

void CRESky::DrawSkySphere(float fHeight)
{
  float nWSize = 256/16;  

  float a_in  = 1, a_out = 1;

  struct_VERTEX_FORMAT_P3F_COL4UB vert;
  vert.color.bcolor[0]=255;
  vert.color.bcolor[1]=255;
  vert.color.bcolor[2]=255;

  PodArray<struct_VERTEX_FORMAT_P3F_COL4UB> lstVertData;

  for(float r=0; r<3; r++)
  {
    a_in = a_out;
    a_out = 1.f-r/2;
    a_out*=a_out; a_out*=a_out; a_out*=a_out;

    lstVertData.Clear();

    for(int i=0; i<=360; i+=40)
    {
      float rad = (i) * ((float)M_PI/180.0f);

      vert.xyz.x = cry_sinf(rad)*nWSize*r;
      vert.xyz.y = cry_cosf(rad)*nWSize*r;
      vert.xyz.z = fHeight + 8 - (r)*8;
      vert.color.bcolor[3] = uchar(a_in*255.0f);
      lstVertData.Add(vert);

      vert.xyz.x = cry_sinf(rad)*nWSize*(r+1);
      vert.xyz.y = cry_cosf(rad)*nWSize*(r+1);
      vert.xyz.z = fHeight + 8 - (r+1)*8;
      vert.color.bcolor[3] = uchar(a_out*255.0f);
      lstVertData.Add(vert);
    }
		CVertexBuffer pVertexBuffer(&lstVertData[0],VERTEX_FORMAT_P3F_COL4UB);
    gRenDev->DrawTriStrip(&pVertexBuffer,lstVertData.Count());
  }
}


//////////////////////////////////////////////////////////////////////////
// HDR Sky
//////////////////////////////////////////////////////////////////////////


void CREHDRSky::mfPrepare()
{
	gRenDev->EF_CheckOverflow( 0, 0, this );
	gRenDev->m_RP.m_pRE = this;
	gRenDev->m_RP.m_RendNumIndices = 0;
	gRenDev->m_RP.m_RendNumVerts = 0;

	if (!m_pStars)
		m_pStars = new CStars;
}


float CREHDRSky::mfDistanceToCameraSquared( Matrix34& matInst )
{
	return 999999.0f;
}


CREHDRSky::~CREHDRSky()
{
	SAFE_DELETE(m_pStars);
	SAFE_DELETE_ARRAY(m_pTmpStorage);
}


//////////////////////////////////////////////////////////////////////////
// Stars
//////////////////////////////////////////////////////////////////////////


CStars::CStars()
: m_numStars(0)
//, m_pStarVB(0)
, m_pStarMesh(0)
, m_pShader(0)
, m_shaderTech()
, m_vspnStarSize()
, m_pspnStarIntensity()
{
	if (LoadData())
	{
		m_pShader = gRenDev->m_cEF.mfForName("Stars", EF_SYSTEM);
		m_shaderTech = "Stars";
		m_vspnStarSize = "StarSize";
		m_pspnStarIntensity = "StarIntensity";		
	}
}


CStars::~CStars()
{
	//if (m_pStarVB)
	//{
	//	gRenDev->ReleaseBuffer(m_pStarVB);
	//	m_pStarVB = 0;
	//}
	SAFE_RELEASE(m_pStarMesh);
	SAFE_RELEASE(m_pShader);
}


bool CStars::LoadData()
{
	const uint32 c_fileTag(0x52415453);				// "STAR"
	const uint32 c_fileVersion(0x00010001);
	const char c_fileName[] = "Libs/Sky/stars.dat";

	ICryPak* pPak(gEnv->pCryPak);
	if (pPak)
	{
		FILE* f(pPak->FOpen(c_fileName, "rb"));
		if (f)
		{
			// read and validate header
			size_t itemsRead(0);
			uint32 fileTag(0);
			itemsRead = pPak->FRead(&fileTag, 1, f);
			if (itemsRead != 1 || fileTag != c_fileTag)
			{
				pPak->FClose(f);
				return false;
			}

			uint32 fileVersion(0);
			itemsRead = pPak->FRead(&fileVersion, 1, f);
			if (itemsRead != 1 || fileVersion != c_fileVersion)
			{
				pPak->FClose(f);
				return false;
			}

			// read in stars
			pPak->FRead(&m_numStars, 1, f);
			
			struct_VERTEX_FORMAT_P3F_COL4UB* pData(new struct_VERTEX_FORMAT_P3F_COL4UB[m_numStars]);
			
			for (unsigned int i(0); i<m_numStars; ++i)
			{
				float ra(0);
				pPak->FRead(&ra, 1, f);

				float dec(0);
				pPak->FRead(&dec, 1, f);

				uint8 r(0);
				pPak->FRead(&r, 1, f);
				
				uint8 g(0);
				pPak->FRead(&g, 1, f);
				
				uint8 b(0);
				pPak->FRead(&b, 1, f);

				uint8 mag(0);
				pPak->FRead(&mag, 1, f);

				pData[i].xyz.x = -cosf(DEG2RAD(dec)) * sinf(DEG2RAD(ra * 15.0f));
				pData[i].xyz.y =  cosf(DEG2RAD(dec)) * cosf(DEG2RAD(ra * 15.0f));
				pData[i].xyz.z =  sinf(DEG2RAD(dec));

#if defined(DIRECT3D10)
				pData[i].color.dcolor = (mag << 24) + (b << 16) + (g << 8) + r;
#else
				pData[i].color.dcolor = (mag << 24) + (r << 16) + (g << 8) + b;
#endif
			}

			m_pStarMesh = gRenDev->CreateRenderMeshInitialized(pData, m_numStars, VERTEX_FORMAT_P3F_COL4UB, 0, 0, R_PRIMV_TRIANGLES, "Stars", "Stars");

			//m_pStarVB = gRenDev->CreateBuffer(m_numStars, VERTEX_FORMAT_P3F_COL4UB, "Stars", false);
			//if (m_pStarVB)
			//	gRenDev->UpdateBuffer(m_pStarVB, pData, m_numStars, true);

			delete [] pData;

			// check if we read entire file
			long curPos(pPak->FTell(f));
			pPak->FSeek(f, 0, SEEK_END);
			long endPos(pPak->FTell(f));
			if (curPos != endPos)
			{
				pPak->FClose(f);
				return false;
			}

			pPak->FClose(f);
			return true;
		}
	}

	return false;
}