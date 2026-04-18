//--------------------------------------------------------------------------------------
// File: DXUT.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "StdAfx.h"
#include "../DriverD3D.h"

#if !defined(XENON)

#define UNICODE 1
#include "DXUT.h"
#define DXUT_MIN_WINDOW_SIZE_X 200
#define DXUT_MIN_WINDOW_SIZE_Y 200
#undef min // use __min instead inside this source file
#undef max // use __max instead inside this source file

//--------------------------------------------------------------------------------------
// Automatically enters & leaves the CS upon object creation/deletion
//--------------------------------------------------------------------------------------
class DXUTLock
{
public:
    inline DXUTLock()  { }
    inline ~DXUTLock() { }
};



//--------------------------------------------------------------------------------------
// Helper macros to build member functions that access member variables with thread safety
//--------------------------------------------------------------------------------------
#define SET_ACCESSOR( x, y )       inline void Set##y( x t )   { DXUTLock l; m_state.m_##y = t; };
#define GET_ACCESSOR( x, y )       inline x Get##y()           { DXUTLock l; return m_state.m_##y; };
#define GET_SET_ACCESSOR( x, y )   SET_ACCESSOR( x, y ) GET_ACCESSOR( x, y )

#define SETP_ACCESSOR( x, y )      inline void Set##y( x* t )  { DXUTLock l; m_state.m_##y = *t; };
#define GETP_ACCESSOR( x, y )      inline x* Get##y()          { DXUTLock l; return &m_state.m_##y; };
#define GETP_SETP_ACCESSOR( x, y ) SETP_ACCESSOR( x, y ) GETP_ACCESSOR( x, y )


//--------------------------------------------------------------------------------------
// Stores timer callback info
//--------------------------------------------------------------------------------------
struct DXUT_TIMER
{
    LPDXUTCALLBACKTIMER pCallbackTimer;
    void* pCallbackUserContext;
    float fTimeoutInSecs;
    float fCountdown;
    bool  bEnabled;
};


//--------------------------------------------------------------------------------------
// Stores DXUT state and data access is done with thread safety (if g_bThreadSafe==true)
//--------------------------------------------------------------------------------------
class DXUTState
{
protected:
    struct STATE
    {
        // D3D9 specific
        IDirect3D9*             m_D3D9;                    // the main D3D9 object
        IDirect3DDevice9*       m_D3D9Device;              // the D3D9 rendering device
        DXUTDeviceSettings*     m_CurrentDeviceSettings;   // current device settings
        D3DSURFACE_DESC         m_BackBufferSurfaceDesc9;  // D3D9 back buffer surface description
        D3DCAPS9                m_Caps;                    // D3D caps for current device

        // D3D10 specific
        bool                    m_D3D10Available;          // if true, then D3D10 is available 
#if defined (DIRECT3D10)
        IDXGIFactory*           m_DXGIFactory;             // DXGI Factory object
        IDXGIAdapter*           m_D3D10Adapter;            // The DXGI adapter object for the D3D10 device
        IDXGIOutput**           m_D3D10OutputArray;        // The array of output obj for the D3D10 adapter obj
        ID3D10Debug*            m_D3D10Debug;
        UINT                    m_D3D10OutputArraySize;    // Number of elements in m_D3D10OutputArray
        ID3D10Device*           m_D3D10Device;             // the D3D10 rendering device
        IDXGISwapChain*         m_D3D10SwapChain;          // the D3D10 swapchain
        ID3D10Texture2D*        m_D3D10DepthStencil;       // the D3D10 depth stencil texture (optional)
        ID3D10DepthStencilView* m_D3D10DepthStencilView;   // the D3D10 depth stencil view (optional)
        ID3D10RenderTargetView* m_D3D10RenderTargetView;   // the D3D10 render target view
        DXGI_SURFACE_DESC       m_BackBufferSurfaceDesc10; // D3D10 back buffer surface description
#endif
        bool                    m_RenderingOccluded;       // Rendering is occluded by another window
        bool                    m_DoNotStoreBufferSize;    // Do not store the buffer size on WM_SIZE messages
        bool                    m_DoNotResize;    
        bool                    m_NonExclusive;            // Another app has taken exclusive control

        // General
        HWND  m_HWNDFocus;                  // the main app focus window
        HWND  m_HWNDDeviceFullScreen;       // the main app device window in fullscreen mode
        HWND  m_HWNDDeviceWindowed;         // the main app device window in windowed mode
        
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
        HMONITOR m_AdapterMonitor;
        DWORD m_WindowedStyleAtModeChange;  // window style
        WINDOWPLACEMENT m_WindowedPlacement;  // record of windowed HWND position/show state/etc
#endif

        UINT m_FullScreenBackBufferWidthAtModeChange;  // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_FullScreenBackBufferHeightAtModeChange; // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_WindowBackBufferWidthAtModeChange;  // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode
        UINT m_WindowBackBufferHeightAtModeChange; // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode

        double m_Time;                      // current time in seconds
        double m_AbsoluteTime;              // absolute time in seconds
        float m_ElapsedTime;                // time elapsed since last frame

        HINSTANCE m_HInstance;              // handle to the app instance
        double m_LastStatsUpdateTime;       // last time the stats were updated
        DWORD m_LastStatsUpdateFrames;      // frames count since last time the stats were updated
        float m_FPS;                        // frames per second
        int   m_CurrentFrameNumber;         // the current frame number

        bool  m_NoStats;                    // if true, then DXUTGetFrameStats() and DXUTGetDeviceStats() will return blank strings
        float m_TimePerFrame;               // the constant time per frame in seconds, only valid if m_ConstantFrameTime==true
        bool  m_WireframeMode;              // if true, then D3DRS_FILLMODE==D3DFILL_WIREFRAME else D3DRS_FILLMODE==D3DFILL_SOLID 
        bool  m_AutoChangeAdapter;          // if true, then the adapter will automatically change if the window is different monitor
        bool  m_WindowCreatedWithDefaultPositions; // if true, then CW_USEDEFAULT was used and the window should be moved to the right adapter
        int   m_ExitCode;                   // the exit code to be returned to the command line

        bool  m_DXUTInited;                 // if true, then DXUTInit() has succeeded
        bool  m_WindowCreated;              // if true, then DXUTCreateWindow() or DXUTSetWindow() has succeeded
        bool  m_DeviceCreated;              // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() has succeeded

        bool  m_DXUTInitCalled;             // if true, then DXUTInit() was called
        bool  m_WindowCreateCalled;         // if true, then DXUTCreateWindow() or DXUTSetWindow() was called
        bool  m_DeviceCreateCalled;         // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() was called

        bool  m_DeviceObjectsCreated;       // if true, then DeviceCreated callback has been called (if non-NULL)
        bool  m_DeviceObjectsReset;         // if true, then DeviceReset callback has been called (if non-NULL)
        bool  m_InsideDeviceCallback;       // if true, then the framework is inside an app device callback
        bool  m_Active;                     // if true, then the app is the active top level window
        bool  m_TimePaused;                 // if true, then time is paused
        bool  m_RenderingPaused;            // if true, then rendering is paused
        int   m_PauseRenderingCount;        // pause rendering ref count
        int   m_PauseTimeCount;             // pause time ref count
        bool  m_DeviceLost;                 // if true, then the device is lost and needs to be reset
        bool  m_Automation;                 // if true, automation is enabled
        bool  m_InSizeMove;                 // if true, app is inside a WM_ENTERSIZEMOVE

        bool  m_Minimized;                  // if true, the HWND is minimized
        bool  m_Maximized;                  // if true, the HWND is maximized
        bool  m_MinimizedWhileFullscreen;   // if true, the HWND is minimized due to a focus switch away when fullscreen mode
        bool  m_IgnoreSizeChange;           // if true, DXUT won't reset the device upon HWND size change

        int   m_OverrideForceAPI;           // if != -1, then override to use this Direct3D API version
        int   m_OverrideAdapterOrdinal;     // if != -1, then override to use this adapter ordinal
        bool  m_OverrideWindowed;           // if true, then force to start windowed
        int   m_OverrideOutput;             // if != -1, then override to use the particular output on the adapter
        bool  m_OverrideFullScreen;         // if true, then force to start full screen
        int   m_OverrideStartX;             // if != -1, then override to this X position of the window
        int   m_OverrideStartY;             // if != -1, then override to this Y position of the window
        int   m_OverrideWidth;              // if != 0, then override to this width
        int   m_OverrideHeight;             // if != 0, then override to this height
        bool  m_OverrideForceHAL;           // if true, then force to HAL device (failing if one doesn't exist)
        bool  m_OverrideForceREF;           // if true, then force to REF device (failing if one doesn't exist)
				bool  m_OverrideForceNullREF;       // if true, then force to NULL_REF device (failing if one doesn't exist)
        bool  m_OverrideForcePureHWVP;      // if true, then force to use pure HWVP (failing if device doesn't support it)
        bool  m_OverrideForceHWVP;          // if true, then force to use HWVP (failing if device doesn't support it)
        bool  m_OverrideForceSWVP;          // if true, then force to use SWVP 
        int   m_OverrideForceVsync;         // if == 0, then it will force the app to use D3DPRESENT_INTERVAL_IMMEDIATE, if == 1 force use of D3DPRESENT_INTERVAL_DEFAULT

        LPDXUTCALLBACKMODIFYDEVICESETTINGS  m_ModifyDeviceSettingsFunc; // modify Direct3D device settings callback
        LPDXUTCALLBACKDEVICEREMOVED         m_DeviceRemovedFunc;        // Direct3D device removed callback
        LPDXUTCALLBACKFRAMEMOVE             m_FrameMoveFunc;            // frame move callback
        LPDXUTCALLBACKKEYBOARD              m_KeyboardFunc;             // keyboard callback
        LPDXUTCALLBACKMOUSE                 m_MouseFunc;                // mouse callback
        LPDXUTCALLBACKMSGPROC               m_WindowMsgFunc;            // window messages callback

        LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE    m_IsD3D9DeviceAcceptableFunc;   // D3D9 is device acceptable callback
        LPDXUTCALLBACKD3D9DEVICECREATED         m_D3D9DeviceCreatedFunc;        // D3D9 device created callback
        LPDXUTCALLBACKD3D9DEVICERESET           m_D3D9DeviceResetFunc;          // D3D9 device reset callback
        LPDXUTCALLBACKD3D9DEVICELOST            m_D3D9DeviceLostFunc;           // D3D9 device lost callback
        LPDXUTCALLBACKD3D9DEVICEDESTROYED       m_D3D9DeviceDestroyedFunc;      // D3D9 device destroyed callback
        LPDXUTCALLBACKD3D9FRAMERENDER           m_D3D9FrameRenderFunc;          // D3D9 frame render callback

#ifdef DIRECT3D10
        LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE   m_IsD3D10DeviceAcceptableFunc;  // D3D10 is device acceptable callback
        LPDXUTCALLBACKD3D10DEVICECREATED        m_D3D10DeviceCreatedFunc;       // D3D10 device created callback
        LPDXUTCALLBACKD3D10SWAPCHAINRESIZED     m_D3D10SwapChainResizedFunc;    // D3D10 SwapChain reset callback
        LPDXUTCALLBACKD3D10SWAPCHAINRELEASING   m_D3D10SwapChainReleasingFunc;  // D3D10 SwapChain lost callback
        LPDXUTCALLBACKD3D10DEVICEDESTROYED      m_D3D10DeviceDestroyedFunc;     // D3D10 device destroyed callback
        LPDXUTCALLBACKD3D10FRAMERENDER          m_D3D10FrameRenderFunc;         // D3D10 frame render callback
#endif

        void* m_ModifyDeviceSettingsFuncUserContext;     // user context for modify Direct3D device settings callback
        void* m_DeviceRemovedFuncUserContext;            // user context for Direct3D device removed callback
        void* m_FrameMoveFuncUserContext;                // user context for frame move callback
        void* m_KeyboardFuncUserContext;                 // user context for keyboard callback
        void* m_MouseFuncUserContext;                    // user context for mouse callback
        void* m_WindowMsgFuncUserContext;                // user context for window messages callback

        void* m_IsD3D9DeviceAcceptableFuncUserContext;   // user context for is D3D9 device acceptable callback
        void* m_D3D9DeviceCreatedFuncUserContext;        // user context for D3D9 device created callback
        void* m_D3D9DeviceResetFuncUserContext;          // user context for D3D9 device reset callback
        void* m_D3D9DeviceLostFuncUserContext;           // user context for D3D9 device lost callback
        void* m_D3D9DeviceDestroyedFuncUserContext;      // user context for D3D9 device destroyed callback
        void* m_D3D9FrameRenderFuncUserContext;          // user context for D3D9 frame render callback

#ifdef DIRECT3D10
        void* m_IsD3D10DeviceAcceptableFuncUserContext;  // user context for is D3D10 device acceptable callback
        void* m_D3D10DeviceCreatedFuncUserContext;       // user context for D3D10 device created callback
        void* m_D3D10SwapChainResizedFuncUserContext;    // user context for D3D10 SwapChain resized callback
        void* m_D3D10SwapChainReleasingFuncUserContext;  // user context for D3D10 SwapChain releasing callback
        void* m_D3D10DeviceDestroyedFuncUserContext;     // user context for D3D10 device destroyed callback
        void* m_D3D10FrameRenderFuncUserContext;         // user context for D3D10 frame render callback
#endif

        bool m_Keys[256];                                // array of key state
        bool m_MouseButtons[5];                          // array of mouse states

        CGrowableArray<DXUT_TIMER>*  m_TimerList;        // list of DXUT_TIMER structs
        WCHAR m_StaticFrameStats[256];                   // static part of frames stats 
        WCHAR m_FPSStats[64];                            // fps stats
        WCHAR m_FrameStats[256];                         // frame stats (fps, width, etc)
        WCHAR m_DeviceStats[256];                        // device stats (description, device type, etc)
        WCHAR m_WindowTitle[256];                        // window title
    };
    
    STATE m_state;

public:
		bool  m_bFastExit;                  // if true, app is exiting with exit() call

public:
    DXUTState()  { m_bFastExit = false; Create(); }
    ~DXUTState() { m_bFastExit = true; Destroy(); }

    void Create()
    {
        ZeroMemory( &m_state, sizeof(STATE) ); 
        m_state.m_OverrideStartX = -1; 
        m_state.m_OverrideStartY = -1; 
#if defined(PS3)
        m_state.m_OverrideForceAPI = 10; 
#else
        m_state.m_OverrideForceAPI = -1; 
#endif
        m_state.m_OverrideAdapterOrdinal = -1; 
        m_state.m_OverrideOutput = -1;
        m_state.m_OverrideForceVsync = -1;
        m_state.m_AutoChangeAdapter = true; 
        m_state.m_Active = true;
    }

    void Destroy()
    {
        DXUTShutdown();
    }

    // Macros to define access functions for thread safe access into m_state 

    // D3D9 specific
    GET_SET_ACCESSOR( IDirect3D9*, D3D9 );
    GET_SET_ACCESSOR( IDirect3DDevice9*, D3D9Device );
    GET_SET_ACCESSOR( DXUTDeviceSettings*, CurrentDeviceSettings );
    GETP_SETP_ACCESSOR( D3DSURFACE_DESC, BackBufferSurfaceDesc9 );
    GETP_SETP_ACCESSOR( D3DCAPS9, Caps );

    // D3D10 specific
#if defined (DIRECT3D10)
    GET_SET_ACCESSOR( bool, D3D10Available );
    GET_SET_ACCESSOR( IDXGIFactory*, DXGIFactory );
    GET_SET_ACCESSOR( IDXGIAdapter*, D3D10Adapter );
    GET_SET_ACCESSOR( IDXGIOutput**, D3D10OutputArray );
    GET_SET_ACCESSOR( UINT, D3D10OutputArraySize );
    GET_SET_ACCESSOR( ID3D10Device*, D3D10Device );
    GET_SET_ACCESSOR( ID3D10Debug*, D3D10Debug );
    GET_SET_ACCESSOR( IDXGISwapChain*, D3D10SwapChain );
    GET_SET_ACCESSOR( ID3D10Texture2D*, D3D10DepthStencil );
    GET_SET_ACCESSOR( ID3D10DepthStencilView*, D3D10DepthStencilView );   
    GET_SET_ACCESSOR( ID3D10RenderTargetView*, D3D10RenderTargetView );
    GETP_SETP_ACCESSOR( DXGI_SURFACE_DESC, BackBufferSurfaceDesc10 );
    GET_SET_ACCESSOR( bool, RenderingOccluded );
    GET_SET_ACCESSOR( bool, DoNotStoreBufferSize );
    GET_SET_ACCESSOR( bool, DoNotResize );
    GET_SET_ACCESSOR( bool, NonExclusive );
#endif

    GET_SET_ACCESSOR( HWND, HWNDFocus );
    GET_SET_ACCESSOR( HWND, HWNDDeviceFullScreen );
    GET_SET_ACCESSOR( HWND, HWNDDeviceWindowed );
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    GET_SET_ACCESSOR( HMONITOR, AdapterMonitor );

    GETP_SETP_ACCESSOR( WINDOWPLACEMENT, WindowedPlacement );
    GET_SET_ACCESSOR( DWORD, WindowedStyleAtModeChange );
#endif

    GET_SET_ACCESSOR( UINT, FullScreenBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, FullScreenBackBufferHeightAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferHeightAtModeChange );

    GET_SET_ACCESSOR( bool, Minimized );
    GET_SET_ACCESSOR( bool, Maximized );
    GET_SET_ACCESSOR( bool, MinimizedWhileFullscreen );
    GET_SET_ACCESSOR( bool, IgnoreSizeChange );   

    GET_SET_ACCESSOR( double, Time );
    GET_SET_ACCESSOR( double, AbsoluteTime );
    GET_SET_ACCESSOR( float, ElapsedTime );

    GET_SET_ACCESSOR( HINSTANCE, HInstance );
    GET_SET_ACCESSOR( double, LastStatsUpdateTime );   
    GET_SET_ACCESSOR( DWORD, LastStatsUpdateFrames );   
    GET_SET_ACCESSOR( float, FPS );    
    GET_SET_ACCESSOR( int, CurrentFrameNumber );

    GET_SET_ACCESSOR( bool, NoStats );
    GET_SET_ACCESSOR( float, TimePerFrame );
    GET_SET_ACCESSOR( bool, WireframeMode );   
    GET_SET_ACCESSOR( bool, AutoChangeAdapter );
    GET_SET_ACCESSOR( bool, WindowCreatedWithDefaultPositions );
    GET_SET_ACCESSOR( int, ExitCode );

