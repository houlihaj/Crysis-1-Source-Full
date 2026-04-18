//////////////////////////////////////////////////////////////////////
//
//	Utils header
//	
//	File: utils.h
//	Description : Various utility functions definitions
//
//	History:
//	-:Created by Anton Knyazev
//
//////////////////////////////////////////////////////////////////////
#ifndef utils_h
#define utils_h

#include MATH_H

typedef int index_t;

#include "Cry_Math.h"
#include "MTPseudoRandom.h"
using namespace primitives;

void ExtrudeBox(const primitives::box *pbox, const Vec3& dir,float step, primitives::box *pextbox);

#define USE_MATRIX_POOLS

//#define si (3)
//#define sj (1)

//////////////////////////////////////////////////////////////////////////
extern CMTRand_int32 g_random_generator;

//////////////////////////////////////////////////////////////////////////
inline unsigned int physics_rand()
{
	return g_random_generator.Generate() & RAND_MAX;
}
//////////////////////////////////////////////////////////////////////////
inline unsigned int physics_rand( int nRange )
{
	return g_random_generator.Generate() % nRange;
}
//////////////////////////////////////////////////////////////////////////
inline float physics_frand( float range=1.0f )
{
	return g_random_generator.GenerateFloat()*range;
}
//////////////////////////////////////////////////////////////////////////
inline float physics_frand( float fStart,float fEnd )
{
	return physics_frand(fEnd-fStart) + fStart;
}
//////////////////////////////////////////////////////////////////////////

#ifdef max
 #undef max
#endif
#ifdef min
 #undef min
#endif
#ifndef __GNUC__
	inline double min(double op1,double op2) { return (op1+op2-fabs(op1-op2))*0.5; }
	inline double max(double op1,double op2) { return (op1+op2+fabs(op1-op2))*0.5; }
	inline float max(float op1,float op2) { return (op1+op2+fabsf(op1-op2))*0.5f; }
	inline float min(float op1,float op2) { return (op1+op2-fabsf(op1-op2))*0.5f; }
	inline int max(int op1,int op2) { return op1 - (op1-op2 & (op1-op2)>>31); }
	inline int min(int op1,int op2) { return op2 + (op1-op2 & (op1-op2)>>31); }
#endif //__GNUC__
inline double minmax(double op1,double op2,int bMax) { return (op1+op2+fabs(op1-op2)*(bMax*2-1))*0.5; }
inline float minmax(float op1,float op2,int bMax) { return (op1+op2+fabsf(op1-op2)*(bMax*2-1))*0.5f; }
inline int minmax(int op1,int op2,int bMax) {	return (op1&-bMax | op2&~-bMax) + ((op1-op2 & (op1-op2)>>31)^-bMax)+bMax; }
template<class F> inline F max_safe(F op1,F op2) { return op1>op2 ? op1:op2; }//int mask=isneg(op2-op1); return op1*mask+op2*(mask^1); }
template<class F> inline F min_safe(F op1,F op2) { return op1<op2 ? op1:op2; }//{ int mask=isneg(op1-op2); return op1*mask+op2*(mask^1); }

template<class F> inline Vec3_tpl<F> min(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(min(v0.x,v1.x), min(v0.y,v1.y), min(v0.z,v1.z)); 
}
template<class F> inline Vec3_tpl<F> max(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(max(v0.x,v1.x), max(v0.y,v1.y), max(v0.z,v1.z)); 
}
template<class F> inline Vec3_tpl<F> min_safe(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(min_safe(v0.x,v1.x), min_safe(v0.y,v1.y), min_safe(v0.z,v1.z)); 
}
template<class F> inline Vec3_tpl<F> max_safe(const Vec3_tpl<F> &v0,const Vec3_tpl<F> &v1) { 
	return Vec3_tpl<F>(max_safe(v0.x,v1.x), max_safe(v0.y,v1.y), max_safe(v0.z,v1.z)); 
}

template<class F> inline Vec2_tpl<F> min(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(min(v0.x,v1.x), min(v0.y,v1.y));
}
template<class F> inline Vec2_tpl<F> max(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(max(v0.x,v1.x), max(v0.y,v1.y)); 
}
template<class F> inline Vec2_tpl<F> min_safe(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(min_safe(v0.x,v1.x), min_safe(v0.y,v1.y));
}
template<class F> inline Vec2_tpl<F> max_safe(const Vec2_tpl<F> &v0,const Vec2_tpl<F> &v1) { 
	return Vec2_tpl<F>(max_safe(v0.x,v1.x), max_safe(v0.y,v1.y));
}

template<class F> inline F len(const Vec2_tpl<F> &v) { return sqrt_tpl(v.x*v.x+v.y*v.y); }
template<class F> inline F len2(const Vec2_tpl<F> &v) { return v.x*v.x+v.y*v.y; }
template<class F> inline Vec2_tpl<F> norm(const Vec2_tpl<F> &v) { 
	F rlen = v.x*v.x+v.y*v.y;
	if (rlen>0) {
		rlen = (F)1/sqrt_tpl(rlen);	return Vec2_tpl<F>(v.x*rlen, v.y*rlen);
	}
	return Vec2_tpl<F>(1,0);
}

#include "matrixnm.h"
#include "vectorn.h"
#include "quotient.h"
#include "polynomial.h"
#include "primitives.h"

const float fmag = 1.5f*(1<<23);

