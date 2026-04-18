#ifndef D3DSETTINGS_H
#define D3DSETTINGS_H

//-----------------------------------------------------------------------------
// Name: enum VertexProcessingType
// Desc: Enumeration of all possible D3D vertex processing types.
//-----------------------------------------------------------------------------
enum VertexProcessingType
{
  SOFTWARE_VP,
  MIXED_VP,
  HARDWARE_VP,
  PURE_HARDWARE_VP
};

//-----------------------------------------------------------------------------
// Name: struct D3DDSMSConflict
// Desc: A depth/stencil buffer format that is incompatible with a
//       multisample type.
//-----------------------------------------------------------------------------
struct D3DDSMSConflict
{
  D3DFORMAT DSFormat;
  D3DMULTISAMPLE_TYPE MSType;
};

//-----------------------------------------------------------------------------
// Name: struct D3DDeviceCombo
// Desc: A combination of adapter format, back buffer format, and windowed/fullscreen 
//       that is compatible with a particular D3D device (and the app).
//-----------------------------------------------------------------------------
struct D3DDeviceCombo
{
  int AdapterOrdinal;
  D3DDEVTYPE DeviceType;
  D3DFORMAT AdapterFormat;
  D3DFORMAT BackBufferFormat;
  bool Windowed;
  TArray<D3DFORMAT> *pDepthStencilFormatList; // List of D3DFORMATs
  TArray<D3DMULTISAMPLE_TYPE> *pMultiSampleTypeList; // List of D3DMULTISAMPLE_TYPEs
  TArray<DWORD> *pMultiSampleQualityList; // List of DWORDs (number of quality 
  // levels for each multisample type)
  TArray<D3DDSMSConflict> *pDSMSConflictList; // List of D3DDSMSConflicts
  TArray<VertexProcessingType> *pVertexProcessingTypeList; // List of VertexProcessingTypes
  TArray<UINT> *pPresentIntervalList; // List of D3DPRESENT_INTERVALs

  ~D3DDeviceCombo( void );
};

//-----------------------------------------------------------------------------
// Name: struct D3DDeviceInfo
// Desc: Info about a D3D device, including a list of D3DDeviceCombos (see below) 
//       that work with the device.
//-----------------------------------------------------------------------------
struct D3DDeviceInfo
{
  int AdapterOrdinal;
  D3DDEVTYPE DevType;
  D3DCAPS9 Caps;
  TArray<D3DDeviceCombo *> *pDeviceComboList; // List of D3DDeviceCombo pointers
  ~D3DDeviceInfo( void );
};

//-----------------------------------------------------------------------------
// Name: struct D3DAdapterInfo
// Desc: Info about a display adapter.
//-----------------------------------------------------------------------------
struct D3DAdapterInfo
{
  int AdapterOrdinal;
  uint m_MaxWidth;
  uint m_MaxHeight;
  D3DADAPTER_IDENTIFIER9 AdapterIdentifier;
  TArray<D3DDISPLAYMODE> *pDisplayModeList; // List of D3DDISPLAYMODEs
  TArray<D3DDeviceInfo*> *pDeviceInfoList; // List of D3DDeviceInfo pointers
  ~D3DAdapterInfo( void );
};

//-----------------------------------------------------------------------------
// Name: class CD3DSettings
// Desc: Current D3D settings: adapter, device, mode, formats, etc.
//-----------------------------------------------------------------------------
class CD3DSettings 
{
public:
    bool IsWindowed;

    D3DAdapterInfo* pWindowed_AdapterInfo;
    D3DDeviceInfo* pWindowed_DeviceInfo;
    D3DDeviceCombo* pWindowed_DeviceCombo;

    D3DDISPLAYMODE Windowed_DisplayMode; // not changable by the user
    D3DFORMAT Windowed_DepthStencilBufferFormat;
    D3DMULTISAMPLE_TYPE Windowed_MultisampleType;
    DWORD Windowed_MultisampleQuality;
    VertexProcessingType Windowed_VertexProcessingType;
    UINT Windowed_PresentInterval;
    int Windowed_Width;
    int Windowed_Height;

    D3DAdapterInfo* pFullscreen_AdapterInfo;
    D3DDeviceInfo* pFullscreen_DeviceInfo;
    D3DDeviceCombo* pFullscreen_DeviceCombo;

    D3DDISPLAYMODE Fullscreen_DisplayMode; // changable by the user
    D3DFORMAT Fullscreen_DepthStencilBufferFormat;
    D3DMULTISAMPLE_TYPE Fullscreen_MultisampleType;
    DWORD Fullscreen_MultisampleQuality;
    VertexProcessingType Fullscreen_VertexProcessingType;
    UINT Fullscreen_PresentInterval;

    D3DAdapterInfo* PAdapterInfo() { return IsWindowed ? pWindowed_AdapterInfo : pFullscreen_AdapterInfo; }
    D3DDeviceInfo* PDeviceInfo() { return IsWindowed ? pWindowed_DeviceInfo : pFullscreen_DeviceInfo; }
    D3DDeviceCombo* PDeviceCombo() { return IsWindowed ? pWindowed_DeviceCombo : pFullscreen_DeviceCombo; }

    int AdapterOrdinal() { return PDeviceCombo()->AdapterOrdinal; }
    D3DDEVTYPE DevType() { return PDeviceCombo()->DeviceType; }
    D3DFORMAT BackBufferFormat() { return PDeviceCombo()->BackBufferFormat; }
    D3DFORMAT AdapterFormat() { return PDeviceCombo()->AdapterFormat; }

    D3DDISPLAYMODE DisplayMode() { return IsWindowed ? Windowed_DisplayMode : Fullscreen_DisplayMode; }
    void SetDisplayMode(D3DDISPLAYMODE value) { if (IsWindowed) Windowed_DisplayMode = value; else Fullscreen_DisplayMode = value; }

    D3DFORMAT DepthStencilBufferFormat() { return IsWindowed ? Windowed_DepthStencilBufferFormat : Fullscreen_DepthStencilBufferFormat; }
    void SetDepthStencilBufferFormat(D3DFORMAT value) { if (IsWindowed) Windowed_DepthStencilBufferFormat = value; else Fullscreen_DepthStencilBufferFormat = value; }

    D3DMULTISAMPLE_TYPE MultisampleType() { return IsWindowed ? Windowed_MultisampleType : Fullscreen_MultisampleType; }
    void SetMultisampleType(D3DMULTISAMPLE_TYPE value) { if (IsWindowed) Windowed_MultisampleType = value; else Fullscreen_MultisampleType = value; }

    DWORD MultisampleQuality() { return IsWindowed ? Windowed_MultisampleQuality : Fullscreen_MultisampleQuality; }
    void SetMultisampleQuality(DWORD value) { if (IsWindowed) Windowed_MultisampleQuality = value; else Fullscreen_MultisampleQuality = value; }

    VertexProcessingType GetVertexProcessingType() { return IsWindowed ? Windowed_VertexProcessingType : Fullscreen_VertexProcessingType; }
    void SetVertexProcessingType(VertexProcessingType value) { if (IsWindowed) Windowed_VertexProcessingType = value; else Fullscreen_VertexProcessingType = value; }

    UINT PresentInterval() { return IsWindowed ? Windowed_PresentInterval : Fullscreen_PresentInterval; }
    void SetPresentInterval(UINT value) { if (IsWindowed) Windowed_PresentInterval = value; else Fullscreen_PresentInterval = value; }
};





#endif



