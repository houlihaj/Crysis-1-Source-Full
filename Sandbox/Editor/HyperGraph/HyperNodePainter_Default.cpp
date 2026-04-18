#include "StdAfx.h"
#include "HyperNodePainter_Default.h"

#define GET_GDI_COLOR(x) (Gdiplus::Color(GetRValue(x),GetGValue(x),GetBValue(x)))

#define NODE_BACKGROUND_COLOR  GET_GDI_COLOR(gSettings.hyperGraphColors.colorNodeBkg)
#define NODE_BACKGROUND_SELECTED_COLOR  GET_GDI_COLOR(gSettings.hyperGraphColors.colorNodeBkgSelected)
#define NODE_BACKGROUND_CUSTOM_COLOR1  Gdiplus::Color(240,170,170)
#define NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1  Gdiplus::Color(255,255,245)

//#define NODE_OUTLINE_COLOR     Gdiplus::Color(80,80,80)
#define NODE_OUTLINE_COLOR     GET_GDI_COLOR(gSettings.hyperGraphColors.colorNodeOutline)

//#define TITLE_TEXT_COLOR       Gdiplus::Color(255,255,255)
//#define TITLE_TEXT_SELECTED_COLOR Gdiplus::Color(255,255,0)
#define TITLE_TEXT_COLOR       GET_GDI_COLOR(gSettings.hyperGraphColors.colorText)
#define TITLE_TEXT_SELECTED_COLOR GET_GDI_COLOR(gSettings.hyperGraphColors.colorText)

#define PORT_BACKGROUND_COLOR  Gdiplus::Color(200,200,200)
#define PORT_BACKGROUND_SELECTED_COLOR  Gdiplus::Color(200,200,255)

#define PORT_TEXT_COLOR        GET_GDI_COLOR(gSettings.hyperGraphColors.colorText)
#define PORT_OUTLINE_COLOR     Gdiplus::Color(190,190,190)

#define GRIPPER_COLOR          Gdiplus::Color(140,140,180)
#define GRIPPER_OUTLINE_COLOR  Gdiplus::Color(180,180,180)

#define DOWN_ARROW_COLOR       Gdiplus::Color(0,0,0)

#define BITMAP_BACKGROUND_COLOR  Gdiplus::Color(180,180,180)

// Sizing.
#define TITLE_X_OFFSET 4
#define TITLE_Y_OFFSET 0
#define TITLE_HEIGHT 16
#define MINIMIZE_BOX_WIDTH 16
static const float MINIMIZE_BOX_MAX_HEIGHT = 12.0f;

#define PORT_DOWN_ARROW_WIDTH 20
#define PORT_DOWN_ARROW_HEIGHT 8
#define PORTS_OUTER_MARGIN 12
#define PORTS_INNER_MARGIN 12
#define PORTS_Y_SPACING 14

static COLORREF PortTypeToColor[] =
{
	0x0000FF00,
	0x00FF0000,
	0x000000FF,
	0x00FFFFFF,
	0x00FF00FF,
	0x00FFFF00,
	0x0000FFFF,
	0x007F00FF,
	0x007FFF7F,
	0x00FF7F00,
	0x0000FF7F,
	0x007F7F7F,
	0x00000000
};

static const int NumPortColors = sizeof(PortTypeToColor)/sizeof(*PortTypeToColor);

namespace
{