union floatint {
	float fval;
	int ival;
};

#if defined(WIN64) || defined(LINUX64)
const int	imag	= (23+127)<<23 | 1<<22; 
int float2int(float x);

#elif defined(_XBOX)

/////////////////////////////////////////////////////////////////////
// For Xenon
const int	imag	= (23+127)<<23 | 1<<22; 
inline int float2int(float x) {
	floatint u;
	u.fval = x+fmag;
	return u.ival-imag;
}
/////////////////////////////////////////////////////////////////////

#elif defined(LINUX32) || defined(PS3)
const int	imag	= (23+127)<<23 | 1<<22; 
inline int float2int(float x) {
	floatint u;
	u.fval = x+fmag;
	return u.ival-imag;
}

#else //WIN64

const int	imag	= -((23+127)<<23 | 1<<22);
// had to rewrite in assembly, since it's impossible to tell the compiler that in this case 
// addition is not associative, i.e. (x+0.5)+fmag!=x+(fmag+0.5)
__declspec(naked) inline int float2int(float x)
{ __asm {
	fld [esp+4]
	fadd fmag
	mov eax, imag
	fstp [esp-4]
	add eax, [esp-4]
	ret
}}

#endif //WIN64


template<class F> F _condmax(F, int masknot) {
	return 1<<(sizeof(int)*8-2) & ~masknot;
}
inline float _condmax(float, int masknot) {
	return 1E30f*(masknot+1);
}

// Import std swap.
using std::swap;
//template<class F> inline void swap(F &v0,F &v1) { F vt=v0; v0=v1; v1=vt; }

template<class F> int unite_lists(F *pSrc0,int nSrc0, F *pSrc1,int nSrc1, F *pDst,int szdst)	//AMD Port
{
	int i0,i1,n;
	INT_PTR inrange0=-nSrc0>>31,inrange1=-nSrc1>>31; //AMD Port
	F a0,a1,ares,dummy=0;
	INT_PTR pDummy( (INT_PTR) &dummy );
	pSrc0 = (F*)((INT_PTR)pSrc0&inrange0 | pDummy&~inrange0); // make pSrc point to valid data even if nSrc is zero	//AMD Port
	pSrc1 = (F*)((INT_PTR)pSrc1&inrange1 | pDummy&~inrange1);									//AMD Port
	for(n=i0=i1=0; (inrange0 | inrange1) & n-szdst>>31; inrange0=(i0+=isneg(a0-ares-1))-nSrc0>>31, inrange1=(i1+=isneg(a1-ares-1))-nSrc1>>31)
	{
		a0 = pSrc0[i0&inrange0] + _condmax(pSrc0[0],inrange0); //(1<<(sizeof(index)*8-2)&~inrange0);
		a1 = pSrc1[i1&inrange1] + _condmax(pSrc1[0],inrange1); //(1<<(sizeof(index)*8-2)&~inrange1);
		pDst[n++] = ares = min(a0,a1);
	}
	return n;
}

template<class F> int intersect_lists(F *pSrc0,int nSrc0, F *pSrc1,int nSrc1, F *pDst)
{
	int i0,i1,n; F ares;
	for(i0=i1=n=0; isneg(i0-nSrc0) & isneg(i1-nSrc1); i0+=isneg(pSrc0[i0]-ares-1),i1+=isneg(pSrc1[i1]-ares-1)) {
		pDst[n] = ares = min(pSrc0[i0],pSrc1[i1]); n += iszero(pSrc0[i0]-pSrc1[i1]);
	}
	return n;
}


int qhull(strided_pointer<Vec3> pts, int npts, index_t*& pTris);

struct ptitem2d {
	vector2df pt;
	ptitem2d *next,*prev;
	int iContact;
};
struct edgeitem {
	ptitem2d *pvtx;
	ptitem2d *plist;
	edgeitem *next,*prev;
	float area,areanorm2;
	edgeitem *next1,*prev1;
	int idx;
};
int qhull2d(ptitem2d *pts,int nVtx, edgeitem *edges);

real ComputeMassProperties(strided_pointer<const Vec3> points, const index_t *faces, int nfaces, Vec3r &center,Matrix33r &I);

template<class ftype> 
void OffsetInertiaTensor(Matrix33_tpl<ftype> &I,const Vec3_tpl<ftype> &center,ftype M)
{
	I(0,0) += M*(center.y*center.y + center.z*center.z);
	I(1,1) += M*(center.x*center.x + center.z*center.z);
	I(2,2) += M*(center.x*center.x + center.y*center.y);
	I(1,0) = I(0,1) = I(0,1)-M*center.x*center.y;
	I(2,0) = I(0,2) = I(0,2)-M*center.x*center.z;
	I(2,1) = I(1,2) = I(1,2)-M*center.y*center.z;
}

enum booltype { bool_intersect=1 };

int boolean2d(booltype type, vector2df *ptbuf1,int npt1, vector2df *ptbuf2,int npt2,int bClosed, vector2df *&ptbuf,int *&pidbuf);

real RotatePointToPlane(const Vec3r &pt, const Vec3r &axis,const Vec3r &center, const Vec3r &n,const Vec3r &origin);

