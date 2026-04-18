/****h* EZ.LCD.SDK.Wrapper/EZ_LCD_Page.cpp
 * NAME
 *   EZ_LCD_Page.cpp
 * COPYRIGHT
 *   The Logitech EZ LCD SDK Wrapper, including all accompanying
 *   documentation, is protected by intellectual property laws. All rights
 *   not expressly granted by Logitech are reserved.
 * PURPOSE
 *   Part of the SDK package.
 * AUTHOR
 *   Christophe Juncker (cj@wingmanteam.com)
 *   Vahid Afshar (Vahid_Afshar@logitech.com)
 * CREATION DATE
 *   06/13/2005
 * MODIFICATION HISTORY
 *   03/01/2006 - Added the concept of pages.
 *
 *******
 */

#include "StdAfx.h"
#ifdef USE_G15_LCD

#include "LCDText.h"
#include "LCDStreamingText.h"
#include "LCDIcon.h"
#include "LCDProgressBar.h"
#include "LCDBitmap.h"
#include "EZ_LCD.h"
#include "EZ_LCD_Page.h"

#ifndef ASIAN_LANGUAGES_SUPPORTED
// These settings are used in case this applet does not include support for
// Asian languages (Japanese, Korean and Chinese, which use an extra pixel)

// text box height for various font sizes
CONST INT LG_SMALL_FONT_TEXT_BOX_HEIGHT = 12;
CONST INT LG_MEDIUM_FONT_TEXT_BOX_HEIGHT = 13;
CONST INT LG_BIG_FONT_TEXT_BOX_HEIGHT = 20;

// logical origin Y value for various font sizes
CONST INT LG_SMALL_FONT_LOGICAL_ORIGIN_Y = -3;
CONST INT LG_MEDIUM_FONT_LOGICAL_ORIGIN_Y = -2;
CONST INT LG_BIG_FONT_LOGICAL_ORIGIN_Y = -4;

#else
// text box height for various font sizes
CONST INT LG_SMALL_FONT_TEXT_BOX_HEIGHT = 12;
CONST INT LG_MEDIUM_FONT_TEXT_BOX_HEIGHT = 13;
CONST INT LG_BIG_FONT_TEXT_BOX_HEIGHT = 20;

// logical origin Y value for various font sizes
CONST INT LG_SMALL_FONT_LOGICAL_ORIGIN_Y = -3;
CONST INT LG_MEDIUM_FONT_LOGICAL_ORIGIN_Y = -1;
CONST INT LG_BIG_FONT_LOGICAL_ORIGIN_Y = -2;
#endif

// corresponding font size
CONST INT LG_SMALL_FONT_SIZE = 7;
CONST INT LG_MEDIUM_FONT_SIZE = 8;
CONST INT LG_BIG_FONT_SIZE = 12;

// Scrolling text parameters
CONST INT LG_SCROLLING_SPEED = 14;
CONST INT LG_SCROLLING_STEP = 7;
CONST INT LG_SCROLLING_DELAY_MS = 2000;
CONST TCHAR LG_SCROLLING_GAP_TEXT[] = _T("       ");

// Progress bar parameters
CONST INT LG_PROGRESS_BAR_RANGE_MIN = 0;
CONST INT LG_PROGRESS_BAR_RANGE_MAX = 100;
CONST INT LG_PROGRESS_BAR_INITIAL_HEIGHT = 5;

// Font
CONST LPCTSTR LG_FONT = _T("Microsoft Sans Serif");

CEzLcdPage::CEzLcdPage()
{
    m_container = NULL;
}

CEzLcdPage::CEzLcdPage(CEzLcd * container, INT width, INT height)
{
    m_container = container;
    Init(width, height);
}

CEzLcdPage::~CEzLcdPage()
{
    LCD_OBJECT_LIST::iterator it_ = m_Objects.begin();
    while(it_ != m_Objects.end())
    {
        CLCDBase *pObject_ = *it_;
        LCDUIASSERT(NULL != pObject_);
        delete pObject_;

        ++it_;
    }
}

HANDLE CEzLcdPage::AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels)
{
    return AddText(type, size, alignment, maxLengthPixels, 1);
}

