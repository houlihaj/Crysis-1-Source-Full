//--------------------------------------------------------------------------
// Render element for GPU particle systems
//  Created:     [ATI / jisidoro]
//--------------------------------------------------------------------------
#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS

#ifndef __CREPARTICLEGPU_H__
#define __CREPARTICLEGPU_H__

class CREParticleGPU : public CRendElement
{
public:
	virtual ~CREParticleGPU() {};

	//static   CREParticleGPU* Create( IParticleVertexCreator* pVC, AABB const& bb );
	static   CREParticleGPU* Create( int32 nGPUParticleIdx, AABB const& bb );

	// interface
	virtual void  mfPrepare();

	virtual bool  mfCullBox( Vec3& vmin, Vec3& vmax );
	virtual float mfDistanceToCameraSquared( Matrix34& matInst );
	virtual CRendElement* mfCopyConstruct();
	
	// compile shaders for this pass
	//virtual bool mfCompile(CShader *ef, char *scr);
	virtual bool mfPreDraw( SShaderPass *sl ); //

	virtual bool mfDraw( CShader *ef, SShaderPass *sfm );

	//possible funcs to override from base class
/*
	  virtual void mfPrepare();
	  virtual bool mfCullByClipPlane(CRenderObject *pObj);
	  virtual CRenderChunk *mfGetMatInfo();
	  virtual PodArray<CRenderChunk> *mfGetMatInfoList();
	  virtual int mfGetMatId();
	  virtual void mfReset();
	  virtual bool mfIsHWSkinned() { return false; }
	  virtual CRendElement *mfCopyConstruct(void);
	  virtual void mfCenter(Vec3& centr, CRenderObject*pObj);
	  virtual void mfGetBBox(Vec3& vMins, Vec3& vMaxs)
	  {
		vMins.Set(0,0,0);
		vMaxs.Set(0,0,0);
	  }
	  virtual void mfGetPlane(Plane& pl);
	  virtual float mfDistanceToCameraSquared(Matrix34& matInst);
	  virtual void mfEndFlush();
	  virtual int  mfTransform(Matrix44& ViewMatr, Matrix44& ProjMatr, vec4_t *verts, vec4_t *vertsp, int Num);
	  virtual bool mfIsValidTime(CShader *ef, CRenderObject *obj, float curtime);
	  virtual void mfBuildGeometry(CShader *ef);
	  virtual bool mfCompile(CShader *ef, char *scr);
	  virtual void *mfGetPointer(ESrcPointer ePT, int *Stride, EParamType Type, ESrcPointer Dst, int Flags);
	  virtual bool mfPreDraw(SShaderPass *sl) { return true; }
	  virtual float mfMinDistanceToCamera(Matrix34& matInst) {return -1;};
	  virtual bool mfCheckUpdate(int nVertFormat, int Flags) {return true;}
	  virtual void mfPrecache(const SShaderItem& SH) {}
	  virtual int Size() {return 0;}
*/

private:
	
	//IParticleVertexCreator*		m_pVertexCreator;		//Probably isn't needed since this is all done on the GPU side
	AABB						m_BoundingBox;
	int32						m_nGPUParticleIdx;			//Index of GPU particle system within the arrays

};


#endif  // __CREPARTICLE_H__

#endif
