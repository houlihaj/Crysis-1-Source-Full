////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   cvars.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: console variables used in 3dengine
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "3dEngine.h"
#include "IRenderer.h"

// can not be changed by user
#define INIT_CVAR_CHEAT(_var,_def_val,_comment)\
	GetConsole()->Register((#_var), &(_var), (_def_val), VF_CHEAT, _comment);

#define INIT_CVAR_FLAGS(_var,_def_val,_flags,_comment)\
	GetConsole()->Register((#_var), &(_var), (_def_val), (_flags), _comment);

// can be changed in options menu, will be saved into ini file
#define INIT_CVAR_SER__( _var, _def_val,_comment )\
	GetConsole()->Register( (#_var), &(_var), (_def_val), (e_allow_cvars_serialization ? VF_DUMPTODISK : 0), _comment );

// can be changed in options menu, will be saved into ini file, require level restart
#define INIT_CVAR_SER_R( _var, _def_val,_comment )\
	GetConsole()->Register( (#_var), &(_var), (_def_val), (e_allow_cvars_serialization ? VF_DUMPTODISK : 0)|VF_REQUIRE_LEVEL_RELOAD, _comment );

#define INIT_CVAR_SER_R_NET( _var, _def_val,_comment )\
	GetConsole()->Register( (#_var), &(_var), (_def_val), (e_allow_cvars_serialization ? VF_DUMPTODISK : 0)|VF_REQUIRE_LEVEL_RELOAD, _comment );

// can be changed by game or user
#define INIT_CVAR_PUBL_(_var,_def_val,_comment)\
	GetConsole()->Register((#_var), &(_var), (_def_val), 0, _comment);

//////////////////////////////////////////////////////////////////////////
void OnTimeOfDayVarChange( ICVar *pArgs )
{
	gEnv->p3DEngine->GetTimeOfDay()->SetTime( Cry3DEngineBase::GetCVars()->e_time_of_day );
}
void OnTimeOfDaySpeedVarChange( ICVar *pArgs )
{
	ITimeOfDay::SAdvancedInfo advInfo;
	gEnv->p3DEngine->GetTimeOfDay()->GetAdvancedInfo(advInfo);
	advInfo.fAnimSpeed = Cry3DEngineBase::GetCVars()->e_time_of_day_speed;
	gEnv->p3DEngine->GetTimeOfDay()->SetAdvancedInfo(advInfo);
}
void CVars::Init()
{
	INIT_CVAR_CHEAT(e_allow_cvars_serialization,	1, "If set to zero - will not save cvars to cfg file");
  INIT_CVAR_CHEAT(e_fog,												1, "Activates global height/distance based fog");
	INIT_CVAR_CHEAT(e_fogvolumes,									1, "Activates local height/distance based fog volumes");
	INIT_CVAR_CHEAT(e_entities,										1, "Activates drawing of entities and brushes");
	INIT_CVAR_CHEAT(e_sky_box,										1, "Activates drawing of skybox and moving cloud layers");
	INIT_CVAR_CHEAT(e_water_ocean,								1, "Activates drawing of ocean");
  INIT_CVAR_CHEAT(e_water_ocean_bottom,								1, "Activates drawing bottom of ocean");
  
  INIT_CVAR_PUBL_(e_water_ocean_fft,					  0, "Activates fft based ocean");

  INIT_CVAR_PUBL_(e_water_ocean_soft_particles, 1, "Enables ocean soft particles");
  
	INIT_CVAR_CHEAT(e_debug_draw,						      0, "Draw helpers with information for each object (same number negative hides the text)\n"
																									 " 1: Name of the used cgf, polycount, used LOD\n"
																									 " 2: Color coded polygon count\n"
																									 " 3: Show color coded LODs count, flashing color indicates no Lod\n"
																									 " 4: Display object texture memory usage\n"
																									 " 5: Display color coded number of render materials\n"
																									 " 6: Display ambient color\n"
																									 " 7: Display tri count, number of render materials, texture memory\n"
																									 " 8: RenderWorld statistics (with view cones)\n"
																									 " 9: RenderWorld statistics (with view cones without lights)\n"
																									 "10: Render geometry with simple lines and triangles\n"
																									 "11: Render occlusion geometry additionally\n"
																									 "12: Render occlusion geometry without render geometry\n"
																									 "13: Render occlusion ammount (used during AO computations)\n"
																									 "15: Display helpers"
																									 );

	INIT_CVAR_PUBL_(e_detail_objects,							1, "Turn detail objects on/off");
	INIT_CVAR_CHEAT(e_detail_materials,						1, "Activates drawing of detail materials on terrain ground");
	INIT_CVAR_CHEAT(e_detail_materials_debug,			0, "Shows number of materials in use per terrain sector");
	INIT_CVAR_PUBL_(e_detail_materials_view_dist_z,  128.f, "Max view distance of terrain Z materials");
	INIT_CVAR_PUBL_(e_detail_materials_view_dist_xy, 2048.f, "Max view distance of terrain XY materials");
	INIT_CVAR_PUBL_(e_sun_clipplane_range,					 192.f, "Max range for sun shadowmap frustum");
  INIT_CVAR_PUBL_(e_sun_angle_snap_sec,					0.1f, "Sun dir snap control");
  INIT_CVAR_PUBL_(e_sun_angle_snap_dot,					0.999999f, "Sun dir snap control");
	
	INIT_CVAR_CHEAT(e_particles,									1, "Activates drawing of particles");
	INIT_CVAR_FLAGS(e_particles_debug,						0, VF_CHEAT | VF_BITFIELD, "Particle debug flags:\n"
																										" 1 = show basic stats\n"
																										" f+ = show fill rate stats\n"
																										" r+ = show reuse/reject stats\n"
																										" b+ = draw bounding boxes and labels\n"
																										" s+ = log emitter and particle counts by effect (for 1 frame)\n"
																										" e+ = log particle system timing info (for 1 frame)\n"
																										" t+ = log particle texture usage\n"
																										" z+ = freeze particle system"
																									);
	INIT_CVAR_FLAGS(e_particles_thread,						1, VF_BITFIELD, "Enable particle threading");
	INIT_CVAR_PUBL_(e_particles_object_collisions,1, "Enable particle/object collisions for SimpleCollision");
	INIT_CVAR_CHEAT(e_particles_receive_shadows,  0, "Enable shadow maps receiving for particles");
	INIT_CVAR_PUBL_(e_particles_preload,          1, "Enable preloading of all particle effects at the begining");
	INIT_CVAR_PUBL_(e_particles_lod,							1, "Multiplier to particle count");
	INIT_CVAR_PUBL_(e_particles_min_draw_pixels,	1, "Pixel size cutoff for rendering particles -- fade out earlier");
	INIT_CVAR_PUBL_(e_particles_max_screen_fill, 64, "Max screen-fulls of particles to draw");

	INIT_CVAR_CHEAT(e_roads,									 		1, "Activates drawing of road objects");

  INIT_CVAR_PUBL_(e_decals,									 		1, "Activates drawing of decals (game decals and hand-placed)");
  INIT_CVAR_PUBL_(e_decals_allow_game_decals,		1, "Allows creation of decals by game (like weapon bullets marks)");
  INIT_CVAR_CHEAT(e_decals_hit_cache,					  1, "Use smart hit cacheing for bullet hits (may cause no decals in some cases)");
	INIT_CVAR_PUBL_(e_decals_merge,								1, "Combine pieces of decals into one render call");
	INIT_CVAR_PUBL_(e_decals_precreate,						1, "Pre-create decals at load time");
	INIT_CVAR_PUBL_(e_decals_clip,								1, "Clip decal geometry by decal bbox");
	INIT_CVAR_PUBL_(e_decals_range,								20.0f, "Less precision for decals outside this range");
	INIT_CVAR_PUBL_(e_decals_wrap_around_min_size, .05f, "Controls usage of wrapping around objects feature. When decal radius is smaller than X - simple quad decal is used");
	INIT_CVAR_SER__(e_decals_life_time_scale,		1.f, "Allows to increase or reduce decals life time for different specs");
	INIT_CVAR_CHEAT(e_decals_neighbor_max_life_time, 4.f, "If not zero - new decals will force old decals to fade in X seconds");
  INIT_CVAR_CHEAT(e_decals_overlapping,       0, "If zero - new decals will not be spawned if the distance to nearest decals less than X");
	INIT_CVAR_PUBL_(e_decals_scissor,							1, "Enable decal rendering optimization by using scissor");

	INIT_CVAR_PUBL_(e_vegetation_bending,					2, "Debug");
  INIT_CVAR_CHEAT(e_vegetation_static_instancing,0, "Enable 3dengine side static instancing");
	INIT_CVAR_CHEAT(e_vegetation_alpha_blend,			1, "Allow alpha blending for vegetations");
  INIT_CVAR_PUBL_(e_vegetation_use_terrain_color, 1, "Allow blend with terrain color for vegetations");
  INIT_CVAR_CHEAT(e_vegetation_sphericalskinning,	1, "Activates vegetation spherical skinning support");  
  INIT_CVAR_CHEAT(e_vegetation_wind,					  1, "Activates vegetation with wind support");
  INIT_CVAR_CHEAT(e_vegetation,									1, "Activates drawing of distributed objects like trees");
  INIT_CVAR_CHEAT(e_vegetation_sprites,					1, "Activates drawing of sprites instead of distributed objects at far distance");
	INIT_CVAR_PUBL_(e_vegetation_node_level,		 -1, "Debug");
	INIT_CVAR_PUBL_(e_vegetation_mem_sort_test,		0, "Debug");
	INIT_CVAR_PUBL_(e_vegetation_sprites_min_distance, 8.0f, "Sets minimal distance when distributed object can be replaced with sprite");

	INIT_CVAR_PUBL_(e_vegetation_sprites_distance_ratio, 1.0f, "Allows changing distance on what vegetation switch into sprite");
  INIT_CVAR_PUBL_(e_vegetation_sprites_distance_custom_ratio_min, 1.0f, "Clamps SpriteDistRatio setting in vegetation properties");
  INIT_CVAR_PUBL_(e_force_detail_level_for_resolution, 0, "Force sprite distance and other values used for some specific screen resolution, 0 means current");
	INIT_CVAR_SER_R_NET(e_vegetation_min_size,0.f, "Minimal size of static object, smaller objects will be not rendered");

	INIT_CVAR_CHEAT(e_wind,												1, "Debug");
	INIT_CVAR_CHEAT(e_wind_areas,									1, "Debug");
	INIT_CVAR_PUBL_(e_shadows,										1, "Activates drawing of shadows");


  INIT_CVAR_PUBL_(e_shadows_from_terrain_in_all_lods, 1, "Enable shadows from terrain");
  INIT_CVAR_PUBL_(e_shadows_on_alpha_blended,   1, "Enable shadows on aplhablended ");
	INIT_CVAR_CHEAT(e_shadows_frustums,						0, "Debug");
	INIT_CVAR_CHEAT(e_shadows_debug,							0, "0=off, 2=visualize shadow maps on the screen");
  INIT_CVAR_PUBL_(e_shadows_clouds,							1, "Cloud shadows");	// no cheat var because this feature shouldn't be strong enough to affect gameplay a lot
	INIT_CVAR_PUBL_(e_shadows_max_texture_size,1024, "Set maximum resolution of shadow map\n256(faster), 512(medium), 1024(better quality)");
	INIT_CVAR_PUBL_(e_shadows_slope_bias,       4.0, "Shadows slope bias for shadowgen");
	INIT_CVAR_PUBL_(e_shadows_water,              0, "Enable/disable shadows on water");
	INIT_CVAR_PUBL_(e_shadows_cast_view_dist_ratio,			0.8f, "View dist ratio for shadow maps");
	INIT_CVAR_PUBL_(e_shadows_occ_check,					0, "Enable/disable shadow-caster test against the occlusion buffer");
	INIT_CVAR_PUBL_(e_shadows_occ_cutCaster,			0, "Clips the caster extrusion to the zoro height.");
	INIT_CVAR_PUBL_(e_gsm_range,							 3.0f, "Size of LOD 0 GSM area (in meters)");
	INIT_CVAR_PUBL_(e_gsm_range_step,					 3.0f, "Range of next GSM lod is previous range multiplied by step");
	INIT_CVAR_PUBL_(e_gsm_lods_num,								5, "Number of GSM lods (0..5)");
	INIT_CVAR_PUBL_(e_gsm_depth_bounds_debug, 		0, "Debug GSM bounds regions calculation");
	INIT_CVAR_CHEAT(e_gsm_stats,									0, "Show GSM statistics 0=off, 1=enable debug to the screens");
	INIT_CVAR_PUBL_(e_gsm_view_space,							0, "0=world axis aligned GSM layout, 1=Rotate GSM frustums depending on view camera");
  INIT_CVAR_PUBL_(e_gsm_cache,									1, "Cache sun shadows maps over several frames 0=off, 1=on if MultiGPU is deactivated");
  INIT_CVAR_PUBL_(e_gsm_cache_lod_offset,       3, "Makes first X GSM lods not cached");
	INIT_CVAR_PUBL_(e_gsm_combined,								0, "Variable to tweak the performace of directional shadow maps\n0=individual textures are used for each GSM level, 1=texture are combined into one texture");
	INIT_CVAR_PUBL_(e_gsm_range_step_terrain,		 16, "gsm_range_step for last gsm lod containg terrain");
  INIT_CVAR_PUBL_(e_gsm_scatter_lod_dist,	     70, "Size of Scattering LOD GSM in meters");

	INIT_CVAR_CHEAT(e_debug_mask,									0, "debug variable to activate certain features for debugging (each bit represents one feature\nbit 0(1): use EFSLIST_TERRAINLAYER\nbit 1(2):\nbit 3(4):\nbit 4(8):");
	
	INIT_CVAR_CHEAT(e_terrain,										1, "Activates drawing of terrain ground");
  INIT_CVAR_CHEAT(e_terrain_deformations,       0, "Allows in-game terrain surface deformations");
  INIT_CVAR_CHEAT(e_level_auto_precache_camera_jump_dist,  64, 
    "When not 0 - Force full pre-cache of textures, procedural vegetation and shaders"
    "if camera moved for more than X meters in one frame or on new cut scene start");
  INIT_CVAR_CHEAT(e_level_auto_precache_terrain_and_proc_veget, 1, "Force auro pre-cache of terrain textures and procedural vegetation");
  INIT_CVAR_CHEAT(e_level_auto_precache_textures_and_shaders,  0, "Force auro pre-cache of general textures and shaders");

	INIT_CVAR_CHEAT(e_terrain_bboxes,							0, "Show terrain nodes bboxes");
  INIT_CVAR_PUBL_(e_terrain_normal_map,				  1, "Use terrain normal map for base pass if available");
	INIT_CVAR_CHEAT(e_terrain_occlusion_culling,	1, "heightmap occlusion culling with time coherency 0=off, 1=on");
  INIT_CVAR_CHEAT(e_terrain_occlusion_culling_step_size_delta,	1.05f, "Step size scale on every next step (for version 1)");
  INIT_CVAR_PUBL_(e_terrain_occlusion_culling_max_dist,	200.f, "Max length of ray (for version 1)");
  INIT_CVAR_CHEAT(e_terrain_occlusion_culling_version, 1, "0 - old, 1 - new");
  INIT_CVAR_CHEAT(e_terrain_occlusion_culling_debug, 0, "Draw sphere onevery terrain height sample");
	INIT_CVAR_CHEAT(e_terrain_occlusion_culling_max_steps, 50, "Max number of tests per ray (for version 0)");
	INIT_CVAR_CHEAT(e_terrain_occlusion_culling_step_size, 4, "Initial size of single step (in heightmap units)");
  INIT_CVAR_CHEAT(e_terrain_texture_debug,			0, "Debug");
	INIT_CVAR_CHEAT(e_terrain_texture_streaming_debug,			0, "Debug");
  INIT_CVAR_CHEAT(e_terrain_log,								0, "Debug");
	INIT_CVAR_CHEAT(e_terrain_draw_this_sector_only, 0, "1 - render only sector where camera is and objects registered in sector 00, 2 - render only sector where camera is");
	INIT_CVAR_CHEAT(e_terrain_occlusion_culling_precision, 0.25f, "Density of rays");
  INIT_CVAR_CHEAT(e_terrain_occlusion_culling_precision_dist_ratio, 3.f, "Controlls density of rays depending on distance to the object");
	INIT_CVAR_PUBL_(e_terrain_lod_ratio,				1.f, "Set heightmap LOD");

  INIT_CVAR_CHEAT(e_occlusion_culling_view_dist_ratio, 0.5f, "Skip per object occlusion test for very far objects - culling on tree level will handle it");

  INIT_CVAR_PUBL_(e_terrain_lm_gen_threshold, 0.1f, "Debug");
  INIT_CVAR_PUBL_(e_terrain_texture_lod_ratio, 1.0f, "Adjust terrain base texture resolution on distance");

  INIT_CVAR_CHEAT(e_timedemo_frames,						0, "Will quit appication in X number of frames, r_DisplayInfo must be also enabled");
  
	INIT_CVAR_CHEAT(e_sun,												1, "Activates sun light source");
  INIT_CVAR_CHEAT(e_cbuffer,										1, "Activates usage of software coverage buffer. 1 - camera culling only; 2 - camera culling and light-to-object check");

  INIT_CVAR_PUBL_(e_cbuffer_terrain,				    0, "Activates usage of coverage buffer for terrain");
  INIT_CVAR_PUBL_(e_cbuffer_terrain_lod_shift,	2, "Controlls tesselation of terrain mesh");
  INIT_CVAR_PUBL_(e_cbuffer_terrain_max_distance,  512.f, "Only near sectors are rasterized");
  INIT_CVAR_PUBL_(e_cbuffer_terrain_elevation_shift, 4.f, "Shift low lods down in order to avoid false occlusion");
  INIT_CVAR_PUBL_(e_cbuffer_terrain_lod_ratio,       4.f, "Terrain lod ratio for mesh rendered into cbuffer");
	INIT_CVAR_CHEAT(e_cbuffer_version,						2, "1 Vladimir's, 2MichaelK's");
	INIT_CVAR_CHEAT(e_cbuffer_hw,									0, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_lc,									0, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_clip_planes_num,		6, "Debug"); 
	INIT_CVAR_CHEAT(e_cbuffer_debug,							0, "Display content of main camera coverage buffer");
	INIT_CVAR_CHEAT(e_cbuffer_debug_freeze,				0, "Freezes viewmatrix/-frustum ");
	INIT_CVAR_CHEAT(e_cbuffer_draw_occluders,			0, "Debug draw of occluders for coverage buffer");
	INIT_CVAR_CHEAT(e_cbuffer_test_mode,					2, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_bias,						1.05f, "Coverage buffer z-biasing");
	INIT_CVAR_CHEAT(e_cbuffer_lights_debug_side, -1, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_debug_draw_scale,		1, "Debug");
	INIT_CVAR_PUBL_(e_cbuffer_resolution,			  256, "Resolution of software coverage buffer");
	INIT_CVAR_CHEAT(e_cbuffer_occluders_test_min_tris_num, 0, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_occluders_view_dist_ratio, 1.0f, "Debug");
  INIT_CVAR_CHEAT(e_cbuffer_occluders_lod_ratio, 0.25f, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_lazy_test,					1, "Dont test visible objects every frame");
	INIT_CVAR_CHEAT(e_cbuffer_tree_depth,					7, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_tree_debug,					0, "Debug");
	INIT_CVAR_CHEAT(e_cbuffer_max_add_render_mesh_time, 2, "Max time for unlimited AddRenderMesh");

  INIT_CVAR_CHEAT(e_dynamic_light,							1, "Activates dynamic light sources");
  INIT_CVAR_PUBL_(e_dynamic_light_frame_id_vis_test, 1, "Use based on last draw frame visibility test");
  INIT_CVAR_PUBL_(e_dynamic_light_consistent_sort_order, 1, "Debug");

	INIT_CVAR_PUBL_(e_hw_occlusion_culling_water,	1, "Activates usage of HW occlusion test for ocean");
	INIT_CVAR_CHEAT(e_hw_occlusion_culling_objects,0, "Activates usage of HW occlusion test for objects");

  INIT_CVAR_CHEAT(e_portals,										1, "Activates drawing of visareas content (indoors), values 2,3,4 used for debugging");
  INIT_CVAR_CHEAT(e_portals_big_entities_fix,		1, "Enables special processing of big entities like vehicles intersecting portals");
	INIT_CVAR_PUBL_(e_max_entity_lights,				 16, "Set maximum number of lights affecting object");
	INIT_CVAR_CHEAT(e_max_view_dst,							 -1, "Far clipping plane distance");
  INIT_CVAR_PUBL_(e_max_view_dst_spec_lerp,     1, "1 - use max view distance set by designer for very high spec\n0 - for very low spec\nValues between 0 and 1 - will lerp between high and low spec max view distances");
  INIT_CVAR_CHEAT(e_max_view_dst_full_dist_cam_height, 1000, "Debug");
	INIT_CVAR_CHEAT(e_water_volumes,							1, "Activates drawing of water volumes");
  INIT_CVAR_CHEAT(e_water_waves,			  				0, "Activates drawing of water waves");
  INIT_CVAR_PUBL_(e_water_waves_tesselation_amount,	5, "Sets water waves tesselation amount");  
  INIT_CVAR_PUBL_(e_water_tesselation_amount,	 10, "Set tesselation amount");  
  INIT_CVAR_PUBL_(e_water_tesselation_swath_width, 12, "Set the swath width for the boustrophedonic mesh stripping"); 
	INIT_CVAR_CHEAT(e_bboxes,											0, "Activates drawing of bounding boxes");

	INIT_CVAR_CHEAT(e_stream_cgf,									0, "Debug");
  INIT_CVAR_CHEAT(e_stream_for_physics,					1, "Debug"); 
  INIT_CVAR_CHEAT(e_stream_for_visuals,					1, "Debug"); 
	INIT_CVAR_CHEAT(e_stream_areas,								0, "Stream content of terrain and indoor sectors");

	INIT_CVAR_CHEAT(e_scissor_debug,							0, "Debug");
  INIT_CVAR_CHEAT(e_materials,									1, "Activates material support for non animated objects");

	INIT_CVAR_SER_R(e_ram_maps,									1, "Use RAM (realtime ambient maps) on brushes");
	INIT_CVAR_CHEAT(e_ram_DumpDXF,							0, "Dump DXF files of the UV-set of ram objects/brushes to visualize overlapping UVs");

  INIT_CVAR_CHEAT(e_brushes,										1, "Draw brushes");
  INIT_CVAR_CHEAT(e_cull_lights_per_triangle_min_obj_radius, 8.f, "Debug");

  INIT_CVAR_CHEAT(e_scene_merging,						0, "Merge marked brushes during loading");
	INIT_CVAR_PUBL_(e_scene_merging_compact_vertices,1, "Minimize number of vertices in decals and merged brushes");
	INIT_CVAR_CHEAT(e_scene_merging_show_onlymerged,					0, "Show only merged brushes");
	INIT_CVAR_PUBL_(e_scene_merging_max_tris_in_chunk, 64, "Only objects having less than X triangles per chunk will me merged");
	INIT_CVAR_PUBL_(e_scene_merging_max_time_ms,5, "Spend only X ms per frame on merging");
	INIT_CVAR_PUBL_(e_scene_merging_min_merge_distance, 0, "Don't merge nodes closer than X");

	INIT_CVAR_PUBL_(e_on_demand_physics, gEnv->pSystem->IsEditor() ? 0:1, "Turns on on-demand vegetation physicalization");
	INIT_CVAR_PUBL_(e_on_demand_maxsize,		  20.0f, "Specifies the maximum size of vegetation objects that are physicalized on-demand");
	INIT_CVAR_CHEAT(e_sleep,											0, "Sleep X in C3DEngine::Draw");
	INIT_CVAR_CHEAT(e_obj,												1, "Render or not all objects");
	INIT_CVAR_CHEAT(e_render,											1, "Enable engine rendering");
  INIT_CVAR_CHEAT(e_obj_tree_min_node_size,			0, "Debug draw of object tree bboxes");
  INIT_CVAR_CHEAT(e_obj_tree_max_node_size,			0, "Debug draw of object tree bboxes");
	INIT_CVAR_CHEAT(e_obj_stats,									0, "Show instances count");
  INIT_CVAR_CHEAT(e_obj_fast_register,          1, "Debug");

	INIT_CVAR_CHEAT(e_occlusion_volumes,					1, "Enable occlusion volumes(antiportals)");
	INIT_CVAR_PUBL_(e_occlusion_volumes_view_dist_ratio, 0.05f, "Controls how far occlusion volumes starts to occlude objects");

	INIT_CVAR_PUBL_(e_precache_level,							1, "Pre-render objects right after level loading");
	INIT_CVAR_PUBL_(e_dissolve,										1, "Objects alphatest_noise_fading out on distance");
	INIT_CVAR_CHEAT(e_dissolve_transition_time, .3f, "Lod switch duration");
  INIT_CVAR_CHEAT(e_dissolve_transition_threshold, 0.05f, "Controls disabling of smooth transition if camera moves too fast");
	INIT_CVAR_PUBL_(e_lods,         							1, "Load and use LOD models for static geometry");

	INIT_CVAR_PUBL_(e_voxel,											1, "Render voxels");
  INIT_CVAR_PUBL_(e_voxel_rasterize_selected_brush, 0, "Debug");
	INIT_CVAR_PUBL_(e_voxel_build,								0, "Regenerate voxel world");
	INIT_CVAR_PUBL_(e_voxel_debug,								0, "Draw voxel debug info");
	INIT_CVAR_PUBL_(e_voxel_ao_radius,						8, "Ambient occlusion test range in units");
	INIT_CVAR_PUBL_(e_voxel_ao_scale,							4, "Ambient occlusion amount");
  INIT_CVAR_PUBL_(e_voxel_fill_mode,						0, "Use create brush as fill brush");

	INIT_CVAR_PUBL_(e_voxel_lods_num,							1, "Generate LODs for voxels");

	INIT_CVAR_PUBL_(e_voxel_make_shadows,					0, "Calculate per vertex shadows");
	INIT_CVAR_PUBL_(e_voxel_make_physics,					1, "Physicalize geometry");

	INIT_CVAR_PUBL_(e_proc_vegetation,					  1, "Show procedurally distributed vegetation");
	INIT_CVAR_PUBL_(e_proc_vegetation_max_view_distance, 128, "Debug");

	INIT_CVAR_PUBL_(e_recursion,									1, "If 0 - will skip recursive render calls like render into texture");
  INIT_CVAR_PUBL_(e_recursion_occlusion_culling,0, "If 0 - will disable occlusion tests for recursive render calls like render into texture");
  INIT_CVAR_PUBL_(e_recursion_view_dist_ratio, .1f,"Set all view distances shorter by factor of X");

	INIT_CVAR_PUBL_(e_clouds,											0, "Enable clouds rendering");

//	INIT_CVAR_PUBL_(e_panorama_distance,					0, "Maximum view distance for impostor content");
//	INIT_CVAR_PUBL_(e_panorama_clusters_num,			6, "Number of impostors/clusters in panorama");
//	INIT_CVAR_PUBL_(e_panorama_tex_res,					512, "Resolution of impostor textures");
//	INIT_CVAR_PUBL_(e_panorama_update_distance,	4.f, "Impostors will be updated when camera moves for X meters");

	INIT_CVAR_PUBL_(e_sky_update_rate, 0.12f, "Percentage of a full dynamic sky update calculated per frame (0..100].");	
	INIT_CVAR_PUBL_(e_sky_quality, 1, "Quality of dynamic sky: 1 (very high), 2 (high).");
	INIT_CVAR_PUBL_(e_sky_type, 1, "Type of sky used: 0 (static), 1 (dynamic).");

	INIT_CVAR_PUBL_(e_lod_ratio,				     6.0f, "LOD distance ratio for objects");
	INIT_CVAR_PUBL_(e_view_dist_ratio,      60.0f, "View distance ratio for objects");
	INIT_CVAR_PUBL_(e_view_dist_ratio_detail,    30.0f, "View distance ratio for detail objects");
	INIT_CVAR_PUBL_(e_view_dist_ratio_vegetation,30.0f, "View distance ratio for vegetation");
	INIT_CVAR_CHEAT(e_view_dist_custom_ratio,  100.0f, "View distance ratio for special marked objects (Players,AI,Vehicles)");
	INIT_CVAR_CHEAT(e_debug_lights,							0, "Use different colors for objects affected by different number of lights\n black:0, blue:1, green:2, red:3 or more, blinking yellow: more then the maximum enabled");
	INIT_CVAR_CHEAT(e_view_dist_min,			0.f, "Min distance on what far objects will be culled out");
	INIT_CVAR_PUBL_(e_lod_min,							    0, "Min LOD for objects");
	INIT_CVAR_CHEAT(e_lod_max,							    6, "Max LOD for objects");
	INIT_CVAR_CHEAT(e_lod_min_tris,							300, "LODs with less triangles will not be used");
	INIT_CVAR_CHEAT(e_mesh_simplify,						0, "Mesh simplification debugging");
	INIT_CVAR_CHEAT(e_ambient_occlusion,				0, "Activate non deferred terrain occlusion and indirectional lighitng system");
  INIT_CVAR_PUBL_(e_terrain_ao,				        1, "Activate deferred terrain ambient occlusion");

	INIT_CVAR_PUBL_(e_phys_foliage,								    2, "Enables physicalized foliage (1 - only for dynamic objects, 2 - for static and dynamic)");
	INIT_CVAR_PUBL_(e_foliage_stiffness,		       3.2f, "Stiffness of the spongy obstruct geometry");
	INIT_CVAR_PUBL_(e_foliage_branches_stiffness,	100.f, "Stiffness of branch ropes");
	INIT_CVAR_PUBL_(e_foliage_branches_damping,	   10.f, "Damping of branch ropes");
	INIT_CVAR_PUBL_(e_foliage_broken_branches_damping,15.f, "Damping of branch ropes of broken vegetation");
	INIT_CVAR_PUBL_(e_foliage_branches_timeout,	    4.f, "Maximum lifetime of branch ropes (if there are no collisions)");
	INIT_CVAR_PUBL_(e_foliage_wind_activation_dist,	0,  "If the wind is sufficiently strong, visible foliage in this view dist will be forcefully activated");

  INIT_CVAR_PUBL_(e_deformable_objects,						1, "Enable / Disable morph based deformable objects");

	INIT_CVAR_PUBL_(e_cull_veg_activation,				200, "Vegetation activation distance limit; 0 disables visibility-based culling (= unconditional activation)");

	INIT_CVAR_PUBL_(e_phys_ocean_cell,							0, "Cell size for ocean approximation in physics, 0 assumes flat plane");

	INIT_CVAR_PUBL_(e_joint_strength_scale, 		 1.0f, "Scales the strength of prebroken objects\' joints (for tweaking)");

	INIT_CVAR_PUBL_(e_volobj_shadow_strength,	0.4f, "Self shadow intensity of volume objects [0..1].");

	INIT_CVAR_PUBL_(e_screenshot,										0, 
		"Make screenshot combined up of multiple rendered frames\n"
		"(negative values for multiple frames, positive for a a single frame)\n"
		" 1 highres\n"
		" 2 360 degree panorama\n"
		" 3 Map top-down view\n"
		"\n"
		"see:\n"
		"  e_screenshot_width, e_screenshot_height, e_screenshot_quality, e_screenshot_map_center_x,\n"
		"  e_screenshot_map_center_y, e_screenshot_map_size, e_screenshots_min_slices, e_screenshot_debug");
	
	INIT_CVAR_PUBL_(e_screenshot_width,					2000, "used for e_panorama_screenshot to define the width of the destination image, 2000 default");
	INIT_CVAR_PUBL_(e_screenshot_height,				1500, "used for e_panorama_screenshot to define the height of the destination image, 1500 default");
	INIT_CVAR_PUBL_(e_screenshot_quality,					 30, "used for e_panorama_screenshot to define the quality\n0=fast, 10 .. 30 .. 100 = extra border in percent (soften seams), negative value to debug");
	INIT_CVAR_PUBL_(e_screenshot_min_slices,				4, "used for e_panorama_screenshot to define the quality\n0=fast, 10 .. 30 .. 100 = extra border in percent (soften seams), negative value to debug");
	INIT_CVAR_PUBL_(e_screenshot_map_center_x,		0.f, "param for the centerX position of the camera, see e_screenshot_map\ndefines the x position of the top left corner of the screenshot-area on the terrain,\n0.0 - 1.0 (0.0 is default)");
	INIT_CVAR_PUBL_(e_screenshot_map_center_y,		0.f, "param for the centerX position of the camera, see e_screenshot_map\ndefines the y position of the top left corner of the screenshot-area on the terrain,\n0.0 - 1.0 (0.0 is default)");
	INIT_CVAR_PUBL_(e_screenshot_map_size_x,				1024.f, "param for the size in worldunits of area to make map screenshot, see e_screenshot_map\ndefines the x position of the bottom right corner of the screenshot-area on the terrain,\n0.0 - 1.0 (1.0 is default)");
	INIT_CVAR_PUBL_(e_screenshot_map_size_y,				1024.f, "param for the size in worldunits of area to make map screenshot, see e_screenshot_map\ndefines the x position of the bottom right corner of the screenshot-area on the terrain,\n0.0 - 1.0 (1.0 is default)");
	INIT_CVAR_PUBL_(e_screenshot_map_camheight,	 4000.f, "param for top-down-view screenshot creation, defining the camera height for screenshots, see e_screenshot_map\ndefines the y position of the bottom right corner of the screenshot-area on the terrain,\n0.0 - 1.0 (1.0 is default)");
	INIT_CVAR_PUBL_(e_screenshot_debug,							0, "0 off\n1 show stiching borders\n2 show overlapping areas");



	INIT_CVAR_CHEAT(e_ropes,1, "Turn Rendering of Ropes on/off");

	INIT_CVAR_PUBL_(e_stat_obj_merge,							1, "Enable CGF sub-objects meshes merging");
	INIT_CVAR_CHEAT(e_default_material,						0, "use gray illum as default");

  INIT_CVAR_CHEAT(e_profile_level_loading,			1, "Output level loading stats into log\n"
    "0 = Off\n"
    "1 = Output basic info about loading time per function\n"
    "2 = Output full statistics including loading time and memory allocations with call stack info");

	INIT_CVAR_PUBL_(e_obj_quality,							  0, "Current object detail quality");
	INIT_CVAR_PUBL_(e_particles_quality,				  0, "Current particles detail quality");
  INIT_CVAR_PUBL_(e_particles_lights,				    1, "Allows to have simpe light source attached to every article");
	INIT_CVAR_CHEAT(e_phys_bullet_coll_dist,			75.0f, "Max distance for bullet rendermesh checks");

	// Strings
	e_screenshot_file_format = GetConsole()->RegisterString("e_screenshot_file_format", "tga",  0, "Set output image file format for hires screen shots. Can be JPG or TGA");

	GetConsole()->Register( "e_time_of_day",&e_time_of_day,0.0f,VF_CHEAT, "Current Time of Day",OnTimeOfDayVarChange );
	GetConsole()->Register( "e_time_of_day_speed",&e_time_of_day_speed,0.0f,VF_CHEAT, "Time of Day change speed",OnTimeOfDaySpeedVarChange );
}