template<class ftype,int si,int sj> 
Matrix33_tpl<ftype> GetMtxStrided(const ftype *pdata) {
	Matrix33_tpl<ftype> res;
	res(0,0)=pdata[0*si+0*sj]; res(0,1)=pdata[0*si+1*sj]; res(0,2)=pdata[0*si+2*sj];
	res(1,0)=pdata[1*si+0*sj]; res(1,1)=pdata[1*si+1*sj]; res(1,2)=pdata[1*si+2*sj];
	res(2,0)=pdata[2*si+0*sj]; res(2,1)=pdata[2*si+1*sj]; res(2,2)=pdata[2*si+2*sj];
	return res;
}
template<class ftype,int si,int sj> 
void SetMtxStrided(const Matrix33_tpl<ftype> &mtx, ftype *pdata) {
	pdata[0*si+0*sj]=mtx(0,0); pdata[0*si+1*sj]=mtx(0,1); pdata[0*si+2*sj]=mtx(0,2);
	pdata[1*si+0*sj]=mtx(1,0); pdata[1*si+1*sj]=mtx(1,1); pdata[1*si+2*sj]=mtx(1,2);
	pdata[2*si+0*sj]=mtx(2,0); pdata[2*si+1*sj]=mtx(2,1); pdata[2*si+2*sj]=mtx(2,2);
}
template<class ftype> 
Matrix33_tpl<ftype> GetMtxFromBasis(const Vec3_tpl<ftype> *pBasis) { 
	Matrix33_tpl<ftype> res;
	res(0,0)=pBasis[0].x; res(0,1)=pBasis[0].y; res(0,2)=pBasis[0].z;
	res(1,0)=pBasis[1].x; res(1,1)=pBasis[1].y; res(1,2)=pBasis[1].z;
	res(2,0)=pBasis[2].x; res(2,1)=pBasis[2].y; res(2,2)=pBasis[2].z;
	return res;
}
template<class ftype> Matrix33_tpl<ftype> GetMtxFromBasisT(const Vec3_tpl<ftype> *pBasis) { 
	Matrix33_tpl<ftype> res;
	res(0,0)=pBasis[0].x; res(1,0)=pBasis[0].y; res(2,0)=pBasis[0].z;
	res(0,1)=pBasis[1].x; res(1,1)=pBasis[1].y; res(2,1)=pBasis[1].z;
	res(0,2)=pBasis[2].x; res(1,2)=pBasis[2].y; res(2,2)=pBasis[2].z;
	return res;
}
template<class ftype,class ftype1> void SetBasisFromMtx(Vec3_tpl<ftype1> *pBasis, const Matrix33_tpl<ftype> &mtx) { 
	pBasis[0].x=mtx(0,0); pBasis[0].y=mtx(0,1); pBasis[0].z=mtx(0,2);
	pBasis[1].x=mtx(1,0); pBasis[1].y=mtx(1,1); pBasis[1].z=mtx(1,2);
	pBasis[2].x=mtx(2,0); pBasis[2].y=mtx(2,1); pBasis[2].z=mtx(2,2);
}
template<class ftype,class ftype1> void SetBasisTFromMtx(Vec3_tpl<ftype1> *pBasis, const Matrix33_tpl<ftype> &mtx) { 
	pBasis[0].x=mtx(0,0); pBasis[0].y=mtx(1,0); pBasis[0].z=mtx(2,0);
	pBasis[1].x=mtx(0,1); pBasis[1].y=mtx(1,1); pBasis[1].z=mtx(2,1);
	pBasis[2].x=mtx(0,2); pBasis[2].y=mtx(1,2); pBasis[2].z=mtx(2,2);
}

template<class ftype> void TransposeBasis(Vec3_tpl<ftype> *axes) {
	ftype t;
	t=axes[0].y; axes[0].y=axes[1].x; axes[1].x=t;
	t=axes[0].z; axes[0].z=axes[2].x; axes[2].x=t;
	t=axes[1].z; axes[1].z=axes[2].y; axes[2].y=t;
}
template<class ftype> void IdentityBasis(Vec3_tpl<ftype> *axes) {
	axes[0].Set((ftype)1,(ftype)0,(ftype)0);
	axes[1].Set((ftype)0,(ftype)1,(ftype)0);
	axes[2].Set((ftype)0,(ftype)0,(ftype)1);
}

inline void LeftOffsetSpatialMatrix(matrixf &mtx, const Vec3 &offset){
	Matrix33 A=GetMtxStrided<float,6,1>(mtx.data); 
	Matrix33 B=GetMtxStrided<float,6,1>(mtx.data+3);
	Matrix33 C=GetMtxStrided<float,6,1>(mtx.data+18); 
	Matrix33 D=GetMtxStrided<float,6,1>(mtx.data+21); 
	Matrix33 rmtx;

	crossproduct_matrix(offset,rmtx);

	SetMtxStrided<float,6,1>(C+rmtx*A, mtx.data+18);
	SetMtxStrided<float,6,1>(D+rmtx*B, mtx.data+21);
}

inline void RightOffsetSpatialMatrix(matrixf &mtx, const Vec3 &offset){
	Matrix33 A=GetMtxStrided<float,6,1>(mtx.data), B=GetMtxStrided<float,6,1>(mtx.data+3),
						 C=GetMtxStrided<float,6,1>(mtx.data+18), D=GetMtxStrided<float,6,1>(mtx.data+21), rmtx;
	crossproduct_matrix(offset,rmtx);
	SetMtxStrided<float,6,1>(A+B*rmtx, mtx.data);
	SetMtxStrided<float,6,1>(C+D*rmtx, mtx.data+18);
}

