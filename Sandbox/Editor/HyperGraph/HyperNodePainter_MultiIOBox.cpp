#include "StdAfx.h"
#include "HyperNodePainter_MultiIOBox.h"

#include "AnimationGraph.h"

namespace
{

	struct SAssets
	{
		SAssets() :
			font(L"Tahoma", 18.0f),
			// font_associated(L"Tahoma", 14.0f), 12.0f for more detailed
			brushBackground( Gdiplus::Color(239,237,89) ),
			brushBackgroundSelected( Gdiplus::Color(198,228,249) ),
			brushBackgroundNotInGame( Gdiplus::Color(255,128,128) ),
			brushBackgroundAllowSelectionInGame( Gdiplus::Color(128,255,128) ),
			brushText( Gdiplus::Color(0,0,0) ),
			penBorder( Gdiplus::Color(0,0,0), 3.0f )
		{
			sf.SetAlignment( Gdiplus::StringAlignmentCenter );
		}
		~SAssets()
		{
		}

		Gdiplus::Font font;
		Gdiplus::SolidBrush brushBackground;
		Gdiplus::SolidBrush brushBackgroundSelected;
		Gdiplus::SolidBrush brushBackgroundNotInGame;
		Gdiplus::SolidBrush brushBackgroundAllowSelectionInGame;
		Gdiplus::SolidBrush brushText;
		Gdiplus::StringFormat sf;
		Gdiplus::Pen penBorder;
	};

	static SAssets * pAssets = 0;
}

void CHyperNodePainter_MultiIOBox::Paint( CHyperNode * pNode, CDisplayList * pList )
{
	if (!pAssets)
		pAssets = new SAssets();

	CDisplayRectangle * pBackground = pList->Add<CDisplayRectangle>()
		->SetHitEvent(eSOID_ShiftFirstOutputPort)
		->SetStroked(&pAssets->penBorder);

	CAGNode * pAGNode = (CAGNode*) pNode;
	CAGStatePtr pState = pAGNode->GetState();

	if (pNode->IsSelected())
		pBackground->SetFilled(&pAssets->brushBackgroundSelected);
	else if (!pState->IncludeInGame())
		pBackground->SetFilled( &pAssets->brushBackgroundNotInGame );
	else if (pState->AllowSelect())
		pBackground->SetFilled( &pAssets->brushBackgroundAllowSelectionInGame );
	else
		pBackground->SetFilled(&pAssets->brushBackground);

	CDisplayString * pString = pList->Add<CDisplayString>()
		->SetHitEvent(eSOID_ShiftFirstOutputPort)
		->SetText( pNode->GetClassName() )
		->SetBrush( &pAssets->brushText )
		->SetFont( &pAssets->font )
		->SetStringFormat( &pAssets->sf );

	Gdiplus::RectF rect = pString->GetBounds(pList->GetGraphics());
	rect.Width += 32.0f;
	rect.Height += 6.0f;
	rect.X = 0.0f;
	rect.Y = 0.0f;
	pBackground->SetRect( rect );
	pString->SetLocation( Gdiplus::PointF( rect.Width * 0.5f, 3.0f ) );

	pList->SetAttachRect( 0, rect );
	pList->SetAttachRect( 1000, rect );
}

void CHyperNodePainter_Image::Paint( CHyperNode * pNode, CDisplayList * pList )
{
	if (!pAssets)
		pAssets = new SAssets();

	CAGImageNode* pImageNode = static_cast<CAGImageNode*>(pNode);
	CAGStatePtr pState = pImageNode->GetState();

	CDisplayImage* pImage = pList->Add<CDisplayImage>()
		->SetImage(pImageNode->GetImage()->GetImage());

	CDisplayRectangle * pBackground = pList->Add<CDisplayRectangle>()
		->SetHitEvent(eSOID_ShiftFirstOutputPort)
		->SetStroked(&pAssets->penBorder);

	//if (pNode->IsSelected())
	//	pBackground->SetFilled(&pAssets->brushBackgroundSelected);
	//else if (!pState->IncludeInGame())
	//	pBackground->SetFilled( &pAssets->brushBackgroundNotInGame );
	//else if (pState->AllowSelect())
	//	pBackground->SetFilled( &pAssets->brushBackgroundAllowSelectionInGame );
	//else
	//	pBackground->SetFilled(&pAssets->brushBackground);

	int width = pImageNode->GetImage()->GetImage()->GetWidth();
	int height = pImageNode->GetImage()->GetImage()->GetHeight();
	Gdiplus::RectF rect(0, 0, width, height);
	pBackground->SetRect( rect );
	pImage->SetLocation(Gdiplus::PointF(0, 0));

	// ?
	pList->SetAttachRect( 0, rect );
	pList->SetAttachRect( 1000, rect );
}
