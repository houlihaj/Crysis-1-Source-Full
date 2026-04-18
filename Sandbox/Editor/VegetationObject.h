// StaticObjParam.h: interface for the CVegetationObject class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STATICOBJPARAM_H__A3F2DD70_73A9_47E1_9B0F_C650E5A16838__INCLUDED_)
#define AFX_STATICOBJPARAM_H__A3F2DD70_73A9_47E1_9B0F_C650E5A16838__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Material\Material.h"

// Static object structure for the final tree output list
struct StatObj
{
	USHORT iX;
	USHORT iY;
	//float scale;
	unsigned char iStatObjTypeIndex;
	unsigned char brightness;
};

#pragma pack(push,1)
// for saving/loading 
struct	Vegetationance
{
	//static_obj_info_struct() {}
	//  static_obj_info_struct(unsigned short _x, unsigned short _y, unsigned char _id)
	//{ x=_x; y=_y; id=_id; }
	//  unsigned short x,y;
	//unsigned char id;
	/*
	float GetX() { return float(x)*WORLD_SIZE/65536.f; }
	float GetY() { return float(y)*WORLD_SIZE/65536.f; }
	float GetZ() { return float(z)*WORLD_SIZE/65536.f; }
	*/
	float GetScale() { return scale; }
	int GetID() { return id; }
	/*
	Vec3d GetAgles() 
	{ 
	return Vec3d( float(angles[0])*360.f/256.f, 
	float(angles[1])*360.f/256.f, 
	float(angles[2])*360.f/256.f);
	}
	*/

	void SetXYZ( float vx,float vy,float vz,int worldSize )
	{
		x = ftoi(vx*65536/worldSize);
		y = ftoi(vy*65536/worldSize);
		z = ftoi(vz*65536/worldSize);
	}
	void SetScale( float s ) { scale = s; }
	void SetID( int _id ) { id = _id; };
	void SetBrightness( uchar br ) { brightness = br; };
	void SetAngle( uchar nAngle ) { angle = nAngle; };

	/*
	void SetAgles( float x,float y,float z )
	{ 
	angles[0] = x*(256.0f/360.0f);
	angles[1] = y*(256.0f/360.0f);
	angles[2] = z*(256.0f/360.0f);
	reserved = 0;
	}
	*/

protected:
	float scale;
	unsigned short x,y,z;
	uchar id;
	uchar brightness;
	uchar angle;
};
#pragma pack(pop)

// List of trees
typedef std::vector<StatObj> StatObjArray;

class CXmlArchive;
class CVegetationMap;

//////////////////////////////////////////////////////////////////////////
/** Description of vegetation object which can be place on the map.
*/
class CVegetationObject : public CVarObject
{
public:
	CVegetationObject( int id,CVegetationMap *pVegMap );
	virtual ~CVegetationObject();

	void Serialize( XmlNodeRef &node,bool bLoading );

	// Member access
	const char* GetFileName() const { return m_strFileName; };
	void SetFileName( const CString &strFileName );
	
	//! Get this vegetation object GUID.
	REFGUID GetGUID() const { return m_guid; }

	//! Set current object category.
	void SetCategory( const CString &category );
	//! Get object's category.
	const char* GetCategory() const { return m_category; };

	//! Temporary unload IStatObj.
	void UnloadObject();
	//! Load IStatObj again.
	void LoadObject();

	float GetSize() { return mv_size; };
	void SetSize(float fSize) { mv_size = fSize; };

	float GetSizeVar() { return mv_sizevar; };
	void SetSizeVar(float fSize) { mv_sizevar = fSize; };

	//! Get actual size of object.
	float GetObjectSize() const { return m_objectSize; }

