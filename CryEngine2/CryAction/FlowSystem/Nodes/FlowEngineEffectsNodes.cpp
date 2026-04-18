#include "StdAfx.h"
#include "FlowBaseNode.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Color correction
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SColorCorrection
{  
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("Global_User_ColorC", 0.0f, 0, "Cyan"),
      InputPortConfig<float>("Global_User_ColorM", 0.0f, 0, "Magenta"),
      InputPortConfig<float>("Global_User_ColorY", 0.0f, 0, "Yellow"),
      InputPortConfig<float>("Global_User_ColorK", 0.0f, 0, "Luminance"),
      InputPortConfig<float>("Global_User_Brightness", 1.0f, 0, "Brightness"),
      InputPortConfig<float>("Global_User_Contrast", 1.0f, 0, "Contrast"),
      InputPortConfig<float>("Global_User_Saturation", 1.0f, 0, "Saturation"),
      InputPortConfig<float>("Global_User_ColorHue", 0.0f, "Change Hue", "Hue"),      
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets color correction parameters");
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Color grading
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
struct SCGColorGeneral
{  
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ColorGrading_Brightness", 1.0f, 0, "Brightness"),
      InputPortConfig<float>("ColorGrading_Contrast", 1.0f, 0, "Contrast"),
      InputPortConfig<float>("ColorGrading_Saturation", 1.0f, 0, "Saturation"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets color adjustment parameters");
  }
};

struct SCGLevels
{  
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ColorGrading_minInput", 0.0f, 0, "Min Input"),
      InputPortConfig<float>("ColorGrading_gammaInput", 1.0f, 0, "Gamma"),
      InputPortConfig<float>("ColorGrading_maxInput", 255.0f, 0, "Max Input"),

      InputPortConfig<float>("ColorGrading_minOutput", 0.0f, 0, "Min Input"),
      InputPortConfig<float>("ColorGrading_maxOutput", 255.0f, 0, "Max Output"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets color levels adjusment parameters");
  }
};

struct SCGSelectiveColor
{  
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<Vec3>("clr_ColorGrading_SelectiveColor", Vec3(0.0f, 0.0f, 0.0f), 0, "Color"),
      InputPortConfig<float>("ColorGrading_SelectiveColorCyans", 0.0f, 0, "Cyans"),
      InputPortConfig<float>("ColorGrading_SelectiveColorMagentas", 0.0f, 0, "Magentas"),
      InputPortConfig<float>("ColorGrading_SelectiveColorYellows", 0.0f, 0, "Yellows"),
      InputPortConfig<float>("ColorGrading_SelectiveColorBlacks", 0.0f, 0, "Blacks"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets seletive color correction parameters");
  }
};

struct SCGFilters
{  
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ColorGrading_GrainAmount", 0.0f, 0, "Grain"),
      InputPortConfig<float>("ColorGrading_SharpenAmount", 0.0f, 0, "Sharpening"),
      InputPortConfig<Vec3>("clr_ColorGrading_PhotoFilterColor", Vec3( 0.952f, 0.517f, 0.09f ), 0, "PhotoFilter Color"),
      InputPortConfig<float>("ColorGrading_PhotoFilterColorDensity", 0.0f, 0, "PhotoFilter Density"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets miscelaneous filters parameters");
  }
};
*/


////////////////////////////////////////////////////////////////////////////////////////////////////
// Filters
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SFilterBlur
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<int>("FilterBlurring_Type", 0, "Set blur type, 0 is Gaussian", "Type"),
      InputPortConfig<float>("FilterBlurring_Amount", 0.0f, "Amount of blurring", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets blur filter");
  }
};

struct SFilterRadialBlur
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("FilterRadialBlurring_Amount", 0.0f, "Amount of blurring", "Amount"),
      InputPortConfig<float>("FilterRadialBlurring_ScreenPosX", 0.5f, "Horizontal screen position", "ScreenPosX"),
      InputPortConfig<float>("FilterRadialBlurring_ScreenPosY", 0.5f, "Vertical screen position", "ScreenPosY"),
      InputPortConfig<float>("FilterRadialBlurring_Radius", 1.0f, "Blurring radius", "BlurringRadius"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets radial blur filter");
  }
};

struct SFilterSharpen
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<int>("FilterSharpening_Type", 0, "Sets sharpening filter, 0 is Unsharp Mask", "Type"),
      InputPortConfig<float>("FilterSharpening_Amount", 1.0f, "Amount of sharpening", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets sharpen filter");
  }
};

struct SFilterChromaShift
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {            
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("FilterChromaShift_User_Amount", 0.0f, "Amount of chroma shift", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets chroma shift filter");
  }
};

struct SFilterGrain
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {            
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("FilterGrain_Amount", 0.0f, "Amount of grain", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets grain filter");
  }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Effects 
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SEffectFrost
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ScreenFrost_Amount", 0.0f, "Amount of frost", "Amount"),
      InputPortConfig<float>("ScreenFrost_CenterAmount", 1.0f, "Amount of frost at center of screen", "CenterAmount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Freezing effect");
  }
};