template<class itype>
void ComputeMeshEigenBasis(strided_pointer<const Vec3> pVertices,strided_pointer<itype> pIndices,int nTris, Vec3r *eigen_axes,Vec3r &center)
{
	int i,j,k;
	Vec3r m,mean(ZERO),v[3];
	real s,t,sum=0, mtxbuf[9];
	matrix C(3,3, mtx_symmetric, mtxbuf); C.zero();

	// find mean point
	for(i=0; i<nTris*3; i+=3) {
		v[0] = pVertices[pIndices[i+0]];
		v[1] = pVertices[pIndices[i+1]];
		v[2] = pVertices[pIndices[i+2]];
		s = (v[1]-v[0] ^ v[2]-v[0]).len()*(real)0.5;
		mean += (v[0]+v[1]+v[2])*real(1.0/3)*s;
		sum += s;
	}
	if (sum==0) {
		center = nTris>0 ? pVertices[pIndices[0]] : Vec3(ZERO);
		IdentityBasis(eigen_axes);
		return;
	}
	mean /= sum;

	// calculate covariance matrix
	for(i=0; i<nTris*3; i+=3) {
		v[0] = pVertices[pIndices[i+0]]-mean;
		v[1] = pVertices[pIndices[i+1]]-mean;
		v[2] = pVertices[pIndices[i+2]]-mean;
		s = (v[1]-v[0] ^ v[2]-v[0]).len()*(real)0.5;
		m = v[0]+v[1]+v[2]; 
		for(j=0;j<3;j++) for(k=j;k<3;k++) {
			t = s*real(1.0/12)*(m[j]*m[k] + v[0][j]*v[0][k] + v[1][j]*v[1][k] + v[2][j]*v[2][k]);
			C[j][k] += t; if (k!=j) C[k][j] += t;
		}
	}

	// find eigenvectors of covariance matrix (normalized)
	real eval[3];
	matrix eigenBasis(3,3,0,eigen_axes[0]);
	C.jacobi_transformation(eigenBasis,eval,0);
	center = mean;
}

inline void SpatialTranspose(matrixf &src, matrixf& dst) {
	int i,j;
	dst.nRows = src.nCols; dst.nCols = src.nRows;
	for(i=0;i<src.nRows;i++) for(j=0;j<3;j++) {
		dst[j][i] = src[i][j+3];
		dst[j+3][i] = src[i][j];
	}
}

template<class ftype> inline Vec3_tpl<ftype> cross_with_ort(const Vec3_tpl<ftype> &vec, int iz) {
	Vec3_tpl<ftype> res(ZERO);
	int ix=inc_mod3[iz], iy=dec_mod3[iz];
	res[iz]=0; res[ix]=vec[iy]; res[iy]=-vec[ix];
	return res;
}

template<class dtype> dtype* ReallocateList(dtype *&plist, int szold,int sznew,bool bZero=false)
{
	dtype *newlist = new dtype[sznew];
	if (bZero)
		memset(newlist+szold,0,sizeof(dtype)*max(0,sznew-szold));
	memcpy(newlist,plist,min(szold,sznew)*sizeof(dtype));
	if (plist)
		delete[] plist;
	plist = newlist;
	return plist;
}

void get_xqs_from_matrices(Matrix34 *pMtx3x4,Matrix33 *pMtx3x3, Vec3 &pos,quaternionf &q,float &scale);

//int BreakPolygon(vector2df *ptSrc,int nPt, int nCellx,int nCelly, int maxPatchTris, vector2df *&ptout,int *&nPtOut, 
//								 float jointhresh=0.5f,int seed=-1);

void RasterizePolygonIntoCubemap(const Vec3 *pt,int npt, int iPass, int *pGrid[6],int nRes, float rmin,float rmax,float zscale);

void GrowAndCompareCubemaps(int *pGridOcc[6],int *pGrid[6],int nRes, int nGrow, int &nCells,int &nOccludedCells);

void CalcMediumResistance(const Vec3 *ptsrc,int npt, const Vec3& n, const plane &waterPlane,
													const Vec3 &vworld,const Vec3 &wworld,const Vec3 &com, Vec3 &P,Vec3 &L);

int CoverPolygonWithCircles(strided_pointer<vector2df> pt,int npt,bool bConsecutive, const vector2df &center, vector2df *&centers,
														float *&radii, float minCircleRadius);

int ChoosePrimitiveForMesh(strided_pointer<const Vec3> pVertices,strided_pointer<const unsigned short> pIndices,int nTris, const Vec3r *eigen_axes,
													 const Vec3r &center, int flags, float tolerance, primitive *&pprim);
void ExtrudeBox(const box *pbox, const Vec3& dir,float step, box *pextbox);

int TriangulatePoly(vector2df *pVtx, int nVtx, int *pTris, int szTriBuf);