    GET_SET_ACCESSOR( bool, DXUTInited );
    GET_SET_ACCESSOR( bool, WindowCreated );
    GET_SET_ACCESSOR( bool, DeviceCreated );
    GET_SET_ACCESSOR( bool, DXUTInitCalled );
    GET_SET_ACCESSOR( bool, WindowCreateCalled );
    GET_SET_ACCESSOR( bool, DeviceCreateCalled );
    GET_SET_ACCESSOR( bool, InsideDeviceCallback );
    GET_SET_ACCESSOR( bool, DeviceObjectsCreated );
    GET_SET_ACCESSOR( bool, DeviceObjectsReset );
    GET_SET_ACCESSOR( bool, Active );
    GET_SET_ACCESSOR( bool, RenderingPaused );
    GET_SET_ACCESSOR( bool, TimePaused );
    GET_SET_ACCESSOR( int, PauseRenderingCount );
    GET_SET_ACCESSOR( int, PauseTimeCount );
    GET_SET_ACCESSOR( bool, DeviceLost );
    GET_SET_ACCESSOR( bool, Automation );
    GET_SET_ACCESSOR( bool, InSizeMove );

    GET_SET_ACCESSOR( int, OverrideForceAPI );
    GET_SET_ACCESSOR( int, OverrideAdapterOrdinal );
    GET_SET_ACCESSOR( bool, OverrideWindowed );
    GET_SET_ACCESSOR( int, OverrideOutput );
    GET_SET_ACCESSOR( bool, OverrideFullScreen );
    GET_SET_ACCESSOR( int, OverrideStartX );
    GET_SET_ACCESSOR( int, OverrideStartY );
    GET_SET_ACCESSOR( int, OverrideWidth );
    GET_SET_ACCESSOR( int, OverrideHeight );
    GET_SET_ACCESSOR( bool, OverrideForceHAL );
    GET_SET_ACCESSOR( bool, OverrideForceREF );
		GET_SET_ACCESSOR( bool, OverrideForceNullREF );
    GET_SET_ACCESSOR( bool, OverrideForcePureHWVP );
    GET_SET_ACCESSOR( bool, OverrideForceHWVP );
    GET_SET_ACCESSOR( bool, OverrideForceSWVP );
    GET_SET_ACCESSOR( int, OverrideForceVsync );

    GET_SET_ACCESSOR( LPDXUTCALLBACKMODIFYDEVICESETTINGS, ModifyDeviceSettingsFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKDEVICEREMOVED, DeviceRemovedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKFRAMEMOVE, FrameMoveFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKKEYBOARD, KeyboardFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMOUSE, MouseFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMSGPROC, WindowMsgFunc );

    GET_SET_ACCESSOR( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE, IsD3D9DeviceAcceptableFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICECREATED, D3D9DeviceCreatedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICERESET, D3D9DeviceResetFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICELOST, D3D9DeviceLostFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICEDESTROYED, D3D9DeviceDestroyedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9FRAMERENDER, D3D9FrameRenderFunc );

#ifdef DIRECT3D10
    GET_SET_ACCESSOR( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE, IsD3D10DeviceAcceptableFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10DEVICECREATED, D3D10DeviceCreatedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10SWAPCHAINRESIZED, D3D10SwapChainResizedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10SWAPCHAINRELEASING, D3D10SwapChainReleasingFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10DEVICEDESTROYED, D3D10DeviceDestroyedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10FRAMERENDER, D3D10FrameRenderFunc );
#endif

    GET_SET_ACCESSOR( void*, ModifyDeviceSettingsFuncUserContext );
    GET_SET_ACCESSOR( void*, DeviceRemovedFuncUserContext );
    GET_SET_ACCESSOR( void*, FrameMoveFuncUserContext );
    GET_SET_ACCESSOR( void*, KeyboardFuncUserContext );
    GET_SET_ACCESSOR( void*, MouseFuncUserContext );
    GET_SET_ACCESSOR( void*, WindowMsgFuncUserContext );

    GET_SET_ACCESSOR( void*, IsD3D9DeviceAcceptableFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceCreatedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceResetFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceLostFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceDestroyedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9FrameRenderFuncUserContext );

#ifdef DIRECT3D10
    GET_SET_ACCESSOR( void*, IsD3D10DeviceAcceptableFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10DeviceCreatedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10DeviceDestroyedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10SwapChainResizedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10SwapChainReleasingFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10FrameRenderFuncUserContext );
#endif

    GET_SET_ACCESSOR( CGrowableArray<DXUT_TIMER>*, TimerList );
    GET_ACCESSOR( bool*, Keys );
    GET_ACCESSOR( bool*, MouseButtons );
    GET_ACCESSOR( WCHAR*, StaticFrameStats );
    GET_ACCESSOR( WCHAR*, FPSStats );
    GET_ACCESSOR( WCHAR*, FrameStats );
    GET_ACCESSOR( WCHAR*, DeviceStats );    
    GET_ACCESSOR( WCHAR*, WindowTitle );
};


//--------------------------------------------------------------------------------------
// Global state class
//--------------------------------------------------------------------------------------
DXUTState& GetDXUTState()
{
    // Using an accessor function gives control of the construction order
    //static DXUTState state;
    //return state;
	static DXUTState* s_pState(0);
	if (!s_pState)
		s_pState = new DXUTState;
	return *s_pState;
}


//--------------------------------------------------------------------------------------
// Internal functions forward declarations
//--------------------------------------------------------------------------------------
void    DXUTParseCommandLine( WCHAR* strCommandLine );
bool    DXUTIsNextArg( WCHAR*& strCmdLine, WCHAR* strArg );
bool    DXUTGetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag );
void    DXUTAllowShortcutKeys( bool bAllowKeys );
void    DXUTHandleTimers();
void    DXUTDisplayErrorMessage( HRESULT hr );
int     DXUTMapButtonToArrayIndex( BYTE vButton );

HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings, IDirect3DDevice9* pd3d9DeviceFromApp,
#ifdef DIRECT3D10
                         ID3D10Device* pd3d10DeviceFromApp,
#endif
                         bool bForceRecreate, bool bClipWindowToSingleAdapter );
bool    DXUTCanDeviceBeReset( DXUTDeviceSettings *pOldDeviceSettings, DXUTDeviceSettings *pNewDeviceSettings, IDirect3DDevice9 *pd3d9DeviceFromApp
#ifdef DIRECT3D10
                             , ID3D10Device *pd3d10DeviceFromApp
#endif
                             );
#ifdef DIRECT3D10
HRESULT DXUTDelayLoadDXGI();
#else
HRESULT DXUTDelayLoadD3D9();
#endif
void    DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings );
void    DXUTCheckForWindowSizeChange();
void    DXUTCheckForWindowChangingMonitors();
void    DXUTCleanup3DEnvironment( bool bReleaseSettings );

HRESULT DXUTHandleDeviceRemoved();
void    DXUTUpdateBackBufferDesc();

HMONITOR DXUTGetMonitorFromAdapter( DXUTDeviceSettings* pDeviceSettings );
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal );

#if defined (DIRECT3D9) || defined (OPENGL)
HRESULT DXUTCreate3DEnvironment9( IDirect3DDevice9* pd3dDeviceFromApp );
HRESULT DXUTReset3DEnvironment9();
void    DXUTCleanup3DEnvironment9( bool bReleaseSettings = true );
void    DXUTUpdateD3D9DeviceStats( D3DDEVTYPE DeviceType, DWORD BehaviorFlags, D3DADAPTER_IDENTIFIER9* pAdapterIdentifier );
HRESULT DXUTFindD3D9AdapterFormat( UINT AdapterOrdinal, D3DDEVTYPE DeviceType, D3DFORMAT BackBufferFormat, BOOL Windowed, D3DFORMAT* pAdapterFormat );
#endif

#ifdef DIRECT3D10
HRESULT DXUTSetupD3D10Views( ID3D10Device* pd3dDevice, DXUTDeviceSettings* pDeviceSettings );
HRESULT DXUTCreate3DEnvironment10( ID3D10Device* pd3dDeviceFromApp );
HRESULT DXUTReset3DEnvironment10();
void    DXUTCleanup3DEnvironment10( bool bReleaseSettings = true );
void    DXUTUpdateD3D10DeviceStats( D3D10_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc );

void    WINAPI DXUTD3D10SetRenderingOccluded(bool bOccluded) { GetDXUTState().SetRenderingOccluded(bOccluded); }
bool    WINAPI DXUTD3D10GetRenderingOccluded() { return GetDXUTState().GetRenderingOccluded(); }
void    WINAPI DXUTD3D10SetRenderingNonExclusive(bool bNonExclusove)  { GetDXUTState().SetNonExclusive(bNonExclusove); }
bool    WINAPI DXUTD3D10GetRenderingNonExclusive() { return GetDXUTState().GetNonExclusive(); }
#endif

void DXUTSetMinimized( bool bSet )
{
  GetDXUTState().SetMinimized( bSet );
}
void DXUTSetMaximized( bool bSet )
{
  GetDXUTState().SetMaximized( bSet );
}
void DXUTSetMinimizedFS( bool bSet )
{
  GetDXUTState().SetMinimizedWhileFullscreen( bSet );
}
bool DXUTGetMinimizedFS( )
{
  return GetDXUTState().GetMinimizedWhileFullscreen( );
}
void DXUTSetActive( bool bSet )
{
  GetDXUTState().SetActive( bSet );
  gcpRendD3D->m_bDeviceLost = !bSet;
}

//--------------------------------------------------------------------------------------
// Internal helper functions 
//--------------------------------------------------------------------------------------
bool DXUTIsD3D9( DXUTDeviceSettings* pDeviceSettings )                          { return (pDeviceSettings && pDeviceSettings->ver == DXUT_D3D9_DEVICE ); };
bool DXUTIsCurrentDeviceD3D9()                                                  { DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();  return DXUTIsD3D9(pDeviceSettings); };
UINT DXUTGetBackBufferWidthFromDS( DXUTDeviceSettings* pNewDeviceSettings )
{
  if (DXUTIsD3D9(pNewDeviceSettings))
    return pNewDeviceSettings->d3d9.pp.BackBufferWidth;
#ifdef DIRECT3D10
  else
    return pNewDeviceSettings->d3d10.sd.BufferDesc.Width;
#else
  return 0;
#endif
}
UINT DXUTGetBackBufferHeightFromDS( DXUTDeviceSettings* pNewDeviceSettings )
{
  if (DXUTIsD3D9(pNewDeviceSettings))
    return pNewDeviceSettings->d3d9.pp.BackBufferHeight;
#ifdef DIRECT3D10
  else
    return pNewDeviceSettings->d3d10.sd.BufferDesc.Height;
#else
  return 0;
#endif
}
bool DXUTGetIsWindowedFromDS( DXUTDeviceSettings* pNewDeviceSettings )
{
  if (!pNewDeviceSettings)
    return true;
  if (DXUTIsD3D9(pNewDeviceSettings))
    return pNewDeviceSettings->d3d9.pp.Windowed != 0;
#ifdef DIRECT3D10
  else
    return pNewDeviceSettings->d3d10.sd.Windowed != 0;
#else
  return 0;
#endif
}

//--------------------------------------------------------------------------------------
// External state access functions
//--------------------------------------------------------------------------------------
DXUTDeviceSettings* WINAPI DXUTGetCurrentDeviceSettings()           { return GetDXUTState().GetCurrentDeviceSettings(); }
IDirect3DDevice9* WINAPI DXUTGetD3D9Device()               { return GetDXUTState().GetD3D9Device(); }
const D3DSURFACE_DESC* WINAPI DXUTGetD3D9BackBufferSurfaceDesc() { return GetDXUTState().GetBackBufferSurfaceDesc9(); }
const D3DCAPS9* WINAPI DXUTGetD3D9DeviceCaps()             { return GetDXUTState().GetCaps(); }
#ifdef DIRECT3D10
ID3D10Device* WINAPI DXUTGetD3D10Device()                  { return GetDXUTState().GetD3D10Device(); }
IDXGISwapChain* WINAPI DXUTGetDXGISwapChain()              { return GetDXUTState().GetD3D10SwapChain(); }
ID3D10RenderTargetView* WINAPI DXUTGetD3D10RenderTargetView() { return GetDXUTState().GetD3D10RenderTargetView(); }
ID3D10DepthStencilView* WINAPI DXUTGetD3D10DepthStencilView() { return GetDXUTState().GetD3D10DepthStencilView(); }
ID3D10Texture2D* WINAPI DXUTGetD3D10DepthStencil() { return GetDXUTState().GetD3D10DepthStencil(); }
const DXGI_SURFACE_DESC* WINAPI DXUTGetDXGIBackBufferSurfaceDesc() { return GetDXUTState().GetBackBufferSurfaceDesc10(); }
ID3D10Debug* WINAPI DXUTGetD3D10Debug()
#if defined(PS3)
	 {return 0;}
#else
	 {return GetDXUTState().GetD3D10Debug(); }
