/////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	Created by Ivo Herzeg
//	
//  Notes:
//    IController interface declaration
//    See the IController comment for more info
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _CRYTEK_CONTROLLER_HEADER_
#define _CRYTEK_CONTROLLER_HEADER_

#define TICKS_PER_SECOND (30)						//the maximum keyframe value is 4800Hz per second   
#define SECONDS_PER_TICK (1.0f/30.0f)		//the maximum keyframe value is 4800Hz per second   
#define TICKS_PER_FRAME (1)							//if we have 30 keyframes per second, then one tick is 1
#define TICKS_CONVERT (160)

//#define TICKS_PER_SECOND (120)					//the maximum keyframe value is 4800Hz per second   
//#define SECONDS_PER_TICK (1.0f/120.0f)	//the maximum keyframe value is 4800Hz per second   
//#define TICKS_PER_FRAME (4)							//if we have 30 keyframes per second, then one tick is 4
//#define TICKS_CONVERT (40)

//#define TICKS_PER_SECOND (4800)						//the maximum keyframe value is 4800Hz per second   
//#define SECONDS_PER_TICK (1.0f/4800.0f)		//the time for 1 tick    
//#define TICKS_PER_FRAME (160)							//if we have 30 keyframes per second, then one tick is 160
//#define TICKS_CONVERT (1)  


struct Status4
{
	union 
	{
		uint32 ops;
		struct { uint8 o,p,s,pad; };
	};
	ILINE Status4() { ops=0; };
	ILINE Status4(uint8 _x, uint8 _y, uint8 _z, uint8 _w=1)	{	o=_x; p=_y; s=_z; pad=_w;	}
};


#define STATUS_O		0x1 
#define STATUS_P		0x2
#define STATUS_S		0x4
#define STATUS_PAD	0x8

struct CInfo
{
	uint32	numKeys;
	Quat		quat;
	Vec3		pos;
	int			stime;
	int			etime;
	int			realkeys;
}; 


enum EControllerInfo
{
	ePQLog,
	eTCB,
	eCController,
	eControllerOpt
};
//////////////////////////////////////////////////////////////////////////////////////////
// interface IController  
// Describes the position and orientation of an object, changing in time.
// Responsible for loading itself from a binary file, calculations
//////////////////////////////////////////////////////////////////////////////////////////
class IController: public _reference_target_t
{
public:
	// each controller has an ID, by which it is identifiable
	virtual uint32 GetID () const = 0;

	// returns the orientation,position and scaling of the controller at the given time
	virtual Status4 GetOPS (int GAID, f32 t, Quat& quat, Vec3& pos, Diag33& scale) = 0;
	// returns the orientation and position of the controller at the given time
	virtual Status4 GetOP (int GAID, f32 t, Quat& quat, Vec3& pos) = 0;

	// returns the orientation of the controller at the given time
	virtual uint32 GetO (int GAID, f32 t, Quat& quat) = 0;
	// returns position of the controller at the given time
	virtual uint32 GetP (int GAID, f32 t, Vec3& pos) = 0;
	// returns scale of the controller at the given time
	virtual uint32 GetS (int GAID, f32 t, Diag33& pos) = 0;

	virtual Quat GetOrientationByKey(uint32 KeyNo) = 0;


	virtual EControllerInfo GetControllerType() = 0;

	virtual CInfo GetControllerInfo() const = 0;

	virtual size_t SizeOfThis() const = 0;

	virtual size_t GetRotationKeysNum() = 0;

	virtual size_t GetPositionKeysNum() = 0;

};



TYPEDEF_AUTOPTR(IController);




// adjusts the rotation of these PQs: if necessary, flips them or one of them (effectively NOT changing the whole rotation,but
// changing the rotation angle to Pi-X and flipping the rotation axis simultaneously)
// this is needed for blending between animations represented by quaternions rotated by ~PI in quaternion space
// (and thus ~2*PI in real space)
extern void AdjustLogRotations (Vec3& vRotLog1, Vec3& vRotLog2);


//////////////////////////////////////////////////////////////////////////
// This class is a non-trivial predicate used for sorting an
// ARRAY OF smart POINTERS  to IController's. That's why I need a separate
// predicate class for that. Also, to avoid multiplying predicate classes,
// there are a couple of functions that are also used to find a IController
// in a sorted by ID array of IController* pointers, passing only ID to the
// lower_bound function instead of creating and passing a dummy IController*
class AnimCtrlSortPred
{
public:
	bool operator() (const IController_AutoPtr& a, const IController_AutoPtr& b) {assert (a!=(IController*)NULL && b != (IController*)NULL); return a->GetID() < b->GetID();}
	bool operator() (const IController_AutoPtr& a, uint32 nID) {assert (a != (IController*)NULL);return a->GetID() < nID;}
};


class AnimCtrlSortPredInt
{
public:
	bool operator() (const IController_AutoPtr& a, uint32 nID) {assert (a != (IController*)NULL);return a->GetID() < nID;}
};


#endif