/*
struct SEffectScreenShatter
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ScreenShatter_Amount", 0.0f, "Amount of shatter", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Screen shatter effect");
  }
}; */

struct SEffectCondensation
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("ScreenCondensation_Amount", 0.0f, "Amount of condensation ", "Amount"),
      InputPortConfig<float>("ScreenCondensation_CenterAmount", 1.0f, "Amount of condensation at center of screen", "CenterAmount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Condensation effect");
  }
};

struct SEffectWaterDroplets
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("WaterDroplets_Amount", 0.0f, "Amount of water", "Amount"),      
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Water droplets effect - Use when camera goes out of water or similar extreme condition");
  }
};

struct SEffectWaterFlow
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {      
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("WaterFlow_Amount", 0.0f, "Amount of water", "Amount"),      
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Water flow effect - Use when camera receiving splashes of water or similar");
  }
};


struct SEffectBloodSplats
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<int>  ("BloodSplats_Type", 0, "Blood type: human or alien", "Type"),
      InputPortConfig<float>("BloodSplats_Amount", 0.0f, "Amount of visible blood", "Amount"),
      InputPortConfig<bool> ("BloodSplats_Spawn", false, "Spawn more blood particles", "Spawn"),			
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Blood splats effect");
  }
};

struct SEffectDistantRain
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),      
      InputPortConfig<float>("DistantRain_Amount", 0.0f, "Amount of visible distant rain", "Amount"),
      InputPortConfig<float>("DistantRain_Speed", 1.0f, "Distant rain speed multiplier", "Speed"),
      InputPortConfig<float>("DistantRain_DistanceScale", 1.0f, "Distant rain distance scale", "DistanceScale"),
      InputPortConfig<Vec3>("clr_DistantRain_Color", Vec3(1.0f, 1.0f, 1.0f), "Sets distance rain color", "Color"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Distant rain effect");
  }
};

struct SEffectDof
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<bool>  ("Dof_User_Active", false, "Enables dof effect", "EnableDof"),
      InputPortConfig<float>("Dof_User_FocusDistance", 3.5f, "Set focus distance", "FocusDistance"),
      InputPortConfig<float> ("Dof_User_FocusRange", 5.0f, "Set focus range", "FocusRange"),			
      InputPortConfig<float> ("Dof_User_BlurAmount", 1.0f, "Set blurring amount", "BlurAmount"),			
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Depth of field effect");
  }
};

struct SEffectDirectionalBlur
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<Vec3>("Global_DirectionalBlur_Vec", Vec3(0.0f, 0.0f, 0.0f), "Sets directional blur direction", "Direction"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Directional Blur effect");
  }
};

struct SEffectAlienInterference
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("AlienInterference_Amount", 0.0f, "Sets alien interference amount", "Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Alien interference effect");
  }
};

struct SEffectRainDrops
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("RainDrops_Amount", 0.0f, "Sets rain drops visibility", "Amount"),
      InputPortConfig<float>("RainDrops_SpawnTimeDistance", 0.35f, "Sets rain drops spawn time distance", "Spawn Time Distance"),
      InputPortConfig<float>("RainDrops_Size", 5.0f, "Sets rain drops size", "Size"),
      InputPortConfig<float>("RainDrops_SizeVariation", 2.5f, "Sets rain drops size variation", "Size Variation"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Rain drops effect");
  }
};

struct SEffectVolumetricScattering
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<float>("VolumetricScattering_Amount", 0.0f, "Sets volumetric scattering amount", "Amount"),
      InputPortConfig<float>("VolumetricScattering_Tilling", 1.0f, "Sets volumetric scattering tilling", "Tilling"),
      InputPortConfig<float>("VolumetricScattering_Speed", 1.0f, "Sets volumetric scattering animation speed", "Speed"),
      InputPortConfig<Vec3>("clr_VolumetricScattering_Color", Vec3(0.6f, 0.75f, 1.0f), "Sets volumetric scattering amount", "Color"),
      InputPortConfig<int>("VolumetricScattering_Type", 0, "Set volumetric scattering type", "Type"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Volumetric scattering effect");
  }
};

struct SEffectNightVision
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<bool> ("NightVision_Active", false, "Enables night vision effect", "Enable NightVision"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Night vision effect");
  }
};

struct SEffectStripes
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
      InputPortConfig<bool> ("CryVision_Type", true, "Disable stripes effect", "Disable Stripes"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Stripes effect");
  }
};

struct SEffectMotionBlur
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = 
    {			
      InputPortConfig<bool> ("Enabled", false, "Enables node", "Enabled"),
			InputPortConfig<float> ("MotionBlur_Amount",0.0f,"Amount of motion blur","Amount"),
      {0}
    };

    static const SOutputPortConfig outputs[] = 
    {
      {0}
    };

    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Motion blur effect");
  }
};

template<class T>
class CFlowImageNode : public CFlowBaseNode
{
public:
  CFlowImageNode( SActivationInfo * pActInfo )
  {
  }