#endif
#endif
HWND WINAPI DXUTGetHWND()                                  { return DXUTIsWindowed() ? GetDXUTState().GetHWNDDeviceWindowed() : GetDXUTState().GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDFocus()                             { return GetDXUTState().GetHWNDFocus(); }
HWND WINAPI DXUTGetHWNDDeviceFullScreen()                  { return GetDXUTState().GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDDeviceWindowed()                    { return GetDXUTState().GetHWNDDeviceWindowed(); }
double WINAPI DXUTGetTime()                                { return GetDXUTState().GetTime(); }
float WINAPI DXUTGetElapsedTime()                          { return GetDXUTState().GetElapsedTime(); }
LPCWSTR WINAPI DXUTGetDeviceStats()                        { return GetDXUTState().GetDeviceStats(); }
bool WINAPI DXUTIsTimePaused()                             { return GetDXUTState().GetPauseTimeCount() > 0; }
bool WINAPI DXUTIsActive()                                 { return GetDXUTState().GetActive(); }
int WINAPI DXUTGetExitCode()                               { return GetDXUTState().GetExitCode(); }
bool WINAPI DXUTGetAutomation()                            { return GetDXUTState().GetAutomation(); }
bool WINAPI DXUTIsWindowed()                               { return DXUTGetIsWindowedFromDS( GetDXUTState().GetCurrentDeviceSettings() ); }

#if defined (DIRECT3D9) || defined (OPENGL)
IDirect3D9* WINAPI DXUTGetD3D9Object()                     { DXUTDelayLoadD3D9(); return GetDXUTState().GetD3D9(); }
#endif

#ifdef DIRECT3D10
IDXGIFactory* WINAPI DXUTGetDXGIFactory()                  { DXUTDelayLoadDXGI(); return GetDXUTState().GetDXGIFactory(); }
bool WINAPI DXUTIsD3D10Available()                         { DXUTDelayLoadDXGI(); return GetDXUTState().GetD3D10Available(); }
#endif
bool WINAPI DXUTIsAppRenderingWithD3D9()                   { return (GetDXUTState().GetD3D9Device() != NULL); }
#ifdef DIRECT3D10
bool WINAPI DXUTIsAppRenderingWithD3D10()                  { return (GetDXUTState().GetD3D10Device() != NULL); }
#endif

//--------------------------------------------------------------------------------------
// External callback setup functions
//--------------------------------------------------------------------------------------

// General callbacks
void WINAPI DXUTSetCallbackDeviceChanging( LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallback, void* pUserContext )                  { GetDXUTState().SetModifyDeviceSettingsFunc( pCallback ); GetDXUTState().SetModifyDeviceSettingsFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackDeviceRemoved( LPDXUTCALLBACKDEVICEREMOVED pCallback, void* pUserContext )                          { GetDXUTState().SetDeviceRemovedFunc( pCallback ); GetDXUTState().SetDeviceRemovedFuncUserContext( pUserContext ); }

// Direct3D 9 callbacks
void WINAPI DXUTSetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE pCallback, void* pUserContext )          { GetDXUTState().SetIsD3D9DeviceAcceptableFunc( pCallback ); GetDXUTState().SetIsD3D9DeviceAcceptableFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceCreated( LPDXUTCALLBACKD3D9DEVICECREATED pCallback, void* pUserContext )                  { GetDXUTState().SetD3D9DeviceCreatedFunc( pCallback ); GetDXUTState().SetD3D9DeviceCreatedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceReset( LPDXUTCALLBACKD3D9DEVICERESET pCallback, void* pUserContext )                      { GetDXUTState().SetD3D9DeviceResetFunc( pCallback );  GetDXUTState().SetD3D9DeviceResetFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceLost( LPDXUTCALLBACKD3D9DEVICELOST pCallback, void* pUserContext )                        { GetDXUTState().SetD3D9DeviceLostFunc( pCallback );  GetDXUTState().SetD3D9DeviceLostFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceDestroyed( LPDXUTCALLBACKD3D9DEVICEDESTROYED pCallback, void* pUserContext )              { GetDXUTState().SetD3D9DeviceDestroyedFunc( pCallback );  GetDXUTState().SetD3D9DeviceDestroyedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9FrameRender( LPDXUTCALLBACKD3D9FRAMERENDER pCallback, void* pUserContext )                      { GetDXUTState().SetD3D9FrameRenderFunc( pCallback );  GetDXUTState().SetD3D9FrameRenderFuncUserContext( pUserContext ); }
void DXUTGetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE* ppCallback, void** ppUserContext )             { *ppCallback = GetDXUTState().GetIsD3D9DeviceAcceptableFunc(); *ppUserContext = GetDXUTState().GetIsD3D9DeviceAcceptableFuncUserContext(); }

#ifdef DIRECT3D10
// Direct3D 10 callbacks
void WINAPI DXUTSetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE pCallback, void* pUserContext )        { GetDXUTState().SetIsD3D10DeviceAcceptableFunc( pCallback ); GetDXUTState().SetIsD3D10DeviceAcceptableFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10DeviceCreated( LPDXUTCALLBACKD3D10DEVICECREATED pCallback, void* pUserContext )                { GetDXUTState().SetD3D10DeviceCreatedFunc( pCallback ); GetDXUTState().SetD3D10DeviceCreatedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10SwapChainResized( LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallback, void* pUserContext )          { GetDXUTState().SetD3D10SwapChainResizedFunc( pCallback );  GetDXUTState().SetD3D10SwapChainResizedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10FrameRender( LPDXUTCALLBACKD3D10FRAMERENDER pCallback, void* pUserContext )                    { GetDXUTState().SetD3D10FrameRenderFunc( pCallback );  GetDXUTState().SetD3D10FrameRenderFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10SwapChainReleasing( LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallback, void* pUserContext )      { GetDXUTState().SetD3D10SwapChainReleasingFunc( pCallback );  GetDXUTState().SetD3D10SwapChainReleasingFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10DeviceDestroyed( LPDXUTCALLBACKD3D10DEVICEDESTROYED pCallback, void* pUserContext )            { GetDXUTState().SetD3D10DeviceDestroyedFunc( pCallback );  GetDXUTState().SetD3D10DeviceDestroyedFuncUserContext( pUserContext ); }
void DXUTGetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE* ppCallback, void** ppUserContext )           { *ppCallback = GetDXUTState().GetIsD3D10DeviceAcceptableFunc(); *ppUserContext = GetDXUTState().GetIsD3D10DeviceAcceptableFuncUserContext(); }
#endif

//--------------------------------------------------------------------------------------
// Optionally parses the command line and sets if default hotkeys are handled
//
//       Possible command line parameters are:
//          -forceapi:#             forces app to use specified Direct3D API version (fails if the application doesn't support this API or if no device is found)
//          -adapter:#              forces app to use this adapter # (fails if the adapter doesn't exist)
//          -output:#               [D3D10 only] forces app to use a particular output on the adapter (fails if the output doesn't exist) 
//          -windowed               forces app to start windowed
//          -fullscreen             forces app to start full screen
//          -forcehal               forces app to use HAL (fails if HAL doesn't exist)
//          -forceref               forces app to use REF (fails if REF doesn't exist)
//          -forcenullref           forces app to use NULL_REF (fails if REF doesn't exist)
//          -forcepurehwvp          [D3D9 only] forces app to use pure HWVP (fails if device doesn't support it)
//          -forcehwvp              [D3D9 only] forces app to use HWVP (fails if device doesn't support it)
//          -forceswvp              [D3D9 only] forces app to use SWVP 
//          -forcevsync:#           if # is 0, then vsync is disabled 
//          -width:#                forces app to use # for width. for full screen, it will pick the closest possible supported mode
//          -height:#               forces app to use # for height. for full screen, it will pick the closest possible supported mode
//          -startx:#               forces app to use # for the x coord of the window position for windowed mode
//          -starty:#               forces app to use # for the y coord of the window position for windowed mode
//          -constantframetime:#    forces app to use constant frame time, where # is the time/frame in seconds
//          -quitafterframe:x       forces app to quit after # frames
//          -noerrormsgboxes        prevents the display of message boxes generated by the framework so the application can be run without user interaction
//          -nostats                prevents the display of the stats
//          -automation             a hint to other components that automation is active 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTInit( bool bParseCommandLine, bool bShowMsgBoxOnError, WCHAR* strExtraCommandLineParams )
{
    GetDXUTState().SetDXUTInitCalled( true );

    // Not always needed, but lets the app create GDI dialogs
    //InitCommonControls();

#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    if( bParseCommandLine )
        DXUTParseCommandLine( GetCommandLineW() );
    if( strExtraCommandLineParams )
        DXUTParseCommandLine( strExtraCommandLineParams );
#endif

    // Reset the timer
    DXUTGetGlobalTimer()->Reset();

    GetDXUTState().SetDXUTInited( true );

    return S_OK;
}

#if defined (DIRECT3D9) || defined (DIRECT3D10)
//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
void DXUTParseCommandLine( WCHAR* strCommandLine )
{
	//no commandline on PS3
#if !defined(PS3)
    WCHAR* strCmdLine;
    WCHAR strFlag[MAX_PATH];

    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( strCommandLine, &nNumArgs );
    for( int iArg=0; iArg<nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( DXUTIsNextArg( strCmdLine, L"forceapi" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAPIVersion = _wtoi(strFlag);
                    GetDXUTState().SetOverrideForceAPI( nAPIVersion );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"adapter" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAdapter = _wtoi(strFlag);
                    GetDXUTState().SetOverrideAdapterOrdinal( nAdapter );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"windowed" ) )
            {
                GetDXUTState().SetOverrideWindowed( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"output" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int Output = _wtoi(strFlag);
                    GetDXUTState().SetOverrideOutput( Output );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"fullscreen" ) )
            {
                GetDXUTState().SetOverrideFullScreen( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcehal" ) )
            {
                GetDXUTState().SetOverrideForceHAL( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forceref" ) )
            {
                GetDXUTState().SetOverrideForceREF( true );
                continue;
            }
						
						if( DXUTIsNextArg( strCmdLine, L"forcenullref" ) )
						{
							GetDXUTState().SetOverrideForceNullREF( true );
							continue;
						} 

            if( DXUTIsNextArg( strCmdLine, L"forcepurehwvp" ) )
            {
                GetDXUTState().SetOverrideForcePureHWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcehwvp" ) )
            {
                GetDXUTState().SetOverrideForceHWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forceswvp" ) )
            {
                GetDXUTState().SetOverrideForceSWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcevsync" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nOn = _wtoi(strFlag);
                    GetDXUTState().SetOverrideForceVsync( nOn );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"width" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nWidth = _wtoi(strFlag);
                    GetDXUTState().SetOverrideWidth( nWidth );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"height" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nHeight = _wtoi(strFlag);
                    GetDXUTState().SetOverrideHeight( nHeight );
                continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"startx" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nX = _wtoi(strFlag);
                    GetDXUTState().SetOverrideStartX( nX );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"starty" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nY = _wtoi(strFlag);
                    GetDXUTState().SetOverrideStartY( nY );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"nostats" ) )
            {
                GetDXUTState().SetNoStats( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"automation" ) )
            {
                GetDXUTState().SetAutomation( true );
                continue;
            }
        }

        // Unrecognized flag
        StringCchCopyW( strFlag, 256, strCmdLine ); 
        WCHAR* strSpace = strFlag;
        while (*strSpace && (*strSpace > L' '))
            strSpace++;
        *strSpace = 0;

        DXUTOutputDebugStringW( L"Unrecognized flag: %s", strFlag );
        strCmdLine += wcslen(strFlag);
    }
#endif // PS3
}


#if !defined(PS3)
//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine
//--------------------------------------------------------------------------------------
bool DXUTIsNextArg( WCHAR*& strCmdLine, WCHAR* strArg )
{
    int nArgLen = (int) wcslen(strArg);
    int nCmdLen = (int) wcslen(strCmdLine);

    if( nCmdLen >= nArgLen && 
        _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 && 
        (strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':') )
    {
        strCmdLine += nArgLen;
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool DXUTGetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
    if( *strCmdLine == L':' )
    {       
        strCmdLine++; // Skip ':'

        // Place NULL terminator in strFlag after current token
        StringCchCopyW( strFlag, 256, strCmdLine );
        WCHAR* strSpace = strFlag;
        while (*strSpace && (*strSpace > L' '))
            strSpace++;
        *strSpace = 0;
    
        // Update strCmdLine
        strCmdLine += wcslen(strFlag);
        return true;
    }
    else
    {
        strFlag[0] = 0;
        return false;
    }
}
#endif //PS3

#endif


//--------------------------------------------------------------------------------------
// Sets a previously created window for the framework to use.  If DXUTInit() 
// has not already been called, it will call it with the default parameters.  
// Instead of calling this, you can call DXUTCreateWindow() to create a new window.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetWindow( HWND hWndFocus, HWND hWndDeviceFullScreen, HWND hWndDeviceWindowed, bool bHandleMessages )
{
    HRESULT hr;
 
    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTCreateWindow", E_FAIL );

    GetDXUTState().SetWindowCreateCalled( true );

    // To avoid confusion, we do not allow any HWND to be NULL here.  The
    // caller must pass in valid HWND for all three parameters.  The same
    // HWND may be used for more than one parameter.
    if( hWndFocus == NULL || hWndDeviceFullScreen == NULL || hWndDeviceWindowed == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetWindow", E_INVALIDARG );

    // If subclassing the window, set the pointer to the local window procedure
    if( bHandleMessages )
    {
      assert(0);
    }
 
    if( !GetDXUTState().GetDXUTInited() ) 
    {
        // If DXUTInit() was already called and failed, then fail.
        // DXUTInit() must first succeed for this function to succeed
        if( GetDXUTState().GetDXUTInitCalled() )
            return E_FAIL; 
 
        // If DXUTInit() hasn't been called, then automatically call it
        // with default params
        hr = DXUTInit();
        if( FAILED(hr) )
            return hr;
    }
 
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    WCHAR* strCachedWindowTitle = GetDXUTState().GetWindowTitle();
    GetWindowTextW( hWndFocus, strCachedWindowTitle, 255 );
    strCachedWindowTitle[255] = 0;
   
    HINSTANCE hInstance = (HINSTANCE) (LONG_PTR) GetWindowLongPtr( hWndFocus, GWLP_HINSTANCE ); 
    GetDXUTState().SetHInstance( hInstance );
#endif
    GetDXUTState().SetWindowCreatedWithDefaultPositions( false );
    GetDXUTState().SetWindowCreated( true );
    GetDXUTState().SetHWNDFocus( hWndFocus );
    GetDXUTState().SetHWNDDeviceFullScreen( hWndDeviceFullScreen );
    GetDXUTState().SetHWNDDeviceWindowed( hWndDeviceWindowed );

    return S_OK;
}

//======================================================================================
//======================================================================================
// Direct3D section
//======================================================================================
//======================================================================================


//--------------------------------------------------------------------------------------
// Creates a Direct3D device. If DXUTCreateWindow() or DXUTSetWindow() has not already 
// been called, it will call DXUTCreateWindow() with the default parameters.  
// Instead of calling this, you can call DXUTSetD3D*Device() or DXUTCreateDeviceFromSettings().
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateDevice( bool bWindowed, int nSuggestedWidth, int nSuggestedHeight, int nSuggestStencil, int nSuggestColor )
{
    HRESULT hr = S_OK;

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTCreateWindow", E_FAIL );

    GetDXUTState().SetDeviceCreateCalled( true );

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion         = DXUTMT_IGNORE_INPUT;
    matchOptions.eAdapterOrdinal     = DXUTMT_IGNORE_INPUT;
    matchOptions.eDeviceType         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eWindowed           = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterFormat      = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eVertexProcessing   = DXUTMT_IGNORE_INPUT;
    if( bWindowed || (nSuggestedWidth != 0 && nSuggestedHeight != 0) )
        matchOptions.eResolution     = DXUTMT_CLOSEST_TO_INPUT;
    else
        matchOptions.eResolution     = DXUTMT_IGNORE_INPUT;
    matchOptions.eBackBufferFormat   = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample        = DXUTMT_IGNORE_INPUT;
    matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat      = nSuggestStencil ? DXUTMT_CLOSEST_TO_INPUT : DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentFlags       = DXUTMT_IGNORE_INPUT;
    matchOptions.eRefreshRate        = DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;

    // Building D3D9 device settings for match options.  These
    // will be converted to D3D10 settings if app can use D3D10
    DXUTDeviceSettings deviceSettings;
    ZeroMemory( &deviceSettings, sizeof(DXUTDeviceSettings) );
    deviceSettings.ver = DXUT_D3D9_DEVICE;
    deviceSettings.d3d9.pp.Windowed         = bWindowed;
    deviceSettings.d3d9.pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    deviceSettings.d3d9.pp.BackBufferWidth  = nSuggestedWidth;
    deviceSettings.d3d9.pp.BackBufferHeight = nSuggestedHeight;
    deviceSettings.d3d9.pp.AutoDepthStencilFormat = nSuggestStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
    deviceSettings.d3d9.pp.BackBufferFormat = nSuggestColor==32 ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    deviceSettings.d3d9.pp.BackBufferCount = CD3D9Renderer::CV_d3d9_triplebuffering ? 2 : 1;
    deviceSettings.d3d9.pp.PresentationInterval = gcpRendD3D->m_VSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
    deviceSettings.d3d9.AdapterFormat = D3DFMT_X8R8G8B8;
    deviceSettings.d3d9.DeviceType = D3DDEVTYPE_HAL;
    deviceSettings.d3d9.BehaviorFlags = D3DCREATE_FPU_PRESERVE;


    GetDXUTState().SetOverrideForcePureHWVP( true );

    // Override with settings from the command line
    if( GetDXUTState().GetOverrideWidth() != 0 )
    {
        deviceSettings.d3d9.pp.BackBufferWidth = GetDXUTState().GetOverrideWidth();
        matchOptions.eResolution = DXUTMT_PRESERVE_INPUT;
    }
    if( GetDXUTState().GetOverrideHeight() != 0 )
    {
        deviceSettings.d3d9.pp.BackBufferHeight = GetDXUTState().GetOverrideHeight();
        matchOptions.eResolution = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
    {
        deviceSettings.d3d9.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideFullScreen() )
    {
        deviceSettings.d3d9.pp.Windowed = FALSE;
        if( GetDXUTState().GetOverrideWidth() == 0 && GetDXUTState().GetOverrideHeight() == 0 )
            matchOptions.eResolution = DXUTMT_IGNORE_INPUT;
    }
    if( GetDXUTState().GetOverrideWindowed() )
        deviceSettings.d3d9.pp.Windowed = TRUE;

    if( GetDXUTState().GetOverrideForceHAL() )
    {
        deviceSettings.d3d9.DeviceType = D3DDEVTYPE_HAL;
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }
    if( GetDXUTState().GetOverrideForceREF() )
    {
        deviceSettings.d3d9.DeviceType = D3DDEVTYPE_REF;
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }
		if( GetDXUTState().GetOverrideForceNullREF() )
		{
			deviceSettings.d3d9.DeviceType = D3DDEVTYPE_NULLREF;
			matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
		}

    if( GetDXUTState().GetOverrideForcePureHWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceHWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceSWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideForceVsync() == 0 )
    {
        deviceSettings.d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        matchOptions.ePresentInterval = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceVsync() == 1 )
    {
        deviceSettings.d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        matchOptions.ePresentInterval = DXUTMT_PRESERVE_INPUT;
    }
 
    if( GetDXUTState().GetOverrideForceAPI() != -1 )
    {
        if( GetDXUTState().GetOverrideForceAPI() == 9 )
        {
            deviceSettings.ver = DXUT_D3D9_DEVICE;
            matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
        }
        else if( GetDXUTState().GetOverrideForceAPI() == 10 )
        {
#ifdef DIRECT3D10
            deviceSettings.ver = DXUT_D3D10_DEVICE;
            matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;

            // Convert the struct we're making to be D3D10 settings since 
            // that is what DXUTFindValidDeviceSettings will expect
            DXUTD3D10DeviceSettings d3d10In;
            ZeroMemory( &d3d10In, sizeof(DXUTD3D10DeviceSettings) );
            DXUTConvertDeviceSettings9to10( &deviceSettings.d3d9, &d3d10In );
            deviceSettings.d3d10 = d3d10In;
#endif
        }
    }

    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
//on PS3 we know the device and skip the validation stage/result
#if !defined(PS3)
    if (FAILED(hr) && !CD3D9Renderer::CV_d3d9_null_ref_device) // the call will fail if no valid devices were found
    {
        DXUTDisplayErrorMessage( hr );
        return DXUT_ERR( L"DXUTFindValidDeviceSettings", hr );
    }
#endif

    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreated the scene
    hr = DXUTChangeDevice( &deviceSettings, NULL,
#ifdef DIRECT3D10
      NULL,
#endif
      false, false );
    if( FAILED(hr) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Tells the framework to change to a device created from the passed in device settings
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() 
// or DXUTSetD3D*Device() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateDeviceFromSettings( DXUTDeviceSettings* pDeviceSettings, bool bPreserveInput, bool bClipWindowToSingleAdapter )
{
    HRESULT hr;

    GetDXUTState().SetDeviceCreateCalled( true );

    if( !bPreserveInput )
    {
        // If not preserving the input, then find the closest valid to it
        DXUTMatchOptions matchOptions;
        matchOptions.eAPIVersion         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterOrdinal     = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDeviceType         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eWindowed           = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterFormat      = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eVertexProcessing   = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eResolution         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferFormat   = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eMultiSample        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eStencilFormat      = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentFlags       = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eRefreshRate        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;

        hr = DXUTFindValidDeviceSettings( pDeviceSettings, pDeviceSettings, &matchOptions );
        if( FAILED(hr) ) // the call will fail if no valid devices were found
        {
            DXUTDisplayErrorMessage( hr );
            return DXUT_ERR( L"DXUTFindValidD3D9DeviceSettings", hr );
        }
    }

    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreate the scene
    hr = DXUTChangeDevice( pDeviceSettings, NULL,
#ifdef DIRECT3D10
      NULL,
#endif
      false, bClipWindowToSingleAdapter );
    if( FAILED(hr) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// All device changes are sent to this function.  It looks at the current 
// device (if any) and the new device and determines the best course of action.  It 
// also remembers and restores the window state if toggling between windowed and fullscreen
// as well as sets the proper window and system state for switching to the new device.
//--------------------------------------------------------------------------------------

HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings, 
                          IDirect3DDevice9* pd3d9DeviceFromApp,
#ifdef DIRECT3D10
                          ID3D10Device* pd3d10DeviceFromApp, 
#endif
                          bool bForceRecreate, bool bClipWindowToSingleAdapter )
{
	  HRESULT hr;
    DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

    if( !pNewDeviceSettings )
        return S_FALSE;

#if defined (DIRECT3D9) || defined (OPENGL)
    hr = DXUTDelayLoadD3D9();
#endif
#ifdef DIRECT3D10
    hr = DXUTDelayLoadDXGI();
#endif
    if( FAILED(hr) )
        return DXUTERR_NODIRECT3D;

    // Make a copy of the pNewDeviceSettings on the heap
    DXUTDeviceSettings* pNewDeviceSettingsOnHeap = new DXUTDeviceSettings;
    if( pNewDeviceSettingsOnHeap == NULL )
        return E_OUTOFMEMORY;
    memcpy( pNewDeviceSettingsOnHeap, pNewDeviceSettings, sizeof(DXUTDeviceSettings) );
    pNewDeviceSettings = pNewDeviceSettingsOnHeap;

    // If the ModifyDeviceSettings callback is non-NULL, then call it to let the app 
    // change the settings or reject the device change by returning false.
    LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallbackModifyDeviceSettings = GetDXUTState().GetModifyDeviceSettingsFunc();
    if( pCallbackModifyDeviceSettings && pd3d9DeviceFromApp == NULL 
#ifdef DIRECT3D10
      && pd3d10DeviceFromApp == NULL
#endif
      )
    {
        bool bContinue = pCallbackModifyDeviceSettings( pNewDeviceSettings, GetDXUTState().GetModifyDeviceSettingsFuncUserContext() );
        if( !bContinue )
        {
            // The app rejected the device change by returning false, so just use the current device if there is one.
            if( pOldDeviceSettings == NULL )
                DXUTDisplayErrorMessage( DXUTERR_NOCOMPATIBLEDEVICES );
            SAFE_DELETE( pNewDeviceSettings );
            return E_ABORT;
        }
        if( GetDXUTState().GetD3D9() == NULL
#ifdef DIRECT3D10
          && GetDXUTState().GetDXGIFactory() == NULL
#endif
          ) // if DXUTShutdown() was called in the modify callback, just return
        {
            SAFE_DELETE( pNewDeviceSettings );
            return S_FALSE;
        }
    }

    GetDXUTState().SetCurrentDeviceSettings( pNewDeviceSettings );

    DXUTPause( true, true );

    // Only apply the cmd line overrides if this is the first device created
    // and DXUTSetD3D*Device() isn't used
    if( NULL == pd3d9DeviceFromApp &&
#ifdef DIRECT3D10
      NULL == pd3d10DeviceFromApp &&
#endif
      NULL == pOldDeviceSettings )
    {
        // Updates the device settings struct based on the cmd line args.
        // Warning: if the device doesn't support these new settings then CreateDevice9() will fail.
        DXUTUpdateDeviceSettingsWithOverrides( pNewDeviceSettings );
    }

    // Take note if the backbuffer width & height are 0 now as they will change after pd3dDevice->Reset()
    bool bKeepCurrentWindowSize = false;
    if( DXUTGetBackBufferWidthFromDS( pNewDeviceSettings ) == 0 && DXUTGetBackBufferHeightFromDS( pNewDeviceSettings ) == 0 )
        bKeepCurrentWindowSize = true;

    //////////////////////////
    // Before reset
    /////////////////////////

    // If we are using D3D9, adjust window style when switching from windowed to fullscreen and
    // vice versa.  Note that this is not necessary in D3D10 because DXGI handles this.  If both
    // DXUT and DXGI handle this, incorrect behavior would result.
#if defined (DIRECT3D9)
    if( DXUTIsCurrentDeviceD3D9() )
    {
        if( DXUTGetIsWindowedFromDS(pNewDeviceSettings) )
        {
            // Going to windowed mode
            if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS( pOldDeviceSettings ) )
            {
                // Going from fullscreen -> windowed
                GetDXUTState().SetFullScreenBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(pOldDeviceSettings) );
                GetDXUTState().SetFullScreenBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(pOldDeviceSettings) );

                // Restore windowed mode style
                SetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE, GetDXUTState().GetWindowedStyleAtModeChange() );
            }

            // If different device windows are used for windowed mode and fullscreen mode,
            // hide the fullscreen window so that it doesn't obscure the screen.
            if( DXUTGetHWNDDeviceFullScreen() != DXUTGetHWNDDeviceWindowed() )
                ShowWindow( DXUTGetHWNDDeviceFullScreen(), SW_HIDE );
        }
        else 
        {
            // Going to fullscreen mode
            if( pOldDeviceSettings == NULL || (pOldDeviceSettings && DXUTGetIsWindowedFromDS(pOldDeviceSettings) ) )
            {
                // Transistioning to full screen mode from a standard window so 
                // save current window position/size/style now in case the user toggles to windowed mode later 
                WINDOWPLACEMENT* pwp = GetDXUTState().GetWindowedPlacement();
                ZeroMemory( pwp, sizeof(WINDOWPLACEMENT) );
                pwp->length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement( DXUTGetHWNDDeviceWindowed(), pwp );
                DWORD dwStyle = GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE );
                dwStyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
                GetDXUTState().SetWindowedStyleAtModeChange( dwStyle );
                if( pOldDeviceSettings )
                {
                    GetDXUTState().SetWindowBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(pOldDeviceSettings) );
                    GetDXUTState().SetWindowBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(pOldDeviceSettings) );
                }
            }

            // Hide the window to avoid animation of blank windows
            //ShowWindow( DXUTGetHWNDDeviceFullScreen(), SW_HIDE );

            // Set FS window style
            SetWindowLong( DXUTGetHWNDDeviceFullScreen(), GWL_STYLE, WS_POPUP|WS_SYSMENU );

            // If using the same window for windowed and fullscreen mode, save and remove menu 
            if( DXUTGetHWNDDeviceFullScreen() == DXUTGetHWNDDeviceWindowed() )
            {
                //HMENU hMenu = GetMenu( DXUTGetHWNDDeviceFullScreen() );
                //GetDXUTState().SetMenu( hMenu );
                //SetMenu( DXUTGetHWNDDeviceFullScreen(), NULL );
            }
          
            WINDOWPLACEMENT wpFullscreen;
            ZeroMemory( &wpFullscreen, sizeof(WINDOWPLACEMENT) );
            wpFullscreen.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement( DXUTGetHWNDDeviceFullScreen(), &wpFullscreen );
            if( (wpFullscreen.flags & WPF_RESTORETOMAXIMIZED) != 0 )
            {
                // Restore the window to normal if the window was maximized then minimized.  This causes the 
                // WPF_RESTORETOMAXIMIZED flag to be set which will cause SW_RESTORE to restore the 
                // window from minimized to maxmized which isn't what we want
                wpFullscreen.flags &= ~WPF_RESTORETOMAXIMIZED;
                wpFullscreen.showCmd = SW_RESTORE;
                SetWindowPlacement( DXUTGetHWNDDeviceFullScreen(), &wpFullscreen );
            }
        }
    }
#elif defined (DIRECT3D10) && !defined(PS3)
    if( DXUTGetIsWindowedFromDS(pNewDeviceSettings) )
    {
      // Going to windowed mode
      if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS( pOldDeviceSettings ) )
      {
        // Going from fullscreen -> windowed
        GetDXUTState().SetFullScreenBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(pOldDeviceSettings) );
        GetDXUTState().SetFullScreenBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(pOldDeviceSettings) );
      }
    }
    else 
    {
      // Going to fullscreen mode
      if( pOldDeviceSettings == NULL || (pOldDeviceSettings && DXUTGetIsWindowedFromDS(pOldDeviceSettings) ) )
      {
        // Transistioning to full screen mode from a standard window so 
        if( pOldDeviceSettings )
        {
          GetDXUTState().SetWindowBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(pOldDeviceSettings) );
          GetDXUTState().SetWindowBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(pOldDeviceSettings) );
        }
      }
    }

#endif
        
    // If API version, AdapterOrdinal and DeviceType are the same, we can just do a Reset().
    // If they've changed, we need to do a complete device tear down/rebuild.
    // Also only allow a reset if pd3dDevice is the same as the current device 
    if( !bForceRecreate && 
        DXUTCanDeviceBeReset( pOldDeviceSettings, pNewDeviceSettings, pd3d9DeviceFromApp
#ifdef DIRECT3D10
        , pd3d10DeviceFromApp
#endif
        ) )
    {
        // Reset the Direct3D device and call the app's device callbacks
#if (defined (DIRECT3D9) || defined (OPENGL)) && !defined(PS3)
        if( DXUTIsD3D9( pOldDeviceSettings ) )
            hr = DXUTReset3DEnvironment9();
#elif defined (DIRECT3D10)
            hr = DXUTReset3DEnvironment10();
#endif
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
        if( FAILED(hr) )
        {
            if( D3DERR_DEVICELOST == hr )
            {
                // The device is lost, just mark it as so and continue on with 
                // capturing the state and resizing the window/etc.
                GetDXUTState().SetDeviceLost( true );
            }
            else if( DXUTERR_RESETTINGDEVICEOBJECTS == hr || 
                     DXUTERR_MEDIANOTFOUND == hr )
            {
                // Something bad happened in the app callbacks
                SAFE_DELETE( pOldDeviceSettings );
                DXUTDisplayErrorMessage( hr );
                DXUTShutdown();
                return hr;
            }
            else // DXUTERR_RESETTINGDEVICE
            {
                // Reset failed and the device wasn't lost and it wasn't the apps fault, 
                // so recreate the device to try to recover
                GetDXUTState().SetCurrentDeviceSettings( pOldDeviceSettings );
                if( DXUTChangeDevice( pNewDeviceSettings, pd3d9DeviceFromApp,
#ifdef DIRECT3D10
                  pd3d10DeviceFromApp,
#endif
                  true, bClipWindowToSingleAdapter) != S_OK)
                {
                    // If that fails, then shutdown
                    SAFE_DELETE( pOldDeviceSettings );
                    DXUTShutdown();
                    return DXUTERR_CREATINGDEVICE;
                }
                else
                {
                    SAFE_DELETE( pOldDeviceSettings );
                    return S_OK;
                }
            }
        }
#endif
    }
    else
    {
        // Cleanup if not first device created
        if( pOldDeviceSettings ) 
            DXUTCleanup3DEnvironment( false );

        // Create the D3D device and call the app's device callbacks
#if defined (DIRECT3D9) || defined (OPENGL)
        if( DXUTIsD3D9( pNewDeviceSettings ) )
            hr = DXUTCreate3DEnvironment9( pd3d9DeviceFromApp );
#endif
#ifdef DIRECT3D10
        else
            hr = DXUTCreate3DEnvironment10( pd3d10DeviceFromApp );
#endif
        if( FAILED(hr) )
        {
            SAFE_DELETE( pOldDeviceSettings );
            DXUTCleanup3DEnvironment( true );
            DXUTDisplayErrorMessage( hr );
            DXUTPause( false, false );
            return hr;
        }
    }

#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    HMONITOR hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );
    GetDXUTState().SetAdapterMonitor( hAdapterMonitor );
#endif

    if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS(pOldDeviceSettings) && DXUTGetIsWindowedFromDS(pNewDeviceSettings) )
    {
        // Going from fullscreen -> windowed

        // Restore the show state, and positions/size of the window to what it was
        // It is important to adjust the window size 
        // after resetting the device rather than beforehand to ensure 
        // that the monitor resolution is correct and does not limit the size of the new window.
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
        WINDOWPLACEMENT* pwp = GetDXUTState().GetWindowedPlacement();
        SetWindowPlacement( DXUTGetHWNDDeviceWindowed(), pwp );

        // Also restore the z-order of window to previous state
        HWND hWndInsertAfter = HWND_NOTOPMOST;
        SetWindowPos( DXUTGetHWNDDeviceWindowed(), hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSIZE );
#endif
    }

    // Check to see if the window needs to be resized.  
    // Handle cases where the window is minimized and maxmimized as well.
    bool bNeedToResize = false;
#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    // Check to see if the window needs to be resized.  
    // Handle cases where the window is minimized and maxmimized as well.
    if( DXUTGetIsWindowedFromDS(pNewDeviceSettings) && // only resize if in windowed mode
      !bKeepCurrentWindowSize )                      // only resize if pp.BackbufferWidth/Height were not 0
    {
      UINT nClientWidth;
      UINT nClientHeight;    
      if( IsIconic(DXUTGetHWNDDeviceWindowed()) )
      {
        // Window is currently minimized. To tell if it needs to resize, 
        // get the client rect of window when its restored the 
        // hard way using GetWindowPlacement()
        WINDOWPLACEMENT wp;
        ZeroMemory( &wp, sizeof(WINDOWPLACEMENT) );
        wp.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement( DXUTGetHWNDDeviceWindowed(), &wp );

        if( (wp.flags & WPF_RESTORETOMAXIMIZED) != 0 && wp.showCmd == SW_SHOWMINIMIZED )
        {
          // WPF_RESTORETOMAXIMIZED means that when the window is restored it will
          // be maximized.  So maximize the window temporarily to get the client rect 
          // when the window is maximized.  GetSystemMetrics( SM_CXMAXIMIZED ) will give this 
          // information if the window is on the primary but this will work on multimon.
          ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_RESTORE );
          RECT rcClient;
          GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
          nClientWidth  = (UINT)(rcClient.right - rcClient.left);
          nClientHeight = (UINT)(rcClient.bottom - rcClient.top);
          ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_MINIMIZE );
        }
        else
        {
          // Use wp.rcNormalPosition to get the client rect, but wp.rcNormalPosition 
          // includes the window frame so subtract it
          RECT rcFrame = {0};
          AdjustWindowRect( &rcFrame, GetDXUTState().GetWindowedStyleAtModeChange(), FALSE );
          LONG nFrameWidth = rcFrame.right - rcFrame.left;
          LONG nFrameHeight = rcFrame.bottom - rcFrame.top;
          nClientWidth  = (UINT)(wp.rcNormalPosition.right - wp.rcNormalPosition.left - nFrameWidth);
          nClientHeight = (UINT)(wp.rcNormalPosition.bottom - wp.rcNormalPosition.top - nFrameHeight);
        }
      }
      else
      {
        // Window is restored or maximized so just get its client rect
        RECT rcClient;
        GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
        nClientWidth  = (UINT)(rcClient.right - rcClient.left);
        nClientHeight = (UINT)(rcClient.bottom - rcClient.top);
      }

      // Now that we know the client rect, compare it against the back buffer size
      // to see if the client rect is already the right size
      if( nClientWidth  != DXUTGetBackBufferWidthFromDS(pNewDeviceSettings) ||
        nClientHeight != DXUTGetBackBufferHeightFromDS(pNewDeviceSettings) )
      {
        bNeedToResize = true;
      }       

      if( bClipWindowToSingleAdapter && !IsIconic(DXUTGetHWNDDeviceWindowed()) )
      {
        // Get the rect of the monitor attached to the adapter
        MONITORINFO miAdapter;
        miAdapter.cbSize = sizeof(MONITORINFO);
        HMONITOR hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );
        DXUTGetMonitorInfo( hAdapterMonitor, &miAdapter );
        HMONITOR hWindowMonitor = DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY );

        // Get the rect of the window
        RECT rcWindow;
        GetWindowRect( DXUTGetHWNDDeviceWindowed(), &rcWindow );

        // Check if the window rect is fully inside the adapter's vitural screen rect
        if( (rcWindow.left   < miAdapter.rcWork.left  ||
          rcWindow.right  > miAdapter.rcWork.right ||
          rcWindow.top    < miAdapter.rcWork.top   ||
          rcWindow.bottom > miAdapter.rcWork.bottom) )
        {
          if( hWindowMonitor == hAdapterMonitor && IsZoomed(DXUTGetHWNDDeviceWindowed()) )
          {
            // If the window is maximized and on the same monitor as the adapter, then 
            // no need to clip to single adapter as the window is already clipped 
            // even though the rcWindow rect is outside of the miAdapter.rcWork
          }
          else
          {
            bNeedToResize = true;
          }
        }
      }
    }
#endif

    // Dont resize/show window in CryENGINE
#ifdef DIRECT3D10
    // Only resize window if needed 
#ifndef PS3
    if (bNeedToResize && !gcpRendD3D->m_bEditor) 
    {
        // Need to resize, so if window is maximized or minimized then restore the window
        if( bClipWindowToSingleAdapter )
        {
            // Get the rect of the monitor attached to the adapter
            MONITORINFO miAdapter;
            miAdapter.cbSize = sizeof(MONITORINFO);
            HMONITOR hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );
            DXUTGetMonitorInfo( hAdapterMonitor, &miAdapter );

            // Get the rect of the monitor attached to the window
            MONITORINFO miWindow;
            miWindow.cbSize = sizeof(MONITORINFO);
            DXUTGetMonitorInfo( DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY ), &miWindow );

            // Do something reasonable if the BackBuffer size is greater than the monitor size
            int nAdapterMonitorWidth = miAdapter.rcWork.right - miAdapter.rcWork.left;
            int nAdapterMonitorHeight = miAdapter.rcWork.bottom - miAdapter.rcWork.top;

            int nClientWidth = DXUTGetBackBufferWidthFromDS( pNewDeviceSettings );
            int nClientHeight = DXUTGetBackBufferHeightFromDS( pNewDeviceSettings );

            // Get the rect of the window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceWindowed(), &rcWindow );

            // Make a window rect with a client rect that is the same size as the backbuffer
            RECT rcResizedWindow;
            rcResizedWindow.left = 0;
            rcResizedWindow.right = nClientWidth;
            rcResizedWindow.top = 0;
            rcResizedWindow.bottom = nClientHeight;
            AdjustWindowRect( &rcResizedWindow, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ), false );

            int nWindowWidth = rcResizedWindow.right - rcResizedWindow.left;
            int nWindowHeight = rcResizedWindow.bottom - rcResizedWindow.top;

            if( nWindowWidth > nAdapterMonitorWidth )
                nWindowWidth = nAdapterMonitorWidth;
            if( nWindowHeight > nAdapterMonitorHeight )
                nWindowHeight = nAdapterMonitorHeight;

            if( rcResizedWindow.left < miAdapter.rcWork.left ||
                rcResizedWindow.top < miAdapter.rcWork.top ||
                rcResizedWindow.right > miAdapter.rcWork.right ||
                rcResizedWindow.bottom > miAdapter.rcWork.bottom )
            {
                int nWindowOffsetX = (nAdapterMonitorWidth - nWindowWidth) / 2;
                int nWindowOffsetY = (nAdapterMonitorHeight - nWindowHeight) / 2;

                rcResizedWindow.left = miAdapter.rcWork.left + nWindowOffsetX;
                rcResizedWindow.top = miAdapter.rcWork.top + nWindowOffsetY;
                rcResizedWindow.right = miAdapter.rcWork.left + nWindowOffsetX + nWindowWidth;
                rcResizedWindow.bottom = miAdapter.rcWork.top + nWindowOffsetY + nWindowHeight;
            }

            // Resize the window.  It is important to adjust the window size 
            // after resetting the device rather than beforehand to ensure 
            // that the monitor resolution is correct and does not limit the size of the new window.
            SetWindowPos( DXUTGetHWNDDeviceWindowed(), 0, rcResizedWindow.left, rcResizedWindow.top, nWindowWidth, nWindowHeight, SWP_NOZORDER );
        }        
        else
        {      
            // Make a window rect with a client rect that is the same size as the backbuffer
            RECT rcWindow = {0};
            rcWindow.right = (long)( DXUTGetBackBufferWidthFromDS(pNewDeviceSettings) );
            rcWindow.bottom = (long)( DXUTGetBackBufferHeightFromDS(pNewDeviceSettings) );
            AdjustWindowRect( &rcWindow, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ), false );

            // Resize the window.  It is important to adjust the window size 
            // after resetting the device rather than beforehand to ensure 
            // that the monitor resolution is correct and does not limit the size of the new window.
            int cx = (int)(rcWindow.right - rcWindow.left);
            int cy = (int)(rcWindow.bottom - rcWindow.top);
            SetWindowPos( DXUTGetHWNDDeviceWindowed(), 0, 0, 0, cx, cy, SWP_NOZORDER|SWP_NOMOVE );
        }

        // Its possible that the new window size is not what we asked for.  
        // No window can be sized larger than the desktop, so see see if the Windows OS resized the 
        // window to something smaller to fit on the desktop.  Also if WM_GETMINMAXINFO
        // will put a limit on the smallest/largest window size.
        RECT rcClient;
        GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
        UINT nClientWidth  = (UINT)(rcClient.right - rcClient.left);
        UINT nClientHeight = (UINT)(rcClient.bottom - rcClient.top);
        if( nClientWidth  != DXUTGetBackBufferWidthFromDS(pNewDeviceSettings)  ||
            nClientHeight != DXUTGetBackBufferHeightFromDS(pNewDeviceSettings) )
        {
            // If its different, then resize the backbuffer again.  This time create a backbuffer that matches the 
            // client rect of the current window w/o resizing the window.
            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
            if( DXUTIsD3D9( &deviceSettings ) )
              deviceSettings.d3d9.pp.BackBufferWidth = 0;
#ifdef DIRECT3D10
            else
              deviceSettings.d3d10.sd.BufferDesc.Width = 0; 
#endif
            if( DXUTIsD3D9( &deviceSettings ) )
              deviceSettings.d3d9.pp.BackBufferHeight = 0;
#ifdef DIRECT3D10
            else
              deviceSettings.d3d10.sd.BufferDesc.Height = 0;
#endif
            hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, bClipWindowToSingleAdapter );
            if( FAILED( hr ) )
            {
                SAFE_DELETE( pOldDeviceSettings );
                DXUTCleanup3DEnvironment( true ); 
                DXUTPause( false, false );
                //GetDXUTState().SetIgnoreSizeChange( false );
                return hr;
            }
        }
    }
