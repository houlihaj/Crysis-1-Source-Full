////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CloudGroup.cpp
//  Version:     v1.00
//  Created:     06/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: CCloudGroup implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CloudGroup.h"

#include "CloudObject.h"
#include "Cloud\CloudGroupPanel.h"
#include "Material\Material.h"

#include <I3DEngine.h>
#include <IEntityRenderState.h>


class CCloudSprite
{
public:
	CCloudSprite (float _radius, Vec3 _pos, int _texID, float _angle, Vec3 _boxPos, CBaseObject * _pObj = 0)
	{
		radius = _radius;
		//realRaduis = _radius;
		pos = _pos;
		texID = _texID;
		angle = _angle;
		boxPos = _boxPos;
		pObj = _pObj;

		//randRaduis = RandomInterval( -1.0f, 1.0f);
		//radius = realRaduis + * randRaduis
	}

	Vec3 pos;
	float radius;
	int texID;
	float angle;

	float realRaduis;
	float randRaduis;

	Vec3 boxPos;
	CBaseObject * pObj;
};





/////////////////////////////////////////////////////////////////////////
// CCloudGroup implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CCloudGroup,CGroup)


namespace
{
	CCloudGroupPanel* s_cloudGroupPanel = NULL;
	int s_cloudGroupId = 0;


	float RandomInterval(float min, float max)
	{
		return (float(rand())/RAND_MAX) * (max-min) + min;
	}

	int RandomInterval(int min, int max)
	{
		return int((float(rand())/RAND_MAX) * (max-min) + min + 0.5f);
	}

	float GetDistance2(Vec3 v1, Vec3 v2)
	{
		return (v2.x-v1.x)*(v2.x-v1.x) + (v2.y-v1.y)*(v2.y-v1.y) + (v2.z-v1.z)*(v2.z-v1.z);
	}
}


//////////////////////////////////////////////////////////////////////////
CCloudGroup::CCloudGroup()
{
	pObj = 0;
	SI.m_pShader = 0;

	ppCloudSprites = 0;
	nNumCloudSprites = 0;

	m_pCloudRenderNode = 0;

	mv_numRows = 4;
	mv_numCols = 4;

	mv_spriteRow = 0;
	mv_numSprites = 20;
	mv_sizeSprites = 1;
	mv_cullDistance = 0;
	mv_randomSizeValue = 0.5f;
	m_angleVariations = 0.0f;
	mv_spritePerBox = false;
	mv_density = 1;

	mv_bShowSpheres = false;
	mv_bPreviewCloud = false;
	mv_bAutoUpdate = false;
	//mv_isRegeneratePos = true;


	AddVariable( varTextureArray,"Texture" );
	AddVariable( varTextureArray,mv_numRows,"NumRows", "Number of Rows",functor(*this,&CCloudGroup::OnParamChange) );
	mv_numRows.SetLimits(1, 64);
	AddVariable( varTextureArray,mv_numCols,"NumCols", "Number of Columns",functor(*this,&CCloudGroup::OnParamChange) );
	mv_numCols.SetLimits(1, 64);

	AddVariable( varCloudArray,"Cloud" );
	AddVariable( varCloudArray,mv_spriteRow,"SpriteRow","Sprite Row",functor(*this,&CCloudGroup::OnParamChange) );
	mv_spriteRow.SetLimits(0, 63);
	AddVariable( varCloudArray,mv_numSprites,"NumberofSprites","Number of Sprites",functor(*this,&CCloudGroup::OnParamChange) );
	mv_numSprites.SetLimits(0, 99999);
	AddVariable( varCloudArray,mv_sizeSprites,"SizeofSprites", "Size of Sprites",functor(*this,&CCloudGroup::OnParamChange));
	mv_sizeSprites.SetLimits(0, 99999);
	AddVariable( varCloudArray,mv_randomSizeValue,"SizeVariation","Size Variation",functor(*this,&CCloudGroup::OnParamChange) );
	mv_randomSizeValue.SetLimits(0, 99999);
	AddVariable( varCloudArray,m_angleVariations,"AngleVariations","Angle Variations",functor(*this,&CCloudGroup::OnParamChange), IVariable::DT_ANGLE );
	m_angleVariations.SetLimits(0, 99999);
	AddVariable( varCloudArray,mv_cullDistance,"MinimalDistance","Minimal Distance between Sprites",functor(*this,&CCloudGroup::OnParamChange) );
	mv_cullDistance.SetLimits(0, 99999);
	AddVariable( varCloudArray,mv_spritePerBox,"SpritePerBox","Every Box has Sprites",functor(*this,&CCloudGroup::OnParamChange) );
	AddVariable( varCloudArray,mv_density,"Density","Density",functor(*this,&CCloudGroup::OnParamChange) );
	mv_density.SetLimits(0, 1);

	AddVariable( varVisArray,"Visualisation" );
	//AddVariable( varVisArray,mv_isRegeneratePos,"RegeneratePosition","Regenerate position",functor(*this,&CCloudGroup::OnParamChange) );
	AddVariable( varVisArray,mv_bShowSpheres,"ShowSpheres","Show Particles like Spheres",functor(*this,&CCloudGroup::OnParamChange) );
	AddVariable( varVisArray,mv_bPreviewCloud,"Preview","Preview Cloud",functor(*this,&CCloudGroup::OnPreviewChange) );
	AddVariable( varVisArray,mv_bAutoUpdate,"AutoUpdate","Auto Update",functor(*this,&CCloudGroup::OnParamChange) );
	UpdateSpritesBox();
}