HANDLE CEzLcdPage::AddText(LGObjectType type, LGTextSize size, INT alignment, INT maxLengthPixels, INT numberOfLines)
{
    LCDUIASSERT(LG_SCROLLING_TEXT == type || LG_STATIC_TEXT == type);
    CLCDText* pStaticText_;
    CLCDStreamingText* pStreamingText_;

    INT iBoxHeight_ = LG_MEDIUM_FONT_TEXT_BOX_HEIGHT;
    INT iFontSize_ = LG_MEDIUM_FONT_SIZE;
    INT iLocalOriginY_ = LG_MEDIUM_FONT_LOGICAL_ORIGIN_Y;

    switch (type)
    {
    case LG_SCROLLING_TEXT:
        pStreamingText_ = new CLCDStreamingText();
        LCDUIASSERT(NULL != pStreamingText_);
        pStreamingText_->Initialize();
        pStreamingText_->SetOrigin(0, 0);
        pStreamingText_->SetFontFaceName(LG_FONT);
        pStreamingText_->SetAlignment(alignment);
        pStreamingText_->SetText(_T(" "));
        pStreamingText_->SetGapText(LG_SCROLLING_GAP_TEXT);
        pStreamingText_->SetSpeed(LG_SCROLLING_SPEED);
        pStreamingText_->SetScrollingStep(LG_SCROLLING_STEP);
        pStreamingText_->SetStartDelay(LG_SCROLLING_DELAY_MS);

        if (LG_SMALL == size)
        {
            iBoxHeight_ = LG_SMALL_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_SMALL_FONT_SIZE;
            iLocalOriginY_ = LG_SMALL_FONT_LOGICAL_ORIGIN_Y;
        }
        else if (LG_MEDIUM == size)
        {
            iBoxHeight_ = LG_MEDIUM_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_MEDIUM_FONT_SIZE;
            iLocalOriginY_ = LG_MEDIUM_FONT_LOGICAL_ORIGIN_Y;
        }
        else if (LG_BIG == size)
        {
            pStreamingText_->SetFontWeight(FW_BOLD);
            iBoxHeight_ = LG_BIG_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_BIG_FONT_SIZE;
            iLocalOriginY_ = LG_BIG_FONT_LOGICAL_ORIGIN_Y;
        }

        pStreamingText_->SetSize(maxLengthPixels, iBoxHeight_);
        pStreamingText_->SetFontPointSize(iFontSize_);
        pStreamingText_->SetLogicalOrigin(0, iLocalOriginY_);
        pStreamingText_->SetObjectType(LG_SCROLLING_TEXT);

        AddObject(pStreamingText_);

        return pStreamingText_;
        break;
    case LG_STATIC_TEXT:
        pStaticText_ = new CLCDText();
        LCDUIASSERT(NULL != pStaticText_);
        pStaticText_->Initialize();
        pStaticText_->SetOrigin(0, 0);
        pStaticText_->SetFontFaceName(LG_FONT);
        pStaticText_->SetAlignment(alignment);
        pStaticText_->SetBackgroundMode(OPAQUE);
        pStaticText_->SetText(_T(" "));

        if (LG_SMALL == size)
        {
            iBoxHeight_ = numberOfLines * LG_SMALL_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_SMALL_FONT_SIZE;
            iLocalOriginY_ = LG_SMALL_FONT_LOGICAL_ORIGIN_Y;
        }
        else if (LG_MEDIUM == size)
        {
            iBoxHeight_ = numberOfLines * LG_MEDIUM_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_MEDIUM_FONT_SIZE;
            iLocalOriginY_ = LG_MEDIUM_FONT_LOGICAL_ORIGIN_Y;
        }
        else if (LG_BIG == size)
        {
            pStaticText_->SetFontWeight(FW_BOLD);
            iBoxHeight_ = numberOfLines * LG_BIG_FONT_TEXT_BOX_HEIGHT;
            iFontSize_ = LG_BIG_FONT_SIZE;
            iLocalOriginY_ = LG_BIG_FONT_LOGICAL_ORIGIN_Y;
        }

        pStaticText_->SetSize(maxLengthPixels, iBoxHeight_);
        pStaticText_->SetFontPointSize(iFontSize_);
        pStaticText_->SetLogicalOrigin(0, iLocalOriginY_);
        pStaticText_->SetObjectType(LG_STATIC_TEXT);

        if (1 < numberOfLines)
            pStaticText_->SetWordWrap(TRUE);

        AddObject(pStaticText_);

        return pStaticText_;
        break;
    default:
        LCDUITRACE(_T("ERROR: trying to add text object with undefined type\n"));
    }

    return NULL;
}

