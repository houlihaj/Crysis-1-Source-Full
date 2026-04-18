
#ifndef AI_HASH_H
#define AI_HASH_H

//===================================================================
// HashFromFloat
//===================================================================
inline unsigned int HashFromFloat(float key, float tol)
{
	float val = key;
	if (tol > 0.0f)
	{
		val /= tol;
		val = floor(val);
		val *= tol;
	}
	unsigned hash = *(unsigned * )&val;
	hash += ~(hash << 15);
	hash ^= (hash >> 10);
	hash += (hash << 3);
	hash ^= (hash >> 6);
	hash += ~(hash << 11);
	hash ^= (hash >> 16);
	return hash;
}

//===================================================================
// HashFromVec3
//===================================================================
inline unsigned int HashFromVec3(const Vec3 &v, float tol)
{
	return HashFromFloat(v.x, tol) + HashFromFloat(v.y, tol) + HashFromFloat(v.z, tol);
}

//===================================================================
// HashFromQuat
//===================================================================
inline unsigned int HashFromQuat(const Quat &q, float tol)
{
	return HashFromFloat(q.v.x, tol) + HashFromFloat(q.v.y, tol) + HashFromFloat(q.v.z, tol) + HashFromFloat(q.w, tol);
}

//===================================================================
// GetHashFromEntities
//===================================================================
inline unsigned GetHashFromEntities(IPhysicalEntity *entities[], unsigned nEntities)
{
	unsigned hash = 0;
	pe_status_pos status;
	IPhysicalWorld *pPhysics = gEnv->pPhysicalWorld;
	for (unsigned i = 0 ; i < nEntities ; ++i)
	{
		IPhysicalEntity *pEntity = entities[i];
		hash += (unsigned)(UINT_PTR)pEntity;
		pEntity->GetStatus(&status);
		hash += HashFromVec3(status.pos, 0.05f);
		hash += HashFromQuat(status.q, 0.01f);
	}
	if (hash == 0)
		hash = 1;
	return hash;
}

#endif // AI_HASH_H