#endif
#endif

    // Make the window visible
#if !defined(PS3)
    if( !IsWindowVisible( DXUTGetHWND() ) && !gRenDev->m_bEditor )
        ShowWindow( DXUTGetHWND(), SW_SHOW );

#if defined (DIRECT3D9) || defined (DIRECT3D10)
    // Ensure that the display doesn't power down when fullscreen but does when windowed
    if( !DXUTIsWindowed() )
        SetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_CONTINUOUS ); 
    else
        SetThreadExecutionState( ES_CONTINUOUS );   
#endif
#endif

    SAFE_DELETE( pOldDeviceSettings );
    DXUTPause( false, false );
    GetDXUTState().SetDeviceCreated( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Check if the new device is close enough to the old device to simply reset or 
// resize the back buffer
//--------------------------------------------------------------------------------------
bool DXUTCanDeviceBeReset( DXUTDeviceSettings *pOldDeviceSettings, DXUTDeviceSettings *pNewDeviceSettings, 
                           IDirect3DDevice9 *pd3d9DeviceFromApp
#ifdef DIRECT3D10
                           , ID3D10Device *pd3d10DeviceFromApp
#endif
                           )
{
    if( pOldDeviceSettings == NULL || pOldDeviceSettings->ver != pNewDeviceSettings->ver ) 
        return false;

    if( pNewDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        if( DXUTGetD3D9Device() &&
            (pd3d9DeviceFromApp == NULL || pd3d9DeviceFromApp == DXUTGetD3D9Device()) &&
            (pOldDeviceSettings->d3d9.AdapterOrdinal == pNewDeviceSettings->d3d9.AdapterOrdinal) &&
            (pOldDeviceSettings->d3d9.DeviceType     == pNewDeviceSettings->d3d9.DeviceType) &&
            (pOldDeviceSettings->d3d9.BehaviorFlags  == pNewDeviceSettings->d3d9.BehaviorFlags) )
            return true;

        return false;
    }
    else
    {
#ifdef DIRECT3D10
        if( DXUTGetD3D10Device() &&
            (pd3d10DeviceFromApp == NULL || pd3d10DeviceFromApp == DXUTGetD3D10Device()) &&
            (pOldDeviceSettings->d3d10.AdapterOrdinal == pNewDeviceSettings->d3d10.AdapterOrdinal) &&
            (pOldDeviceSettings->d3d10.DriverType     == pNewDeviceSettings->d3d10.DriverType) && 
            (pOldDeviceSettings->d3d10.CreateFlags    == pNewDeviceSettings->d3d10.CreateFlags) )
            return true;
#endif        
        return false;
    }
}

#ifdef DIRECT3D10
//--------------------------------------------------------------------------------------
// Creates a DXGI factory object if one has not already been created  
//--------------------------------------------------------------------------------------
HRESULT DXUTDelayLoadDXGI()
{
    IDXGIFactory* pDXGIFactory = GetDXUTState().GetDXGIFactory();
    if( pDXGIFactory == NULL )
    {
        DXUT_Dynamic_CreateDXGIFactory( __uuidof( IDXGIFactory ), (LPVOID*)&pDXGIFactory );
        GetDXUTState().SetDXGIFactory( pDXGIFactory );
        if( pDXGIFactory == NULL )
        {
            // If still NULL, then DXGI is not availible
            GetDXUTState().SetD3D10Available( false );
            return DXUTERR_NODIRECT3D;
        }

        GetDXUTState().SetD3D10Available( true );
    }

    return S_OK;
}
#endif

//--------------------------------------------------------------------------------------
// Creates a Direct3D object if one has not already been created  
//--------------------------------------------------------------------------------------
#if defined (DIRECT3D9) || defined (OPENGL)
HRESULT DXUTDelayLoadD3D9()
{
    IDirect3D9* pD3D = GetDXUTState().GetD3D9();
    if( pD3D == NULL )
    {
        // This may fail if Direct3D 9 isn't installed
        // This may also fail if the Direct3D headers are somehow out of sync with the installed Direct3D DLLs
        pD3D = DXUT_Dynamic_Direct3DCreate9( D3D_SDK_VERSION );
        if( pD3D == NULL )
        {
            // If still NULL, then D3D9 is not availible
            return DXUTERR_NODIRECT3D;
        }

        GetDXUTState().SetD3D9(pD3D);
        gcpRendD3D->SetD3D(pD3D);
    }
    return S_OK;
}
#endif


//--------------------------------------------------------------------------------------
// Updates the device settings struct based on the cmd line args.  
//--------------------------------------------------------------------------------------
void DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings )
{
    // TODO: ?
    if( DXUTIsD3D9( pDeviceSettings ) ) 
    {
        if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
            pDeviceSettings->d3d9.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();

        if( GetDXUTState().GetOverrideFullScreen() )
            pDeviceSettings->d3d9.pp.Windowed = false;
        if( GetDXUTState().GetOverrideWindowed() )
            pDeviceSettings->d3d9.pp.Windowed = true;

        if( GetDXUTState().GetOverrideForceREF() )
            pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
        else if( GetDXUTState().GetOverrideForceHAL() )
            pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_HAL;
				if( GetDXUTState().GetOverrideForceNullREF() )
					pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_NULLREF;

        if( GetDXUTState().GetOverrideWidth() != 0 )
            pDeviceSettings->d3d9.pp.BackBufferWidth = GetDXUTState().GetOverrideWidth();
        if( GetDXUTState().GetOverrideHeight() != 0 )
            pDeviceSettings->d3d9.pp.BackBufferHeight = GetDXUTState().GetOverrideHeight();

        if( GetDXUTState().GetOverrideForcePureHWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_PUREDEVICE;
        }
        else if( GetDXUTState().GetOverrideForceHWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
        else if( GetDXUTState().GetOverrideForceSWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
    }
    else
    {
#ifdef DIRECT3D10
        if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
            pDeviceSettings->d3d10.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();

        if( GetDXUTState().GetOverrideFullScreen() )
            pDeviceSettings->d3d10.sd.Windowed = false;
        if( GetDXUTState().GetOverrideWindowed() )
            pDeviceSettings->d3d10.sd.Windowed = true;

        if( GetDXUTState().GetOverrideForceREF() )
            pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
        else if( GetDXUTState().GetOverrideForceHAL() )
            pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;

        if( GetDXUTState().GetOverrideWidth() != 0 )
            pDeviceSettings->d3d10.sd.BufferDesc.Width = GetDXUTState().GetOverrideWidth();
        if( GetDXUTState().GetOverrideHeight() != 0 )
            pDeviceSettings->d3d10.sd.BufferDesc.Height = GetDXUTState().GetOverrideHeight();
#endif
    }
}


//--------------------------------------------------------------------------------------
// Returns true if app has registered any D3D9 callbacks or 
// used the DXUTSetD3DVersionSupport API and passed true for bAppCanUseD3D9
//--------------------------------------------------------------------------------------
bool WINAPI DXUTDoesAppSupportD3D9()
{
  return GetDXUTState().GetIsD3D9DeviceAcceptableFunc() || 
          GetDXUTState().GetD3D9DeviceCreatedFunc()      ||
          GetDXUTState().GetD3D9DeviceResetFunc()        || 
          GetDXUTState().GetD3D9DeviceLostFunc()         ||
          GetDXUTState().GetD3D9DeviceDestroyedFunc()    || 
          GetDXUTState().GetD3D9FrameRenderFunc();
}


//--------------------------------------------------------------------------------------
// Returns true if app has registered any D3D10 callbacks or 
// used the DXUTSetD3DVersionSupport API and passed true for bAppCanUseD3D10
//--------------------------------------------------------------------------------------
bool WINAPI DXUTDoesAppSupportD3D10()
{
#ifdef DIRECT3D10
  return GetDXUTState().GetIsD3D10DeviceAcceptableFunc() || 
          GetDXUTState().GetD3D10DeviceCreatedFunc()      ||
          GetDXUTState().GetD3D10SwapChainResizedFunc()   || 
          GetDXUTState().GetD3D10FrameRenderFunc()        ||
          GetDXUTState().GetD3D10SwapChainReleasingFunc() || 
          GetDXUTState().GetD3D10DeviceDestroyedFunc();  
#else
      return false;
#endif
}


//======================================================================================
//======================================================================================
// Direct3D 9 section
//======================================================================================
//======================================================================================

#if defined (DIRECT3D9) || defined (OPENGL)

//--------------------------------------------------------------------------------------
// Passes a previously created Direct3D9 device for use by the framework.  
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() or 
// DXUTCreateDeviceFromSettings() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetD3D9Device( IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;

    if( pd3dDevice == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D9Device", E_INVALIDARG );

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D9Device", E_FAIL );

    GetDXUTState().SetDeviceCreateCalled( true );

    DXUTDeviceSettings DeviceSettings;
    ZeroMemory( &DeviceSettings, sizeof(DXUTDeviceSettings) );
    DeviceSettings.ver = DXUT_D3D9_DEVICE;

    // Get the present params from the swap chain
    IDirect3DSurface9* pBackBuffer = NULL;
    hr = pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
    if( SUCCEEDED(hr) )
    {
        IDirect3DSwapChain9* pSwapChain = NULL;
        hr = pBackBuffer->GetContainer( IID_IDirect3DSwapChain9, (void**) &pSwapChain );
        if( SUCCEEDED(hr) )
        {
            pSwapChain->GetPresentParameters( &DeviceSettings.d3d9.pp );
            SAFE_RELEASE( pSwapChain );
        }
        SAFE_RELEASE( pBackBuffer );
    }

    D3DDEVICE_CREATION_PARAMETERS d3dCreationParams;
    pd3dDevice->GetCreationParameters( &d3dCreationParams );

    // Fill out the rest of the device settings struct
    DeviceSettings.d3d9.AdapterOrdinal = d3dCreationParams.AdapterOrdinal;
    DeviceSettings.d3d9.DeviceType     = d3dCreationParams.DeviceType;
    DXUTFindD3D9AdapterFormat( DeviceSettings.d3d9.AdapterOrdinal, DeviceSettings.d3d9.DeviceType, 
                               DeviceSettings.d3d9.pp.BackBufferFormat, DeviceSettings.d3d9.pp.Windowed, 
                               &DeviceSettings.d3d9.AdapterFormat );
    DeviceSettings.d3d9.BehaviorFlags  = d3dCreationParams.BehaviorFlags;

    // Change to the Direct3D device passed in
    hr = DXUTChangeDevice( &DeviceSettings, pd3dDevice,
      false, false );
    if( FAILED(hr) ) 
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Creates the 3D environment
//--------------------------------------------------------------------------------------
HRESULT DXUTCreate3DEnvironment9( IDirect3DDevice9* pd3dDeviceFromApp )
{
    HRESULT hr = S_OK;
    IDirect3DDevice9* pd3dDevice = NULL;
    DXUTDeviceSettings* pNewDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

    // Only create a Direct3D device if one hasn't been supplied by the app
    if( pd3dDeviceFromApp == NULL )
    {
				HWND hFocusWnd = DXUTGetHWNDFocus();
        // Try to create the device with the chosen settings
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        hr = pD3D->CreateDevice( pNewDeviceSettings->d3d9.AdapterOrdinal, pNewDeviceSettings->d3d9.DeviceType, 
                                 DXUTGetHWNDFocus(), pNewDeviceSettings->d3d9.BehaviorFlags,
                                 &pNewDeviceSettings->d3d9.pp, &pd3dDevice );

        if( hr == D3DERR_DEVICELOST ) 
        {
            GetDXUTState().SetDeviceLost( true );
            return S_OK;
        }
        else if( FAILED(hr) )
        {
            DXUT_ERR( L"CreateDevice", hr );
            return DXUTERR_CREATINGDEVICE;
        }
    }
    else
    {
        pd3dDeviceFromApp->AddRef();
        pd3dDevice = pd3dDeviceFromApp;
    }

    GetDXUTState().SetD3D9Device( pd3dDevice );


    // If switching to REF, set the exit code to 10.  If switching to HAL and exit code was 10, then set it back to 0.
    if( pNewDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF && GetDXUTState().GetExitCode() == 0 )
        GetDXUTState().SetExitCode(10);
    else if( pNewDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_HAL && GetDXUTState().GetExitCode() == 10 )
        GetDXUTState().SetExitCode(0);


    // Update back buffer desc before calling app's device callbacks
    DXUTUpdateBackBufferDesc();

    // Update GetDXUTState()'s copy of D3D caps 
    D3DCAPS9* pd3dCaps = GetDXUTState().GetCaps();
    DXUTGetD3D9Device()->GetDeviceCaps( pd3dCaps );
    gcpRendD3D->m_pd3dCaps = pd3dCaps;

    // Update the device stats text
    CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
    CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo( pNewDeviceSettings->d3d9.AdapterOrdinal );
    DXUTUpdateD3D9DeviceStats( pNewDeviceSettings->d3d9.DeviceType, 
                               pNewDeviceSettings->d3d9.BehaviorFlags, 
                               &pAdapterInfo->AdapterIdentifier );

    // Call the app's device created callback if non-NULL
    const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D9DEVICECREATED pCallbackDeviceCreated = GetDXUTState().GetD3D9DeviceCreatedFunc();
    hr = S_OK;
    if( pCallbackDeviceCreated != NULL )
        hr = pCallbackDeviceCreated( DXUTGetD3D9Device(), pBackBufferSurfaceDesc, GetDXUTState().GetD3D9DeviceCreatedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D9Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;

    if( FAILED(hr) )  
    {
        DXUT_ERR( L"DeviceCreated callback", hr );        
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_CREATINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsCreated( true );

    // Call the app's device reset callback if non-NULL
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D9DEVICERESET pCallbackDeviceReset = GetDXUTState().GetD3D9DeviceResetFunc();
    hr = S_OK;
    if( pCallbackDeviceReset != NULL )
        hr = pCallbackDeviceReset( DXUTGetD3D9Device(), pBackBufferSurfaceDesc, GetDXUTState().GetD3D9DeviceResetFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D9Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED(hr) )
    {
        DXUT_ERR( L"DeviceReset callback", hr );
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_RESETTINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resets the 3D environment by:
//      - Calls the device lost callback 
//      - Resets the device
//      - Stores the back buffer description
//      - Sets up the full screen Direct3D cursor if requested
//      - Calls the device reset callback 
//--------------------------------------------------------------------------------------
HRESULT DXUTReset3DEnvironment9()
{
    HRESULT hr;

    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
    assert( pd3dDevice != NULL );

    // Call the app's device lost callback
    if( GetDXUTState().GetDeviceObjectsReset() == true )
    {
        GetDXUTState().SetInsideDeviceCallback( true );
        LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
        if( pCallbackDeviceLost != NULL )
            pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
        GetDXUTState().SetDeviceObjectsReset( false );
        GetDXUTState().SetInsideDeviceCallback( false );
    }

    // Reset the device
    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    if (!gcpRendD3D->m_bEditor)
      pDeviceSettings->d3d9.pp.Flags &= ~D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    else
      pDeviceSettings->d3d9.pp.Flags |= D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;

    hr = pd3dDevice->Reset( &pDeviceSettings->d3d9.pp );
    if( FAILED(hr) )  
    {
        if( hr == D3DERR_DEVICELOST )
            return D3DERR_DEVICELOST; // Reset could legitimately fail if the device is lost
        else
            return DXUT_ERR( L"Reset", DXUTERR_RESETTINGDEVICE );
    }

    // Update back buffer desc before calling app's device callbacks
    DXUTUpdateBackBufferDesc();

    // Call the app's OnDeviceReset callback
    GetDXUTState().SetInsideDeviceCallback( true );
    const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    LPDXUTCALLBACKD3D9DEVICERESET pCallbackDeviceReset = GetDXUTState().GetD3D9DeviceResetFunc();
    hr = S_OK;
    if( pCallbackDeviceReset != NULL )
        hr = pCallbackDeviceReset( pd3dDevice, pBackBufferSurfaceDesc, GetDXUTState().GetD3D9DeviceResetFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( FAILED(hr) )
    {
        // If callback failed, cleanup
        DXUT_ERR( L"DeviceResetCallback", hr );
        if( hr != DXUTERR_MEDIANOTFOUND )
            hr = DXUTERR_RESETTINGDEVICEOBJECTS;

        GetDXUTState().SetInsideDeviceCallback( true );
        LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
        if( pCallbackDeviceLost != NULL )
            pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
        GetDXUTState().SetInsideDeviceCallback( false );
        return hr;
    }

    // Success
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Cleans up the 3D environment by:
//      - Calls the device lost callback 
//      - Calls the device destroyed callback 
//      - Releases the D3D device
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment9( bool bReleaseSettings )
{
    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
    if( pd3dDevice != NULL )
    {
        GetDXUTState().SetInsideDeviceCallback( true );

        // Call the app's device lost callback
        if( GetDXUTState().GetDeviceObjectsReset() == true )
        {
            LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
            if( pCallbackDeviceLost != NULL )
                pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
            GetDXUTState().SetDeviceObjectsReset( false );
        }

        // Call the app's device destroyed callback
        if( GetDXUTState().GetDeviceObjectsCreated() == true )
        {
            LPDXUTCALLBACKD3D9DEVICEDESTROYED pCallbackDeviceDestroyed = GetDXUTState().GetD3D9DeviceDestroyedFunc();
            if( pCallbackDeviceDestroyed != NULL )
                pCallbackDeviceDestroyed( GetDXUTState().GetD3D9DeviceDestroyedFuncUserContext() );
            GetDXUTState().SetDeviceObjectsCreated( false );
        }

        GetDXUTState().SetInsideDeviceCallback( false );

        // Release the D3D device and in debug configs, displays a message box if there 
        // are unrelease objects.
        if( pd3dDevice )
        {
            UINT references = pd3dDevice->Release();
            if( references > 0 )
            {
                DXUTDisplayErrorMessage( DXUTERR_NONZEROREFCOUNT );
                DXUT_ERR( L"DXUTCleanup3DEnvironment", DXUTERR_NONZEROREFCOUNT );
            }
        }
        GetDXUTState().SetD3D9Device( NULL );

        if( bReleaseSettings )
        {
            DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
            SAFE_DELETE(pOldDeviceSettings);  
            GetDXUTState().SetCurrentDeviceSettings( NULL );
        }

        D3DSURFACE_DESC* pBackBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc9();
        ZeroMemory( pBackBufferSurfaceDesc, sizeof(D3DSURFACE_DESC) );

        D3DCAPS9* pd3dCaps = GetDXUTState().GetCaps();
        ZeroMemory( pd3dCaps, sizeof(D3DCAPS9) );

        GetDXUTState().SetDeviceCreated( false );
    }
}

//--------------------------------------------------------------------------------------
// Internal helper function to return the adapter format from the first device settings 
// combo that matches the passed adapter ordinal, device type, backbuffer format, and windowed.  
//--------------------------------------------------------------------------------------
HRESULT DXUTFindD3D9AdapterFormat( UINT AdapterOrdinal, D3DDEVTYPE DeviceType, D3DFORMAT BackBufferFormat, 
                                   BOOL Windowed, D3DFORMAT* pAdapterFormat )
{
    CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration( false );
    CD3D9EnumDeviceInfo* pDeviceInfo = pd3dEnum->GetDeviceInfo( AdapterOrdinal, DeviceType );
    if( pDeviceInfo )
    {
        for( int iDeviceCombo=0; iDeviceCombo<pDeviceInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pDeviceInfo->deviceSettingsComboList.GetAt(iDeviceCombo);
            if( pDeviceSettingsCombo->BackBufferFormat == BackBufferFormat &&
                pDeviceSettingsCombo->Windowed == Windowed )
            {
                // Return the adapter format from the first match
                *pAdapterFormat = pDeviceSettingsCombo->AdapterFormat;
                return S_OK;
            }
        }
    }

    *pAdapterFormat = BackBufferFormat;
    return E_FAIL;
}
#endif

//======================================================================================
//======================================================================================
// Direct3D 10 section
//======================================================================================
//======================================================================================

#ifdef DIRECT3D10
//--------------------------------------------------------------------------------------
// Passes a previously created Direct3D 10 device for use by the framework.  
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() or 
// DXUTCreateDeviceFromSettings() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetD3D10Device(ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain, bool bUseDepth )
{
  HRESULT hr;

  if( pd3dDevice == NULL )
    return DXUT_ERR_MSGBOX( L"DXUTSetD3D10Device", E_INVALIDARG );

  // Not allowed to call this from inside the device callbacks
  if( GetDXUTState().GetInsideDeviceCallback() )
    return DXUT_ERR_MSGBOX( L"DXUTSetD3D10Device", E_FAIL );

  GetDXUTState().SetDeviceCreateCalled( true );

  DXUTDeviceSettings DeviceSettings;
  ZeroMemory( &DeviceSettings, sizeof(DXUTDeviceSettings) );

#ifndef PS3
  // Get adapter info
  IDXGIDevice *pDXGIDevice = NULL;
  if( SUCCEEDED( hr = pd3dDevice->QueryInterface( __uuidof( *pDXGIDevice ), reinterpret_cast< void** >( &pDXGIDevice ) ) ) )
  {
    IDXGIAdapter *pAdapter = NULL;
    hr = pDXGIDevice->GetAdapter( &pAdapter );
    if( SUCCEEDED( hr ) )
    {
      DXGI_ADAPTER_DESC id;
      pAdapter->GetDesc( &id );

      // Find the ordinal by inspecting the enum list.
      DeviceSettings.d3d10.AdapterOrdinal = 0;  // Default
      CD3D10Enumeration *pd3dEnum = DXUTGetD3D10Enumeration();
      CGrowableArray<CD3D10EnumAdapterInfo*> *pAdapterInfoList = pd3dEnum->GetAdapterInfoList();
      for( int i = 0; i < pAdapterInfoList->GetSize(); ++i )
      {
        CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterInfoList->GetAt(i);
        if( !wcscmp( pAdapterInfo->AdapterDesc.Description, id.Description ) )
        {
          DeviceSettings.d3d10.AdapterOrdinal = i;
          break;
        }
      }
      SAFE_RELEASE( pAdapter );
    }
    SAFE_RELEASE( pDXGIDevice );
  }
#endif

  if( FAILED( hr ) )
    return hr;

  // Get the swap chain description
  DXGI_SWAP_CHAIN_DESC sd;
  pSwapChain->GetDesc( &sd );
  DeviceSettings.ver = DXUT_D3D10_DEVICE;
  DeviceSettings.d3d10.sd = sd;

  // Fill out the rest of the device settings struct
  DeviceSettings.d3d10.CreateFlags = 0;
  DeviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
  DeviceSettings.d3d10.Output = 0;
  DeviceSettings.d3d10.PresentFlags = 0;
  DeviceSettings.d3d10.SyncInterval = 0;
  // DeviceSettings.d3d10.AutoCreateDepthStencil = true; // TODO: verify

  // Change to the Direct3D device passed in
  hr = DXUTChangeDevice( &DeviceSettings, NULL, pd3dDevice, false, false );
  if( FAILED(hr) ) 
    return hr;

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Sets the viewport, creates a render target view, and depth scencil texture and view.
//--------------------------------------------------------------------------------------
HRESULT DXUTSetupD3D10Views( ID3D10Device* pd3dDevice, DXUTDeviceSettings* pDeviceSettings )
{
  HRESULT hr = S_OK;
  IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
  ID3D10DepthStencilView* pDSV = NULL;
  ID3D10RenderTargetView* pRTV = NULL;

  // Get the back buffer and desc
  ID3D10Texture2D *pBackBuffer;
#if defined PS3
  hr = pSwapChain->GetBuffer( 0, __uuidof(ID3D10Texture2D), (LPVOID*)&pBackBuffer );
#else
  hr = pSwapChain->GetBuffer( 0, __uuidof(*pBackBuffer), (LPVOID*)&pBackBuffer );
#endif
  if( FAILED(hr) )
    return hr;
  D3D10_TEXTURE2D_DESC backBufferSurfaceDesc;
  pBackBuffer->GetDesc( &backBufferSurfaceDesc );

  // Setup the viewport to match the backbuffer
  D3D10_VIEWPORT vp;
  vp.Width = backBufferSurfaceDesc.Width;
  vp.Height = backBufferSurfaceDesc.Height;
  vp.MinDepth = 0;
  vp.MaxDepth = 1;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  pd3dDevice->RSSetViewports( 1, &vp );

  // Create the render target view
  hr = pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &pRTV );
  SAFE_RELEASE( pBackBuffer );
  if( FAILED(hr) )
    return hr;
  GetDXUTState().SetD3D10RenderTargetView( pRTV );

  if( pDeviceSettings->d3d10.AutoCreateDepthStencil )
  {
    // Create depth stencil texture
    ID3D10Texture2D* pDepthStencil = NULL;
    D3D10_TEXTURE2D_DESC descDepth;
    descDepth.Width = backBufferSurfaceDesc.Width;
    descDepth.Height = backBufferSurfaceDesc.Height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = pDeviceSettings->d3d10.AutoDepthStencilFormat;
    descDepth.SampleDesc.Count = pDeviceSettings->d3d10.sd.SampleDesc.Count;
    descDepth.SampleDesc.Quality = pDeviceSettings->d3d10.sd.SampleDesc.Quality;
    descDepth.Usage = D3D10_USAGE_DEFAULT;
    descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = pd3dDevice->CreateTexture2D( &descDepth, NULL, &pDepthStencil );
    if( FAILED(hr) )
      return hr;
    GetDXUTState().SetD3D10DepthStencil( pDepthStencil );

    // Create the depth stencil view
    D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
    descDSV.Format = descDepth.Format;
    if( descDepth.SampleDesc.Count > 1 )
      descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    else
      descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = pd3dDevice->CreateDepthStencilView( pDepthStencil, &descDSV, &pDSV );
    if( FAILED(hr) )
      return hr;
    GetDXUTState().SetD3D10DepthStencilView( pDSV );
  }

  // Set the render targets
  pDSV = GetDXUTState().GetD3D10DepthStencilView();
  pd3dDevice->OMSetRenderTargets( 1, &pRTV, pDSV );

  return hr;
}

//--------------------------------------------------------------------------------------
// Creates the 3D environment
//--------------------------------------------------------------------------------------
HRESULT DXUTCreate3DEnvironment10( ID3D10Device* pd3dDeviceFromApp )
{
  HRESULT hr = S_OK;

  ID3D10Device* pd3dDevice = NULL;
  IDXGISwapChain *pSwapChain = NULL;
  DXUTDeviceSettings* pNewDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

  IDXGIFactory* pDXGIFactory = DXUTGetDXGIFactory();
  hr = pDXGIFactory->MakeWindowAssociation(DXUTGetHWNDDeviceFullScreen(), 0);

  // Only create a Direct3D device if one hasn't been supplied by the app
  if( pd3dDeviceFromApp == NULL )
  {
    // Try to create the device with the chosen settings
    IDXGIAdapter* pAdapter = NULL;

    hr = S_OK;
    //if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE )
    hr = pDXGIFactory->EnumAdapters( pNewDeviceSettings->d3d10.AdapterOrdinal, &pAdapter );
    if( SUCCEEDED(hr) )
    {
      hr = DXUT_Dynamic_D3D10CreateDevice( pAdapter, pNewDeviceSettings->d3d10.DriverType, 
        pNewDeviceSettings->d3d10.CreateFlags,
        NULL, D3D10_SDK_VERSION, &pd3dDevice );
      if( SUCCEEDED(hr) )
      {
        if( pNewDeviceSettings->d3d10.DriverType != D3D10_DRIVER_TYPE_HARDWARE )
        {
          IDXGIDevice* pDXGIDev = NULL;
          hr = pd3dDevice->QueryInterface( __uuidof( IDXGIDevice ), (LPVOID*)&pDXGIDev );
          if( SUCCEEDED(hr) && pDXGIDev )
          {
            pDXGIDev->GetAdapter( &pAdapter );
          }
          SAFE_RELEASE( pDXGIDev );
        }

        GetDXUTState().SetD3D10Adapter( pAdapter );
      }
    }

    if( FAILED(hr) )
    {
      DXUT_ERR( L"D3D10CreateDevice", hr );
      return DXUTERR_CREATINGDEVICE;
    }

    // Enumerate its outputs.
    UINT OutputCount, iOutput;
    for( OutputCount = 0; ; ++OutputCount )
    {
      IDXGIOutput* pOutput;
      if( FAILED( pAdapter->EnumOutputs( OutputCount, &pOutput ) ) )
        break;
      SAFE_RELEASE( pOutput );
    }
    IDXGIOutput** ppOutputArray = new IDXGIOutput*[OutputCount];
    if( !ppOutputArray )
      return E_OUTOFMEMORY;
    for( iOutput = 0; iOutput < OutputCount; ++iOutput )
      pAdapter->EnumOutputs( iOutput, ppOutputArray + iOutput );
    GetDXUTState().SetD3D10OutputArray( ppOutputArray );
    GetDXUTState().SetD3D10OutputArraySize( OutputCount );

    if (!pNewDeviceSettings->d3d10.sd.Windowed)
      pNewDeviceSettings->d3d10.sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Create the swapchain
    hr = pDXGIFactory->CreateSwapChain( pd3dDevice, &pNewDeviceSettings->d3d10.sd, &pSwapChain );
    //const char *szStr = gcpRendD3D->D3DError(hr);
    if( FAILED(hr) )
    {
      DXUT_ERR( L"CreateSwapChain", hr );
      return DXUTERR_CREATINGDEVICE;
    }
    else
    if (hr == DXGI_STATUS_OCCLUDED)
    {
      gcpRendD3D->m_bPendingSetWindow = true;
    }
  }
  else
  {
    pd3dDeviceFromApp->AddRef();
    pd3dDevice = pd3dDeviceFromApp;
  }
  GetDXUTState().SetD3D10Device( pd3dDevice );
  GetDXUTState().SetD3D10SwapChain( pSwapChain );

  // If switching to REF, set the exit code to 10.  If switching to HAL and exit code was 10, then set it back to 0.
  if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE && GetDXUTState().GetExitCode() == 0 )
    GetDXUTState().SetExitCode(10);
  else if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE && GetDXUTState().GetExitCode() == 10 )
    GetDXUTState().SetExitCode(0);

  // Update back buffer desc before calling app's device callbacks
  DXUTUpdateBackBufferDesc();

  // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
  //DXUTSetupCursor();

  // Update the device stats text
  CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
  CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo( pNewDeviceSettings->d3d10.AdapterOrdinal );
  DXUTUpdateD3D10DeviceStats( pNewDeviceSettings->d3d10.DriverType, &pAdapterInfo->AdapterDesc );

  // Call the app's device created callback if non-NULL
  const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
  GetDXUTState().SetInsideDeviceCallback( true );
  LPDXUTCALLBACKD3D10DEVICECREATED pCallbackDeviceCreated = GetDXUTState().GetD3D10DeviceCreatedFunc();
  hr = S_OK;
  if( pCallbackDeviceCreated != NULL )
    hr = pCallbackDeviceCreated( DXUTGetD3D10Device(), pBackBufferSurfaceDesc, GetDXUTState().GetD3D10DeviceCreatedFuncUserContext() );
  GetDXUTState().SetInsideDeviceCallback( false );
  if( DXUTGetD3D10Device() == NULL ) // Handle DXUTShutdown from inside callback
    return E_FAIL;
  if( FAILED(hr) )
  {
    DXUT_ERR( L"DeviceCreated callback", hr );
    return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_CREATINGDEVICEOBJECTS;
  }
  GetDXUTState().SetDeviceObjectsCreated( true );

  // Setup the render target view and viewport
  hr = DXUTSetupD3D10Views( pd3dDevice, pNewDeviceSettings );
  if( FAILED(hr) )
  {
    DXUT_ERR( L"DXUTSetupD3D10Views", hr );
    return DXUTERR_CREATINGDEVICEOBJECTS;
  }

  // Create performance counters
  //DXUTCreateD3D10Counters( pd3dDevice );

  // Call the app's swap chain reset callback if non-NULL
  GetDXUTState().SetInsideDeviceCallback( true );
  LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallbackSwapChainResized = GetDXUTState().GetD3D10SwapChainResizedFunc();
  hr = S_OK;
  if( pCallbackSwapChainResized != NULL )
    hr = pCallbackSwapChainResized( DXUTGetD3D10Device(), pSwapChain, pBackBufferSurfaceDesc, GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
  GetDXUTState().SetInsideDeviceCallback( false );
  if( DXUTGetD3D10Device() == NULL ) // Handle DXUTShutdown from inside callback
    return E_FAIL;
  if( FAILED(hr) )
  {
    DXUT_ERR( L"DeviceReset callback", hr );
    return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_RESETTINGDEVICEOBJECTS;
  }
  GetDXUTState().SetDeviceObjectsReset( true );

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Resets the 3D environment by:
//      - Calls the device lost callback 
//      - Resets the device
//      - Stores the back buffer description
//      - Sets up the full screen Direct3D cursor if requested
//      - Calls the device reset callback 
//--------------------------------------------------------------------------------------
HRESULT DXUTReset3DEnvironment10()
{
    HRESULT hr;

    GetDXUTState().SetDeviceObjectsReset( false );
    DXUTPause( true, true );

    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();

    DXGI_SWAP_CHAIN_DESC SCDesc;
    pSwapChain->GetDesc( &SCDesc );

    // Resize backbuffer and target of the swapchain in case they have changed.
    // For windowed mode, use the client rect as the desired size. Unlike D3D9,
    // we can't use 0 for width or height.  Therefore, fill in the values from
    // the window size. For fullscreen mode, the width and height should have
    // already been filled with the desktop resolution, so don't change it.
    if( pDeviceSettings->d3d10.sd.Windowed && SCDesc.Windowed && pDeviceSettings->d3d10.sd.BufferDesc.Width==0)
    {
      RECT rcWnd;
      GetClientRect( DXUTGetHWND(), &rcWnd );
      pDeviceSettings->d3d10.sd.BufferDesc.Width = rcWnd.right - rcWnd.left;
      pDeviceSettings->d3d10.sd.BufferDesc.Height = rcWnd.bottom - rcWnd.top;
    }

    if( SCDesc.Windowed != pDeviceSettings->d3d10.sd.Windowed )
    {
      // Set the fullscreen state
      if( pDeviceSettings->d3d10.sd.Windowed )
      {
        GetDXUTState().SetDoNotResize(true);
        V_RETURN( pSwapChain->SetFullscreenState( FALSE, NULL ) );
        GetDXUTState().SetDoNotResize(false);
        V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d10.sd.BufferDesc ) );
      }
      else
      {
        // Set fullscreen state by setting the display mode to fullscreen, then changing the resolution
        // to the desired value.

        // SetFullscreenState causes a WM_SIZE message to be sent to the window.  The WM_SIZE message calls
        // DXUTCheckForDXGIBufferChange which normally stores the new height and width in 
        // pDeviceSettings->d3d10.sd.BufferDesc.  SetDoNotStoreBufferSize tells DXUTCheckForDXGIBufferChange
        // not to store the height and width so that we have the correct values when calling ResizeTarget.
        GetDXUTState().SetDoNotStoreBufferSize(true);
        V_RETURN( pSwapChain->SetFullscreenState( TRUE, NULL ) );
        GetDXUTState().SetDoNotStoreBufferSize(false);
        V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d10.sd.BufferDesc ) );
      }
    }
    else
    {
      V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d10.sd.BufferDesc ) );
    }

    // Success
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Cleans up the 3D environment by:
//      - Calls the device lost callback 
//      - Calls the device destroyed callback 
//      - Releases the D3D device
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment10( bool bReleaseSettings )
{
    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    if( pd3dDevice != NULL )
    {
        // Call the app's SwapChain lost callback
        GetDXUTState().SetInsideDeviceCallback( true );
        if( GetDXUTState().GetDeviceObjectsReset() )
        {
            LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallbackSwapChainReleasing = GetDXUTState().GetD3D10SwapChainReleasingFunc();
            if( pCallbackSwapChainReleasing != NULL )
                pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainReleasingFuncUserContext() );
            GetDXUTState().SetDeviceObjectsReset( false );
        }

        // Release our old depth stencil texture and view 
        ID3D10Texture2D* pDS = GetDXUTState().GetD3D10DepthStencil();
        SAFE_RELEASE( pDS );
        GetDXUTState().SetD3D10DepthStencil( NULL );
        ID3D10DepthStencilView* pDSV = GetDXUTState().GetD3D10DepthStencilView();
        SAFE_RELEASE( pDSV );
        GetDXUTState().SetD3D10DepthStencilView( NULL );

        // Cleanup the render target view
        ID3D10RenderTargetView* pRTV = GetDXUTState().GetD3D10RenderTargetView();
        SAFE_RELEASE( pRTV );
        GetDXUTState().SetD3D10RenderTargetView( NULL );

        // Call the app's device destroyed callback
        if( GetDXUTState().GetDeviceObjectsCreated() )
        {
            LPDXUTCALLBACKD3D10DEVICEDESTROYED pCallbackDeviceDestroyed = GetDXUTState().GetD3D10DeviceDestroyedFunc();
            if( pCallbackDeviceDestroyed != NULL )
                pCallbackDeviceDestroyed( GetDXUTState().GetD3D10DeviceDestroyedFuncUserContext() );
            GetDXUTState().SetDeviceObjectsCreated( false );
        }

        GetDXUTState().SetInsideDeviceCallback( false );

        // Release the swap chain
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
        SAFE_RELEASE( pSwapChain );
        GetDXUTState().SetD3D10SwapChain( NULL );

        // Release the outputs.
        IDXGIOutput **ppOutputArray = GetDXUTState().GetD3D10OutputArray();
        UINT OutputCount = GetDXUTState().GetD3D10OutputArraySize();
        for( UINT o = 0; o < OutputCount; ++o )
            SAFE_RELEASE( ppOutputArray[o] );
        delete[] ppOutputArray;
        GetDXUTState().SetD3D10OutputArray( NULL );
        GetDXUTState().SetD3D10OutputArraySize( 0 );

        // Release the D3D adapter.
        IDXGIAdapter *pAdapter = GetDXUTState().GetD3D10Adapter();
        SAFE_RELEASE( pAdapter );
        GetDXUTState().SetD3D10Adapter( NULL );

        // Release the D3D device and in debug configs, displays a message box if there 
        // are unrelease objects.
        if( pd3dDevice )
        {
            UINT references = pd3dDevice->Release();
            if( references > 0 )
            {
                DXUTDisplayErrorMessage( DXUTERR_NONZEROREFCOUNT );
                DXUT_ERR( L"DXUTCleanup3DEnvironment", DXUTERR_NONZEROREFCOUNT );
            }
        }
        GetDXUTState().SetD3D10Device( NULL );

        if( bReleaseSettings )
        {
            DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
            SAFE_DELETE(pOldDeviceSettings);
            GetDXUTState().SetCurrentDeviceSettings( NULL );
        }

        DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc10();
        ZeroMemory( pBackBufferSurfaceDesc, sizeof(DXGI_SURFACE_DESC) );

        GetDXUTState().SetDeviceCreated( false );
    }
}
#endif


//--------------------------------------------------------------------------------------
// Pauses time or rendering.  Keeps a ref count so pausing can be layered
//--------------------------------------------------------------------------------------
void WINAPI DXUTPause( bool bPauseTime, bool bPauseRendering )
{
    int nPauseTimeCount = GetDXUTState().GetPauseTimeCount();
    if( bPauseTime ) nPauseTimeCount++; else nPauseTimeCount--; 
    if( nPauseTimeCount < 0 ) nPauseTimeCount = 0;
    GetDXUTState().SetPauseTimeCount( nPauseTimeCount );

    int nPauseRenderingCount = GetDXUTState().GetPauseRenderingCount();
    if( bPauseRendering ) nPauseRenderingCount++; else nPauseRenderingCount--; 
    if( nPauseRenderingCount < 0 ) nPauseRenderingCount = 0;
    GetDXUTState().SetPauseRenderingCount( nPauseRenderingCount );

    if( nPauseTimeCount > 0 )
    {
        // Stop the scene from animating
        DXUTGetGlobalTimer()->Stop();
    }
    else
    {
        // Restart the timer
        DXUTGetGlobalTimer()->Start();
    }

    GetDXUTState().SetRenderingPaused( nPauseRenderingCount > 0 );
    GetDXUTState().SetTimePaused( nPauseTimeCount > 0 );
}


//--------------------------------------------------------------------------------------
// Starts a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetTimer( LPDXUTCALLBACKTIMER pCallbackTimer, float fTimeoutInSecs, UINT* pnIDEvent, void* pCallbackUserContext ) 
{ 
    if( pCallbackTimer == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetTimer", E_INVALIDARG ); 

    HRESULT hr;
    DXUT_TIMER DXUTTimer;
    DXUTTimer.pCallbackTimer = pCallbackTimer;
    DXUTTimer.pCallbackUserContext = pCallbackUserContext;
    DXUTTimer.fTimeoutInSecs = fTimeoutInSecs;
    DXUTTimer.fCountdown = fTimeoutInSecs;
    DXUTTimer.bEnabled = true;

    CGrowableArray<DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
    {
        pTimerList = new CGrowableArray<DXUT_TIMER>;
        if( pTimerList == NULL )
            return E_OUTOFMEMORY; 
        GetDXUTState().SetTimerList( pTimerList );
    }

    if( FAILED( hr = pTimerList->Add( DXUTTimer ) ) )
        return hr;

    if( pnIDEvent )
        *pnIDEvent = pTimerList->GetSize();

    return S_OK; 
}


//--------------------------------------------------------------------------------------
// Stops a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTKillTimer( UINT nIDEvent ) 
{ 
    CGrowableArray<DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
        return S_FALSE;

    if( nIDEvent < (UINT)pTimerList->GetSize() )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt(nIDEvent);
        DXUTTimer.bEnabled = false;
        pTimerList->SetAt(nIDEvent, DXUTTimer);
    }
    else
    {
        return DXUT_ERR_MSGBOX( L"DXUTKillTimer", E_INVALIDARG );
    }

    return S_OK; 
}


//--------------------------------------------------------------------------------------
// Internal helper function to handle calling the user defined timer callbacks
//--------------------------------------------------------------------------------------
void DXUTHandleTimers()
{
    float fElapsedTime = DXUTGetElapsedTime();

    CGrowableArray<DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
        return;

    // Walk through the list of timer callbacks
    for( int i=0; i<pTimerList->GetSize(); i++ )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt(i);
        if( DXUTTimer.bEnabled )
        {
            DXUTTimer.fCountdown -= fElapsedTime;

            // Call the callback if count down expired
            if( DXUTTimer.fCountdown < 0 )
            {
                DXUTTimer.pCallbackTimer( i, DXUTTimer.pCallbackUserContext );
                DXUTTimer.fCountdown = DXUTTimer.fTimeoutInSecs;
            }
            pTimerList->SetAt(i, DXUTTimer);
        }
    }
}


//--------------------------------------------------------------------------------------
// Display an custom error msg box 
//--------------------------------------------------------------------------------------
void DXUTDisplayErrorMessage( HRESULT hr )
{
    CHAR strBuffer[512];

#if (defined (DIRECT3D9) || defined (DIRECT3D10)) && !defined(PS3)
    int nExitCode;
    bool bFound = true; 
    switch( hr )
    {
        case DXUTERR_NODIRECT3D:
        {
            nExitCode = 2;
            if( DXUTDoesAppSupportD3D10() && !DXUTDoesAppSupportD3D9() )
                StringCchCopyA( strBuffer, 512, "Could not initialize Direct3D 10. This application requires a Direct3D 10 class\ndevice (hardware or reference rasterizer) running on Windows Vista (or later)." );
            else
                StringCchCopyA( strBuffer, 512, "Could not initialize Direct3D 9. Check that the latest version of DirectX is correctly installed on your system.  Also make sure that this program was compiled with header files that match the installed DirectX DLLs." );
            break;
        }
        case DXUTERR_NOCOMPATIBLEDEVICES:    
            nExitCode = 3; 
            if( GetSystemMetrics(0x1000) != 0 ) // SM_REMOTESESSION
                StringCchCopyA( strBuffer, 512, "Direct3D does not work over a remote session." ); 
            else
                StringCchCopyA( strBuffer, 512, "Could not find any compatible Direct3D devices." ); 
            break;
        case DXUTERR_MEDIANOTFOUND:          nExitCode = 4; StringCchCopyA( strBuffer, 512, "Could not find required media." ); break;
        case DXUTERR_NONZEROREFCOUNT:        nExitCode = 5; StringCchCopyA( strBuffer, 512, "The Direct3D device has a non-zero reference count, meaning some objects were not released." ); break;
        case DXUTERR_CREATINGDEVICE:         nExitCode = 6; StringCchCopyA( strBuffer, 512, "Failed creating the Direct3D device." ); break;
        case DXUTERR_RESETTINGDEVICE:        nExitCode = 7; StringCchCopyA( strBuffer, 512, "Failed resetting the Direct3D device." ); break;
        case DXUTERR_CREATINGDEVICEOBJECTS:  nExitCode = 8; StringCchCopyA( strBuffer, 512, "An error occurred in the device create callback function." ); break;
        case DXUTERR_RESETTINGDEVICEOBJECTS: nExitCode = 9; StringCchCopyA( strBuffer, 512, "An error occurred in the device reset callback function." ); break;
        // nExitCode 10 means the app exited using a REF device 
        case DXUTERR_DEVICEREMOVED:          nExitCode = 11; StringCchCopyA( strBuffer, 512, "The Direct3D device was removed."  ); break;
        default: bFound = false; nExitCode = 1; break; // nExitCode 1 means the API was incorrectly called

    }   

    GetDXUTState().SetExitCode(nExitCode);

    if( bFound )
    {
      iLog->Log("D3D Error: (%s)", strBuffer);
    }
#endif
}


//--------------------------------------------------------------------------------------
// Toggle between full screen and windowed
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleFullScreen()
{
    HRESULT hr;

    // Get the current device settings and flip the windowed state then
    // find the closest valid device settings with this change
    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Togggle windowed/fullscreen bit
    if( DXUTIsD3D9( &deviceSettings ) )
        deviceSettings.d3d9.pp.Windowed = !deviceSettings.d3d9.pp.Windowed;
#ifdef DIRECT3D10
    else
        deviceSettings.d3d10.sd.Windowed = !deviceSettings.d3d10.sd.Windowed;
#endif

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion         = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterOrdinal     = DXUTMT_PRESERVE_INPUT;
    matchOptions.eDeviceType         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eWindowed           = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterFormat      = DXUTMT_IGNORE_INPUT;
    matchOptions.eVertexProcessing   = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferFormat   = DXUTMT_IGNORE_INPUT;
    matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat      = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentFlags       = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eRefreshRate        = DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;

    // Go back to previous state

    BOOL bIsWindowed = DXUTGetIsWindowedFromDS( &deviceSettings );
    UINT nWidth  = ( bIsWindowed ) ? GetDXUTState().GetWindowBackBufferWidthAtModeChange() : GetDXUTState().GetFullScreenBackBufferWidthAtModeChange();
    UINT nHeight = ( bIsWindowed ) ? GetDXUTState().GetWindowBackBufferHeightAtModeChange() : GetDXUTState().GetFullScreenBackBufferHeightAtModeChange();

    if( nWidth > 0 && nHeight > 0 )
    {
        matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
        if( DXUTIsD3D9( &deviceSettings ) )
        {
            deviceSettings.d3d9.pp.BackBufferWidth = nWidth;
            deviceSettings.d3d9.pp.BackBufferHeight = nHeight;
        }
#ifdef DIRECT3D10
        else
        {
            deviceSettings.d3d10.sd.BufferDesc.Width = nWidth;
            deviceSettings.d3d10.sd.BufferDesc.Height = nHeight;
        }
#endif
    }
    else
    {
        // No previous data, so just switch to defaults
        matchOptions.eResolution = DXUTMT_IGNORE_INPUT;
    }
    
    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( SUCCEEDED(hr) ) 
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL,
#ifdef DIRECT3D10
          NULL,
#endif
          false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED(hr) && (hr != E_ABORT) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL,
#ifdef DIRECT3D10
              NULL,
#endif
              false, false );
            if( FAILED(hr2) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Toggle between HAL and REF
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleREF()
{
    HRESULT hr;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Toggle between REF & HAL
    if( DXUTIsCurrentDeviceD3D9() )
    {
        if( deviceSettings.d3d9.DeviceType == D3DDEVTYPE_HAL )
            deviceSettings.d3d9.DeviceType = D3DDEVTYPE_REF;
        else if( deviceSettings.d3d9.DeviceType == D3DDEVTYPE_REF )
            deviceSettings.d3d9.DeviceType = D3DDEVTYPE_HAL;
    }
#ifdef DIRECT3D10
    else
    {
        if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE )
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
        else if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE )
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
    }
#endif

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion         = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterOrdinal     = DXUTMT_PRESERVE_INPUT;
    matchOptions.eDeviceType         = DXUTMT_PRESERVE_INPUT;
    matchOptions.eWindowed           = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eAdapterFormat      = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eVertexProcessing   = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eResolution         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferFormat   = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat      = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentFlags       = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eRefreshRate        = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;
    
    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( SUCCEEDED(hr) ) 
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL,
#ifdef DIRECT3D10
          NULL,
#endif
          false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && (hr != E_ABORT) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL,
#ifdef DIRECT3D10
              NULL,
#endif
              false, false );
            if( FAILED(hr2) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}

#if defined (DIRECT3D9) || defined (DIRECT3D10)
//--------------------------------------------------------------------------------------
// Checks to see if the HWND changed monitors, and if it did it creates a device 
// from the monitor's adapter and recreates the scene.
//--------------------------------------------------------------------------------------
void DXUTCheckForWindowChangingMonitors()
{
	//no monitor change on PS3
#if !defined(PS3)
    // Skip this check for various reasons
    if( !GetDXUTState().GetAutoChangeAdapter() || !GetDXUTState().GetDeviceCreated() || !DXUTIsWindowed()  )
        return;

    HRESULT hr;
    HMONITOR hWindowMonitor = DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY );
    HMONITOR hAdapterMonitor = GetDXUTState().GetAdapterMonitor();
    if( hWindowMonitor != hAdapterMonitor )
    {
        UINT newOrdinal;
        if( SUCCEEDED( DXUTGetAdapterOrdinalFromMonitor( hWindowMonitor, &newOrdinal ) ) )
        {
            // Find the closest valid device settings with the new ordinal
            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
            if( DXUTIsD3D9( &deviceSettings ) ) 
                deviceSettings.d3d9.AdapterOrdinal = newOrdinal; 
#ifdef DIRECT3D10
            else
                deviceSettings.d3d10.AdapterOrdinal = newOrdinal; 
#endif

            DXUTMatchOptions matchOptions;
            matchOptions.eAPIVersion         = DXUTMT_PRESERVE_INPUT;
            matchOptions.eAdapterOrdinal     = DXUTMT_PRESERVE_INPUT;
            matchOptions.eDeviceType         = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eWindowed           = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eAdapterFormat      = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eVertexProcessing   = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eResolution         = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eBackBufferFormat   = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eMultiSample        = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eStencilFormat      = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.ePresentFlags       = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eRefreshRate        = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;

            hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
            if( SUCCEEDED(hr) ) 
            {
                // Create a Direct3D device using the new device settings.  
                    // If there is an existing device, then it will either reset or recreate the scene.
                hr = DXUTChangeDevice( &deviceSettings, NULL,
#ifdef DIRECT3D10
                  NULL,
#endif
                  false, false );

                // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback
                if( hr == E_ABORT )
                {
                    // so nothing changed and keep from attempting to switch adapters next time
                    GetDXUTState().SetAutoChangeAdapter( false );
                }
                else if( FAILED(hr) )
                {
                    DXUTShutdown();
                    DXUTPause( false, false );
                    return;
                }
            }
        }
    }    
#endif
}