	//////////////////////////////////////////////////////////////////////////
	// Operators.
	//////////////////////////////////////////////////////////////////////////
	void SetElevation( float min,float max ) { mv_hmin = min; mv_hmax = max; };
	void SetSlope( float min,float max ) { mv_slope_min = min; mv_slope_max = max; };
	void SetDensity( float dens ) { mv_density = dens; };
	void SetSelected( bool bSelected );
	void SetHidden( bool bHidden );
	void SetNumInstances( int numInstances);
	void SetBending(float fBending) { mv_bending = fBending; }
  void SetFrozen(bool bFrozen) { mv_layerFrozen = bFrozen; }
	void SetInUse(bool bNewState) { m_bInUse = bNewState; };

	//////////////////////////////////////////////////////////////////////////
	// Accessors.
	//////////////////////////////////////////////////////////////////////////
	float GetElevationMin() const { return mv_hmin; };
	float GetElevationMax() const { return mv_hmax; };
	float GetSlopeMin() const { return mv_slope_min; };
	float GetSlopeMax() const { return mv_slope_max; };
	float GetDensity() const { return mv_density; };
	bool	IsHidden() const { return m_bHidden; };
	bool	IsSelected() const { return m_bSelected; }
	int		GetNumInstances() { return m_numInstances; };
	float GetBending() { return mv_bending; };
  bool  GetFrozen() const { return mv_layerFrozen; };
	bool	GetInUse() { return m_bInUse; };

	//////////////////////////////////////////////////////////////////////////
	void SetIndex( int i ) { m_index = i; };
	int GetIndex() const { return m_index; };

	//! Set is object cast shadow.
	void SetCastShadow( bool on ) { mv_castShadows = on; }
	//! Check if object casting shadow.
	bool IsCastShadow() const { return mv_castShadows; };

	//! Set if object recieve shadow.
	void SetReceiveShadow( bool on ) { mv_recvShadows = on; }
	//! Check if object recieve shadow.
	bool IsReceiveShadow() const { return mv_recvShadows; };

	//////////////////////////////////////////////////////////////////////////
	void SetPrecalcShadow( bool on ) { mv_precalcShadows = on; }
	bool IsPrecalcShadow() const { return mv_precalcShadows; };

	//! Set to true if characters can hide behind this object.
	void SetHideable( bool on ) { mv_hideable = on; }
	//! Check if chracters can hide behind this object.
	bool IsHideable() const { return mv_hideable; };

  //! if < 0 then AI calculates the navigation radius automatically
  //! if >= 0 then AI uses this radius
  float GetAIRadius() const {return mv_aiRadius;}
  void SetAIRadius(float radius) {mv_aiRadius = radius;}

	bool IsRandomRotation() const { return mv_rotation; }
	bool IsUseSprites() const { return mv_UseSprites; }
	bool IsAlignToTerrain() const { return mv_alignToTerrain; }
	bool IsUseTerrainColor() const { return mv_useTerrainColor; }
  bool IsAffectedByVoxels() const { return mv_affectedByVoxels; }

	//////////////////////////////////////////////////////////////////////////
	void SetAlphaBlend( bool bEnable ) { mv_alphaBlend = bEnable; }
	bool IsAlphaBlend() const { return mv_alphaBlend; };

	//! Copy all parameters from specified vegetation object.
	void CopyFrom( const CVegetationObject &o );

	//! Return pointer to static object.
	IStatObj*	GetObject() { return m_statObj; };

	//! Return true when the brush can paint on a location with the supplied parameters
	bool IsPlaceValid( float height,float slope ) const;

	//! Calculate variable size for this object.
	float CalcVariableSize() const;

	//! Id of this object.
	int GetId() const { return m_id; };

	void Validate( CErrorReport &report );

	void GetTerrainLayers( std::vector<CString> &layers ) { layers = m_terrainLayers; };
	void SetTerrainLayers( const std::vector<CString> &layers ) { m_terrainLayers = layers; };
	bool IsUsedOnTerrainLayer( const CString &layer );

	// Handle changing of the current configuration spec.
	void OnConfigSpecChange();

