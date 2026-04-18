/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 09:05:2005   11:08 : Created by Carsten Wenzel

*************************************************************************/

#ifndef _SKY_LIGHT_NISHITA_H_
#define _SKY_LIGHT_NISHITA_H_

#pragma once


#include <vector>


class CSkyLightNishita : public Cry3DEngineBase
{
public:
	CSkyLightNishita();
	~CSkyLightNishita();

	// set sky dome conditions
	void SetAtmosphericConditions( const Vec3& sunIntensity, const f32& Km, const f32& Kr, const f32& g );
	void SetRGBWaveLengths( const Vec3& rgbWaveLengths );
	void SetSunDirection( const Vec3& sunDir );
	
	// compute sky colors
	void ComputeSkyColor( const Vec3& skyDir, Vec3* pInScattering, Vec3* pInScatteringMieNoPremul, Vec3* pInScatteringRayleighNoPremul, Vec3* pInScatteringRayleigh ) const;

	void SetInScatteringIntegralStepSize( int32 stepSize );
	int32 GetInScatteringIntegralStepSize() const;

	// constants for final pixel shader processing, if "no pre-multiplied in-scattering " colors are to be processed in a pixel shader
	Vec4 GetPartialMieInScatteringConst() const;
	Vec4 GetPartialRayleighInScatteringConst() const;
	Vec3 GetSunDirection() const;
	Vec4 GetPhaseFunctionConsts() const;

private:
	// definition of optical depth LUT for mie/rayleigh scattering 
	struct SOpticalDepthLUTEntry
	{
		f32 mie;
		f32 rayleigh;

		AUTO_STRUCT_INFO
	};

	// definition of optical scale LUT for mie/rayleigh scattering 
	struct SOpticalScaleLUTEntry
	{
		f32 atmosphereLayerHeight;
		f32 mie;
		f32 rayleigh;

		AUTO_STRUCT_INFO
	};

	// definition lookup table entry for phase function
	struct SPhaseLUTEntry
	{
		f32 mie;
		f32 rayleigh;		
	};

	// definition of lookup tables
	typedef std::vector< SOpticalDepthLUTEntry > OpticalDepthLUT;
	typedef std::vector< SOpticalScaleLUTEntry > OpticalScaleLUT;
	typedef std::vector< SPhaseLUTEntry > PhaseLUT;

private:
	// mapping helpers
	f64 MapIndexToHeight( uint32 index ) const;
	f64 MapIndexToCosVertAngle( uint32 index ) const;
	f32 MapIndexToCosPhaseAngle( uint32 index ) const;

	void MapCosVertAngleToIndex( const f32& cosVertAngle, uint32& index, f32& indexFrc ) const;
	void MapCosPhaseAngleToIndex( const f32& cosPhaseAngle, uint32& index, f32& indexFrc ) const;

	// optical lookup table access helpers
	uint32 OpticalLUTIndex( uint32 heightIndex, uint32 cosVertAngleIndex ) const;

	// computes lookup tables for optical depth, etc.
	void ComputeOpticalLUTs();

	// computes lookup table for phase function
	void ComputePhaseLUT();

	// computes optical depth (helpers for ComputeOpticalLUTs())
	f64 IntegrateOpticalDepth( const Vec3_f64& start, const Vec3_f64& end, 
		const f64& avgDensityHeightInv, const f64& error ) const;

	bool ComputeOpticalDepth( const Vec3_f64& cameraLookDir, const f64& cameraHeight, const f64& avgDensityHeightInv, f32& depth ) const;

	// does a bilinearily filtered lookup into the optical depth LUT
	SOpticalDepthLUTEntry LookupBilerpedOpticalDepthLUTEntry( uint32 heightIndex, const f32& cosVertAngle ) const;

	// does a lookup into the phase LUT
	const SOpticalScaleLUTEntry& LookupOpticalScaleLUTEntry( uint32 heightIndex ) const;

	// does a bilinearily filtered lookup into the phase LUT
	SPhaseLUTEntry LookupBilerpedPhaseLUTEntry( const f32& cosPhaseAngle ) const;

	// computes in-scattering
	void SamplePartialInScatteringAtHeight( const SOpticalScaleLUTEntry& osAtHeight, 
		const f32& outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const SOpticalDepthLUTEntry& odAtHeightSky, 
		const SOpticalDepthLUTEntry& odAtViewerSky, const SOpticalDepthLUTEntry& odAtHeightSun, 
		Vec3& partialInScatteringMie, Vec3& partialInScatteringRayleigh ) const;

	void ComputeInScatteringNoPremul( const f32& outScatteringConstMie, const Vec3& outScatteringConstRayleigh, const Vec3& skyDir,
		Vec3& inScatteringMieNoPremul, Vec3& inScatteringRayleighNoPremul ) const;

	// serialization of optical LUTs
	bool LoadOpticalLUTs();
	void SaveOpticalLUTs() const;

private:
	// lookup tables
	OpticalDepthLUT m_opticalDepthLUT;
	OpticalScaleLUT m_opticalScaleLUT;
	PhaseLUT m_phaseLUT;

	// mie scattering constant
	f32 m_Km; 

	// rayleigh scattering constant
	f32 m_Kr; 

	// sun intensity
	Vec3 m_sunIntensity;

	// mie scattering asymmetry factor (g is always 0.0 for rayleigh scattering)
	f32 m_g;

	// wavelengths for r, g, and b to the -4th used for mie rayleigh scattering
	Vec3 m_invRGBWaveLength4;

	// direction towards the sun
	Vec3 m_sunDir;

	// step size for solving in-scattering integral
	int32 m_inScatteringStepSize;
};


#endif // #ifndef _SKY_LIGHT_NISHITA_H_