#ifdef DIRECT3D10
//--------------------------------------------------------------------------------------
// Checks if DXGI buffers need to change
//--------------------------------------------------------------------------------------
void DXUTCheckForDXGIBufferChange(int nWidth, int nHeight)
{
  if( GetDXUTState().GetDoNotResize() )
    return;

  HRESULT hr = S_OK;
  ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
  RECT rcCurrentClient;
  GetClientRect( DXUTGetHWND(), &rcCurrentClient );
  if (nWidth > 0)
  {
    rcCurrentClient.right = nWidth;
    rcCurrentClient.bottom = nHeight;
  }

  DXUTDeviceSettings* pDevSettings = GetDXUTState().GetCurrentDeviceSettings();

  IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
  IDXGIFactory* pDXGIFactory = DXUTGetDXGIFactory();
  hr = pDXGIFactory->MakeWindowAssociation((HWND)gcpRendD3D->GetHWND(), 0 );

  // Determine if we're fullscreen
  BOOL bFullScreen;
  pSwapChain->GetFullscreenState( &bFullScreen, NULL );
  pDevSettings->d3d10.sd.Windowed = !bFullScreen;

  // Call releasing
  GetDXUTState().SetInsideDeviceCallback( true );
  LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallbackSwapChainReleasing = GetDXUTState().GetD3D10SwapChainReleasingFunc();
  if( pCallbackSwapChainReleasing != NULL )
    pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
  GetDXUTState().SetInsideDeviceCallback( false );

  // Release our old depth stencil texture and view 
  ID3D10Texture2D* pDS = GetDXUTState().GetD3D10DepthStencil();
  SAFE_RELEASE( pDS );
  GetDXUTState().SetD3D10DepthStencil( NULL );
  ID3D10DepthStencilView* pDSV = GetDXUTState().GetD3D10DepthStencilView();
  SAFE_RELEASE( pDSV );
  GetDXUTState().SetD3D10DepthStencilView( NULL );

  // Release our old render target view
  ID3D10RenderTargetView* pRTV = GetDXUTState().GetD3D10RenderTargetView();
  SAFE_RELEASE( pRTV );
  GetDXUTState().SetD3D10RenderTargetView( NULL );

  // Alternate between 0 and DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH when resizing buffers.
  // When in windowed mode, we want 0 since this allows the app to change to the desktop
  // resolution from windowed mode during alt+enter.  However, in fullscreen mode, we want
  // the ability to change display modes from the Device Settings dialog.  Therefore, we
  // want to set the DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH flag.
  UINT Flags = 0;
  if( bFullScreen )
    Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  gcpRendD3D->m_CVFullScreen->Set(bFullScreen);
  gcpRendD3D->AdjustWindowForChange();

  // ResizeBuffers
  V( pSwapChain->ResizeBuffers( pDevSettings->d3d10.sd.BufferCount,
    nWidth,
    nHeight,
    pDevSettings->d3d10.sd.BufferDesc.Format,
    Flags ) );
  V( pSwapChain->ResizeTarget( &pDevSettings->d3d10.sd.BufferDesc ) );

  if( !GetDXUTState().GetDoNotStoreBufferSize() )
  {
    pDevSettings->d3d10.sd.BufferDesc.Width = nWidth;
    pDevSettings->d3d10.sd.BufferDesc.Height = nHeight;
  }

  // Save off backbuffer desc
  DXUTUpdateBackBufferDesc();

  // Setup the render target view and viewport
  hr = DXUTSetupD3D10Views( pd3dDevice, pDevSettings );
  if( FAILED(hr) )
  {
    DXUT_ERR( L"DXUTSetupD3D10Views", hr );
    return;
  }

  // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
  //DXUTSetupCursor();

  // Call the app's SwapChain reset callback
  GetDXUTState().SetInsideDeviceCallback( true );
  const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
  LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallbackSwapChainResized = GetDXUTState().GetD3D10SwapChainResizedFunc();
  hr = S_OK;
  if( pCallbackSwapChainResized != NULL )
    hr = pCallbackSwapChainResized( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
  GetDXUTState().SetInsideDeviceCallback( false );
  if( FAILED(hr) )
  {
    // If callback failed, cleanup
    DXUT_ERR( L"DeviceResetCallback", hr );
    if( hr != DXUTERR_MEDIANOTFOUND )
      hr = DXUTERR_RESETTINGDEVICEOBJECTS;

    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallbackSwapChainReleasing = GetDXUTState().GetD3D10SwapChainReleasingFunc();
    if( pCallbackSwapChainReleasing != NULL )
      pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    DXUTPause( false, false );
#if defined PS3
    // PS3 FIXME handle termination on PS3
#else
    PostQuitMessage( 0 );
#endif
  }
  else
  {
    GetDXUTState().SetDeviceObjectsReset( true );
    DXUTPause( false, false );
  }

#if !defined PS3
  if (!gRenDev->m_bEditor)
    ShowWindow( DXUTGetHWND(), SW_SHOW );
#endif
}

void DXUTCheckForDXGIChange()
{
  HRESULT hr = S_OK;
  ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
  DXUTDeviceSettings* pDevSettings = GetDXUTState().GetCurrentDeviceSettings();

  IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
  IDXGIFactory* pDXGIFactory = DXUTGetDXGIFactory();

  // Determine if we're fullscreen
  BOOL bFullScreen;
  pSwapChain->GetFullscreenState( &bFullScreen, NULL );

  gcpRendD3D->m_CVFullScreen->Set(bFullScreen);

  //if (!bFullScreen)
  //{
  //  RECT rcCurrentClient;
  //  GetClientRect( DXUTGetHWND(), &rcCurrentClient );
  //  int nW = rcCurrentClient.right-rcCurrentClient.left;
  //  int nH = rcCurrentClient.bottom-rcCurrentClient.top;
  //  if (nW && abs(nW-gcpRendD3D->m_CVWidth->GetIVal()) > 10)
  //    gcpRendD3D->SetWidth(nW);
  //}
}
#endif


//--------------------------------------------------------------------------------------
// Checks if the window client rect has changed and if it has, then reset the device
//--------------------------------------------------------------------------------------
void DXUTCheckForWindowSizeChange(int nWidth, int nHeight)
{
  // Skip the check for various reasons
  if( !GetDXUTState().GetDeviceCreated() || ( DXUTIsCurrentDeviceD3D9() && !DXUTIsWindowed()) )
    return;

  DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
#ifdef DIRECT3D9
  {
    RECT rcCurrentClient;
    GetClientRect( DXUTGetHWND(), &rcCurrentClient );

    if( (UINT)rcCurrentClient.right != DXUTGetBackBufferWidthFromDS( &deviceSettings ) ||
        (UINT)rcCurrentClient.bottom != DXUTGetBackBufferHeightFromDS( &deviceSettings ) )
    {
      // A new window size will require a new backbuffer size size
      // Tell DXUTChangeDevice and D3D to size according to the HWND's client rect
      deviceSettings.d3d9.pp.BackBufferWidth = 0;
      deviceSettings.d3d9.pp.BackBufferHeight = 0;

      DXUTChangeDevice( &deviceSettings, NULL, false, false );
    }
  }
#else
  {
    DXUTCheckForDXGIBufferChange(nWidth, nHeight);
  }
#endif
#if !defined PS3
  HWND temp = GetDesktopWindow();
  RECT trect;

  GetWindowRect(temp, &trect);

  gcpRendD3D->m_deskwidth =trect.right-trect.left;
  gcpRendD3D->m_deskheight=trect.bottom-trect.top;  
#endif
}

void DXUTCheckChange()
{
  // Skip the check for various reasons
  if( !GetDXUTState().GetDeviceCreated() || ( DXUTIsCurrentDeviceD3D9() && !DXUTIsWindowed()) )
    return;

  DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
#ifdef DIRECT3D9
#else
  {
    DXUTCheckForDXGIChange();
  }
#endif
}
#endif


//--------------------------------------------------------------------------------------
// Cleans up both the D3D9 and D3D10 3D environment (but only one should be active at a time)
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment( bool bReleaseSettings )
{
#if defined (DIRECT3D9) || defined (OPENGL)
    if( DXUTGetD3D9Device() )
        DXUTCleanup3DEnvironment9( bReleaseSettings );
#endif
#ifdef DIRECT3D10
    if( DXUTGetD3D10Device() )
        DXUTCleanup3DEnvironment10( bReleaseSettings );
#endif
}


#if defined (DIRECT3D9) || defined (DIRECT3D10)
//--------------------------------------------------------------------------------------
// Returns the HMONITOR attached to an adapter/output
//--------------------------------------------------------------------------------------
HMONITOR DXUTGetMonitorFromAdapter( DXUTDeviceSettings* pDeviceSettings )
{
#if defined (DIRECT3D9) || defined (OPENGL)
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        return pD3D->GetAdapterMonitor( pDeviceSettings->d3d9.AdapterOrdinal );
    }
#endif
#if defined(DIRECT3D10) && !defined(PS3)
    {
        CD3D10Enumeration* pD3DEnum = DXUTGetD3D10Enumeration();
        CD3D10EnumOutputInfo* pOutputInfo = pD3DEnum->GetOutputInfo( pDeviceSettings->d3d10.AdapterOrdinal, pDeviceSettings->d3d10.Output );
        if( !pOutputInfo )
            return 0;
        return DXUTMonitorFromRect( &pOutputInfo->Desc.DesktopCoordinates, MONITOR_DEFAULTTONEAREST );
    }
#else
  return 0;
#endif
}