template<class CellChecker> 
int DrawRayOnGrid(primitives::grid *pgrid, Vec3 &origin,Vec3 &dir, CellChecker &cell_checker) 
{
	int i,ishort,ilong,bStep,bPrevStep,ilastcell;
	float dshort,dlong,frac;
	quotientf tx[2],ty[2],t[2];
	vector2di istep,icell,idirsgn;

	// crop ray with grid bounds
	idirsgn.set(sgnnz(dir.x),sgnnz(dir.y));
	i = idirsgn.x;
	tx[1-i>>1].set(-origin.x*i, dir.x*i);
	tx[1+i>>1].set((pgrid->size.x*pgrid->step.x-origin.x)*i, dir.x*i);
	i = idirsgn.y;
	ty[1-i>>1].set(-origin.y*i, dir.y*i);
	ty[1+i>>1].set((pgrid->size.y*pgrid->step.y-origin.y)*i, dir.y*i);
	t[0] = max(t[0].set(0,1),max(tx[0],ty[0]));
	t[1] = min(t[1].set(1,1),min(tx[1],ty[1]));
	if (t[0]>=t[1])
		return 0;
	if (t[0]>0)
		origin += dir*t[0].val();
	if (t[0]>0 || t[1]<1)
		dir *= (t[1]-t[0]).val();

	ilong = isneg(fabs_tpl(dir.x)*pgrid->stepr.x-fabs_tpl(dir.y)*pgrid->stepr.y);
	ishort = ilong^1;
	dshort = fabs_tpl(dir[ishort])*pgrid->stepr[ishort]; 
	dlong  = fabs_tpl(dir[ilong])*pgrid->stepr[ilong];
	istep[ilong] = idirsgn[ilong];
	istep[ishort] = idirsgn[ishort];

	icell.set(float2int(origin.x*pgrid->stepr.x-0.5f), float2int(origin.y*pgrid->stepr.y-0.5f));
	icell.x = min(pgrid->size.x-1,max(0,icell.x)); icell.y = min(pgrid->size.y-1,max(0,icell.y)); 
	ilastcell = min(pgrid->size.y-1,max(0,float2int((origin.y+dir.y)*pgrid->stepr.y-0.5f)))<<16 | 
							min(pgrid->size.x-1,max(0,float2int((origin.x+dir.x)*pgrid->stepr.x-0.5f)));
	if (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell)
		return 1;
	if (fabs_tpl(dir[ilong])*pgrid->stepr[ilong]<0.0001f)
		return 0;

	t[0].set((icell[ilong]+(istep[ilong]+1>>1))*pgrid->step[ilong]-origin[ilong], dir[ilong]);
	frac = (origin[ishort]+dir[ishort]*t[0].val())*pgrid->stepr[ishort] - icell[ishort];
	frac = (1-idirsgn[ishort]>>1)+frac*idirsgn[ishort];
	if (frac>1.0f) {
		icell[ishort] += istep[ishort];
		if (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell)
			return 1;
		frac -= 1;
	}
	frac *= dlong;
	icell[ilong] += istep[ilong];

	bPrevStep = 0;
	do {
		if (cell_checker.check_cell(icell,ilastcell) || (icell.y<<16|icell.x)==ilastcell)
			return 1;
		frac += dshort*(bPrevStep^1); bStep = isneg(dlong-frac);
		icell[ishort] += bStep*istep[ishort]; frac -= dlong*bStep;
		icell[ilong] += istep[ilong]&~-bStep; 
		bPrevStep = bStep;
	} while(true);

	return 0;
}


struct jgrid_checker {
	grid *pgrid;
	char *pCellMask;
	vector2df org,dir,dirn,*ppt,*pnorms;
	int iedge[2],iedgeExit0;
	int iprevcell;
	int bMarkCenters;

	int check_cell(const vector2di &icell, int &ilastcell) {
		int i,idx,icurcell,mask=2,bhit;
		vector2df c,step=pgrid->step,pt,pt0(pgrid->origin.x,pgrid->origin.y);

		idx = icell*pgrid->stride;
		pCellMask[idx] |= 2;	// mark the cell as used
		c.set(step.x*(icell.x+0.5f), step.y*(icell.y+0.5f));
		icurcell = icell.y<<16|icell.x;
		pCellMask[idx] |= bMarkCenters & isneg((c-org)*dir); // set a bit if the cell's center is outside the current edge
		if (pnorms)
			pnorms[idx] += dirn.rot90cw();

		if ((icurcell|iprevcell>>31)!=iprevcell) { // false if this is the first edge cell
			if (iedge[0]!=(iedge[1]|iedge[0]>>31)) for(i=iedge[1]+1&3; i!=iedge[0]; i=i+1&3)
				mask |= 4<<i;
			pCellMask[vector2di(iprevcell&0xFFFF,iprevcell>>16)*pgrid->stride] &= mask;
			for(i=0;i<2;i++) { // compute the enter point
				bhit = -inrange((dirn^org-c)*(1-i*2)-fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[0] &= ~bhit) |= (i^1 | (i^isnonneg(dirn[i]))<<1) & bhit;
			}
			if (ppt && icurcell!=ilastcell) {
				for(pt=org+dirn*(dirn*(c-org)),i=0; (fabs_tpl(pt.x-c.x)>step.x*0.5f || fabs_tpl(pt.y-c.y)>step.y*0.5f) && i<4; i++)
					pt = org+dirn*(dirn*(c+vector2df(step.x*(0.5f-(i&1)),0)+vector2df(0,step.y*(0.5f-(i>>1)))-org));
				ppt[idx] = pt+pt0;
			}
		}	else if (ppt)
			ppt[idx] = org+pt0;
		if (icurcell!=ilastcell)	{	// false if this is the last edge cell
			for(i=0;i<2;i++) { // compute the leave point
				bhit = -inrange((dirn^org-c)*(1-i*2)+fabs_tpl(dirn[i^1])*step[i], -dirn[i]*step[i^1]*1.001f,dirn[i]*step[i^1]*1.001f);
				(iedge[1] &= ~bhit) |= (i^1 | (i^isneg(dirn[i]))<<1) & bhit;
			}
			iedgeExit0 += iedge[1]+1 & iedgeExit0>>31;
		}	else if (ppt)
			ppt[idx] = org+dir+pt0;

		iprevcell = icurcell;
		return 0;
	}