HRESULT CEzLcdPage::SetText(HANDLE handle, LPCTSTR text)
{
    return SetText(handle, text, FALSE);
}

HRESULT CEzLcdPage::SetText(HANDLE handle, LPCTSTR text, BOOL resetScrollingTextPosition)
{
    CLCDBase* myObject = GetObject(handle);

    if (NULL != myObject)
    {
        if (!((LG_STATIC_TEXT == myObject->GetObjectType() || LG_SCROLLING_TEXT == myObject->GetObjectType())))
            return E_FAIL;

        if (LG_STATIC_TEXT == myObject->GetObjectType())
        {
            CLCDText* staticText = static_cast<CLCDText*>(myObject);
            if (NULL == staticText)
                return E_FAIL;

            staticText->SetText(text);
            return S_OK;
        }
        else if (LG_SCROLLING_TEXT == myObject->GetObjectType())
        {
            CLCDStreamingText* streamingText = static_cast<CLCDStreamingText*>(myObject);
            if (NULL == streamingText)
                return E_FAIL;

            streamingText->SetText(text);
            if (resetScrollingTextPosition)
            {
                streamingText->ResetUpdate();
            }
            return S_OK;
        }
    }

    return E_FAIL;
}

HRESULT CEzLcdPage::SetTextAlignment(HANDLE handle, INT alignment)
{
    CLCDBase* myObject_ = GetObject(handle);

    if (NULL != myObject_)
    {
        CLCDText* pText_ = static_cast<CLCDText*>(myObject_);
        LCDUIASSERT(NULL != pText_);
        pText_->SetAlignment(alignment);
    }

    return E_FAIL;
}

HANDLE CEzLcdPage::AddIcon(HICON hIcon, INT width, INT height)
{
    CLCDIcon* hIcon_ = new CLCDIcon();
    LCDUIASSERT(NULL != hIcon_);
    hIcon_->Initialize();
    hIcon_->SetOrigin(0, 0);
    hIcon_->SetSize(width, height);
    hIcon_->SetIcon(hIcon, width, height);
    hIcon_->SetObjectType(LG_ICON);

    AddObject(hIcon_);

    return hIcon_;
}

HANDLE CEzLcdPage::AddProgressBar(LGProgressBarType type)
{
    LCDUIASSERT(LG_FILLED == type || LG_CURSOR == type || LG_DOT_CURSOR == type);
    CLCDProgressBar *pProgressBar_ = new CLCDProgressBar();
    LCDUIASSERT(NULL != pProgressBar_);
    pProgressBar_->Initialize();
    pProgressBar_->SetOrigin(0, 0);
    pProgressBar_->SetSize(m_lcdWidth, LG_PROGRESS_BAR_INITIAL_HEIGHT);
    pProgressBar_->SetRange(LG_PROGRESS_BAR_RANGE_MIN, LG_PROGRESS_BAR_RANGE_MAX );
    pProgressBar_->SetPos(static_cast<FLOAT>(LG_PROGRESS_BAR_RANGE_MIN));
    pProgressBar_->SetObjectType(LG_PROGRESS_BAR);

// Map the progress style into what the UI classes understand
    CLCDProgressBar::ePROGRESS_STYLE eStyle = CLCDProgressBar::STYLE_FILLED;
    switch (type)
    {
    case LG_FILLED:
        eStyle = CLCDProgressBar::STYLE_FILLED;
        break;
    case LG_CURSOR:
        eStyle = CLCDProgressBar::STYLE_CURSOR;
        break;
    case LG_DOT_CURSOR:
        eStyle = CLCDProgressBar::STYLE_DASHED_CURSOR;
        break;
    }

    pProgressBar_->SetProgressStyle(eStyle);

    AddObject(pProgressBar_);

    return pProgressBar_;
}

HRESULT CEzLcdPage::SetProgressBarPosition(HANDLE handle, FLOAT percentage)
{
    CLCDBase* myObject_ = GetObject(handle);

    if (NULL != myObject_)
    {
        LCDUIASSERT(LG_PROGRESS_BAR == myObject_->GetObjectType());
// only allow this function for progress bars
        if (LG_PROGRESS_BAR == myObject_->GetObjectType())
        {
            CLCDProgressBar *progressBar_ = static_cast<CLCDProgressBar*>(myObject_);
            LCDUIASSERT(NULL != progressBar_);
            progressBar_->SetPos(percentage);
            return S_OK;
        }
    }

    return E_FAIL;
}

