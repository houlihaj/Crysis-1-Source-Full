#ifndef CWaterVolumeManager_H
#define CWaterVolumeManager_H

/*
class CWaterVolume : public IWaterVolume, public Cry3DEngineBase
{
public:
	CWaterVolume(IRenderer * pRenderer) 
  { 
    m_pRenderer = pRenderer; m_pRenderMesh = 0; 
    m_fFlowSpeed = 0; 
		m_bAffectToVolFog = false;
		m_fTriMaxSize = m_fTriMinSize = 8.f; 
		m_fPrevTriMaxSize = m_fPrevTriMinSize = 0; 
		m_fHeight = 0; m_vCurrentCamPos.Set(0,0,0); m_nLastRndFrame=0; 
		m_szName[0] = 0;
		m_vWaterLevelOffset.Set(0,0,0);
		m_pPhysArea = 0;
  }
	void UpdatePoints(const Vec3 * pPoints, int nCount, float fHeight);
	void SetFlowSpeed(float fSpeed) { m_fFlowSpeed = fSpeed; }
	void SetAffectToVolFog(bool bAffectToVolFog) { m_bAffectToVolFog = bAffectToVolFog; }
	void SetTriSizeLimits(float fTriMinSize, float fTriMaxSize);
	void SetMaterial(const char * szShaderName);
	void SetMaterial( IMaterial *pMat );
	void SetName(const char * szName) 
	{ 
		strncpy(m_szName,szName,64); 
	}
	virtual const char* GetName() const { return m_szName; };
	void SetPositionOffset(const Vec3 & vNewOffset);
  IMaterial * GetMaterial();
	void UpdateVisArea();
  void CheckForUpdate(bool bMakeLowestLod);
	bool IsWaterVolumeAreasVisible();
	void TesselateStrip(PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & lstVerts, PodArray<ushort> & lstIndices, PodArray<Vec3> & lstDirections);
	bool TesselateFace(PodArray<struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F> & lstVerts, PodArray<ushort> & lstIndices, int nFacePos, PodArray<Vec3> & lstDirections);
	void UpdateVisAreaFogVolumeLevel(CVisArea*pVisArea);

	PodArray<Vec3> m_lstPoints;
	Vec3 m_vWaterLevelOffset;
	PodArray<Vec3> m_lstDirections;
	IRenderMesh * m_pRenderMesh;
	Vec3 m_vBoxMin, m_vBoxMax;
	IRenderer * m_pRenderer;
//	IMaterial * m_pMat;
	char m_szName[64];
	float m_fHeight;
	float m_fTriMinSize, m_fTriMaxSize;
	float m_fPrevTriMaxSize, m_fPrevTriMinSize; 

	_smart_ptr<IMaterial> m_pMaterial;

	float m_fFlowSpeed;
	bool m_bAffectToVolFog;
	PodArray<struct IVisArea *> m_lstVisAreas;
  Vec3 m_vCurrentCamPos;
  int m_nLastRndFrame;

	IPhysicalEntity *m_pPhysArea;
};

struct CWaterVolumeManager : public Cry3DEngineBase
{
	CWaterVolumeManager( );
	~CWaterVolumeManager();
	void LoadWaterVolumesFromXML(XmlNodeRef pDoc);

	PodArray<CWaterVolume*> m_lstWaterVolumes;

	void InitWaterVolumes();
	void RenderWaterVolumes(bool bOutdoorVisible);
	float GetWaterVolumeLevelFor2DPoint(const Vec3 & vPos, Vec3 * pvFlowDir);

	IWaterVolume * CreateWaterVolume();
	void DeleteWaterVolume(IWaterVolume * pWaterVolume);
	void UpdateWaterVolumeVisAreas();

	IWaterVolume * FindWaterVolumeByName(const char * szName);

private:
	// Local shader param.
	TArray<SShaderParam> m_shaderParams;
};
*/
#endif // CWaterVolumeManager_H