	int *pqueue;
	int ihead,itail,nQueuedCells;
	int szQueue;

	void MarkCellInterior(int i) {
		int j,icell; pCellMask[i] = 2;
		for(j=0;j<4;j++) if (!(pCellMask[icell = i+vector2di(1-(j&2) & -(j&1), (j&2)-1 & ~-(j&1))*pgrid->stride] & 2))
			MarkCellInterior(icell);
	}

	void MarkCellInteriorQueue(int icell)
	{
		int j,icellNext;
		pCellMask[pqueue[ihead=itail=0] = icell] = 2;
		for(nQueuedCells=1; nQueuedCells>0; ) {
			icell = pqueue[itail]; itail = itail+1 & itail+1-szQueue>>31;	nQueuedCells--;
			for(j=0;j<4;j++) if (!(pCellMask[icellNext = icell+vector2di(1-(j&2) & -(j&1), (j&2)-1 & ~-(j&1))*pgrid->stride] & 2)) {
				pCellMask[icellNext] = 2; 
				if (nQueuedCells==szQueue) {
					ReallocateList(pqueue,szQueue,szQueue+64);
					memmove(pqueue+itail+64, pqueue+itail, (szQueue-ihead-1)*sizeof(pqueue[0]));
					if (itail>0) itail+=64;	szQueue+=64;
				}
				ihead = ihead+1 & ihead+1-szQueue>>31; nQueuedCells++;
				pqueue[ihead] = icellNext;
			}
		}
	}
};


int bin2ascii(const unsigned char *pin,int sz, unsigned char *pout);
int ascii2bin(const unsigned char *pin,int sz, unsigned char *pout);

template<class T> inline T *_align16(T *ptr) { return (T*)(((INT_PTR)ptr-1&~15)+16); }

inline bool is_valid(float op) { return op*op>=0 && op*op<1E30f; }
inline bool is_valid(int op) { return true; }
inline bool is_valid(unsigned int op) { return true; }
inline bool is_valid(const Quat& op) { return is_valid(op|op); }

template<class dtype> bool is_valid(const dtype &op) { return is_valid(op.x*op.x + op.y*op.y + op.z*op.z); }