HRESULT CEzLcdPage::SetProgressBarSize(HANDLE handle, INT width, INT height)
{
    CLCDBase* myObject_ = GetObject(handle);
    LCDUIASSERT(NULL != myObject_);

    if (NULL != myObject_)
    {
        LCDUIASSERT(LG_PROGRESS_BAR == myObject_->GetObjectType());
// only allow this function for progress bars
        if (LG_PROGRESS_BAR == myObject_->GetObjectType())
        {
            myObject_->SetSize(width, height);
            return S_OK;
        }
    }

    return E_FAIL;
}

HANDLE CEzLcdPage::AddBitmap(INT width, INT height)
{
    CLCDBitmap *bitmap_ = new CLCDBitmap();
    LCDUIASSERT(NULL != bitmap_);
    bitmap_->Initialize();
    bitmap_->SetOrigin(0, 0);
    bitmap_->SetSize(width, height);

    AddObject(bitmap_);

    return bitmap_;
}

HRESULT CEzLcdPage::SetBitmap(HANDLE handle, HBITMAP bitmap)
{
    if (NULL == bitmap)
        return E_FAIL;

    CLCDBase* myObject_ = GetObject(handle);

    if (NULL != myObject_)
    {
        CLCDBitmap* bitmap_ = static_cast<CLCDBitmap*>(myObject_);
        LCDUIASSERT(bitmap_);
        bitmap_->SetBitmap(bitmap);
        return S_OK;
    }

    return E_FAIL;
}

HRESULT CEzLcdPage::SetOrigin(HANDLE handle, INT originX, INT originY)
{
    CLCDBase* myObject_ = GetObject(handle);
    LCDUIASSERT(NULL != myObject_);
    LCDUIASSERT(NULL != myObject_);

    SIZE size_ = myObject_->GetSize();

    LGObjectType objectType_ = myObject_->GetObjectType();

    if (LG_SCROLLING_TEXT == objectType_ || LG_STATIC_TEXT == objectType_ || LG_PROGRESS_BAR == objectType_)
    {
        if (originX < 0 || originY < 0 || originX + size_.cx > LGLCD_BMP_WIDTH) // || originY + size_.cy > LGLCD_BMP_HEIGHT)
        {
            ::MessageBox(NULL, _T("ERROR: text or progress bar object is out of bounds"), _T("Logitech LCD"), MB_ICONEXCLAMATION);
        }
    }

    if (NULL != myObject_ && NULL != myObject_)
    {
        myObject_->SetOrigin(originX, originY);
        return S_OK;
    }

    return E_FAIL;
}

HRESULT CEzLcdPage::SetVisible(HANDLE handle, BOOL visible)
{
    CLCDBase* myObject_ = GetObject(handle);
    LCDUIASSERT(NULL != myObject_);
    LCDUIASSERT(NULL != myObject_);

    if (NULL != myObject_ && NULL != myObject_)
    {
        myObject_->Show(visible);
        return S_OK;
    }

    return E_FAIL;
}

VOID CEzLcdPage::Update()
{
// Save copy of button state
    for (INT ii = 0; ii < NUMBER_SOFT_BUTTONS; ii++)
    {
        m_buttonWasPressed[ii] = m_buttonIsPressed[ii];
    }
}

CLCDBase* CEzLcdPage::GetObject(HANDLE handle)
{
    LCD_OBJECT_LIST::iterator it_ = m_Objects.begin();
    while(it_ != m_Objects.end())
    {
        CLCDBase *pObject_ = *it_;
        LCDUIASSERT(NULL != pObject_);

        if (pObject_ == handle)
        {
            return pObject_;
        }
        ++it_;
    }

    return NULL;
}

VOID CEzLcdPage::Init(INT width, INT height)
{
    m_lcdWidth = width;
    m_lcdHeight = height;

    for (INT ii = 0; ii < NUMBER_SOFT_BUTTONS; ii++)
    {
        m_buttonIsPressed[ii] = FALSE;
        m_buttonWasPressed[ii] = FALSE;
    }
}

void CEzLcdPage::OnLCDButtonDown(int button)
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

    m_container->OnLCDButtonDown(button);
}

void CEzLcdPage::OnLCDButtonUp(int button)
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

    m_container->OnLCDButtonUp(button);
}

#endif//USE_G15_LCD
