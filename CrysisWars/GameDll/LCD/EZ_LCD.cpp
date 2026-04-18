/****h* EZ.LCD.SDK.Wrapper/EzLcd
 * NAME
 *   EZ LCD Wrapper
 * COPYRIGHT
 *   The Logitech EZ LCD SDK Wrapper, including all accompanying
 *   documentation, is protected by intellectual property laws. All rights
 *   not expressly granted by Logitech are reserved.
 * PURPOSE
 *   The EZ LCD SDK Wrapper is aimed at developers wanting to make use of
 *   the LCD display on Logitech G-series keyboard. It comes with a very
 *   intuitive and easy to use interface which enables one to easily 
 *   display static strings, scrolling strings, progress bars, and icons.
 *   See the following to get started:
 *       - readme.txt: Describes how to get started
 *       - Use one of the samples included to see how things work.
 * AUTHOR
 *   Christophe Juncker (cj@wingmanteam.com)
 * CREATION DATE
 *   06/13/2005
 * MODIFICATION HISTORY
 *   03/01/2006 - Vahid Afshar. Added the concept of pages to the API. A
 *                client can now have multiple pages, each with its own
 *                controls. A page can be shown, while another is modified.
 *              - Introduced the InitYourself() method.
 *   11/17/2006 - Added extra AddBitmap(...) method with width and height 
 *                parameters. Added GetCurrentPageNumber() method.
 ******
 */

#include "StdAfx.h"
#ifdef USE_G15_LCD

#include "EZ_LCD_Defines.h"
#include "EZ_LCD_Page.h"
#include "EZ_LCD.h"

/****f* EZ.LCD.Wrapper/CEzLcd::CEzLcd
 * NAME
 *  CEzLcd::CEzLcd() -- Basic constructor. The user must call the
 *      InitYourself(...) method after calling this constructor.
 * FUNCTION
 *  Object is created.
 * INPUTS
 ******
 */
CEzLcd::CEzLcd()
{
    m_pageCount = 0;
}

/****f* EZ.LCD.Wrapper/CEzLcd::CEzLcd
 * NAME
 *  CEzLcd::CEzLcd(LPCTSTR friendlyName, INT width, INT height).
 * FUNCTION
 *  Does necessary initialization. If you are calling this constructor,
 *  then you should NOT call the InitYourself(...) method.
 * INPUTS
 *  friendlyName  - friendly name of the applet/game. This name will be
 *                  displayed in the Logitech G-series LCD Manager.
 *  width         - width in pixels of the LCD.
 *  height        - height in pixels of the LCD.
 ******
 */
CEzLcd::CEzLcd(LPCTSTR friendlyName, INT width, INT height)
{
    m_pageCount = 0;
    InitYourself(friendlyName, FALSE, FALSE, NULL, width, height);
}

CEzLcd::~CEzLcd()
{
    // delete all the screens
    LCD_PAGE_LIST::iterator it = m_LCDPageList.begin();
    while(it != m_LCDPageList.end())
    {
        CEzLcdPage *page_ = *it;
        LCDUIASSERT(NULL != page_);

        delete page_;
        ++it;
    }

    m_output.Shutdown();
}

/****f* EZ.LCD.Wrapper/CEzLcd::InitYourself
 * NAME
 *  HRESULT CEzLcd::InitYourself(LPCTSTR friendlyName, BOOL 
 *      IsAutoStartable, BOOL IsPersistent, lgLcdOnConfigureCB 
 *      callbackFunction ,width , height)
 * FUNCTION
 *  Does necessary initialization. This method SHOULD ONLY be called if
 *  the empty constructor is used: CEzLcd::CEzLcd()
 * INPUTS
 *  friendlyName     - friendly name of the applet/game. This name will be
 *                     displayed in the Logitech G-series LCD Manager.
 *  IsAutoStartable  - Determines if the applet is to be started
 *                     automatically every time by the LCD manager software
 *                     (Part of G15 software package)
 *  IsPersistent     - Determines if the applet's friendlyName will remain
 *                     in the list of applets seen by the LCD manager
 *                     software after the applet terminates
 *  configContext    - Pointer to the lgLcdConfigContext structure used
 *                     during callback into the applet
 *  width            - width in pixels of the LCD.
 *  height           - height in pixels of the LCD.
 * RETURN VALUE
 *  S_OK if it can connect to the LCD Manager
 *  E_FAIL otherwise
 ******
 */