	// Return texture memory used by this object.
	int GetTextureMemUsage( ICrySizer *pSizer=NULL );

protected:
	void SetEngineParams();
	void OnVarChange( IVariable *var );
	void OnMaterialChange( IVariable *var );
	void UpdateMaterial();

	CVegetationObject( const CVegetationObject & o ) {};
	CVegetationObject& operator=( const CVegetationObject &o ) {};

	GUID m_guid;

	CVegetationMap *m_vegetationMap;
	//! Index of object in editor.
	int m_index;

	//! Filename of the associated CGF file
	CString m_strFileName;
	//! Objects category.
	CString m_category;

	//! Used during generation ?
	bool m_bInUse;

	//! True if all instances of this object must be hidden.
	bool m_bHidden;

	//! True if Selected.
	bool m_bSelected;

	//! Real size of geometrical object.
	float m_objectSize;

	//! Number of instances of this vegetation object placed on the map.
	int m_numInstances;

	//! Current group Id of this object (Need not saving).
	int m_id;

	//! Pointer to object for this group.
	IStatObj* m_statObj;

	TSmartPtr<CMaterial> m_pMaterial;

	// Place on terrain layers.
	std::vector<CString> m_terrainLayers;

	bool m_bVarIgnoreChange;
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////  
	CSmartVariable<float>	  mv_density;
	CSmartVariable<float>	  mv_hmin;
	CSmartVariable<float>	  mv_hmax;
	CSmartVariable<float>	  mv_slope_min;
	CSmartVariable<float>	  mv_slope_max;
	CSmartVariable<float>	  mv_size;
	CSmartVariable<float>	  mv_sizevar;
	CSmartVariable<bool>		mv_castShadows;
	CSmartVariable<bool>		mv_recvShadows;
	CSmartVariable<bool>		mv_precalcShadows;
	CSmartVariable<float>	  mv_bending;
	CSmartVariableEnum<int>		mv_hideable;
  CSmartVariable<bool>		mv_pickable;
	CSmartVariable<float>		mv_aiRadius;
	CSmartVariable<bool>		mv_alphaBlend;
	CSmartVariable<float>	  mv_SpriteDistRatio;
	CSmartVariable<float>	  mv_ShadowDistRatio;
	CSmartVariable<float>	  mv_MaxViewDistRatio;
	CSmartVariable<float>	  mv_brightness;
	CSmartVariable<CString>	mv_material;
  CSmartVariable<bool>	  mv_UseSprites;
	CSmartVariable<bool>	  mv_rotation;
	CSmartVariable<bool>	  mv_alignToTerrain;
	CSmartVariable<bool>	  mv_useTerrainColor;
  CSmartVariable<bool>	  mv_affectedByVoxels;
  
	CSmartVariable<bool>		mv_layerFrozen;
	CSmartVariable<bool>		mv_layerWet;
	//CSmartVariable<bool>		mv_layerCloak;
	//CSmartVariable<bool>		mv_layerBurned;

	CSmartVariableEnum<int> mv_minSpec;

	friend class CVegetationPanel;
	friend class CVegetationMap;
};


//////////////////////////////////////////////////////////////////////////
inline bool CVegetationObject::IsPlaceValid( float height,float slope ) const
{
	if (height < mv_hmin || height > mv_hmax)
		return false;
	if (slope < mv_slope_min || slope > mv_slope_max)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
inline float CVegetationObject::CalcVariableSize() const
{
	if (mv_sizevar == 0)
	{
		return mv_size;
	}
	else
	{
		float fval = mv_sizevar * ((2.0f*(float)rand()/RAND_MAX) - 1.0f);
		if (fval >= 0)
		{
			return mv_size*(1.0f + fval);
		}
		else
		{
			return mv_size/(1.0f - fval);
		}
	}
}

#endif // !defined(AFX_STATICOBJPARAM_H__A3F2DD70_73A9_47E1_9B0F_C650E5A16838__INCLUDED_)