	struct SAssets
	{
		SAssets() :
			font(L"Tahoma", 8.0f,  Gdiplus::FontStyleRegular ),
			brushBackgroundSelected( NODE_BACKGROUND_SELECTED_COLOR ),
			brushBackgroundUnselected( NODE_BACKGROUND_COLOR ),
			brushBackgroundCustomSelected1( NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1 ),
			brushBackgroundCustomUnselected1( NODE_BACKGROUND_CUSTOM_COLOR1 ),
			penOutline( NODE_OUTLINE_COLOR ),
			colorTitleSelectedA(250,250,250),
			colorTitleSelectedB(180,180,120),
			colorTitleUnselectedA(220,220,220),
			colorTitleUnselectedB(160,160,165),
			brushTitleTextSelected( TITLE_TEXT_SELECTED_COLOR ),
			brushTitleTextUnselected( TITLE_TEXT_COLOR ),
			colorEntityInvalidA(255,200,200),
			colorEntityInvalidB(255,0,0),
			brushEntityTextValid( GET_GDI_COLOR(gSettings.hyperGraphColors.colorText) ),
			brushEntityTextInvalid( Gdiplus::Color(255,255,255) ),
			penPort( Gdiplus::Color(80,80,80) ),
			sfInputPort(),
			sfOutputPort(),
			brushPortText( GET_GDI_COLOR(gSettings.hyperGraphColors.colorText) ),
			brushPortSelected( Gdiplus::Color(200,200,255)),
			brushPortDebugActivated( PORT_DEBUGGING_COLOR ),
			brushMinimizeArrow( NODE_OUTLINE_COLOR ),
			penDownArrow(DOWN_ARROW_COLOR)
		{
			colorEntityValidA[0] = Gdiplus::Color(255,255,255);
			colorEntityValidB[0] = Gdiplus::Color(0,220,0);
			colorEntityValidA[1] = Gdiplus::Color(255,255,255);
			colorEntityValidB[1] = Gdiplus::Color(0,150,200);
			colorEntityValidA[2] = Gdiplus::Color(255,255,255);
			colorEntityValidB[2] = Gdiplus::Color(0,120,255);

			sfInputPort.SetAlignment( Gdiplus::StringAlignmentNear );
			sfOutputPort.SetAlignment( Gdiplus::StringAlignmentFar );

			for (int i=0; i<NumPortColors; i++)
			{
				Gdiplus::Point p1(0,0);
				Gdiplus::Point p2(16,0);
				Gdiplus::Color col2 = PortTypeToColor[i] | 0xff000000u;
				Gdiplus::Color col1( col2.GetR()/2,col2.GetG()/2,col2.GetB()/2 );
				brushPortFill[i] = new Gdiplus::LinearGradientBrush ( p1,p2,col1,col2 );
			}
		}
		~SAssets()
		{
			for (int i=0; i<NumPortColors; i++)
				delete brushPortFill[i];
		}

		void Update()
		{
			brushBackgroundSelected.SetColor( NODE_BACKGROUND_SELECTED_COLOR );
			brushBackgroundUnselected.SetColor( NODE_BACKGROUND_COLOR );
			brushBackgroundCustomSelected1.SetColor( NODE_BACKGROUND_CUSTOM_SELECTED_COLOR1 );
			brushBackgroundCustomUnselected1.SetColor( NODE_BACKGROUND_CUSTOM_COLOR1 );

			penOutline.SetColor( NODE_OUTLINE_COLOR );
			brushTitleTextSelected.SetColor( TITLE_TEXT_SELECTED_COLOR );
			brushTitleTextUnselected.SetColor( TITLE_TEXT_COLOR );
			brushEntityTextValid.SetColor( GET_GDI_COLOR(gSettings.hyperGraphColors.colorText) );
			brushPortText.SetColor( GET_GDI_COLOR(gSettings.hyperGraphColors.colorText) );
			brushMinimizeArrow.SetColor( NODE_OUTLINE_COLOR );
			penDownArrow.SetColor(DOWN_ARROW_COLOR);
		}

		Gdiplus::Font font;
		Gdiplus::SolidBrush brushBackgroundSelected;
		Gdiplus::SolidBrush brushBackgroundUnselected;
		Gdiplus::SolidBrush brushBackgroundCustomSelected1;
		Gdiplus::SolidBrush brushBackgroundCustomUnselected1;
		Gdiplus::Pen penOutline;
		Gdiplus::Color colorTitleSelectedA;
		Gdiplus::Color colorTitleSelectedB;
		Gdiplus::Color colorTitleUnselectedA;
		Gdiplus::Color colorTitleUnselectedB;
		Gdiplus::SolidBrush brushTitleTextSelected;
		Gdiplus::SolidBrush brushTitleTextUnselected;
		Gdiplus::Color colorEntityValidA[3];
		Gdiplus::Color colorEntityValidB[3];
		Gdiplus::Color colorEntityInvalidA;
		Gdiplus::Color colorEntityInvalidB;
		Gdiplus::SolidBrush brushEntityTextValid;
		Gdiplus::SolidBrush brushEntityTextInvalid;
		Gdiplus::Pen penPort;
		Gdiplus::Pen penDownArrow;
		Gdiplus::StringFormat sfInputPort;
		Gdiplus::StringFormat sfOutputPort;
		Gdiplus::SolidBrush brushPortText;
		Gdiplus::SolidBrush brushPortSelected;
		Gdiplus::SolidBrush brushPortDebugActivated;
		Gdiplus::SolidBrush brushMinimizeArrow;
		Gdiplus::Brush* brushPortFill[NumPortColors];
	};

}

