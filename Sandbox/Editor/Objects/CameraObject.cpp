////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TagPoint.cpp
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: CCameraObject implementation.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CameraObject.h"
#include "ObjectManager.h"
#include "..\Viewport.h"

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CCameraObject,CEntity)
IMPLEMENT_DYNCREATE(CCameraObjectTarget,CEntity)

#define CAMERA_COLOR RGB(0,255,255)
#define CAMERA_CONE_LENGTH 4
#define CAMERABOX_RADIUS 0.7f

//////////////////////////////////////////////////////////////////////////
CCameraObject::CCameraObject()
{
	mv_fov = DEG2RAD(60.0f);		// 60 degrees (to fit with current game)
	mv_shakeAEnabled = false;
	mv_amplitudeA = Vec3(1.0f,1.0f,1.0f);
	mv_amplitudeAMult = 0.0f;
	mv_frequencyA = Vec3(1.0f,1.0f,1.0f);
	mv_frequencyAMult = 0.0f;
	mv_noiseAAmpMult = 0.0f;
	mv_noiseAFreqMult = 0.0f;
	mv_timeOffsetA = 0.0f;
	mv_shakeBEnabled = false;
	mv_amplitudeB = Vec3(1.0f,1.0f,1.0f);
	mv_amplitudeBMult = 0.0f;
	mv_frequencyB = Vec3(1.0f,1.0f,1.0f);
	mv_frequencyBMult = 0.0f;
	mv_noiseBAmpMult = 0.0f;
	mv_noiseBFreqMult = 0.0f;
	mv_timeOffsetB = 0.0f;
	m_creationStep = 0;
	SetColor(CAMERA_COLOR);
	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::Done()
{
	CBaseObjectPtr lookat = GetLookAt();

	CEntity::Done();

	if (lookat)
	{
		// If look at is also camera class, delete lookat target.
		if (lookat->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			GetObjectManager()->DeleteObject( lookat );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	bool res = CEntity::Init( ie,prev,file );

	m_entityClass = "CameraSource";

	if (prev)
	{
		CBaseObjectPtr prevLookat = prev->GetLookAt();
		if (prevLookat)
		{
			CBaseObjectPtr lookat = GetObjectManager()->NewObject( prevLookat->GetClassDesc(),prevLookat );
			if (lookat)
			{
				lookat->SetPos( prevLookat->GetPos() + Vec3(3,0,0) );
				GetObjectManager()->ChangeObjectName( lookat,CString(GetName()) + " Target" );
				SetLookAt( lookat );
			}
		}
	}
	SetEntityCameraFov();
	
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::InitVariables()
{
	AddVariable( mv_fov,"FOV",functor(*this,&CCameraObject::OnFovChange),IVariable::DT_ANGLE );

	mv_shakeParamsArray.DeleteAllChilds();

	AddVariable( mv_shakeParamsArray,"ShakeParams","Shake Parameters" );
	AddVariable( mv_shakeParamsArray,mv_shakeAEnabled,"ShakeAEnabled","Shake Layer A",functor(*this,&CCameraObject::OnShakeWorkingChange) );
	AddVariable( mv_shakeParamsArray,mv_amplitudeA,"AmplitudeA","Amplitude A",functor(*this,&CCameraObject::OnShakeAmpAChange) );
	AddVariable( mv_shakeParamsArray,mv_amplitudeAMult,"AmplitudeAMult","Amplitude A Mult.",functor(*this,&CCameraObject::OnShakeMultChange) );
	AddVariable( mv_shakeParamsArray,mv_frequencyA,"FrequencyA","Frequency A",functor(*this,&CCameraObject::OnShakeFreqAChange) );
	AddVariable( mv_shakeParamsArray,mv_frequencyAMult,"FrequencyAMult","Frequency A Mult.",functor(*this,&CCameraObject::OnShakeMultChange) );
	AddVariable( mv_shakeParamsArray,mv_noiseAAmpMult,"NoiseAAmpMult","Noise A Amp. Mult.",functor(*this,&CCameraObject::OnShakeNoiseChange) );
	AddVariable( mv_shakeParamsArray,mv_noiseAFreqMult,"NoiseAFreqMult","Noise A Freq. Mult.",functor(*this,&CCameraObject::OnShakeNoiseChange) );
	AddVariable( mv_shakeParamsArray,mv_timeOffsetA,"TimeOffsetA","Time Offset A",functor(*this,&CCameraObject::OnShakeWorkingChange) );
	AddVariable( mv_shakeParamsArray,mv_shakeBEnabled,"ShakeBEnabled","Shake Layer B",functor(*this,&CCameraObject::OnShakeWorkingChange) );
	AddVariable( mv_shakeParamsArray,mv_amplitudeB,"AmplitudeB","Amplitude B",functor(*this,&CCameraObject::OnShakeAmpBChange) );
	AddVariable( mv_shakeParamsArray,mv_amplitudeBMult,"AmplitudeBMult","Amplitude B Mult.",functor(*this,&CCameraObject::OnShakeMultChange) );
	AddVariable( mv_shakeParamsArray,mv_frequencyB,"FrequencyB","Frequency B",functor(*this,&CCameraObject::OnShakeFreqBChange) );
	AddVariable( mv_shakeParamsArray,mv_frequencyBMult,"FrequencyBMult","Frequency B Mult.",functor(*this,&CCameraObject::OnShakeMultChange) );
	AddVariable( mv_shakeParamsArray,mv_noiseBAmpMult,"NoiseBAmpMult","Noise B Amp. Mult.",functor(*this,&CCameraObject::OnShakeNoiseChange) );
	AddVariable( mv_shakeParamsArray,mv_noiseBFreqMult,"NoiseBFreqMult","Noise B Freq. Mult.",functor(*this,&CCameraObject::OnShakeNoiseChange) );
	AddVariable( mv_shakeParamsArray,mv_timeOffsetB,"TimeOffsetB","Time Offset B",functor(*this,&CCameraObject::OnShakeWorkingChange) );
}

void CCameraObject::Serialize( CObjectArchive &ar )
{
	CEntity::Serialize( ar );
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::EnableAnimNode( bool bEnable )
{
	if (bEnable)
	{
		int nodeId = GetId().Data1;
		IAnimNode *pAnimNode = GetIEditor()->GetMovieSystem()->CreateNode( ANODE_CAMERA,nodeId );
		SetAnimNode( pAnimNode );
		float fov = mv_fov;
		pAnimNode->SetParamValue( 0,APARAM_FOV,RAD2DEG(fov) );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEAMPA,mv_amplitudeA );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEAMPB,mv_amplitudeB );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEFREQA,mv_frequencyA );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEFREQB,mv_frequencyB );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEMULT,Vec4(mv_amplitudeAMult,mv_amplitudeBMult,mv_frequencyAMult,mv_frequencyBMult) );
		pAnimNode->SetParamValue( 0,APARAM_SHAKENOISE,Vec4(mv_noiseAAmpMult,mv_noiseBAmpMult,mv_noiseAFreqMult,mv_noiseBFreqMult) );
		pAnimNode->SetParamValue( 0,APARAM_SHAKEWORKING,Vec4(mv_timeOffsetA,mv_timeOffsetB,mv_shakeAEnabled ? 1.0f : 0.0f,mv_shakeBEnabled ? 1.0f : 0.0f) );
	}
	else
	{
		__super::EnableAnimNode(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnNodeAnimated( IAnimNode *pNode )
{
	assert( m_pAnimNode != 0 ); // Only can be called if there`s an anim node present.

	CEntity::OnNodeAnimated(pNode);
	// Get fov out of node at current time.
	float fov = RAD2DEG(mv_fov);
	if (m_pAnimNode->GetParamValue( m_pAnimNode->GetTime(),APARAM_FOV,fov ))
	{
		mv_fov = DEG2RAD(fov);
	}
	Vec4 multipliers = Vec4(mv_amplitudeAMult,mv_amplitudeBMult,mv_frequencyAMult,mv_frequencyBMult);
	if (m_pAnimNode->GetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEMULT,multipliers ))
	{
		mv_amplitudeAMult = multipliers.x;
		mv_amplitudeBMult = multipliers.y;
		mv_frequencyAMult = multipliers.z;
		mv_frequencyBMult = multipliers.w;
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetName( const CString &name )
{
	CEntity::SetName(name);
	if (GetLookAt())
	{
		GetLookAt()->SetName( CString(GetName()) + " Target" );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetScale( const Vec3 &scale )
{
	// Ignore scale.
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::BeginEditParams( IEditor *ie,int flags )
{
	// Skip entity begin edit params.
	CBaseObject::BeginEditParams( ie,flags );
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::EndEditParams( IEditor *ie )
{
	// Skip entity end edit params.
	CBaseObject::EndEditParams( ie );
}

//////////////////////////////////////////////////////////////////////////
int CCameraObject::MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags )
{
	if (event == eMouseMove || event == eMouseLDown || event == eMouseLUp)
	{
		Vec3 pos;
		if (GetIEditor()->GetAxisConstrains() != AXIS_TERRAIN)
		{
			pos = view->MapViewToCP(point);
		}
		else
		{
			// Snap to terrain.
			bool hitTerrain;
			pos = view->ViewToWorld( point,&hitTerrain );
			if (hitTerrain)
			{
				pos.z = GetIEditor()->GetTerrainElevation(pos.x,pos.y) + 1.0f;
			}
			pos = view->SnapToGrid(pos);
		}

		if (m_creationStep == 1)
		{
			if (GetLookAt())
			{
				GetLookAt()->SetPos( pos );
			}
		}
		else
		{
			SetPos(pos);
		}
		
		if (event == eMouseLDown && m_creationStep == 0)
		{
			m_creationStep = 1;
		}
		if (event == eMouseMove && 1 == m_creationStep && !GetLookAt())
		{
			float d = pos.GetDistance(GetPos());
			if (d*view->GetScreenScaleFactor(pos) > 1)
			{
				// Create LookAt Target.
				GetIEditor()->ResumeUndo();
				CreateTarget();
				GetIEditor()->SuspendUndo();
			}
		}
		if (eMouseLUp == event && 1 == m_creationStep)
			return MOUSECREATE_OK;

		return MOUSECREATE_CONTINUE;
	}
	return CBaseObject::MouseCreateCallback( view,event,point,flags );
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::CreateTarget()
{
	// Create another camera object for target.
	CCameraObject *camTarget = (CCameraObject*)GetObjectManager()->NewObject( "CameraTarget" );
	if (camTarget)
	{
		camTarget->SetName( CString(GetName()) + " Target" );
		camTarget->SetPos( GetWorldPos() + Vec3(3,0,0) );
		SetLookAt( camTarget );
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CCameraObject::GetLookAtEntityPos() const
{
	Vec3 pos = GetWorldPos();
	if (GetLookAt())
	{
		pos = GetLookAt()->GetWorldPos();

		if (GetLookAt()->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			IEntity *pIEntity = ((CEntity*)GetLookAt())->GetIEntity();
			if (pIEntity)
			{
				pos = pIEntity->GetWorldPos();
			}
		}
	}
	return pos;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::Display( DisplayContext &dc )
{
	Matrix34 wtm = GetWorldTM();

	if (m_entity)
	{
		// Display the actual entity matrix.
		wtm = m_entity->GetWorldTM();
	}

	Vec3 wp = wtm.GetTranslation();

	//float fScale = dc.view->GetScreenScaleFactor(wp) * 0.03f;
	float fScale = GetHelperScale();

	if (IsHighlighted() && !IsFrozen())
		dc.SetLineWidth(3);

	if (GetLookAt())
	{
		// Look at camera.
		if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor( GetColor() );

		bool bSelected = IsSelected();

		if (bSelected || GetLookAt()->IsSelected())
		{
			dc.SetSelectedColor();
		}

		// Line from source to target.
		dc.DrawLine( wp,GetLookAtEntityPos() );

		if (bSelected)
		{
			dc.SetSelectedColor();
		}
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor( GetColor() );

		dc.PushMatrix( wtm );
		
		//Vec3 sz(0.2f*fScale,0.1f*fScale,0.2f*fScale);
		//dc.DrawWireBox( -sz,sz );

		if (bSelected)
		{
			float dist = GetLookAtEntityPos().GetDistance(wtm.GetTranslation());
			DrawCone( dc,dist );
		}
		else
		{
			float dist = CAMERA_CONE_LENGTH/2.0f;
			DrawCone( dc,dist,fScale );
		}
		dc.PopMatrix();
	}
	else
	{
		// Free camera
		if (IsSelected())
			dc.SetSelectedColor();
		else if (IsFrozen())
			dc.SetFreezeColor();
		else
			dc.SetColor( GetColor() );
		
		dc.PushMatrix( wtm );
		
		//Vec3 sz(0.2f*fScale,0.1f*fScale,0.2f*fScale);
		//dc.DrawWireBox( -sz,sz );

		float dist = CAMERA_CONE_LENGTH;
		DrawCone( dc,dist,fScale );
		dc.PopMatrix();
	}

	if (IsHighlighted() && !IsFrozen())
		dc.SetLineWidth(0);

	//dc.DrawIcon( ICON_QUAD,wp,0.1f*dc.view->GetScreenScaleFactor(wp) );

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::HitTest( HitContext &hc )
{
	return __super::HitTest(hc);
	/*
	Vec3 origin = GetWorldPos();
	float radius = CAMERABOX_RADIUS/2.0f;

	//float fScale = hc.view->GetScreenScaleFactor(origin) * 0.03f;
	float fScale = GetHelperScale();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if (d < radius*fScale + hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}

	const Matrix34 &wtm = GetWorldTM();
	Vec3 org = wtm.GetTranslation();

	float dist = CAMERA_CONE_LENGTH;
	Vec3 q[4];
	GetConePoints(q,dist,hc.view->GetAspectRatio());

	q[0] = wtm * (q[0]*fScale);
	q[1] = wtm * (q[1]*fScale);
	q[2] = wtm * (q[2]*fScale);
	q[3] = wtm * (q[3]*fScale);

	float minDist = FLT_MAX;
	dist = FLT_MAX;
	// Draw quad.
	if (hc.view->HitTestLine( org,q[0],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( org,q[1],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( org,q[2],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( org,q[3],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);

	if (hc.view->HitTestLine( q[0],q[1],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( q[1],q[2],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( q[2],q[3],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);
	if (hc.view->HitTestLine( q[3],q[0],hc.point2d,5,&dist ))
		minDist = min(minDist,dist);

	if (dist < FLT_MAX)
	{
		hc.dist = dist;
		return true;
	}


	return false;
	*/
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObject::HitTestRect( HitContext &hc )
{
	// transform all 8 vertices into world space
	CPoint p = hc.view->WorldToView( GetWorldPos() );
	if (hc.rect.PtInRect(p))
	{
		hc.object = this;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetBoundBox( BBox &box )
{
	Vec3 pos = GetWorldPos();
	float r;
	r = 1 * CAMERA_CONE_LENGTH * GetHelperScale();
	box.min = pos - Vec3(r,r,r);
	box.max = pos + Vec3(r,r,r);
	if (GetLookAt())
	{
		box.Add( GetLookAt()->GetWorldPos() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetLocalBounds( BBox &box )
{
	GetBoundBox( box );
	Matrix34 invTM( GetWorldTM() );
	invTM.Invert();
	box.SetTransformedAABB( invTM,box );
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::GetConePoints( Vec3 q[4],float dist, const float fAspectRatio )
{
	if (dist > 1e8f)
		dist = 1e8f;
	float ta = (float)tan(0.5f*GetFOV());
	float h = dist * ta;
	float w = h * fAspectRatio;

	q[0] = Vec3( w,dist, h);
	q[1] = Vec3(-w,dist, h);
	q[2] = Vec3(-w,dist,-h);
	q[3] = Vec3( w,dist,-h);
}

void CCameraObject::DrawCone( DisplayContext &dc,float dist,float fScale )
{
	Vec3 q[4];
	GetConePoints(q,dist,dc.view->GetAspectRatio());

	q[0] *= fScale;
	q[1] *= fScale;
	q[2] *= fScale;
	q[3] *= fScale;

	Vec3 org(0,0,0);
	dc.DrawLine( org,q[0] );
	dc.DrawLine( org,q[1] );
	dc.DrawLine( org,q[2] );
	dc.DrawLine( org,q[3] );

	// Draw quad.
	dc.DrawPolyLine( q,4 );

	// Draw cross.
	//dc.DrawLine( q[0],q[2] );
	//dc.DrawLine( q[1],q[3] );
}

	/*

void CCameraObject::DrawCone( DisplayContext &dc )
{
	float dist = 10;

	Vec3 q[5], u[3];
	GetConePoints(q,dist);

	if (colid)	gw->setColor( LINE_COLOR, GetUIColor(colid));
	if (drawDiags) {
		u[0] =  q[0];	u[1] =  q[2];	
		gw->polyline( 2, u, NULL, NULL, FALSE, NULL );	
		u[0] =  q[1];	u[1] =  q[3];	
		gw->polyline( 2, u, NULL, NULL, FALSE, NULL );	
	}
	gw->polyline( 4, q, NULL, NULL, TRUE, NULL );	
	if (drawSides) {
		gw->setColor( LINE_COLOR, GetUIColor(COLOR_CAMERA_CONE));
		u[0] = Point3(0,0,0);
		for (int i=0; i<4; i++) {
			u[1] =  q[i];	
			gw->polyline( 2, u, NULL, NULL, FALSE, NULL );	
		}
	}
}
	*/

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnFovChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		float fov = mv_fov;
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_FOV, RAD2DEG(fov) );
		SetEntityCameraFov();
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::SetEntityCameraFov()
{
	if (m_entity)
	{
		IEntityCameraProxy *pCameraProxy = (IEntityCameraProxy*)m_entity->GetProxy(ENTITY_PROXY_CAMERA);
		if (pCameraProxy)
		{
			float fov = mv_fov;
			CCamera &cam = pCameraProxy->GetCamera();
			cam.SetFrustum( cam.GetViewSurfaceX(),cam.GetViewSurfaceZ(),fov,cam.GetNearPlane(),cam.GetFarPlane() ); // Change fov.
			pCameraProxy->SetCamera( cam );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeAmpAChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEAMPA,mv_amplitudeA );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeAmpBChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEAMPB,mv_amplitudeB );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeFreqAChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEFREQA,mv_frequencyA );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeFreqBChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEFREQB,mv_frequencyB );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeMultChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEMULT,Vec4(mv_amplitudeAMult,mv_amplitudeBMult,mv_frequencyAMult,mv_frequencyBMult) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeNoiseChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKENOISE,Vec4(mv_noiseAAmpMult,mv_noiseBAmpMult,mv_noiseAFreqMult,mv_noiseBFreqMult) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnShakeWorkingChange( IVariable *var )
{
	if (m_pAnimNode)
	{
		m_pAnimNode->SetParamValue( m_pAnimNode->GetTime(),APARAM_SHAKEWORKING,Vec4(mv_timeOffsetA,mv_timeOffsetB,mv_shakeAEnabled ? 1.0f : 0.0f,mv_shakeBEnabled ? 1.0f : 0.0f) );
	}
}

/*
//////////////////////////////////////////////////////////////////////////
void CCameraObject::OnPropertyChange( const CString &property )
{
	CEntity::OnPropertyChange( property );

	float fov = m_fov;
	GetParams()->getAttr( "FOV",fov );
	m_fov = fov;
	if (m_pAnimNode)
		m_pAnimNode->SetParam( GetIEditor()->GetAnimation()->GetTime(),APARAM_FOV, fov );
}
*/


//////////////////////////////////////////////////////////////////////////
//
// CCameraObjectTarget implementation.
//
//////////////////////////////////////////////////////////////////////////
CCameraObjectTarget::CCameraObjectTarget()
{
	SetColor(CAMERA_COLOR);
	UseMaterialLayersMask(false);
}

bool CCameraObjectTarget::Init( IEditor *ie,CBaseObject *prev,const CString &file )
{
	SetColor( CAMERA_COLOR );
	bool res = CEntity::Init( ie,prev,file );
	m_entityClass = "CameraTarget";
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::InitVariables()
{

}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::Display( DisplayContext &dc )
{
	Vec3 wp = GetWorldPos();

	//float fScale = dc.view->GetScreenScaleFactor(wp) * 0.03f;
	float fScale = GetHelperScale();

	if (IsSelected())
		dc.SetSelectedColor();
	else if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor( GetColor() );
		
	Vec3 sz(0.2f*fScale,0.2f*fScale,0.2f*fScale);
	dc.DrawWireBox( wp-sz,wp+sz );

	DrawDefault( dc );
}

//////////////////////////////////////////////////////////////////////////
bool CCameraObjectTarget::HitTest( HitContext &hc )
{
	Vec3 origin = GetWorldPos();
	float radius = CAMERABOX_RADIUS/2.0f;

	//float fScale = hc.view->GetScreenScaleFactor(origin) * 0.03f;
	float fScale = GetHelperScale();

	Vec3 w = origin - hc.raySrc;
	w = hc.rayDir.Cross( w );
	float d = w.GetLength();

	if (d < radius*fScale + hc.distanceTollerance)
	{
		hc.dist = hc.raySrc.GetDistance(origin);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CCameraObjectTarget::GetBoundBox( BBox &box )
{
	Vec3 pos = GetWorldPos();
	float r = CAMERABOX_RADIUS;
	box.min = pos - Vec3(r,r,r);
	box.max = pos + Vec3(r,r,r);
}

void CCameraObjectTarget::Serialize( CObjectArchive &ar )
{
	CEntity::Serialize( ar );
}