//--------------------------------------------------------------------------------------
// Look for an adapter ordinal that is tied to a HMONITOR
//--------------------------------------------------------------------------------------
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal )
{
    *pAdapterOrdinal = 0;

#if defined (DIRECT3D9) || defined (OPENGL)
    if( DXUTIsCurrentDeviceD3D9() )
    {
        CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
        IDirect3D9* pD3D = DXUTGetD3D9Object();

        CGrowableArray<CD3D9EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
        for( int iAdapter=0; iAdapter<pAdapterList->GetSize(); iAdapter++ )
        {
            CD3D9EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt(iAdapter);
            HMONITOR hAdapterMonitor = pD3D->GetAdapterMonitor( pAdapterInfo->AdapterOrdinal );
            if( hAdapterMonitor == hMonitor )
            {
                *pAdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                return S_OK;
            }
        }
    }
#endif
#if defined(DIRECT3D10) && !defined(PS3)
    {
        // Get the monitor handle information
        MONITORINFOEXW mi;
        mi.cbSize = sizeof(MONITORINFOEX);
        DXUTGetMonitorInfo( hMonitor, &mi );

        // Search for this monitor in our enumeration hierarchy.
        CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
        CGrowableArray<CD3D10EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
        for( int iAdapter=0; iAdapter<pAdapterList->GetSize(); ++iAdapter )
        {
            CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
            for( int o = 0; o < pAdapterInfo->outputInfoList.GetSize(); ++o )
            {
                CD3D10EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( o );
                // Convert output device name from MBCS to Unicode
                if( wcsncmp( pOutputInfo->Desc.DeviceName, mi.szDevice, sizeof(mi.szDevice) / sizeof(mi.szDevice[0]) ) == 0 )
                {
                    *pAdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                    return S_OK;
                }
            }
        }
    }
#endif

    return E_FAIL;
}
#endif