//////////////////////////////////////////////////////////////////////////
CCloudGroup::~CCloudGroup()
{
	if (m_pCloudRenderNode)
		m_pCloudRenderNode->ReleaseNode();

	SAFE_RELEASE(SI.m_pShader);
	SAFE_RELEASE(SI.m_pShaderResources);
	ReleaseCloudSprites();
}

//////////////////////////////////////////////////////////////////////////
bool CCloudGroup::CreateGameObject()
{
	m_pCloudRenderNode = (ICloudRenderNode*)GetIEditor()->Get3DEngine()->CreateRenderNode( eERType_Cloud );
	if(m_pCloudRenderNode)
		m_pCloudRenderNode->SetMatrix( GetWorldTM() );
	return true;
};

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::AddSprite(CCloudSprite * pSprite)
{
	if(!(nNumCloudSprites % 64))
	{
		typedef CCloudSprite * tpCCloudSprite;
		CCloudSprite ** ppNew = new tpCCloudSprite[nNumCloudSprites + 64];
		for(int i = 0; i<nNumCloudSprites; i++)
			ppNew[i] = ppCloudSprites[i];
		if(ppCloudSprites)
			delete[] ppCloudSprites;
		ppCloudSprites = ppNew;
	}

	ppCloudSprites[nNumCloudSprites] = pSprite;
	nNumCloudSprites++;
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::ReleaseCloudSprites()
{
	for(int i = 0; i<nNumCloudSprites; i++)
		if(ppCloudSprites[i])
			delete ppCloudSprites[i];
	if(ppCloudSprites)
		delete[] ppCloudSprites;
	ppCloudSprites = 0;
	nNumCloudSprites = 0;
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::OnParamChange(IVariable *pVar)
{
	if(mv_bAutoUpdate)
		Generate();
}


//////////////////////////////////////////////////////////////////////////
void CCloudGroup::OnChildModified()
{
	if(!nNumCloudSprites)
		return;

	bool bUpdated = false;

	for (int i=0; i<GetChildCount(); i++)
	{
		CBaseObject * obj = GetChild(i);
		for(int j= 0; j<nNumCloudSprites; j++)
		{
			if(ppCloudSprites[j]->pObj && ppCloudSprites[j]->pObj==obj && ppCloudSprites[j]->boxPos!=obj->GetPos())
			{
				ppCloudSprites[j]->pos += obj->GetPos() - ppCloudSprites[j]->boxPos;
				ppCloudSprites[j]->boxPos = obj->GetPos();
				bUpdated = true;
			}
		}
	}

	if(bUpdated)
		UpdateSpritesBox();
}


//////////////////////////////////////////////////////////////////////////
void CCloudGroup::OnPreviewChange(IVariable *pVar)
{
	if (m_pCloudRenderNode)
	{
		if (mv_bPreviewCloud == true && !IsHidden())
		{
			GetIEditor()->Get3DEngine()->RegisterEntity(m_pCloudRenderNode);
		}
		else
		{
			GetIEditor()->Get3DEngine()->UnRegisterEntity(m_pCloudRenderNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::UpdateRenderObject()
{
	SAFE_RELEASE(SI.m_pShader);
	SAFE_RELEASE(SI.m_pShaderResources);

	pObj = GetIEditor()->GetRenderer()->EF_GetObject();
	SI = GetIEditor()->GetRenderer()->EF_LoadShaderItem("Clouds.CryCloudEditor", true);
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::Display( DisplayContext &dc )
{
	if(nNumCloudSprites)
	{
		if(mv_bShowSpheres)
		{
			dc.PushMatrix( GetWorldTM() );
			for (int i=0; i<nNumCloudSprites; ++i)
			{
				CCloudSprite * pSp = ppCloudSprites[i];
				if(pSp)
				{
					dc.SetColor(0.4f, 0.4f, 0.8f, 0.4f);
					dc.DrawBall(pSp->pos, pSp->radius);
				}
			}
			dc.PopMatrix();
		}
	}

	if(pObj && SI.m_pShader)
	{
		TArray<CRendElement*> *pRE = SI.m_pShader->GetREs(SI.m_nTechnique);
		if (pRE)
		{
			pObj->m_II.m_Matrix = Matrix34::CreateTranslationMat(Vec3(1,1,1));
			pObj->m_ObjFlags |= FOB_TRANS_MASK;
			float fRadius = 50.0f;
			pObj->m_fTempVars[0] = fRadius;

			GetIEditor()->GetRenderer()->EF_AddEf(pRE->Get(0), SI, pObj, EFSLIST_TRANSP, 1);
		}
	}
	CGroup::Display( dc );
}


//////////////////////////////////////////////////////////////////////////
void CCloudGroup::BeginEditParams( IEditor *ie,int flags )
{
	CGroup::BeginEditParams( ie,flags );

	if (!s_cloudGroupPanel)
	{
		s_cloudGroupPanel = new CCloudGroupPanel;
		s_cloudGroupPanel->Init(this);
		s_cloudGroupId = ie->AddRollUpPage( ROLLUP_OBJECTS,_T("Cloud Parameters"),s_cloudGroupPanel );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::EndEditParams( IEditor *ie )
{
	CGroup::EndEditParams( ie );

	if (s_cloudGroupPanel)
	{
		ie->RemoveRollUpPage( ROLLUP_OBJECTS,s_cloudGroupId );
		s_cloudGroupPanel = 0;
		s_cloudGroupId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::InvalidateTM( int nWhyFlags )
{
	__super::InvalidateTM( nWhyFlags );
	
	if (m_pCloudRenderNode)
	{
		m_pCloudRenderNode->SetMatrix( GetWorldTM() );
		if (!IsHidden())
			GetIEditor()->Get3DEngine()->RegisterEntity(m_pCloudRenderNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::UpdateVisibility( bool visible )
{
	if (m_pCloudRenderNode)
	{
		if (visible && mv_bPreviewCloud == true)
		{
			GetIEditor()->Get3DEngine()->RegisterEntity(m_pCloudRenderNode);
		}
		else
		{
			GetIEditor()->Get3DEngine()->UnRegisterEntity(m_pCloudRenderNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::UpdateSpritesBox()
{
	m_SpritesBox.Reset();

	if(nNumCloudSprites==0)
		m_SpritesBox.Add(Vec3(0,0,0));
	else
		for(int i = 0; i<nNumCloudSprites; i++)
			m_SpritesBox.Add(ppCloudSprites[i]->pos);

	if (m_pCloudRenderNode)
	{
		XmlNodeRef xml = GenerateXml();
		m_pCloudRenderNode->LoadCloudFromXml( xml );
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CCloudGroup::GenerateXml()
{
	XmlNodeRef xml = CreateXmlNode("Cloud");

	xml->setAttr("TextureNumRows", mv_numRows);
	xml->setAttr("TextureNumCols", mv_numCols);
	xml->setAttr("Density", mv_density);

	if (GetMaterial())
	{
		xml->setAttr( "Material",GetMaterial()->GetName() );
	}

	for (int i=0; i<nNumCloudSprites; ++i)
	{
		CCloudSprite * pSp = ppCloudSprites[i];
		if(pSp)
		{
			XmlNodeRef child = xml->createNode("Sprite");
			child->setAttr("Pos", pSp->pos);
			child->setAttr("texID", pSp->texID);
			child->setAttr("Radius", pSp->radius);
			if(pSp->angle>=0.0001f || pSp->angle<=-0.0001f)
				child->setAttr("Angle", pSp->angle);
			xml->addChild(child);
		}
	}
	return xml;
}



//////////////////////////////////////////////////////////////////////////
void CCloudGroup::Generate()
{
	ReleaseCloudSprites();

	m_boxTotArea = 0;

	for (int i=0; i<GetChildCount(); i++)
	{
		CBaseObject * obj = GetChild(i);
		BBox box;
		obj->GetBoundBox(box);
		m_boxTotArea += ((box.max.x - box.min.x) * (box.max.y - box.min.y) * (box.max.z - box.min.z));
	}

	for (int i=0; i<GetChildCount(); i++)
		FillBox(GetChild(i));


	if(mv_cullDistance> 0.0001f)
	{
		for(int i=0; i<nNumCloudSprites; i++)
			for(int j=i+1; j<nNumCloudSprites; j++)
				if(ppCloudSprites[i] && ppCloudSprites[j])
					if (GetDistance2(ppCloudSprites[i]->pos, ppCloudSprites[j]->pos) < mv_cullDistance * mv_cullDistance)
					{
						delete ppCloudSprites[j];
						ppCloudSprites[j] = 0;
					}

					int tmpNumCloudSprites = nNumCloudSprites;
					nNumCloudSprites = 0;
					for(int i=0; i<tmpNumCloudSprites; i++)
						if(ppCloudSprites[i])
						{
							ppCloudSprites[nNumCloudSprites] = ppCloudSprites[i];
							nNumCloudSprites++;
						}
	}

	UpdateSpritesBox();
}


//////////////////////////////////////////////////////////////////////////
void CCloudGroup::Export()
{
	if(nNumCloudSprites<=0)
	{
		MessageBox(GetIEditor()->GetEditorMainWnd(), "Need to generate the cloud first.","Sandbox error",MB_OK | MB_ICONERROR);
		return;
	}

	if(strlen(m_exportFile)==0)
	{
		BrowseFile();
	}

	if(strlen(m_exportFile)==0)
	{
		MessageBox(GetIEditor()->GetEditorMainWnd(), "Need to specify export file name.","Sandbox error",MB_OK | MB_ICONERROR);
		return;
	}

	/*
	string name = "Libs/Clouds/";
	name += GetName();
	name += ".xml";
	CString path = Path::GamePathToFullPath(name.c_str());
	*/

	XmlNodeRef xml = GenerateXml();

	if(CFileUtil::OverwriteFile(m_exportFile))
	{
		SaveXmlNode( xml,m_exportFile );
	}
	else
	{
		MessageBox(GetIEditor()->GetEditorMainWnd(), "Can't overwrite file.","Sandbox error", MB_OK | MB_ICONERROR);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::FillBox(CBaseObject * pObj)
{
	BBox box;
	pObj->GetBoundBox(box);

	box.SetTransformedAABB( GetWorldTM().GetInverted(),box );

	float boxVolume = (box.max.x - box.min.x) * (box.max.y - box.min.y) * (box.max.z - box.min.z);
	int numSprites = int ((boxVolume / m_boxTotArea) * mv_numSprites);

	if (mv_spritePerBox)
	{
		if (numSprites == 0)
			numSprites = 1;
	}

	for (int sprite_list = 1;  sprite_list<=numSprites; sprite_list++)
	{
		//Find the bounds of the box to fill
		float Xval = RandomInterval(box.min.x, box.max.x);
		float Yval = RandomInterval(box.min.y, box.max.y);
		float Zval = RandomInterval(box.min.z, box.max.z);

		int texId = RandomInterval(0, mv_numCols-1);
		int spriteRow = -1;

		float sizeSrites = mv_sizeSprites;
		float ranSize = RandomInterval( -mv_randomSizeValue, mv_randomSizeValue);
		float angleVar = m_angleVariations;

		if(pObj->GetRuntimeClass() == RUNTIME_CLASS(CCloudObject))
		{
			CCloudObject * pCObj = (CCloudObject * ) pObj;
			if(pCObj->mv_sizeSprites > 0)
				sizeSrites = pCObj->mv_sizeSprites;
			if(pCObj->mv_randomSizeValue>0)
				ranSize = RandomInterval( - pCObj->mv_randomSizeValue, pCObj->mv_randomSizeValue);

			spriteRow = pCObj->mv_spriteRow;
			float angVar = pCObj->m_angleVariations;
			if(angVar>=0.0001f || angVar<=-0.0001f)
			{
				angleVar = angVar;
			}
		}

		float ranSpriteSize = sizeSrites + ranSize;

		angleVar = fabs(angleVar);

		if(spriteRow==-1)
			spriteRow = mv_spriteRow;

		if(spriteRow>=0)
			texId += spriteRow * mv_numCols;

		float angle = RandomInterval( - angleVar, angleVar);

		AddSprite(new CCloudSprite (ranSpriteSize/2, Vec3(Xval,Yval,Zval), texId, angle, pObj->GetPos(), pObj));
	}
}

//////////////////////////////////////////////////////////////////////////
void CCloudGroup::Serialize( CObjectArchive &ar )
{
	CGroup::Serialize(ar);
	if(!ar.bUndo)
	{
		if(ar.bLoading)
		{
			ar.node->getAttr("ExportFile", m_exportFile);

			ReleaseCloudSprites();
			XmlNodeRef xml = GetIEditor()->GetSystem()->LoadXmlFile(m_exportFile);

			if(xml)
			{
				for(int i = 0; i<xml->getChildCount(); i++)
				{
					Vec3 pos;
					int texID;
					float angle;
					float radius;
					Vec3 boxPos;
					XmlNodeRef child = xml->getChild(i);
					child->getAttr("Pos", pos);
					child->getAttr("texID", texID);
					child->getAttr("Radius", radius);
					child->getAttr("boxPos", boxPos);
					if(!child->getAttr("Angle", angle))
						angle = 0;

					AddSprite(new CCloudSprite (radius, pos, texID, angle, boxPos));
				}
			}
			UpdateSpritesBox();
		}
		else
		{
			ar.node->setAttr("ExportFile", m_exportFile);
		}
	}
}



//////////////////////////////////////////////////////////////////////////
void CCloudGroup::BrowseFile()
{
	CString fileName = GetName() + ".xml";
	char path[MAX_PATH] = "";
	
	if(strlen(m_exportFile)>0)
	{
		strcpy(path, m_exportFile);
		char * ch1 = strrchr(path, '/');
		char * ch2 = strrchr(path, '\\');
		if(ch2>ch1)
			ch1 = ch2;
		if(ch1)
		{
			fileName = ch1+1;
			*(ch1+1) = 0;
		}
		else
			*path = 0;
	}

	if(!(*path))
		strcpy(path, Path::GamePathToFullPath("Libs/Clouds/"));

	if(CFileUtil::SelectSaveFile( "Clouds (*.xml)|*.xml||","xml",path, fileName ))
	{
		m_exportFile = fileName;
		s_cloudGroupPanel->SetFileBrowseText();
	}
}

const char * CCloudGroup::GetExportFileName()
{
	return m_exportFile;
}

void CCloudGroup::Import()
{
	CString fileName;
	if(CFileUtil::SelectFile("MS FS (*.cld)|*.cld||", "", fileName))
	{
		//ImportExportFolder(fileName);
		ImportFS(fileName);

		/*
		// Export whole directory.
		CString dir = Path::GetPath(fileName);
		CString fname = Path::GetFileName(fileName);
		CFileUtil::FileArray files;
		CFileUtil::ScanDirectory( dir,"*.cld",files );
		for (int i = 0; i < files.size(); i++)
		{
			fname = Path::GetFileName(files[i].filename);
			ImportFS( Path::Make(dir,fname,"cld") );
			m_exportFile = Path::Make(dir,fname,"xml");
			Export();
		}
		*/
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCloudGroup::ImportExportFolder(const CString & fileName)
{
	char path[MAX_PATH];
	strcpy(path, fileName);
	char * ch1 = strrchr(path, '\\');
	char * ch2 = strrchr(path, '/');
	if(ch2>ch1)
		ch1 = ch2;
	if(ch1)
	{
		char folder[MAX_PATH];
		*(ch1+1) = 0;
		strcpy(folder, path);
		strcpy(ch1, "\\*.cld");
		WIN32_FIND_DATA fd;
		HANDLE h = FindFirstFile( path, &fd);
		if(h)
		{
			bool b = true;
			while(b)
			{
				strcpy(path, fd.cFileName);
				char * ch1 = strrchr(path, '.');
				if(ch1)
				{
					if(!stricmp(ch1, ".cld"))
					{
						CString file = folder;
						file+=fd.cFileName;
						ImportFS(file);
						strcpy(ch1, ".xml");
						m_exportFile = folder;
						m_exportFile += path;
						Export();
					}
				}
				b = FindNextFile(h, &fd);
			}
			FindClose(h);
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////
bool CCloudGroup::ImportFS(const CString & name)
{
	ReleaseCloudSprites();

	mv_numRows = 4;
	mv_numCols = 4;

	ISystem * iSystem = GetIEditor()->GetSystem();
	//return false;
  uint i;
  FILE *fp = iSystem->GetIPak()->FOpen(name, "rb");
  if (!fp)
    return false;

  uint iNumParticles;
  Vec3 vCenter = Vec3(0,0,0);

  iSystem->GetIPak()->FRead(&iNumParticles, 1, fp);
  if (iNumParticles == 0x238c)
  {
		SMinMaxBox cloudBox;
    iSystem->GetIPak()->FRead(&iNumParticles, 1, fp);
		char fTexName[128];
		//char texName[256];
		int n = 0;
    int ch;
    do
    {
      ch = iSystem->GetIPak()->Getc(fp);
      fTexName[n++] = ch;
      if (n > 128)
      {
        fTexName[127] = ch;
        break;
      }
    } while (ch != 0);
		/*
		StripExtension(fTexName, fTexName);
    sprintf(texName, "Textures/Clouds/%s.dds", fTexName);
    m_pTexParticle = CTexture::ForName(texName, 0, eTF_Unknown);
		*/
		uint m_nNumColorGradients;
    iSystem->GetIPak()->FRead(&m_nNumColorGradients, 1, fp);
    //m_pColorGradients = new SColorLevel [m_nNumColorGradients];
    for (i=0; i<m_nNumColorGradients; i++)
    {
      float fLevel;
      iSystem->GetIPak()->FRead(&fLevel, 1, fp);
      //m_pColorGradients[i].m_fLevel /= 100.0f;
      uint iColor;
      iSystem->GetIPak()->FRead(&iColor, 1, fp);
      //m_pColorGradients[i].m_vColor = ColorF(iColor);
    }
	
    for (i=0; i<iNumParticles; ++i)
    {
      Vec3 vPosition;
      short nShadingNum;
      short nGroupNum;
      short nWidthMin, nWidthMax;
      short nLengthMin, nLengthMax;
      short nRotMin, nRotMax;
      Vec2 vUV[2];
      iSystem->GetIPak()->FRead(&vPosition, 1, fp);
      iSystem->GetIPak()->FRead(&nShadingNum, 1, fp);
      iSystem->GetIPak()->FRead(&nGroupNum, 1, fp);
      iSystem->GetIPak()->FRead(&nWidthMin, 1, fp);
      iSystem->GetIPak()->FRead(&nWidthMax, 1, fp);
      iSystem->GetIPak()->FRead(&nLengthMin, 1, fp);
      iSystem->GetIPak()->FRead(&nLengthMax, 1, fp);
      iSystem->GetIPak()->FRead(&nRotMin, 1, fp);
      iSystem->GetIPak()->FRead(&nRotMax, 1, fp);
      iSystem->GetIPak()->FRead(&vUV[0], 1, fp);
      iSystem->GetIPak()->FRead(&vUV[1], 1, fp);

      vPosition *= 0.001f;
      float fWidth = (float)nWidthMin * 0.001f;
      float fHeight = (float)nLengthMin * 0.001f;
      float fRotMin = (float)nRotMin;
      float fRotMax = (float)nRotMax;
      Exchange(vPosition.y, vPosition.z);

      vUV[0].y = 1.0f - vUV[0].y;
      vUV[1].y = 1.0f - vUV[1].y;
      Exchange(vUV[0].x, vUV[1].x);

			int cx = 0;
			int cy = 0;

			if(0.24f <= vUV[1].x && vUV[1].x <=0.26f)
				cx=1;
			if(0.49f <= vUV[1].x && vUV[1].x <=0.51f)
				cx=2;
			if(0.74f <= vUV[1].x && vUV[1].x <=0.76f)
				cx=3;

			if(0.24f <= vUV[1].y && vUV[1].y <=0.26f)
				cy=1;
			if(0.49f <= vUV[1].y && vUV[1].y <=0.51f)
				cy=2;
			if(0.74f <= vUV[1].y && vUV[1].y <=0.76f)
				cy=3;

			int texID = cx + cy*4;

      //SCloudParticle *pParticle = new SCloudParticle(vPosition, fWidth, fHeight, fRotMin, fRotMax, vUV);

      Vec3 vMin = vPosition - Vec3(fWidth, fWidth, fHeight);
      Vec3 vMax = vPosition + Vec3(fWidth, fWidth, fHeight);
      //m_boundingBox.AddPoint(vMin);
      //m_boundingBox.AddPoint(vMax);
      //m_particles.push_back(pParticle);
			cloudBox.AddPoint(vMin);
			cloudBox.AddPoint(vMax);
			
			AddSprite(new CCloudSprite (fWidth, vPosition, texID, fRotMax, Vec3(0, 0, 0)));
    }

		/*
		BBox box;
		GetBoundBox(box);
		Vec3 vSm = box.GetCenter() - cloudBox.GetCenter();

		for(int i = 0; i<nNumCloudSprites; i++)
		{
			CCloudSprite * pSprite = ppCloudSprites[i];
			pSprite->pos += vSm;
		}
		*/

		/*
    Vec3 vCenter = m_boundingBox.GetCenter();
    if (vCenter != Vec3(0,0,0))
    {
      m_boundingBox.Clear();
      for (i=0; i<iNumParticles; i++)
      {
        SCloudParticle *pParticle = m_particles[i];
        pParticle->SetPosition(pParticle->GetPosition() - vCenter);
        float fWidth = pParticle->GetRadiusX();
        float fHeight = pParticle->GetRadiusY();
        Vec3 vMin = pParticle->GetPosition() - Vec3(fWidth, fWidth, fHeight);
        Vec3 vMax = pParticle->GetPosition() + Vec3(fWidth, fWidth, fHeight);
        m_boundingBox.AddPoint(vMin);
        m_boundingBox.AddPoint(vMax);
      }
    }
		*/

  }
	/*
  else
  {
    iSystem->GetIPak()->FRead(&vCenter[0], 1, sizeof(Vec3), fp);
    vCenter = Vec3(0,0,0);

    Vec3 *pParticlePositions = new Vec3[iNumParticles];
    float *pParticleRadii     = new float[iNumParticles];
    ColorF *pParticleColors    = new ColorF[iNumParticles];

    iSystem->GetIPak()->FRead(pParticlePositions, sizeof(Vec3), iNumParticles, fp);
    iSystem->GetIPak()->FRead(pParticleRadii, sizeof(float), iNumParticles, fp);
    iSystem->GetIPak()->FRead(pParticleColors, sizeof(ColorF), iNumParticles, fp);

    for (i=0; i<iNumParticles; ++i)
    {
      if (pParticleRadii[i] < 0.8f)
        continue;
      pParticleRadii[i] *= 1.25f;
      Exchange(pParticlePositions[i].y, pParticlePositions[i].z);
      SCloudParticle *pParticle = new SCloudParticle((pParticlePositions[i]+vCenter)*fScale, pParticleRadii[i]*fScale, pParticleColors[i]);

      float fRadiusX = pParticle->GetRadiusX();
      float fRadiusY = pParticle->GetRadiusX();
      Vec3 vRadius = Vec3(fRadiusX, fRadiusX, fRadiusY);

      //Vec3 Mins = pParticle->GetPosition() - vRadius;
      //Vec3 Maxs = pParticle->GetPosition() + vRadius;
      //m_boundingBox.AddPoint(Mins);
      //m_boundingBox.AddPoint(Maxs);
      m_boundingBox.AddPoint(pParticle->GetPosition());

      m_particles.push_back(pParticle);
    }
    SAFE_DELETE_ARRAY(pParticleColors);
    SAFE_DELETE_ARRAY(pParticleRadii);
    SAFE_DELETE_ARRAY(pParticlePositions);

    m_pTexParticle = CTexture::m_Text_Gauss;
  }
  
	/**/
	iSystem->GetIPak()->FClose(fp);
	UpdateSpritesBox();
	return true;
}

#include "TypeInfo_impl.h"
#include "Cry_Vector2_info.h"