void WritePacked(CStream &stm, int num);
void WritePacked(CStream &stm, uint64 num);
void ReadPacked(CStream &stm,int &num);
void ReadPacked(CStream &stm,uint64 &num);
void WriteCompressedPos(CStream &stm, const Vec3 &pos, bool bCompress=true);
void ReadCompressedPos(CStream &stm, Vec3 &pos, bool &bWasCompressed);
Vec3 CompressPos(const Vec3 &pos);
const int CMP_QUAT_SZ = 49;
void WriteCompressedQuat(CStream &stm, const quaternionf &q);
void ReadCompressedQuat(CStream &stm,quaternionf &q);
quaternionf CompressQuat(const quaternionf &q);
const int CMP_VEL_SZ = 48;
inline void WriteCompressedVel(CStream &stm, const Vec3 &vel, const float maxvel) {
	stm.Write((short)max(-32768,min(32767,float2int(vel.x*(32767.0f/maxvel)))));
	stm.Write((short)max(-32768,min(32767,float2int(vel.y*(32767.0f/maxvel)))));
	stm.Write((short)max(-32768,min(32767,float2int(vel.z*(32767.0f/maxvel)))));
}
inline void ReadCompressedVel(CStream &stm, Vec3 &vel, const float maxvel) {
	short i;
	stm.Read(i); vel.x = i*(maxvel/32767.0f);
	stm.Read(i); vel.y = i*(maxvel/32767.0f);
	stm.Read(i); vel.z = i*(maxvel/32767.0f);
}
inline Vec3 CompressVel(const Vec3 &vel, const float maxvel) {
	Vec3 res;
	res.x = max(-32768,min(32767,float2int(vel.x*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	res.y = max(-32768,min(32767,float2int(vel.y*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	res.z = max(-32768,min(32767,float2int(vel.z*(32767.0f/maxvel)))) * (maxvel/32767.0f);
	return res;
}
inline void WriteCompressedFloat(CStream &stm, float f, const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	stm.WriteNumberInBits(max(-imax-1,min(imax,float2int(f*(imax/maxval)))),nBits);
}
inline void ReadCompressedFloat(CStream &stm, float &f, const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	unsigned int num;
	stm.ReadNumberInBits(num,nBits);
	f = ((int)num | ((int)num<<32-nBits & 1<<31)>>31-nBits)*(maxval/imax);
}
inline float CompressFloat(float f,const float maxval,const int nBits) {
	const int imax = (1<<nBits-1)-1;
	return max(-imax-1,min(imax,float2int(f*(imax/maxval)))) * (maxval/imax);
}


inline intptr_t iszero_mask(void *ptr) { return (intptr_t)ptr>>sizeof(intptr_t)*8-1 ^ (intptr_t)ptr-1>>sizeof(intptr_t)*8-1; }

template<class ftype> inline int AABB_overlap(Vec3_tpl<ftype> *BBox0,Vec3_tpl<ftype> *BBox1) {
	return isneg(fabs_tpl(BBox0[0].x+BBox0[1].x-BBox1[0].x-BBox1[1].x) - (BBox0[1].x-BBox0[0].x)-(BBox1[1].x-BBox1[0].x)) & 
				 isneg(fabs_tpl(BBox0[0].y+BBox0[1].y-BBox1[0].y-BBox1[1].y) - (BBox0[1].y-BBox0[0].y)-(BBox1[1].y-BBox1[0].y)) &
				 isneg(fabs_tpl(BBox0[0].z+BBox0[1].z-BBox1[0].z-BBox1[1].z) - (BBox0[1].z-BBox0[0].z)-(BBox1[1].z-BBox1[0].z));
}

// returns !0 if the second box fits inside the first box
// Assumes the given boxes are properly ordered
template<class ftype> inline int AABB_contains(const Vec3_tpl<ftype> *container, const Vec3_tpl<ftype> *test) {
	return isneg(container[0].x - test[0].x) & isneg(container[0].y - test[0].y) & isneg(container[0].z - test[0].z) &
		   isneg(test[1].x - container[1].x) & isneg(test[1].y - container[1].y) & isneg(test[1].z - container[1].z);
}

template<class ftype> inline int PtInAABB(Vec3_tpl<ftype> *BBox0,const Vec3_tpl<ftype> &pt) {
	return isneg(fabs_tpl(BBox0[0].x+BBox0[1].x-pt.x*2) - (BBox0[1].x-BBox0[0].x)) & 
				 isneg(fabs_tpl(BBox0[0].y+BBox0[1].y-pt.y*2) - (BBox0[1].y-BBox0[0].y)) &
				 isneg(fabs_tpl(BBox0[0].z+BBox0[1].z-pt.z*2) - (BBox0[1].z-BBox0[0].z));
}


inline int check_mask(unsigned int *pMask, int i) {
	return pMask[i>>5]>>(i&31) & 1;
}
inline void set_mask(unsigned int *pMask, int i) {
	pMask[i>>5] |= 1u<<(i&31);
}
inline void clear_mask(unsigned int *pMask, int i) {
	pMask[i>>5] &= ~(1u<<(i&31));
}


extern int g_bitcount[256];
const int SINCOSTABSZ = 1024, SINCOSTABSZ_LOG2 = 10;
extern float g_costab[SINCOSTABSZ],g_sintab[SINCOSTABSZ];

struct subref {
	void set(int _iPtrOffs, int _iSzFixed, int _iSzOffs, int _iSzUnit, subref *_next)	{
		iPtrOffs=_iPtrOffs; iSzFixed=_iSzFixed; iSzOffs=_iSzOffs; iSzUnit=_iSzUnit; next=_next;
	}
	int iPtrOffs;
	int iSzFixed;
	int iSzOffs;
	int iSzUnit;
	subref *next;
};

// spinlocks
/*inline void SpinLock(volatile int *pLock,int checkVal,int setVal) { 
#ifdef _CPU_X86
	__asm {
	mov edx, setVal
	mov ecx, pLock
	Spin:
		mov eax, checkVal
		lock cmpxchg [ecx], edx
	jnz Spin }
#else
	while(_InterlockedCompareExchange(pLock,setVal,checkVal)!=checkVal);
#endif
}

inline void AtomicAdd(volatile int *pVal, int iAdd) {
#ifdef _CPU_X86
	__asm {
		mov edx, pVal
		mov eax, iAdd
		lock add [edx], eax
	}
#else
	_InterlockedExchangeAdd(pVal,iAdd);
#endif
}*/

inline void SpinLock(volatile int *pLock,int checkVal,int setVal) { CrySpinLock(pLock,checkVal,setVal); } 
inline void AtomicAdd(volatile int *pVal, int iAdd) {	CryInterlockedAdd(pVal,iAdd); }
inline void AtomicAdd(volatile unsigned int *pVal, int iAdd) { CryInterlockedAdd((volatile int*)pVal,iAdd); }

/*struct ReadLock {
	volatile int *prw;
	ReadLock(volatile int &rw) { 
		AtomicAdd(prw=&rw,1); 
		volatile char *pw=(volatile char*)&rw+2; for(;*pw;); 
	}
	~ReadLock() { AtomicAdd(prw,-1); }
};

struct ReadLockCond {
	volatile int *prw;
	int bActivated;
	ReadLockCond(volatile int &rw,int bActive) { 
		if (bActive) { 
			AtomicAdd(&rw,1);	bActivated = 1;
			volatile char *pw=(volatile char*)&rw+2; for(;*pw;); 
		} else bActivated = 0;
		prw = &rw; 
	}
	~ReadLockCond() { AtomicAdd(prw,-bActivated); }
};

struct WriteLock {
	volatile int *prw;
	WriteLock(volatile int &rw) { SpinLock(&rw,0,WRITE_LOCK_VAL); prw=&rw; }
	~WriteLock() { AtomicAdd(prw,-WRITE_LOCK_VAL); }
};

struct WriteLockCond {
	volatile int *prw;
	int iActive;
	WriteLockCond(volatile int &rw,int bActive) { 
		if (bActive)
			SpinLock(&rw,0,iActive=WRITE_LOCK_VAL);
		else 
			iActive = 0;
		prw = &rw; 
	}
	~WriteLockCond() { AtomicAdd(prw,-iActive); }
};*/

// uncomment the following block to effectively disable validations
/*#define VALIDATOR_LOG(pLog,str)
#define VALIDATORS_START
#define VALIDATOR(member)
#define VALIDATOR_NORM(member)
#define VALIDATOR_RANGE(member,minval,maxval)
#define VALIDATOR_RANGE2(member,minval,maxval)
#define VALIDATORS_END
#define ENTITY_VALIDATE(strSource,pStructure)*/
#if defined(WIN64) || defined(LINUX64)
#define DoBreak {assert(0);}
#else
#define DoBreak { DEBUG_BREAK; }
#endif

#define VALIDATOR_LOG(pLog,str) if (pLog) CryLog(str) //OutputDebugString(str)
#define VALIDATORS_START bool validate( const char *strSource, ILog *pLog, const Vec3 &pt,\
	IPhysRenderer *pStreamer, void *param0, int param1, int param2 ) { bool res=true; char errmsg[1024];
#define VALIDATOR(member) if (!is_unused(member) && !is_valid(member)) { \
	res=false; _snprintf(errmsg,sizeof errmsg,"%s: (%.50s @ %.1f,%.1f,%.1f) Validation Error: %s is invalid",strSource,\
	pStreamer?pStreamer->GetForeignName(param0,param1,param2):"",pt.x,pt.y,pt.z,#member);errmsg[sizeof errmsg - 1] = 0; \
	VALIDATOR_LOG(pLog,errmsg); } 
#define VALIDATOR_NORM(member) if (!is_unused(member) && !(is_valid(member) && fabs_tpl((member|member)-1.0f)<0.01f)) { \
	res=false; _snprintf(errmsg,sizeof errmsg,"%s: (%.50s @ %.1f,%.1f,%.1f) Validation Error: %s is invalid or unnormalized",\
	strSource,pStreamer?pStreamer->GetForeignName(param0,param1,param2):"",pt.x,pt.y,pt.z,#member);errmsg[sizeof errmsg - 1] = 0; VALIDATOR_LOG(pLog,errmsg); }
#define VALIDATOR_NORM_MSG(member,msg,member1) if (!is_unused(member) && !(is_valid(member) && fabs_tpl((member|member)-1.0f)<0.01f)) { \
	res=false; _snprintf(errmsg,sizeof errmsg,"%s: (%.50s @ %.1f,%.1f,%.1f) Validation Error: %s is invalid or unnormalized %s",\
	strSource,pStreamer?pStreamer->GetForeignName(param0,param1,param2):"",pt.x,pt.y,pt.z,#member,msg);errmsg[sizeof errmsg - 1] = 0; \
	if (!is_unused(member1)) { _snprintf(errmsg+strlen(errmsg),sizeof errmsg - strlen(errmsg)," "#member1": %.1f,%.1f,%.1f",member1.x,member1.y,member1.z);errmsg[sizeof errmsg - 1] = 0;} \
	VALIDATOR_LOG(pLog,errmsg); }
#define VALIDATOR_RANGE(member,minval,maxval) if (!is_unused(member) && !(is_valid(member) && member>=minval && member<=maxval)) { \
	res=false; _snprintf(errmsg,sizeof errmsg,"%s: (%.50s @ %.1f,%.1f,%.1f) Validation Error: %s is invalid or out of range",\
	strSource,pStreamer?pStreamer->GetForeignName(param0,param1,param2):"",pt.x,pt.y,pt.z,#member);errmsg[sizeof errmsg - 1] = 0; VALIDATOR_LOG(pLog,errmsg); }
#define VALIDATOR_RANGE2(member,minval,maxval) if (!is_unused(member) && !(is_valid(member) && member*member>=minval*minval && \
		member*member<=maxval*maxval)) { \
	res=false; _snprintf(errmsg,sizeof errmsg,"%s: (%.50s @ %.1f,%.1f,%.1f) Validation Error: %s is invalid or out of range",\
	strSource,pStreamer?pStreamer->GetForeignName(param0,param1,param2):"",pt.x,pt.y,pt.z,#member);errmsg[sizeof errmsg - 1] = 0; VALIDATOR_LOG(pLog,errmsg); }
#define VALIDATORS_END return res; }

#define ENTITY_VALIDATE(strSource,pStructure) if (!pStructure->validate(strSource,m_pWorld->m_pLog,m_pos,\
	m_pWorld->m_pRenderer,m_pForeignData,m_iForeignData,m_iForeignFlags)) { \
	if (m_pWorld->m_vars.bBreakOnValidation) DoBreak return 0; }
#define ENTITY_VALIDATE_ERRCODE(strSource,pStructure,iErrCode) if (!pStructure->validate(strSource,m_pWorld->m_pLog,m_pos, \
	m_pWorld->m_pRenderer,m_pForeignData,m_iForeignData,m_iForeignFlags)) { \
	if (m_pWorld->m_vars.bBreakOnValidation) DoBreak return iErrCode; }


//////////////////////////////////////////////////////////////////////////
// Return tag name combined with number, ex: Name_1, Name_2
//////////////////////////////////////////////////////////////////////////
inline const char* numbered_tag(const char *s,unsigned int num)
{
	static char str[64];
	assert( strlen(s) < 20 );
	strcpy( str,s );
	sprintf( str+strlen(s),"_%d",num );
	return str;
}

#endif