struct SDefaultRenderPort
{
	SDefaultRenderPort() : pPortArrow(0), pRectangle(0), pText(0), pBackground(0) {}
	CDisplayPortArrow *pPortArrow;
	CDisplayRectangle * pRectangle;
	CDisplayString * pText;
	CDisplayRectangle * pBackground;
	int id;
};

void CHyperNodePainter_Default::Paint( CHyperNode * pNode, CDisplayList * pList )
{
	static SAssets * pAssets = 0;
	struct IHyperGraph *pPrevGraph = 0;

	if (!pAssets)
		pAssets = new SAssets();

	pAssets->Update();

	// background
	CDisplayRectangle * pBackgroundRect = pList->Add<CDisplayRectangle>()
		->SetStroked( &pAssets->penOutline )
		->SetHitEvent( eSOID_Background );

	// node title
	CDisplayGradientRectangle * pTitleRect = pList->Add<CDisplayGradientRectangle>()
		->SetStroked( &pAssets->penOutline )
		->SetHitEvent( eSOID_Title );
	CDisplayMinimizeArrow * pMinimizeArrow = pList->Add<CDisplayMinimizeArrow>()
		->SetStroked( &pAssets->penOutline )
		->SetFilled( &pAssets->brushMinimizeArrow )
		->SetHitEvent( eSOID_Minimize );
	CString title = pNode->GetTitle();
	if (gSettings.bFlowGraphShowNodeIDs)
		title.AppendFormat("\nID=%d", pNode->GetId());

	CDisplayString * pTitleString = pList->Add<CDisplayString>()
		->SetFont( &pAssets->font )
		->SetText( title )
		->SetHitEvent( eSOID_Title );
	
	// selected/unselected fill
	if (pNode->IsSelected())
	{
		if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
			pBackgroundRect->SetFilled( &pAssets->brushBackgroundCustomSelected1 );
		else
			pBackgroundRect->SetFilled( &pAssets->brushBackgroundSelected );
		pTitleRect->SetColors( pAssets->colorTitleSelectedA, /*pAssets->colorTitleSelectedB*/ pNode->GetCategoryColor() );
		pTitleString->SetBrush( &pAssets->brushTitleTextSelected );
	}
	else
	{
		if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
			pBackgroundRect->SetFilled( &pAssets->brushBackgroundCustomUnselected1 );
		else
			pBackgroundRect->SetFilled( &pAssets->brushBackgroundUnselected );
		pTitleRect->SetColors( pAssets->colorTitleUnselectedA, /*pAssets->colorTitleUnselectedB*/ pNode->GetCategoryColor() );
		pTitleString->SetBrush( &pAssets->brushTitleTextUnselected );
	}

	bool bAnyHiddenInput = false;
	bool bAnyHiddenOutput = false;

	bool bEntityInputPort = false;
	bool bEntityPortConnected = false;

	// attached entity?
	CDisplayGradientRectangle * pEntityRect = 0;
	CDisplayString * pEntityString = 0;
	CDisplayPortArrow * pEntityEllipse = 0;
	if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
	{
		bEntityInputPort = true;
		pEntityRect = pList->Add<CDisplayGradientRectangle>()
			->SetStroked( &pAssets->penOutline )
			->SetHitEvent( eSOID_FirstInputPort );
		
		pEntityEllipse = pList->Add<CDisplayPortArrow>()
			->SetFilled( pAssets->brushPortFill[IVariable::UNKNOWN] )
			->SetHitEvent( eSOID_FirstInputPort )
			->SetStroked( &pAssets->penPort );
		pEntityString = pList->Add<CDisplayString>()
			->SetHitEvent( eSOID_FirstInputPort )
			->SetFont( &pAssets->font )
			->SetText( pNode->GetEntityTitle() );

		if (pNode->GetInputs() && (*pNode->GetInputs())[0].nConnected != 0)
			bEntityPortConnected = true;

		if (pNode->CheckFlag(EHYPER_NODE_ENTITY_VALID) || 
			pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY) || 
			pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY2) || 
			bEntityPortConnected)
		{
			if (bEntityPortConnected)
				pEntityRect->SetColors( pAssets->colorEntityValidA[0], pAssets->colorEntityValidB[0] );
			else if (pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY))
				pEntityRect->SetColors( pAssets->colorEntityValidA[1], pAssets->colorEntityValidB[1] );
			else if (pNode->CheckFlag(EHYPER_NODE_GRAPH_ENTITY2))
				pEntityRect->SetColors( pAssets->colorEntityValidA[2], pAssets->colorEntityValidB[2] );
			else
				pEntityRect->SetColors( pAssets->colorEntityValidA[0], pAssets->colorEntityValidB[0] );
			pEntityString->SetBrush( &pAssets->brushEntityTextValid );
		}
		else
		{
			pEntityRect->SetColors( pAssets->colorEntityInvalidA, pAssets->colorEntityInvalidB );
			pEntityString->SetBrush( &pAssets->brushEntityTextInvalid );
		}
	}

	std::vector<SDefaultRenderPort> inputPorts;
	std::vector<SDefaultRenderPort> outputPorts;
	const CHyperNode::Ports& inputs = *pNode->GetInputs();
	const CHyperNode::Ports& outputs = *pNode->GetOutputs();

	inputPorts.reserve(inputs.size());
	outputPorts.reserve(outputs.size());

	// input ports
	for (size_t i=0; i<inputs.size(); i++)
	{
		if (bEntityInputPort && i == 0)
			continue;
		const CHyperNodePort& pp = inputs[i];
		if (pp.bVisible || pp.nConnected != 0)
		{
			SDefaultRenderPort pr;

			Gdiplus::Brush * pBrush = pAssets->brushPortFill[pp.pVar->GetType()];

			if (pp.bSelected)
			{
				pr.pBackground = pList->Add<CDisplayRectangle>()
					->SetFilled( &pAssets->brushPortSelected )
					->SetHitEvent( eSOID_FirstInputPort + i );
			}
			else if(pNode->IsPortActivationModified(&pp))
			{
				pr.pBackground = pList->Add<CDisplayRectangle>()
					->SetFilled( &pAssets->brushPortDebugActivated )
					->SetHitEvent( eSOID_FirstInputPort + i );
			}

			if (pp.bAllowMulti)
			{
				pr.pRectangle = pList->Add<CDisplayRectangle>()
					->SetFilled( pBrush )
					->SetHitEvent( eSOID_FirstInputPort + i )
					->SetStroked( &pAssets->penPort );
			}
			else
			{
				pr.pPortArrow = pList->Add<CDisplayPortArrow>()
					->SetFilled( pBrush )
					->SetHitEvent( eSOID_FirstInputPort + i )
					->SetStroked( &pAssets->penPort );
			}
			pr.pText = pList->Add<CDisplayString>()
				->SetFont( &pAssets->font )
				->SetStringFormat( &pAssets->sfInputPort )
				->SetBrush( &pAssets->brushPortText )
				->SetHitEvent( eSOID_FirstInputPort + i )
				->SetText( pNode->IsPortActivationModified()?pNode->GetDebugPortValue(pp):pNode->GetPortName( pp ) );
			pr.id = i;
			inputPorts.push_back(pr);
		}
		else
			bAnyHiddenInput = true;
	}
	// output ports
	for (size_t i=0; i<outputs.size(); i++)
	{
		const CHyperNodePort& pp = outputs[i];
		if (pp.bVisible || pp.nConnected != 0)
		{
			SDefaultRenderPort pr;

			Gdiplus::Brush * pBrush = pAssets->brushPortFill[pp.pVar->GetType()];

			if (pp.bSelected)
			{
				pr.pBackground = pList->Add<CDisplayRectangle>()
					->SetFilled( &pAssets->brushPortSelected )
					->SetHitEvent( eSOID_FirstOutputPort + i );
			}
			if (pp.bAllowMulti)
			{
				pr.pRectangle = pList->Add<CDisplayRectangle>()
					->SetFilled( pBrush )
					->SetHitEvent( eSOID_FirstOutputPort + i )
					->SetStroked( &pAssets->penPort );
			}
			else
			{
				pr.pPortArrow = pList->Add<CDisplayPortArrow>()
					->SetFilled( pBrush )
					->SetHitEvent( eSOID_FirstOutputPort + i )
					->SetStroked( &pAssets->penPort );
			}

			pr.pText = pList->Add<CDisplayString>()
				->SetFont( &pAssets->font )
				->SetStringFormat( &pAssets->sfOutputPort )
				->SetBrush( &pAssets->brushPortText )
				->SetHitEvent( eSOID_FirstOutputPort + i )
				->SetText( pNode->GetPortName( pp ) );
			pr.id = 1000+i;
			outputPorts.push_back(pr);
		}
		else
			bAnyHiddenOutput = true;
	}

	// calculate size now
	Gdiplus::SizeF size(0,0);

	Gdiplus::Graphics * pG = pList->GetGraphics();
	Gdiplus::RectF rect = pTitleString->GetTextBounds(pG);
	float titleHeight = rect.Height;
	float curY = rect.Height;
	float width = rect.Width + MINIMIZE_BOX_WIDTH; // Add minimize box size.
	float entityHeight = 0;
	Gdiplus::RectF entityTextRect = pTitleString->GetTextBounds(pG);
	if (pEntityString)
	{
		entityTextRect = pEntityString->GetTextBounds(pG);
		entityTextRect.Offset( 0,curY );
		float entityWidth = entityTextRect.Width + PORTS_OUTER_MARGIN;
		width = max(width, entityWidth);
		curY += entityTextRect.Height;
		entityHeight = entityTextRect.Height;
	}
	float portStartY = curY;
	float inputsWidth = 0.0f;
	for (size_t i=0; i<inputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = inputPorts[i];
		Gdiplus::PointF textLoc(Gdiplus::PointF(PORTS_OUTER_MARGIN, curY));
		p.pText->SetLocation( textLoc );
		rect = p.pText->GetBounds(pG);
		curY += rect.Height;
		inputsWidth = max(inputsWidth, rect.Width);
		rect.Height -= 4.0f;
		rect.Y += 2.0f;
		rect.Width = rect.Height;
		rect.X = 2.0f;
		if (p.pPortArrow)
			p.pPortArrow->SetRect( rect );
		if (p.pRectangle)
			p.pRectangle->SetRect( rect );
		pList->SetAttachRect( p.id, Gdiplus::RectF(0, rect.Y+rect.Height*0.5f, 0.0f, 0.0f) );
	}
	curY = portStartY;
	for (size_t i=0; i<inputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = inputPorts[i];
		rect = p.pText->GetBounds(pG);
		if (p.pBackground)
			p.pBackground->SetRect( 1, curY, PORTS_OUTER_MARGIN+rect.Width, rect.Height );
		curY += rect.Height;
	}
	float height = curY;
	float outputsWidth = 0.0f;
	for (size_t i=0; i<outputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = outputPorts[i];
		outputsWidth = max(outputsWidth, p.pText->GetBounds(pG).Width);
	}
	width = max(width, inputsWidth + outputsWidth + 3*PORTS_OUTER_MARGIN);
	curY = portStartY;
	for (size_t i=0; i<outputPorts.size(); i++)
	{
		const SDefaultRenderPort& p = outputPorts[i];
		Gdiplus::PointF textLoc(width - PORTS_OUTER_MARGIN, curY);
		p.pText->SetLocation( textLoc );
		rect = p.pText->GetBounds(pG);
		if (p.pBackground)
			p.pBackground->SetRect( width-PORTS_OUTER_MARGIN-rect.Width-3, curY, PORTS_OUTER_MARGIN+rect.Width+3, rect.Height );
		curY += rect.Height;
		rect.Height -= 4.0f;
		rect.Width = rect.Height;
		rect.Y += 2.0f;
		rect.X = width - PORTS_OUTER_MARGIN;
		if (p.pPortArrow)
			p.pPortArrow->SetRect( rect );
		if (p.pRectangle)
			p.pRectangle->SetRect( rect );
		pList->SetAttachRect( p.id, Gdiplus::RectF(width, rect.Y+rect.Height*0.5f, 0.0f, 0.0f));
	}
	height = max(height, curY);

	// down arrows if any ports are hidden.
	if (bAnyHiddenInput || bAnyHiddenOutput)
	{
		height += 1;
		// Draw hidden ports gripper.
		if (bAnyHiddenInput)
		{
			CDisplayRectangle *pGrip = pList->Add<CDisplayRectangle>()
				->SetHitEvent( eSOID_InputGripper );

			Gdiplus::RectF rect = Gdiplus::RectF(1, height, PORT_DOWN_ARROW_WIDTH, PORT_DOWN_ARROW_HEIGHT);
			//pGrip->SetColors( pAssets->colorTitleUnselectedA, pAssets->colorTitleUnselectedB );
			if (pNode->IsSelected())
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled( &pAssets->brushBackgroundCustomSelected1 );
				else
					pGrip->SetFilled( &pAssets->brushBackgroundSelected );
			}
			else
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled( &pAssets->brushBackgroundCustomUnselected1 );
				else
					pGrip->SetFilled( &pAssets->brushBackgroundUnselected );
			}
			pGrip->SetRect( rect );

			// Draw arrow.
			AddDownArrow( pList,Gdiplus::PointF( rect.X+rect.Width/2,(rect.Y+rect.GetBottom())/2 + 3 ),&pAssets->penDownArrow );
		}
		if (bAnyHiddenOutput)
		{
			CDisplayRectangle *pGrip = pList->Add<CDisplayRectangle>()
				->SetHitEvent( eSOID_OutputGripper );

			Gdiplus::RectF rect = Gdiplus::RectF(width-PORT_DOWN_ARROW_WIDTH-1, height, PORT_DOWN_ARROW_WIDTH, PORT_DOWN_ARROW_HEIGHT);
			//pGrip->SetColors( pAssets->colorTitleUnselectedA, pAssets->colorTitleUnselectedB );
			pGrip->SetRect( rect );
			if (pNode->IsSelected())
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled( &pAssets->brushBackgroundCustomSelected1 );
				else
					pGrip->SetFilled( &pAssets->brushBackgroundSelected );
			}
			else
			{
				if (pNode->CheckFlag(EHYPER_NODE_CUSTOM_COLOR1))
					pGrip->SetFilled( &pAssets->brushBackgroundCustomUnselected1 );
				else
					pGrip->SetFilled( &pAssets->brushBackgroundUnselected );
			}


			// Draw arrow.
			AddDownArrow( pList,Gdiplus::PointF( rect.X+rect.Width/2,(rect.Y+rect.GetBottom())/2 + 3 ),&pAssets->penDownArrow );
		}

		height += PORT_DOWN_ARROW_HEIGHT+1;
	}

	// resize backing boxes...
	pTitleString->SetRect( 0, 0, width-MINIMIZE_BOX_WIDTH, titleHeight );
	pTitleRect->SetRect( 0, 0, width, titleHeight );
	pMinimizeArrow->SetRect( width-(MINIMIZE_BOX_WIDTH-2),2,12, min(MINIMIZE_BOX_MAX_HEIGHT, titleHeight-4) );
	curY = titleHeight;
	if (pEntityRect)
	{
		pEntityString->SetRect( PORTS_OUTER_MARGIN,curY,width-PORTS_OUTER_MARGIN,entityTextRect.Height );
		pEntityRect->SetRect( 0,curY,width,entityTextRect.Height );
		pEntityEllipse->SetRect( 2,curY+2,entityTextRect.Height-4,entityTextRect.Height-4 );
		pList->SetAttachRect( 0, Gdiplus::RectF(0, curY+entityHeight*0.5f, 0.0f, 0.0f) );
		curY += entityHeight;
	}
	pBackgroundRect->SetRect( 0, curY, width, height-curY );
}

