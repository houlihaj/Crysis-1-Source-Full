#include "StdAfx.h"
#include "HyperNodePainter_Comment.h"

namespace
{

	struct SAssets
	{
		SAssets() :
			font(L"Tahoma", 10.0f),
			//brushBackground( Gdiplus::Color(255,214,0) ),
			//brushBackgroundSelected( Gdiplus::Color(255,247,204) ),
			brushBackground( Gdiplus::Color(255,255,225) ),
			brushBackgroundSelected( Gdiplus::Color(255,255,255) ),
			brushText( Gdiplus::Color(0,0,0) ),
			brushTextSelected( Gdiplus::Color(200,0,0) ),
			penBorder( Gdiplus::Color(0,0,0) )
		{
			sf.SetAlignment( Gdiplus::StringAlignmentCenter );
		}
		~SAssets()
		{
		}

		Gdiplus::Font font;
		Gdiplus::SolidBrush brushBackground;
		Gdiplus::SolidBrush brushBackgroundSelected;
		Gdiplus::SolidBrush brushText;
		Gdiplus::SolidBrush brushTextSelected;
		Gdiplus::StringFormat sf;
		Gdiplus::Pen penBorder;
	};

}

void CHyperNodePainter_Comment::Paint( CHyperNode * pNode, CDisplayList * pList )
{
	static SAssets * pAssets = 0;
	if (!pAssets)
		pAssets = new SAssets();

	CDisplayRectangle * pBackground = pList->Add<CDisplayRectangle>()
		->SetHitEvent(eSOID_ShiftFirstOutputPort)
		->SetStroked(&pAssets->penBorder);

	CString text = pNode->GetName();
	text.Replace( "\\n","\r\n" );

	CDisplayString * pString = pList->Add<CDisplayString>()
		->SetHitEvent(eSOID_ShiftFirstOutputPort)
		->SetText( text )
		->SetBrush( &pAssets->brushText )
		->SetFont( &pAssets->font )
		->SetStringFormat( &pAssets->sf );

	if (pNode->IsSelected())
	{
		pString->SetBrush( &pAssets->brushTextSelected );
		pBackground->SetFilled(&pAssets->brushBackgroundSelected);
	}
	else
	{
		pBackground->SetFilled(&pAssets->brushBackground);
	}

	Gdiplus::RectF rect = pString->GetBounds(pList->GetGraphics());
	rect.Width += 32.0f;
	rect.Height += 6.0f;
	rect.X = rect.Y = 0.0f;
	pBackground->SetRect( rect );
	pString->SetLocation( Gdiplus::PointF( rect.Width * 0.5f, 3.0f ) );

	pBackground->SetHitEvent( eSOID_Title );
}
