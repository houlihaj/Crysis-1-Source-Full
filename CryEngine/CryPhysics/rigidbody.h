//////////////////////////////////////////////////////////////////////
//
//	Rigid Body header
//	
//	File: rigidbody.cpp
//	Description : RigidBody class declaration
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////

#ifndef rigidbody_h
#define rigidbody_h
#pragma once

struct entity_contact;
const int MAX_CONTACTS = 4096;

class RigidBody {
public:
	RigidBody();
	void Create(const Vec3 &center,const Vec3 &Ibody0,const quaternionf &q0, float volume,float mass, 
							const quaternionf &qframe,const Vec3 &posframe);
	void Add(const Vec3 &center,const Vec3 Ibodyop,const quaternionf &qop, float volume,float mass);
	void zero();

	void Step(float dt);
	void UpdateState();
	void GetContactMatrix(const Vec3 &r, Matrix33 &K);

	Vec3 pos;
	quaternionf q;
	Vec3 P,L;
	Vec3 w,v;

	float M,Minv; // mass, 1.0/mass (0 for static objects)
	float V; // volume
	Diag33 Ibody; // diagonalized inertia tensor (aligned with body's axes of inertia)
	Diag33 Ibody_inv; // { 1/Ibody.ii }
	quaternionf qfb; // frame->body rotation
	Vec3 offsfb; // frame->body offset

	Matrix33 Iinv; // I^-1(t)

	Vec3 Fcollision,Tcollision;
	int bProcessed; // used internally
	float Eunproj;
	float softness[2];
};

enum contactflags { contact_count_mask=0x3F, contact_new=0x40, contact_2b_verified=0x80, contact_2b_verified_log2=7, 
										contact_angular=0x100, contact_constraint_3dof=0x200, contact_constraint_2dof=0x400, 
										contact_constraint_1dof=0x800, contact_solve_for=0x1000,
										contact_constraint=contact_constraint_3dof|contact_constraint_2dof|contact_constraint_1dof,
										contact_angular_log2=8,contact_bidx=0x2000,contact_bidx_log2=13, contact_maintain_count=0x4000,
										contact_wheel=0x8000, contact_use_C=0x10000, contact_inexact=0x20000, 
										contact_last=0x40000, contact_remove=0x80000, contact_archived=0x100000,
										contact_rope=0x200000, contact_preserve_Pspare=0x400000
									};

struct rope_solver_vtx {
	Vec3 r,v,P;
	RigidBody *pbody;
	int iBody;
	int ivtx;
};

class CPhysicalEntity;

struct entity_contact {
	entity_contact *next,*prev;
	entity_contact *nextAux,*prevAux;
	int bChunkStart;

	Vec3 pt[2];
	Vec3 n;
	Vec3 dir;
	Vec3 ptloc[2];
	CPhysicalEntity *pent[2];
	int ipart[2];
	RigidBody *pbody[2];
	Vec3 nloc;
	float friction;
	int id[2];
	int flags;
	Vec3 vrel;
	Vec3 vreq;
	//float vsep;
	float Pspare;
	float penetration;
	Matrix33 K,Kinv;
	Matrix33 C;
	int iNormal;
	int iPrim[2];
	int iFeature[2];
	int bProcessed;
	int iCount,*pBounceCount;

	Vec3 r0,r;
	Vec3 dP,P;
	float dPn;
};

extern bool g_bUsePreCG;
extern int g_nContacts,g_nBodies;
extern entity_contact *g_pContacts[];
void InitContactSolver(float time_interval);
void RegisterContact(entity_contact *pcontact);
void InvokeContactSolver(float time_interval, SolverSettings *pss, float Ebefore);
char *AllocSolverTmpBuf(int size);

#endif