//////////////////////////////////////////////////////////////////////////
void CHyperNodePainter_Default::AddDownArrow( CDisplayList * pList,const Gdiplus::PointF &pnt,Gdiplus::Pen *pPen )
{
	Gdiplus::PointF p = pnt;
	p.Y += 0;
	pList->AddLine( Gdiplus::PointF(p.X,p.Y),Gdiplus::PointF(p.X,p.Y),pPen );
	pList->AddLine( Gdiplus::PointF(p.X-1,p.Y-1),Gdiplus::PointF(p.X+1,p.Y-1),pPen );
	pList->AddLine( Gdiplus::PointF(p.X-2,p.Y-2),Gdiplus::PointF(p.X+2,p.Y-2),pPen );
	pList->AddLine( Gdiplus::PointF(p.X-3,p.Y-3),Gdiplus::PointF(p.X+3,p.Y-3),pPen );
	pList->AddLine( Gdiplus::PointF(p.X-4,p.Y-4),Gdiplus::PointF(p.X+4,p.Y-4),pPen );

	/*
	pList->AddLine( p,Gdiplus::PointF(p.X-5,p.Y-5),pPen );
	pList->AddLine( p,Gdiplus::PointF(p.X+5,p.Y-5),pPen );
	p.Y -= 3;
	pList->AddLine( p,Gdiplus::PointF(p.X-5,p.Y-5),pPen );
	pList->AddLine( p,Gdiplus::PointF(p.X+5,p.Y-5),pPen );
	*/
}