HRESULT CEzLcd::InitYourself(LPCTSTR friendlyName, BOOL isAutoStartable, BOOL isPersistent,
                             lgLcdConfigureContext * configContext, INT width, INT height)
{
    if (m_pageCount != 0)
    {
        // Maybe the user is calling the old constructor and calling InitYourself as well.
        // Alert him of the problem. If the old constructor is called, then InitYourself should
        // not be called. InitYourself should be called, when the empty parameter constructor is
        // called, CEzLcd::CEzLcd()
        return E_FAIL;
    }

    m_lcdWidth = width;
    m_lcdHeight = height;
    _tcscpy(m_friendlyName, friendlyName);

    m_initNeeded = TRUE;
    m_initSucceeded = FALSE;
    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_OTHER;
    m_preferredDeviceFamily = LGLCD_DEVICE_FAMILY_OTHER;

    for (INT ii = 0; ii < NUMBER_SOFT_BUTTONS; ii++)
    {
        m_buttonIsPressed[ii] = FALSE;
        m_buttonWasPressed[ii] = FALSE;
    }

    m_configContext = configContext;		// Keep the context structure pointer
    m_isPersistent = isPersistent;
    m_isAutoStartable = isAutoStartable;

    // No active page to start with
    m_activePage = NULL;

    m_currentPageNumberShown = 0;

    // Add the first page for backwards compatibility
    AddNewPage();
    ModifyControlsOnPage(0);

    // Make the active screen the one on the output
    m_output.UnlockScreen();
    m_output.LockScreen(m_activePage);

    // Will now connect to the real library and see it the lgLcdInit() succeeds
    lgLcdConnectContextEx   lgdConnectContextEx_;
    lgLcdConfigureContext   lgdConfigureContext_;

    if (m_configContext != NULL)
    {
        lgdConfigureContext_.configCallback = m_configContext->configCallback;
        lgdConfigureContext_.configContext = m_configContext->configContext;
    }
    else
    {
        lgdConfigureContext_.configCallback = NULL;
        lgdConfigureContext_.configContext = NULL;
    }

    lgdConnectContextEx_.appFriendlyName = m_friendlyName;
    lgdConnectContextEx_.isPersistent = m_isPersistent;
    lgdConnectContextEx_.isAutostartable = m_isAutoStartable;
    lgdConnectContextEx_.onConfigure = lgdConfigureContext_;
    lgdConnectContextEx_.dwAppletCapabilitiesSupported = LGLCD_APPLET_CAP_BASIC;
    lgdConnectContextEx_.dwReserved1 = NULL;
    lgdConnectContextEx_.onNotify.notificationCallback = NULL;
    lgdConnectContextEx_.onNotify.notifyContext = NULL;

    if (FAILED(m_output.Initialize(&lgdConnectContextEx_, FALSE)))
    {
        // This means the LCD SDK's lgLcdInit failed, and therefore
        // we will not be able to ever connect to the LCD, even if
        // a G-series keyboard is actually connected.
        LCDUITRACE(_T("ERROR: LCD SDK initialization failed\n"));
        m_initSucceeded = FALSE;
        return E_FAIL;
    }
    else
    {
        m_initSucceeded = TRUE;
    }

    m_initNeeded = FALSE;

    return S_OK;
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddText
 * NAME
 *  HANDLE CEzLcd::AddText(LGObjectType type, LGTextSize size,
 *      INT alignment, INT maxLengthPixels) -- Add a text object to the
 *      page being worked on.
 * INPUTS
 *  type             - specifies whether the text is static or
 *                     scrolling. Possible types are: LG_SCROLLING_TEXT,
 *                     LG_STATIC_TEXT
 *  size             - size of the text. Choose between these three:
 *                     LG_SMALL, LG_MEDIUM or LG_BIG.
 *  alignment       - alignment of the text. Values are: DT_LEFT,
 *                    DT_CENTER, DT_RIGHT.
 *  maxLengthPixels - max length in pixels of the text. If the text is
 *                    longer and of type LG_STATIC_TEXT, it will be cut
 *                    off. If the text is longer and of type
 *                    LG_SCROLLING_TEXT, it will scroll.
 * RETURN VALUE
 *  Handle for this object.
 * SEE ALSO
 *  CEzLcd::AddIcon
 *  CEzLcd::AddProgressBar
 ******
 */
HANDLE CEzLcd::AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels)
{
    return AddText(type, size, alignment, maxLengthPixels, 1);
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddText
 * NAME
 *  HANDLE CEzLcd::AddText(LGObjectType type, LGTextSize size,
 *      INT alignment, INT maxLengthPixels, INT numberOfLines) -- Add a
 *      text object to the page being worked on.
 * INPUTS
 *  type             - specifies whether the text is static or
 *                     scrolling. Possible types are: LG_SCROLLING_TEXT,
 *                     LG_STATIC_TEXT
 *  size             - size of the text. Choose between these three:
 *                     LG_SMALL, LG_MEDIUM or LG_BIG.
 *  alignment       - alignment of the text. Values are: DT_LEFT,
 *                    DT_CENTER, DT_RIGHT.
 *  maxLengthPixels - max length in pixels of the text. If the text is
 *                    longer and of type LG_STATIC_TEXT, it will be cut
 *                    off. If the text is longer and of type
 *                    LG_SCROLLING_TEXT, it will scroll.
 *  numberOfLines   - number of lines the text can use. For static text
 *                    only. If number bigger than 1 and static text is
 *                    too long to fit on LCD, the text will be displayed
 *                    on multiple lines as necessary.
 * RETURN VALUE
 *  Handle for this object.
 * SEE ALSO
 *  CEzLcd::AddIcon
 *  CEzLcd::AddProgressBar
 ******
 */
HANDLE CEzLcd::AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels,
                       INT numberOfLines)
{
    if (m_activePage == NULL)
    {
        return NULL;
    }

    return m_activePage->AddText(type, size, alignment, maxLengthPixels, numberOfLines);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetText
 * NAME
 *  HRESULT CEzLcd::SetText(HANDLE handle, LPCTSTR text) -- Sets the text
 *      in the control on the page being worked on.
 * INPUTS
 *  handle          - handle to the object.
 *  text            - text string.
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetText(HANDLE handle, LPCTSTR text)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetText(handle, text);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetText
 * NAME
 *  HRESULT CEzLcd::SetText(HANDLE handle, LPCTSTR text,
 *       BOOL resetScrollingTextPosition) -- Sets the text in the control on
 *       the page being worked on.
 * INPUTS
 *  handle                      - handle to the object.
 *  text                        - text string.
 *  resetScrollingTextPosition  - indicates if position of scrolling text
 *                                needs to be reset
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetText(HANDLE handle, LPCTSTR text, BOOL resetScrollingTextPosition)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetText(handle, text, resetScrollingTextPosition);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetTextAlignment
 * NAME
 *  HRESULT CEzLcd::SetTextAlignment(HANDLE handle, INT alignment) -- Sets
 *       the text alignment.
 * INPUTS
 *  handle          - handle to the object.
 *  alignment       - alignment of the text. Values are: DT_LEFT,
 *                    DT_CENTER, DT_RIGHT.
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetTextAlignment(HANDLE handle, INT alignment)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetTextAlignment(handle, alignment);
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddIcon
 * NAME
 *  HANDLE CEzLcd::AddIcon(HICON hIcon, INT sizeX, INT sizeY) -- Add an
 *      icon object to the page being worked on.
 * INPUTS
 *  hIcon           - icon to be displayed on the page. Should be 1 bpp
 *                    bitmap.
 *  sizeX           - x-axis size of the bitmap.
 *  sizeY           - y-axis size of the bitmap.
 * RETURN VALUE
 *  Handle for this object.
 * SEE ALSO
 *  CEzLcd::AddText
 *  CEzLcd::AddProgressBar
 ******
 */
HANDLE CEzLcd::AddIcon(HICON hIcon, INT sizeX, INT sizeY)
{
    if (m_activePage == NULL)
    {
        return NULL;
    }

    return m_activePage->AddIcon(hIcon, sizeX, sizeY);
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddProgressBar
 * NAME
 *  HANDLE CEzLcd::AddProgressBar(LGProgressBarType type) -- Add a progress
 *      bar object to the page being worked on.
 * INPUTS
 *  type            - type of the progress bar. Types are: LG_CURSOR,
 *                    LG_FILLED, LG_DOT_CURSOR.
 * RETURN VALUE
 *  Handle for this object.
 * SEE ALSO
 *  CEzLcd::AddText
 *  CEzLcd::AddIcon
 ******
 */
HANDLE CEzLcd::AddProgressBar(LGProgressBarType type)
{
    if (m_activePage == NULL)
    {
        return NULL;
    }

    return m_activePage->AddProgressBar(type);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetProgressBarPosition
 * NAME
 *  HRESULT CEzLcd::SetProgressBarPosition(HANDLE handle, FLOAT percentage)
 *      -- Set position of the progress bar's cursor.
 * INPUTS
 *  handle          - handle to the object.
 *  percentage      - percentage of progress (0 to 100).
 * RETURN VALUE
 *  E_FAIL if there was an error or if handle does not correspond to a
 *  progress bar.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetProgressBarPosition(HANDLE handle, FLOAT percentage)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetProgressBarPosition(handle, percentage);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetProgressBarSize
 * NAME
 *  HRESULT CEzLcd::SetProgressBarSize(HANDLE handle, INT width, INT 
 *      height) -- Set size of progress bar.
 * INPUTS
 *  handle          - handle to the object.
 *  width           - x-axis part of the size
 *  height          - y-axis part of the size (a good default value is 5).
 * RETURN VALUE
 *  E_FAIL if there was an error or if handle does not correspond to a
 *  progress bar.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetProgressBarSize(HANDLE handle, INT width, INT height)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetProgressBarSize(handle, width, height);
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddBitmap
 * NAME
 *  HANDLE CEzLcd::AddBitmap() -- Add a bitmap object to the page being 
 *      worked on.
 * RETURN VALUE
 *  Handle for this object.
 * SEE ALSO
 *  CEzLcd::AddText
 *  CEzLcd::AddIcon
 ******
 */
HANDLE CEzLcd::AddBitmap()
{
    return AddBitmap(LGLCD_BMP_WIDTH, LGLCD_BMP_HEIGHT);
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddBitmap
* NAME
*  HANDLE CEzLcd::AddBitmap(INT width, INT height) -- Add a bitmap object
*       to the page being worked on.
* INPUTS
*  width           - width of the invisible box around the bitmap
*  height          - height of the invisible box around the bitmap
* RETURN VALUE
*  Handle for this object.
* NOTES
*  The width and height parameters define an invisible box that is around 
*  the bitmap, starting at the upper left corner. If for example a bitmap 
*  is 32x32 pixels and the width and height parameters given are 10x10, 
*  only the 10 first pixels of the bitmap will be displayed both in width 
*  and height.
* SEE ALSO
*  CEzLcd::AddText
*  CEzLcd::AddIcon
******
*/
HANDLE CEzLcd::AddBitmap(INT width, INT height)
{
    if (m_activePage == NULL)
    {
        return NULL;
    }

    return m_activePage->AddBitmap(width, height);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetBitmap
 * NAME
 *  HRESULT CEzLcd::SetBitmap(HANDLE handle, HBITMAP bitmap) -- Set the
 *      bitmap.
 * INPUTS
 *  handle          - handle to the object.
 *  bitmap          - 1bpp bitmap.
 * RETURN VALUE
 *  E_FAIL if there was an error or if handle does not correspond to a
 *  bitmap.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetBitmap(HANDLE handle, HBITMAP bitmap)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetBitmap(handle, bitmap);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetOrigin
 * NAME
 *  HRESULT CEzLcd::SetOrigin(HANDLE handle, INT XOrigin, INT YOrigin)
 *      -- Set the origin of an object. The origin corresponds to the
 *      furthest pixel on the upper left corner of an
 *      object.
 * INPUTS
 *  handle          - handle to the object.
 *  XOrigin         - x-axis part of the origin.
 *  YOrigin         - y-axis part of the origin.
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetOrigin(HANDLE handle, INT XOrigin, INT YOrigin)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetOrigin(handle, XOrigin, YOrigin);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetVisible
 * NAME
 *  HRESULT CEzLcd::SetVisible(HANDLE handle, BOOL visible) -- set
 *      corresponding object to be visible or invisible on the page being
 *       worked on.
 * INPUTS
 *  handle          - handle to the object.
 *  visible         - set to FALSE to make object invisible, TRUE to
 *                    make it visible (default).
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetVisible(HANDLE handle, BOOL visible)
{
    if (m_activePage == NULL)
    {
        return E_FAIL;
    }

    return m_activePage->SetVisible(handle, visible);
}

/****f* EZ.LCD.Wrapper/CEzLcd::ButtonTriggered
 * NAME
 *  BOOL CEzLcd::ButtonTriggered(INT button) -- Check if a button was
 *      triggered.
 * INPUTS
 *  button      - name of the button to be checked. Possible names are:
 *                LG_BUTTON_1, LG_BUTTON_2, LG_BUTTON_3, LG_BUTTON_4
 * RETURN VALUE
 *  TRUE if the specific button was triggered
 *  FALSE otherwise
 * SEE ALSO
 *  CEzLcd::ButtonReleased
 *  CEzLcd::ButtonIsPressed
 ******
 */
BOOL CEzLcd::ButtonTriggered(INT button)
{
    if (m_buttonIsPressed[button] && !m_buttonWasPressed[button])
    {
        return TRUE;
    }

    return FALSE;
}

/****f* EZ.LCD.Wrapper/CEzLcd::ButtonReleased
 * NAME
 *  BOOL CEzLcd::ButtonReleased(INT button) -- Check if a button was
 *      released.
 * INPUTS
 *  button      - name of the button to be checked. Possible names are:
 *                LG_BUTTON_1, LG_BUTTON_2, LG_BUTTON_3, LG_BUTTON_4
 * RETURN VALUE
 *  TRUE if the specific button was released
 *  FALSE otherwise
 * SEE ALSO
 *  CEzLcd::ButtonTriggered
 *  CEzLcd::ButtonIsPressed
 ******
 */
BOOL CEzLcd::ButtonReleased(INT button)
{
    if (!m_buttonIsPressed[button] && m_buttonWasPressed[button])
    {
        return TRUE;
    }
    return FALSE;
}

/****f* EZ.LCD.Wrapper/CEzLcd::ButtonIsPressed
 * NAME
 *  BOOL CEzLcd::ButtonIsPressed(INT button) -- Check if a button is being
 *      pressed.
 * INPUTS
 *  button      - name of the button to be checked. Possible names are:
 *                LG_BUTTON_1, LG_BUTTON_2, LG_BUTTON_3, LG_BUTTON_4
 * RETURN VALUE
 *  TRUE if the specific button is being pressed
 *  FALSE otherwise
 * SEE ALSO
 *  CEzLcd::ButtonTriggered
 *  CEzLcd::ButtonReleased
 ******
 */
BOOL CEzLcd::ButtonIsPressed(INT button)
{
    return m_buttonIsPressed[button];
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetAsForeground
 * NAME
 *  HRESULT CEzLcd::SetAsForeground(BOOL setAsForeground) -- Become
 *      foreground applet on LCD, or remove yourself as foreground applet.
 * INPUTS
 *  setAsForeground    - Determines whether to be foreground or not.
 *                       Possible values are:
 *                          - TRUE implies foreground
 *                          - FALSE implies no longer foreground
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetAsForeground(BOOL setAsForeground)
{
    m_output.SetAsForeground(setAsForeground);

    return S_OK;
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetScreenPriority
 * NAME
 *  HRESULT CEzLcd::SetScreenPriority(DWORD priority) -- Set screen
 *      priority.
 * INPUTS
 *  priority    - priority of the screen. Possible values are:
 *                      - LGLCD_PRIORITY_IDLE_NO_SHOW
 *                      - LGLCD_PRIORITY_BACKGROUND
 *                      - LGLCD_PRIORITY_NORMAL
 *                      - LGLCD_PRIORITY_ALERT.
 * NOTES
 *  Default priority is LGLCD_PRIORITY_NORMAL.
 * RETURN VALUE
 *  E_FAIL if there was an error.
 *  S_OK if no error.
 ******
 */
HRESULT CEzLcd::SetScreenPriority(DWORD priority)
{
    m_output.SetScreenPriority(priority);

    Update();

    return S_OK;
}

/****f* EZ.LCD.Wrapper/CEzLcd::GetScreenPriority
* NAME
*  DWORD CEzLcd::GetScreenPriority() -- Get screen priority.
* RETURN VALUE
*  LGLCD_PRIORITY_IDLE_NO_SHOW, LGLCD_PRIORITY_BACKGROUND,
*  LGLCD_PRIORITY_NORMAL, or LGLCD_PRIORITY_ALERT
******
*/
DWORD CEzLcd::GetScreenPriority()
{
    return m_output.GetScreenPriority();
}

/****f* EZ.LCD.Wrapper/CEzLcd::AnyDeviceOfThisFamilyPresent
 * NAME
 *  BOOL CEzLcd::AnyDeviceOfThisFamilyPresent(DWORD deviceFamily) -- Tells
 *       whether a device of the specified family is currently connected.
 *       Multiple families can be specified using the bitwise or operator
 *       (|).
 * INPUTS
 *  deviceFamily     - Family ID to check for. IDs are:
 *                     LGLCD_DEVICE_FAMILY_KEYBOARD_G15,
 *                     LGLCD_DEVICE_FAMILY_SPEAKERS_Z10,
 *                     LGLCD_DEVICE_FAMILY_JACKBOX,
 *                     LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15,
 *                     LGLCD_DEVICE_FAMILY_OTHER
 * RETURN VALUE
 *  TRUE if a device of the specified family is present.
 *  FALSE otherwise.
 * SEE ALSO
 *  CEzLcd::SetDeviceFamilyToUse
 *  CEzLcd::SetPreferredDisplayFamily
 ******
 */
BOOL CEzLcd::AnyDeviceOfThisFamilyPresent(DWORD deviceFamily)
{
    return m_output.AnyDeviceOfThisFamilyPresent(deviceFamily, 0);
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetDeviceFamilyToUse
 * NAME
 *  HRESULT CEzLcd::SetDeviceFamilyToUse(DWORD deviceFamily) -- Call this
 *       method to set the device family to be used. This method is
 *       exclusive, which means that in case no device of the specified
 *       family is present,
 *       the current applet will not be shown anywhere else.
 * INPUTS
 *  deviceFamily     - Family ID to check for. IDs are:
 *                     LGLCD_DEVICE_FAMILY_KEYBOARD_G15,
 *                     LGLCD_DEVICE_FAMILY_SPEAKERS_Z10,
 *                     LGLCD_DEVICE_FAMILY_JACKBOX,
 *                     LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15,
 *                     LGLCD_DEVICE_FAMILY_OTHER
 * RETURN VALUE
 *  S_OK if there is a device of the family specified.
 *  E_FAIL otherwise.
 * SEE ALSO
 *  CEzLcd::AnyDeviceOfThisFamilyPresent
 *  CEzLcd::SetPreferredDisplayFamily
 ******
 */
HRESULT CEzLcd::SetDeviceFamilyToUse(DWORD deviceFamily)
{
    m_output.SetDeviceFamiliesSupported(deviceFamily, 0);

    m_output.Update(GetTickCount());

    if (m_output.IsOpened() == TRUE)
    {
        return S_OK;
    }

    return E_FAIL;
}

/****f* EZ.LCD.Wrapper/CEzLcd::SetPreferredDisplayFamily
 * NAME
 *  HRESULT CEzLcd::SetPreferredDisplayFamily(DWORD deviceFamily) -- Call
 *      this method to set the preferred device family to be used. If a 
 *      device of the specified family is not connected, another family 
 *      will be selected according to devices connected.
 * INPUTS
 *  deviceFamily     - Family ID to check for. IDs are:
 *                     LGLCD_DEVICE_FAMILY_KEYBOARD_G15,
 *                     LGLCD_DEVICE_FAMILY_SPEAKERS_Z10,
 *                     LGLCD_DEVICE_FAMILY_JACKBOX,
 *                     LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15,
 *                     LGLCD_DEVICE_FAMILY_OTHER
 * RETURN VALUE
 *  S_OK if there is a device of the family specified.
 *  E_FAIL otherwise.
 * SEE ALSO
 *  CEzLcd::AnyDeviceOfThisFamilyPresent
 *  CEzLcd::SetDeviceFamilyToUse
 ******
 */
HRESULT CEzLcd::SetPreferredDisplayFamily(DWORD deviceFamily)
{
    m_preferredDeviceFamily = deviceFamily;

    if (AnyDeviceOfThisFamilyPresent(deviceFamily))
    {
        if (m_currentDeviceFamily != deviceFamily)
        {
            if (S_OK == SetDeviceFamilyToUse(deviceFamily))
            {
                m_currentDeviceFamily = deviceFamily;
                return S_OK;
            }
        }
    }
    else
    {
        if (AnyDeviceOfThisFamilyPresent(LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15))
        {
            if (m_currentDeviceFamily != LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15)
            {
                if (S_OK == SetDeviceFamilyToUse(LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15))
                {
                    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_LCDEMULATOR_G15;
                }
            }
        }
        else if (AnyDeviceOfThisFamilyPresent(LGLCD_DEVICE_FAMILY_JACKBOX))
        {
            if (m_currentDeviceFamily != LGLCD_DEVICE_FAMILY_JACKBOX)
            {
                if (S_OK == SetDeviceFamilyToUse(LGLCD_DEVICE_FAMILY_JACKBOX))
                {
                    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_JACKBOX;
                }
            }
        }
        else if (AnyDeviceOfThisFamilyPresent(LGLCD_DEVICE_FAMILY_KEYBOARD_G15))
        {
            if (m_currentDeviceFamily != LGLCD_DEVICE_FAMILY_KEYBOARD_G15)
            {
                if (S_OK == SetDeviceFamilyToUse(LGLCD_DEVICE_FAMILY_KEYBOARD_G15))
                {
                    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_KEYBOARD_G15;
                }
            }
        }
        else if (AnyDeviceOfThisFamilyPresent(LGLCD_DEVICE_FAMILY_SPEAKERS_Z10))
        {
            if (m_currentDeviceFamily != LGLCD_DEVICE_FAMILY_SPEAKERS_Z10)
            {
                if (S_OK == SetDeviceFamilyToUse(LGLCD_DEVICE_FAMILY_SPEAKERS_Z10))
                {
                    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_SPEAKERS_Z10;
                }
            }
        }

        else if (AnyDeviceOfThisFamilyPresent(LGLCD_DEVICE_FAMILY_OTHER))
        {
            if (m_currentDeviceFamily != LGLCD_DEVICE_FAMILY_OTHER)
            {
                if (S_OK == SetDeviceFamilyToUse(LGLCD_DEVICE_FAMILY_OTHER))
                {
                    m_currentDeviceFamily = LGLCD_DEVICE_FAMILY_OTHER;
                }
            }
        }
    }

    return E_FAIL;
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddNewPage
 * NAME
 *  INT CEzLcd::AddNewPage() -- Call this method to create a new page
 *      to be displayed on the LCD. The method returns the current number
 *      of pages, after the page is added.
 * RETURN VALUE
 *  The method returns the current number of pages, after the page is
 *  added.
 ******
 */
INT CEzLcd::AddNewPage(VOID)
{
    CEzLcdPage*	page_ = NULL;

    // Create a new page and add it in
    page_ = new CEzLcdPage(this, m_lcdWidth, m_lcdHeight);
    page_->Initialize();
    page_->SetExpiration(INFINITE);

    m_LCDPageList.push_back(page_);

    m_pageCount = static_cast<INT>(m_LCDPageList.size());
    return m_pageCount;
}

/****f* EZ.LCD.Wrapper/CEzLcd::RemovePage
 * NAME
 *  INT CEzLcd::RemovePage(INT pageNumber) -- Call this method to remove
 *      a page from the pages you've created to be displayed on the LCD.
 *      The method returns the current number of pages, after the page is
 *      deleted.
 * INPUTS
 *  pageNumber      - The number for the page that is to be removed.
 * RETURN VALUE
 *  The method returns the current number of pages, after the page is
 *  removed.
 ******
 */
INT CEzLcd::RemovePage(INT pageNumber)
{
    // Do we have this page, if not return error
    if (pageNumber >= m_pageCount)
    {
        return -1;
    }

    // find the next active screen
    LCD_PAGE_LIST::iterator it = m_LCDPageList.begin();
    m_LCDPageList.erase(it + pageNumber);

    --m_pageCount;		// Decrement the number of pages
    if (m_pageCount > 0)
    {
        m_activePage = *it;
    }
    else
    {
        m_activePage = NULL;
    }
    return m_pageCount;
}

/****f* EZ.LCD.Wrapper/CEzLcd::GetPageCount
 * NAME
 *  INT CEzLcd::GetPageCount() -- returns the current number of pages.
 * RETURN VALUE
 *  The method returns the current number of pages.
 ******
 */
INT CEzLcd::GetPageCount(VOID)
{
    return m_pageCount;
}

/****f* EZ.LCD.Wrapper/CEzLcd::AddNumberOfPages
 * NAME
 *  INT CEzLcd::AddNumberOfPages(INT numberOfPages) -- Adds numberOfPages
 *      to the total of pages you've created.
 * INPUTS
 *  numberOfPages      - Count of pages to add in.
 * RETURN VALUE
 *  The method returns the current number of pages, after the pages are
 *  added.
 ******
 */
INT CEzLcd::AddNumberOfPages(INT numberOfPages)
{
    for (int iCount=0; iCount<numberOfPages; iCount++)
    {
        AddNewPage();
    }

    return GetPageCount();
}


/****f* EZ.LCD.Wrapper/CEzLcd::ModifyControlsOnPage
 * NAME
 *  BOOL CEzLcd::ModifyControlsOnPage(INT pageNumber) -- Call this method
 *      in order to modify the controls on specified page. This method must
 *      be called first prior to any modifications on that page.
 * INPUTS
 *  pageNumber  - The page number that the controls will be modified on.
 * RETURN VALUE
 *  TRUE - If succeeded.
 *  FALSE - If encountered an error.
 ******
 */
BOOL CEzLcd::ModifyControlsOnPage(INT pageNumber)
{
    if (pageNumber >= m_pageCount)
    {
        return FALSE;
    }

    LCD_PAGE_LIST::iterator it = m_LCDPageList.begin();
    m_activePage = *(it + pageNumber);

    return TRUE;
}

/****f* EZ.LCD.Wrapper/CEzLcd::ShowPage
 * NAME
 *  BOOL CEzLcd::ShowPage(INT pageNumber) -- Call this method in order to
 *      make the page shown on the LCD.
 * INPUTS
 *  pageNumber - The page that will be shown among your pages on the LCD.
 * RETURN VALUE
 *  TRUE - If succeeded.
 *  FALSE - If encountered an error.
 ******
 */
BOOL CEzLcd::ShowPage(INT pageNumber)
{
    if (pageNumber >= m_pageCount)
    {
        return FALSE;
    }

    LCD_PAGE_LIST::iterator it = m_LCDPageList.begin();
    m_activePage = *(it + pageNumber);

    m_output.UnlockScreen();
    m_output.LockScreen(m_activePage);

    m_currentPageNumberShown = pageNumber;

    return TRUE;
}

/****f* EZ.LCD.Wrapper/CEzLcd::GetCurrentPageNumber
* NAME
*  INT CEzLcd::GetCurrentPageNumber() -- Get page number currently being 
*       displayed.
* RETURN VALUE
*  Number of page currently being displayed. First page is number 0.
******
*/
INT CEzLcd::GetCurrentPageNumber()
{
    return m_currentPageNumberShown;
}

/****f* EZ.LCD.Wrapper/CEzLcd::IsConnected
 * NAME
 *  BOOL CEzLcd::IsConnected() -- Check if a Logitech G-series LCD is
 *      connected.
 * RETURN VALUE
 *  TRUE if an LCD is connected
 *  FALSE otherwise
 ******
 */
BOOL CEzLcd::IsConnected()
{
    return m_output.IsOpened();
}

/****f* EZ.LCD.Wrapper/CEzLcd::Update
 * NAME
 *  VOID CEzLcd::Update() -- Update LCD display
 * FUNCTION
 *  Updates the display. Must be called every loop.
 ******
 */
VOID CEzLcd::Update()
{
    if (m_initNeeded)
    {
        // find the next active screen
        LCD_PAGE_LIST::iterator it_ = m_LCDPageList.begin();
        while(it_ != m_LCDPageList.end())
        {
            CEzLcdPage *page_ = *it_;
            LCDUIASSERT(NULL != page_);

            // Add the screen to list of screens that m_output manages
            m_output.AddScreen(page_);

            ++it_;
        }

        // Make the active screen the one on the output
        m_output.UnlockScreen();
        m_output.LockScreen(m_activePage);

        // Setup registration and configure contexts.
        lgLcdConnectContext lgdConnectContext_;
        lgLcdConfigureContext lgdConfigureContext_;

        // Further expansion of registration and configure contexts.
        if (m_configContext != NULL)
        {
            lgdConfigureContext_.configCallback = m_configContext->configCallback;
            lgdConfigureContext_.configContext = m_configContext->configContext;
        }
        else
        {
            lgdConfigureContext_.configCallback = NULL;
            lgdConfigureContext_.configContext = NULL;
        }

        lgdConnectContext_.appFriendlyName = m_friendlyName;
        lgdConnectContext_.isPersistent = m_isPersistent;
        lgdConnectContext_.isAutostartable = m_isAutoStartable;
        lgdConnectContext_.onConfigure = lgdConfigureContext_;

        if (FAILED(m_output.Initialize(&lgdConnectContext_, FALSE)))
        {
            // This means the LCD SDK's lgLcdInit failed, and therefore
            // we will not be able to ever connect to the LCD, even if
            // a G-series keyboard is actually connected.
            LCDUITRACE(_T("ERROR: LCD SDK initialization failed\n"));
            m_initSucceeded = FALSE;
        }
        else
        {
            m_initSucceeded = TRUE;
        }

        m_initNeeded = FALSE;
    }

    // Only do stuff if initialization was successful. Otherwise
    // IsConnected will simply return false.
    if (m_initSucceeded)
    {
        // Save copy of button state
        for (INT ii = 0; ii < NUMBER_SOFT_BUTTONS; ii++)
        {
            m_buttonWasPressed[ii] = m_buttonIsPressed[ii];
        }

        // Every so often, if necessary, re-check to see if we need to display on a different device family
        if (m_preferredDeviceFamily != LGLCD_DEVICE_FAMILY_OTHER)
        {
            static DWORD localTimer_ = GetTickCount();

            DWORD currentTime_ = GetTickCount();
            if (currentTime_ - localTimer_ > LG_DEVICE_FAMILY_TIMER)
            {
                SetPreferredDisplayFamily(m_preferredDeviceFamily);
                localTimer_ = currentTime_;
            }
        }

        m_output.Update(GetTickCount());
        m_output.Draw();
    }
}

VOID CEzLcd::OnLCDButtonDown(INT button)
{
    switch(button)
    {
    case LGLCDBUTTON_BUTTON0:
        m_buttonIsPressed[LG_BUTTON_1] = TRUE;
        break;
    case LGLCDBUTTON_BUTTON1:
        m_buttonIsPressed[LG_BUTTON_2] = TRUE;
        break;
    case LGLCDBUTTON_BUTTON2:
        m_buttonIsPressed[LG_BUTTON_3] = TRUE;
        break;
    case LGLCDBUTTON_BUTTON3:
        m_buttonIsPressed[LG_BUTTON_4] = TRUE;
        break;
    default:
        LCDUITRACE(_T("ERROR: unknown button was pressed\n"));
        break;
    }
}

VOID CEzLcd::OnLCDButtonUp(int button)
{
    switch(button)
    {
    case LGLCDBUTTON_BUTTON0:
        m_buttonIsPressed[LG_BUTTON_1] = FALSE;
        break;
    case LGLCDBUTTON_BUTTON1:
        m_buttonIsPressed[LG_BUTTON_2] = FALSE;
        break;
    case LGLCDBUTTON_BUTTON2:
        m_buttonIsPressed[LG_BUTTON_3] = FALSE;
        break;
    case LGLCDBUTTON_BUTTON3:
        m_buttonIsPressed[LG_BUTTON_4] = FALSE;
        break;
    default:
        LCDUITRACE(_T("ERROR: unknown button was pressed\n"));
        break;
    }
}
#endif//USE_G15_LCD