//--------------------------------------------------------------------------------------
// This method is called when D3DERR_DEVICEREMOVED is returned from an API.  DXUT
// calls the application's DeviceRemoved callback to inform it of the event.  The
// application returns true if it wants DXUT to look for a closest device to run on.
// If no device is found, or the app returns false, DXUT shuts down.
//--------------------------------------------------------------------------------------
HRESULT DXUTHandleDeviceRemoved()
{
    HRESULT hr = S_OK;

    // Device has been removed. Call the application's callback if set.  If no callback
    // has been set, then just look for a new device
    bool bLookForNewDevice = true;
    LPDXUTCALLBACKDEVICEREMOVED pDeviceRemovedFunc = GetDXUTState().GetDeviceRemovedFunc();
    if( pDeviceRemovedFunc )
        bLookForNewDevice = pDeviceRemovedFunc( GetDXUTState().GetDeviceRemovedFuncUserContext() );

    if( bLookForNewDevice )
    {
        DXUTDeviceSettings *pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

        DXUTMatchOptions matchOptions;
        matchOptions.eAPIVersion         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterOrdinal     = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDeviceType         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eWindowed           = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterFormat      = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eVertexProcessing   = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eResolution         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferFormat   = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferCount    = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eMultiSample        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eSwapEffect         = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDepthFormat        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eStencilFormat      = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentFlags       = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eRefreshRate        = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentInterval    = DXUTMT_CLOSEST_TO_INPUT;

        hr = DXUTFindValidDeviceSettings( pDeviceSettings, pDeviceSettings, &matchOptions );
        if( SUCCEEDED( hr ) )
        {
            // Change to a Direct3D device created from the new device settings
            // that is compatible with the removed device.
            hr = DXUTChangeDevice( pDeviceSettings, NULL,
#ifdef DIRECT3D10
              NULL,
#endif
              true, false );
            if( SUCCEEDED( hr ) )
                return S_OK;
        }
    }

    // The app does not wish to continue or continuing is not possible.
    return DXUTERR_DEVICEREMOVED;
}


