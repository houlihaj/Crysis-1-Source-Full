
#ifndef __ARCBALL_H
#define __ARCBALL_H

#include <Cry_Math.h>
#include <Cry_Color.h>


#define CrossDist (0.05f)
#define AxisDist  (0.05f)


///////////////////////////////////////////////////////////////////////////////
// CCamera
///////////////////////////////////////////////////////////////////////////////

class CArcBall3D 
{

public:

	uint32 RotControl;

	Sphere sphere;

	uint32 Mouse_CutFlag;
	uint32 Mouse_CutFlagStart;

	uint32 AxisSnap;

	Vec3 LineStart3D;   
	Vec3 Mouse_CutOnUnitSphere;

	Quat DragRotation;
	Quat ObjectRotation;



	CArcBall3D( void ) 
	{
		InitArcBall();
	};

	void InitArcBall()
	{
		RotControl=0;

		sphere(Vec3(ZERO),0.25f);

		Mouse_CutFlag=0;
		Mouse_CutOnUnitSphere(0,0,0);

		LineStart3D(0,-1,0);
		AxisSnap=0;

		DragRotation.SetIdentity();
		ObjectRotation.SetIdentity();
	}

	~CArcBall3D() {};

	void ArcControl( const Ray& ray, uint32 mouseleft );
	void ArcRotation();
	void Draw_Sphere( const CCamera& cam, struct IRenderAuxGeom* pRenderer );

	uint32 Sphere_Lineseg(const Sphere& s, Vec3 LineStart,Vec3 LineEnd, Vec3 &I);

};



#endif