  ~CFlowImageNode()
  {
  }

	/* It's now a singleton node, so clone MUST not be implemented
  IFlowNodePtr Clone( SActivationInfo * pActInfo )
  {
    return this;
  }
	*/

  virtual void GetConfiguration(SFlowNodeConfig& config)
  {
    T::GetConfiguration(config);
    config.SetCategory(EFLN_ADVANCED);
  }

  virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    if ( event != eFE_Activate )
    {
      return;
    }
    
    const bool bIsActive( GetPortBool(pActInfo, 0) );

    if (bIsActive)
    {
      SFlowNodeConfig config;
      T::GetConfiguration(config);
      I3DEngine* pEngine = gEnv->p3DEngine;

      for ( int i( 1 ); config.pInputPorts[i].name ; ++i )
      {        
        const TFlowInputData& anyVal = GetPortAny(pActInfo, i);

        switch( anyVal.GetType() )
        {          

          case eFDT_Vec3:
            {                        
              Vec3 pVal( GetPortVec3( pActInfo, i) );      
              pEngine->SetPostEffectParamVec4(config.pInputPorts[i].name, Vec4(pVal, 1));
              
              break;
            }

          case eFDT_String:
            {            
              const string &pszString = GetPortString( pActInfo, i );
              pEngine->SetPostEffectParamString(config.pInputPorts[i].name, pszString.c_str());

              break;
            }

          default:
            {
              float fVal;
              bool ok( anyVal.GetValueWithConversion(fVal) );

              if( ok )
              {                
                pEngine->SetPostEffectParam(config.pInputPorts[i].name, fVal);
              }

              break;
            }
        }
      }
    }
  }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

// macro similar to REGISTER_FLOW_NODE, but allows a different name for registration
#define REGISTER_FLOW_NODE_SINGLETON_EX( FlowNodeClassName,FlowNodeClass,RegName ) \
  CAutoRegFlowNodeSingleton<FlowNodeClass> g_AutoReg##RegName ( FlowNodeClassName );

// these macros expose the nodes in the FlowSystem, otherwise they're not available in the system
REGISTER_FLOW_NODE_SINGLETON_EX("Image:ColorCorrection", CFlowImageNode<SColorCorrection>, SColorCorrection);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:FilterBlur", CFlowImageNode<SFilterBlur>, SFilterBlur);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:FilterRadialBlur", CFlowImageNode<SFilterRadialBlur>, SFilterRadialBlur);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:FilterSharpen", CFlowImageNode<SFilterSharpen>, SFilterSharpen);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:ChromaShift", CFlowImageNode<SFilterChromaShift>, SFilterChromaShift);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectFrost", CFlowImageNode<SEffectFrost>, SEffectFrost);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:FilterGrain", CFlowImageNode<SFilterGrain>, SFilterGrain);
//REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectScreenShatter", CFlowImageNode<SEffectScreenShatter>, SEffectScreenShatter);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectCondensation", CFlowImageNode<SEffectCondensation>, SEffectCondensation);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectWaterDroplets", CFlowImageNode<SEffectWaterDroplets>, SEffectWaterDroplets);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectWaterFlow", CFlowImageNode<SEffectWaterFlow>, SEffectWaterFlow);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:EffectBloodSplats", CFlowImageNode<SEffectBloodSplats>, SEffectBloodSplats);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:DepthOfField", CFlowImageNode<SEffectDof>, SEffectDof);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:VolumetricScattering", CFlowImageNode<SEffectVolumetricScattering>, SEffectVolumetricScattering);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:DirectionalBlur", CFlowImageNode<SEffectDirectionalBlur>, SEffectDirectionalBlur);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:AlienInterference", CFlowImageNode<SEffectAlienInterference>, SEffectAlienInterference);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:RainDrops", CFlowImageNode<SEffectRainDrops>, SEffectRainDrops);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:DistantRain", CFlowImageNode<SEffectDistantRain>, SEffectDistantRain);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:NightVision", CFlowImageNode<SEffectNightVision>, SEffectNightVision);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:Stripes", CFlowImageNode<SEffectStripes>, SEffectStripes);
REGISTER_FLOW_NODE_SINGLETON_EX("Image:MotionBlur", CFlowImageNode<SEffectMotionBlur>, SEffectMotionBlur);

/*
REGISTER_FLOW_NODE_SINGLETON_EX("ColorGrading:General", CFlowImageNode<SCGColorGeneral>, SCGColorGeneral);
REGISTER_FLOW_NODE_SINGLETON_EX("ColorGrading:Levels", CFlowImageNode<SCGLevels>, SCGLevels);
REGISTER_FLOW_NODE_SINGLETON_EX("ColorGrading:SelectiveColor", CFlowImageNode<SCGSelectiveColor>, SCGSelectiveColor);
REGISTER_FLOW_NODE_SINGLETON_EX("ColorGrading:Filters", CFlowImageNode<SCGFilters>, SCGFilters);
*/