//--------------------------------------------------------------------------------------
// Stores back buffer surface desc in GetDXUTState().GetBackBufferSurfaceDesc10()
//--------------------------------------------------------------------------------------
void DXUTUpdateBackBufferDesc()
{
    if( DXUTIsCurrentDeviceD3D9() )
    {
#if defined(PS3)
				assert("No D3D9 device on PS3");
#else
        HRESULT hr;
        IDirect3DSurface9* pBackBuffer;
        hr = GetDXUTState().GetD3D9Device()->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
        D3DSURFACE_DESC* pBBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc9();
        ZeroMemory( pBBufferSurfaceDesc, sizeof(D3DSURFACE_DESC) );
        if( SUCCEEDED(hr) )
        {
            pBackBuffer->GetDesc( pBBufferSurfaceDesc );
            SAFE_RELEASE( pBackBuffer );
        }
#endif
    }
    else
    {
#if defined (DIRECT3D10)
        HRESULT hr;
        ID3D10Texture2D* pBackBuffer;
        hr = GetDXUTState().GetD3D10SwapChain()->GetBuffer( 0, __uuidof(ID3D10Texture2D), (LPVOID*)&pBackBuffer );
        DXGI_SURFACE_DESC* pBBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc10();
        ZeroMemory( pBBufferSurfaceDesc, sizeof(DXGI_SURFACE_DESC) );
        if( SUCCEEDED(hr) )
        {
            D3D10_TEXTURE2D_DESC TexDesc;
            pBackBuffer->GetDesc( &TexDesc );
            pBBufferSurfaceDesc->Width = (UINT) TexDesc.Width;
            pBBufferSurfaceDesc->Height = (UINT) TexDesc.Height;
            pBBufferSurfaceDesc->Format = TexDesc.Format;
            pBBufferSurfaceDesc->SampleDesc = TexDesc.SampleDesc;
            SAFE_RELEASE( pBackBuffer );
        }
#endif
    }
}


//--------------------------------------------------------------------------------------
// Updates the string which describes the device 
//--------------------------------------------------------------------------------------
#if defined (DIRECT3D9) || defined (OPENGL)
void DXUTUpdateD3D9DeviceStats( D3DDEVTYPE DeviceType, DWORD BehaviorFlags, D3DADAPTER_IDENTIFIER9* pAdapterIdentifier )
{
    if( GetDXUTState().GetNoStats() )
        return;

    // Store device description
    WCHAR* pstrDeviceStats = GetDXUTState().GetDeviceStats();
    if( DeviceType == D3DDEVTYPE_REF )
        StringCchCopyW( pstrDeviceStats, 256, L"REF" );
    else if( DeviceType == D3DDEVTYPE_HAL )
        StringCchCopyW( pstrDeviceStats, 256, L"HAL" );
    else if( DeviceType == D3DDEVTYPE_SW )
        StringCchCopyW( pstrDeviceStats, 256, L"SW" );

    if( BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING &&
        BehaviorFlags & D3DCREATE_PUREDEVICE )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            StringCchCatW( pstrDeviceStats, 256, L" (pure hw vp)" );
        else
            StringCchCatW( pstrDeviceStats, 256, L" (simulated pure hw vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            StringCchCatW( pstrDeviceStats, 256, L" (hw vp)" );
        else
            StringCchCatW( pstrDeviceStats, 256, L" (simulated hw vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            StringCchCatW( pstrDeviceStats, 256, L" (mixed vp)" );
        else
            StringCchCatW( pstrDeviceStats, 256, L" (simulated mixed vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING )
    {
        StringCchCatW( pstrDeviceStats, 256, L" (sw vp)" );
    }

    if( DeviceType == D3DDEVTYPE_HAL )
    {
        // Be sure not to overflow m_strDeviceStats when appending the adapter 
        // description, since it can be long.  
        StringCchCatW( pstrDeviceStats, 256, L": " );

        // Try to get a unique description from the CD3D9EnumDeviceSettingsCombo
        DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
        if( !pDeviceSettings )
            return;

        CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
        CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, pDeviceSettings->d3d9.AdapterFormat, pDeviceSettings->d3d9.pp.BackBufferFormat, pDeviceSettings->d3d9.pp.Windowed );
        if( pDeviceSettingsCombo )
        {
            StringCchCatW( pstrDeviceStats, 256, pDeviceSettingsCombo->pAdapterInfo->szUniqueDescription );
        }
        else
        {
            const int cchDesc = sizeof(pAdapterIdentifier->Description);
            WCHAR szDescription[cchDesc];
            MultiByteToWideChar( CP_ACP, 0, pAdapterIdentifier->Description, -1, szDescription, cchDesc );
            szDescription[cchDesc-1] = 0;
            StringCchCatW( pstrDeviceStats, 256, szDescription );
        }
    }
}
#endif

#ifdef DIRECT3D10
//--------------------------------------------------------------------------------------
// Updates the string which describes the device 
//--------------------------------------------------------------------------------------
void DXUTUpdateD3D10DeviceStats( D3D10_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc )
{
#if defined(PS3)
#define PS3DEVICEINFO "PS3 DXPS Device on RSX"
	memcpy(GetDXUTState().GetDeviceStats(),PS3DEVICEINFO,sizeof(PS3DEVICEINFO));
#else
    if( GetDXUTState().GetNoStats() )
        return;

    // Store device description
    WCHAR* pstrDeviceStats = GetDXUTState().GetDeviceStats();
    if( DeviceType == D3D10_DRIVER_TYPE_REFERENCE )
        StringCchCopyW( pstrDeviceStats, 256, L"REFERENCE" );
    else if( DeviceType == D3D10_DRIVER_TYPE_HARDWARE )
        StringCchCopyW( pstrDeviceStats, 256, L"HARDWARE" );
    else if( DeviceType == D3D10_DRIVER_TYPE_SOFTWARE )
        StringCchCopyW( pstrDeviceStats, 256, L"SOFTWARE" );

    if( DeviceType == D3DDEVTYPE_HAL )
    {
        // Be sure not to overflow m_strDeviceStats when appending the adapter 
        // description, since it can be long.  
        StringCchCatW( pstrDeviceStats, 256, L": " );

        // Try to get a unique description from the CD3D10EnumDeviceSettingsCombo
        DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
        if( !pDeviceSettings )
            return;

        CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
        CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo( pDeviceSettings->d3d10.AdapterOrdinal, pDeviceSettings->d3d10.DriverType, pDeviceSettings->d3d10.Output, pDeviceSettings->d3d10.sd.BufferDesc.Format, pDeviceSettings->d3d10.sd.Windowed );
        if( pDeviceSettingsCombo )
            StringCchCatW( pstrDeviceStats, 256, pDeviceSettingsCombo->pAdapterInfo->szUniqueDescription );
        else
            StringCchCatW( pstrDeviceStats, 256, pAdapterDesc->Description );
    }
#endif
}
#endif

//--------------------------------------------------------------------------------------
// Misc functions
//--------------------------------------------------------------------------------------
DXUTDeviceSettings WINAPI DXUTGetDeviceSettings()
{ 
    // Return a copy of device settings of the current device.  If no device exists yet, then
    // return a blank device settings struct
    DXUTDeviceSettings* pDS = GetDXUTState().GetCurrentDeviceSettings();
    if( pDS )
    {
        return *pDS;
    }
    else
    {
        DXUTDeviceSettings ds;
        ZeroMemory( &ds, sizeof(DXUTDeviceSettings) );
        return ds;
    }
}

D3DPRESENT_PARAMETERS WINAPI DXUTGetD3D9PresentParameters()    
{ 
    // Return a copy of the present params of the current device.  If no device exists yet, then
    // return blank present params
    DXUTDeviceSettings* pDS = GetDXUTState().GetCurrentDeviceSettings();
    if( pDS ) 
    {
        return pDS->d3d9.pp;
    }
    else 
    {
        D3DPRESENT_PARAMETERS pp;
        ZeroMemory( &pp, sizeof(D3DPRESENT_PARAMETERS) );
        return pp;
    }
}

void WINAPI DXUTSetMultimonSettings( bool bAutoChangeAdapter )
{
    GetDXUTState().SetAutoChangeAdapter( bAutoChangeAdapter );
}

//--------------------------------------------------------------------------------------
// Resets the state associated with DXUT 
//--------------------------------------------------------------------------------------
void WINAPI DXUTResetFrameworkState()
{
    GetDXUTState().Destroy();
    GetDXUTState().Create();
}


//--------------------------------------------------------------------------------------
// Closes down the window.  When the window closes, it will cleanup everything
//--------------------------------------------------------------------------------------
void WINAPI DXUTShutdown( int nExitCode )
{
    HWND hWnd = DXUTGetHWND();

    GetDXUTState().SetExitCode(nExitCode);

		if (!GetDXUTState().m_bFastExit)
			DXUTCleanup3DEnvironment( true );

#if !defined(PS3)
    // Shutdown D3D9
    IDirect3D9* pD3D = GetDXUTState().GetD3D9();
    SAFE_RELEASE( pD3D );
    GetDXUTState().SetD3D9( NULL );
#endif

#ifdef DIRECT3D10
    // Shutdown D3D10
    IDXGIFactory* pDXGIFactory = GetDXUTState().GetDXGIFactory();
    SAFE_RELEASE( pDXGIFactory );
    GetDXUTState().SetDXGIFactory( NULL );
#endif
}

#endif

