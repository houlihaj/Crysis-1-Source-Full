#ifndef EZLCD_H_INCLUDED_
#define EZLCD_H_INCLUDED_

#ifdef USE_G15_LCD

#include "LCDManager.h"
#include "LCDOutput.h"


#include "EZ_LCD_Defines.h"
#include "EZ_LCD_Page.h"


class CEzLcd
{
public:
    CEzLcd();
    ~CEzLcd();
    CEzLcd(LPCTSTR friendlyName, 
        INT width = LCD_DEFAULT_WIDTH, 
        INT height = LCD_DEFAULT_HEIGHT);

    HRESULT InitYourself(LPCTSTR friendlyName, 
                    BOOL isAutoStartable = FALSE,
                    BOOL isPersistent = FALSE,
                    lgLcdConfigureContext * configContext = NULL, 
                    INT width = LCD_DEFAULT_WIDTH, 
                    INT	 height = LCD_DEFAULT_HEIGHT);

    BOOL AnyDeviceOfThisFamilyPresent(DWORD deviceFamily);
    HRESULT SetDeviceFamilyToUse(DWORD deviceFamily);
    HRESULT SetPreferredDisplayFamily(DWORD deviceFamily);

    INT AddNewPage(VOID);
    INT RemovePage(INT pageNumber);
    INT GetPageCount(VOID);
    INT AddNumberOfPages(INT numberOfPages);
    // Call this method prior to adjusting any control on a page
    BOOL ModifyControlsOnPage(INT pageNumber);
    BOOL ShowPage(INT pageNumber);
    INT GetCurrentPageNumber();

    // The methods below are used to add or modify a control on a certain page. Method
    // ModifyControlsOnPage(INT pageNumber) must be called prior to using any of the following
    // methods.
    HANDLE AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels);
    HANDLE AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels, INT numberOfLines);
    HRESULT SetText(HANDLE handle, LPCTSTR text);
    HRESULT SetText(HANDLE handle, LPCTSTR text, BOOL resetScrollingTextPosition);
    HRESULT SetTextAlignment(HANDLE handle, INT alignment);

    HANDLE AddIcon(HICON hIcon, INT sizeX, INT sizeY);

    HANDLE AddProgressBar(LGProgressBarType type);
    HRESULT SetProgressBarPosition(HANDLE handle, FLOAT percentage);
    HRESULT SetProgressBarSize(HANDLE handle, INT width, INT height);

    HANDLE AddBitmap();
    HANDLE AddBitmap(INT width, INT height);
    HRESULT SetBitmap(HANDLE handle, HBITMAP bitmap);

    HRESULT SetOrigin(HANDLE handle, INT originX, INT originY);
    HRESULT SetVisible(HANDLE handle, BOOL visible);

    BOOL IsConnected();
    HRESULT SetAsForeground(BOOL setAsForeground);
    HRESULT SetScreenPriority(DWORD priority);
    DWORD GetScreenPriority();

    BOOL ButtonTriggered(INT button);
    BOOL ButtonReleased(INT button);
    BOOL ButtonIsPressed(INT button);

    VOID Update();

    virtual void OnLCDButtonDown(INT button);
    virtual void OnLCDButtonUp(INT button);

protected:
    INT                     m_lcdWidth;
    INT                     m_lcdHeight;
    TCHAR                   m_friendlyName[MAX_PATH];
    CLCDOutput              m_output;
    CEzLcdPage  *           m_activePage;
    LCD_PAGE_LIST           m_LCDPageList;
    INT                     m_pageCount;		// How many pages are there
    INT                     m_currentPageNumberShown;
    BOOL                    m_initNeeded;
    BOOL                    m_initSucceeded;
    BOOL                    m_buttonIsPressed[NUMBER_SOFT_BUTTONS];
    BOOL                    m_buttonWasPressed[NUMBER_SOFT_BUTTONS];
    lgLcdConfigureContext * m_configContext;
    BOOL                    m_isPersistent;
    BOOL                    m_isAutoStartable;
    DWORD                   m_currentDeviceFamily;
    DWORD                   m_preferredDeviceFamily;
};

#endif//USE_G15_LCD

#endif
