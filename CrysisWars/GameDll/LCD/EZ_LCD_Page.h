
#ifndef EZLCD_PAGE_H_INCLUDED_
#define EZLCD_PAGE_H_INCLUDED_

#ifdef USE_G15_LCD

#include "LCDManager.h"
#include "LCDOutput.h"


#include "EZ_LCD_Defines.h"

class CEzLcd;

#pragma warning(disable:4264)

class CEzLcdPage : public CLCDManager
{
public:
    CEzLcdPage();
    CEzLcdPage(CEzLcd * container, INT width, INT height);
    ~CEzLcdPage();

    HANDLE AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels);
    HANDLE AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels, INT numberOfLines);
    HRESULT SetText(HANDLE handle, LPCTSTR text);
    HRESULT SetText(HANDLE handle, LPCTSTR text, BOOL resetScrollingTextPosition);
    HRESULT SetTextAlignment(HANDLE handle, INT alignment);

    HANDLE AddIcon(HICON hIcon, INT sizeX, INT sizeY);

    HANDLE AddProgressBar(LGProgressBarType type);
    HRESULT SetProgressBarPosition(HANDLE handle, FLOAT percentage);
    HRESULT SetProgressBarSize(HANDLE handle, INT width, INT height);

    HANDLE AddBitmap(INT width, INT height);
    HRESULT SetBitmap(HANDLE handle, HBITMAP bitmap);

    HRESULT SetOrigin(HANDLE handle, INT originX, INT originY);
    HRESULT SetVisible(HANDLE handle, BOOL visible);

    VOID Update();

    virtual void OnLCDButtonDown(int button);
    virtual void OnLCDButtonUp(int button);

protected:
    CLCDBase* GetObject(HANDLE handle);
    VOID Init(INT width, INT height);

protected:
    CEzLcd *    m_container;
    INT         m_lcdWidth;
    INT         m_lcdHeight;
    BOOL        m_buttonIsPressed[NUMBER_SOFT_BUTTONS];
    BOOL        m_buttonWasPressed[NUMBER_SOFT_BUTTONS];

};

typedef std::vector <CEzLcdPage*> LCD_PAGE_LIST;
typedef LCD_PAGE_LIST::iterator LCD_PAGE_LIST_ITER;

#pragma warning(default:4264)

#endif//USE_G15_LCD

#endif		// EZLCD_PAGE_H_INCLUDED_
