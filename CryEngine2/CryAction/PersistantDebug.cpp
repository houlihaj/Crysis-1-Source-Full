#include "StdAfx.h"
#include "PersistantDebug.h"
#include "CryAction.h"
#include <IRenderAuxGeom.h>
#include <IUIDraw.h>

CPersistantDebug::CPersistantDebug()
{
	m_pDefaultFont = GetISystem()->GetICryFont()->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);
}

void CPersistantDebug::Begin( const char * name, bool clear )
{
	if (clear)
		m_objects[name].clear();
	else
		m_objects[name];
	m_current = m_objects.find(name);
}

void CPersistantDebug::AddSphere( const Vec3& pos, float radius, ColorF clr, float timeout )
{
	SObj obj;
	obj.obj = eOT_Sphere;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = radius;
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddQuat(const Vec3& pos, const Quat& q, float r, ColorF clr, float timeout)
{
	SObj obj;
	obj.obj = eOT_Quat;
	obj.clr = clr;
	obj.pos = pos;
	obj.q = q;
	obj.radius = r;
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddDirection( const Vec3& pos, float radius, const Vec3& dir, ColorF clr, float timeout )
{
	SObj obj;
	obj.obj = eOT_Arrow;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = radius;
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	obj.dir = dir.GetNormalizedSafe();
	if (obj.dir.GetLengthSquared() > 0.001f)
		m_current->second.push_back(obj);
}

void CPersistantDebug::AddLine( const Vec3& pos1, const Vec3& pos2, ColorF clr, float timeout )
{
	SObj obj;
	obj.obj = eOT_Line;
	obj.clr = clr;
	obj.pos = pos1;
	obj.dir = pos2 - pos1;
	obj.radius = 0.0f;
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	m_current->second.push_back(obj);
}

void CPersistantDebug::AddPlanarDisc( const Vec3& pos, float innerRadius, float outerRadius, ColorF clr, float timeout )
{
	SObj obj;
	obj.obj = eOT_Disc;
	obj.clr = clr;
	obj.pos = pos;
	obj.radius = std::max(0.0f, std::min(innerRadius,outerRadius));
	obj.radius2 = std::min( 100.0f, std::max(0.0f, std::max(innerRadius,outerRadius)) );
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	if (obj.radius2)
		m_current->second.push_back(obj);
}

void CPersistantDebug::AddText(float x, float y, float size, ColorF clr, float timeout, const char * fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start( args, fmt );
	vsprintf_s(buffer, fmt, args);
	va_end( args );
	
	SObj obj;
	obj.obj = eOT_Text;
	obj.clr = clr;
	obj.pos.x = x;
	obj.pos.y = y;
	obj.radius = size;
	obj.text = buffer;
	obj.timeRemaining = timeout;
	obj.totalTime = timeout;
	m_current->second.push_back(obj);
}

void CPersistantDebug::Add2DText(const char *text, float size, ColorF clr, float timeout)
{
	if (text == 0 || *text == '\0')
		return;

	STextObj2D obj;
	obj.clr = clr;
	obj.text = text;
  obj.size = size;
	obj.timeRemaining = timeout > 0.0f ? timeout : .1f;
	obj.totalTime = obj.timeRemaining;
	m_2DTexts.push_front(obj);
}

void CPersistantDebug::Add2DLine( float x1, float y1, float x2, float y2, ColorF clr, float timeout )
{
	SObj obj;
	obj.obj = eOT_Line2D;
	obj.pos.x = x1/400.0f-1.0f;
	obj.pos.y = y1/300.0f-1.0f;
	obj.pos.z = 0;
	obj.dir.x = x2/400.0f-1.0f;
	obj.dir.y = y2/300.0f-1.0f;
	obj.dir.z = 0;
	obj.clr = clr;
	obj.timeRemaining = timeout > 0.0f ? timeout : .1f;
	obj.totalTime = obj.timeRemaining;
	m_current->second.push_back(obj);
}

void CPersistantDebug::Reset()
{
	m_2DTexts.clear();
	m_objects.clear();
	m_current = m_objects.begin();
}

void CPersistantDebug::Update( float frameTime )
{
	if (m_objects.size() == 0)
		return;

	IRenderAuxGeom * pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	static const int flags3D = e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeBack | e_DepthWriteOn | e_DepthTestOn;
	static const int flags2D = e_Mode2D | e_AlphaBlended;

	std::vector<ListObj::iterator> toClear;
	std::vector<MapListObj::iterator> toClearMap;
	for (MapListObj::iterator iterMap = m_objects.begin(); iterMap != m_objects.end(); ++iterMap)
	{
		toClear.resize(0);
		for (ListObj::iterator iterList = iterMap->second.begin(); iterList != iterMap->second.end(); ++iterList)
		{
			iterList->timeRemaining -= frameTime;
			if (iterList->timeRemaining <= 0.0f)
				toClear.push_back(iterList);
			else
			{
				ColorF clr = iterList->clr;
				clr.a *= iterList->timeRemaining / iterList->totalTime;
				switch (iterList->obj)
				{
				case eOT_Sphere:
					pAux->SetRenderFlags( flags3D );
					pAux->DrawSphere( iterList->pos, iterList->radius, clr );
					break;
				case eOT_Quat:
					pAux->SetRenderFlags( flags3D );
					{
						float r = iterList->radius;
						Vec3 x = r * iterList->q.GetColumn0();
						Vec3 y = r * iterList->q.GetColumn1();
						Vec3 z = r * iterList->q.GetColumn2();
						Vec3 p = iterList->pos;
						OBB obb = OBB::CreateOBB( Matrix33::CreateIdentity(), Vec3(0.05f,0.05f,0.05f), ZERO );
						pAux->DrawOBB( obb, p, false, clr, eBBD_Extremes_Color_Encoded );
						pAux->DrawLine( p, ColorF(1,0,0,clr.a), p+x, ColorF(1,0,0,clr.a) );
						pAux->DrawLine( p, ColorF(0,1,0,clr.a), p+y, ColorF(0,1,0,clr.a) );
						pAux->DrawLine( p, ColorF(0,0,1,clr.a), p+z, ColorF(0,0,1,clr.a) );
					}
					break;
				case eOT_Arrow:
					pAux->SetRenderFlags( flags3D );
					pAux->DrawLine( iterList->pos - iterList->dir * iterList->radius, clr, iterList->pos + iterList->dir * iterList->radius, clr );
					pAux->DrawCone( iterList->pos + iterList->dir * iterList->radius, iterList->dir, 0.3f * iterList->radius, 0.3f * iterList->radius, clr );
					break;
				case eOT_Line:
					pAux->SetRenderFlags( flags3D );
					pAux->DrawLine( iterList->pos, clr, iterList->pos + iterList->dir, clr );
					break;
				case eOT_Line2D:
					pAux->SetRenderFlags( flags2D );
					pAux->DrawLine( iterList->pos, clr, iterList->dir, clr, 2.0f );
					break;
				case eOT_Text:
					{
						float clrAry[4] = {clr.r, clr.g, clr.b, clr.a};
						gEnv->pRenderer->Draw2dLabel( iterList->pos.x, iterList->pos.y, iterList->radius, clrAry, false, "%s", iterList->text.c_str() );
					}
					break;
				case eOT_Disc:
					{
						pAux->SetRenderFlags( flags3D );
						uint16 indTriQuad[ 6 ] = 
						{
							0, 2, 1, 
							0, 3, 2
						};	
						uint16 indTriTri[ 3 ] = 
						{
							0, 1, 2
						};	

						int steps = (int)(10 * iterList->radius2);
						steps = std::max(steps, 10);
						float angStep = gf_PI2 / steps;
						for (int i=0; i<steps; i++)
						{
							float a0 = angStep*i;
							float a1 = angStep*(i+1);
							float c0 = cosf( a0 );
							float c1 = cosf( a1 );
							float s0 = sinf( a0 );
							float s1 = sinf( a1 );
							Vec3 pts[4];
							int n, n2;
							uint16 * indTri;
							if (iterList->radius)
							{
								n = 4;
								n2 = 6;
								pts[0] = iterList->pos + iterList->radius * Vec3( c0, s0, 0 );
								pts[1] = iterList->pos + iterList->radius * Vec3( c1, s1, 0 );
								pts[2] = iterList->pos + iterList->radius2 * Vec3( c1, s1, 0 );
								pts[3] = iterList->pos + iterList->radius2 * Vec3( c0, s0, 0 );
								indTri = indTriQuad;
							}
							else
							{
								n = 3;
								n2 = 3;
								pts[0] = iterList->pos;
								pts[1] = pts[0] + iterList->radius2 * Vec3( c0, s0, 0 );
								pts[2] = pts[0] + iterList->radius2 * Vec3( c1, s1, 0 );
								indTri = indTriTri;
							}
							pAux->DrawTriangles( pts, n, indTri, n2, clr );
						}
					}
					break;
				}
			}
		}
		while (!toClear.empty())
		{
			iterMap->second.erase(toClear.back());
			toClear.pop_back();
		}
		if (iterMap->second.empty())
			toClearMap.push_back(iterMap);
	}
	while (!toClearMap.empty())
	{
		m_objects.erase(toClearMap.back());
		toClearMap.pop_back();
	}
}

void CPersistantDebug::PostUpdate( float frameTime )
{
	static const bool bUseUIDraw = true;

	if (m_2DTexts.empty())
		return;

	IUIDraw* pUIDraw = CCryAction::GetCryAction()->GetIUIDraw();
	if (bUseUIDraw && pUIDraw==0)
	{
		m_2DTexts.clear();
		return;
	}

	IRenderer* pRenderer = gEnv->pRenderer;
	const float x = bUseUIDraw ? 0.0f : pRenderer->ScaleCoordX(400.0f);

	float y = 400.0f;

	if (bUseUIDraw)
		pUIDraw->PreRender();

	// now draw 2D texts overlay
	for (ListObjText2D::iterator iter = m_2DTexts.begin(); iter != m_2DTexts.end();)
	{
		STextObj2D& textObj = *iter;
		ColorF clr = textObj.clr;
		clr.a *= textObj.timeRemaining / textObj.totalTime;
		if (bUseUIDraw)
		{	
			static const float TEXT_SPACING = 2;
      float textSize = textObj.size * 12.f;
			float sizeX,sizeY;
			const string& textLabel = textObj.text;
			if (!textLabel.empty() && textLabel[0] == '@')
			{
				wstring localizedString;
				gEnv->pSystem->GetLocalizationManager()->LocalizeString(textLabel, localizedString);
				pUIDraw->GetTextDimW(m_pDefaultFont,&sizeX, &sizeY, textSize, textSize, localizedString.c_str());
				pUIDraw->DrawTextW(m_pDefaultFont,x, y, textSize, textSize, localizedString.c_str(), clr.a, clr.r, clr.g, clr.b, 
					UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP);
			}
			else
			{
				pUIDraw->GetTextDim(m_pDefaultFont,&sizeX, &sizeY, textSize, textSize, textLabel.c_str());
				pUIDraw->DrawText(m_pDefaultFont,x, y, textSize, textSize, textLabel.c_str(), clr.a, clr.r, clr.g, clr.b, 
					UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP);
			}
			y+=sizeY+TEXT_SPACING;
		}
		else
		{
			pRenderer->Draw2dLabel(x, y, textObj.size, &clr[0], true, "%s", textObj.text.c_str());
			y+=18.0f;
		}
		textObj.timeRemaining -= frameTime;
		const bool bDelete = textObj.timeRemaining <= 0.0f;
		if (bDelete)
		{
			ListObjText2D::iterator toDelete = iter;
			++iter;
			m_2DTexts.erase(toDelete);
		}
		else
			++iter;
	}
	
	if (bUseUIDraw)
		pUIDraw->PostRender();
}
