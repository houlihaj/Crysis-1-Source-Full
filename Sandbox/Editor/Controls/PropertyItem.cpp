////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   propertyitem.cpp
//  Version:     v1.00
//  Created:     5/6/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

/*
	Relationship between Variable data, PropertyItem value string, and draw string
	
		Variable: 
			holds raw data
		PropertyItem.m_value string: 
			Converted via Variable.Get/SetDisplayValue
			= Same as Variable.Get(string) and Variable.Set(string), EXCEPT:
				Enums converted to enum values
				Color reformated to 8-bit range
				SOHelper removes prefix before :
		Draw string:
			Same as .m_value, EXCEPT
				.valueMultiplier conversion applied
				Bool reformated to "True/False"
				Curves replaced with "[Curve]"
				bShowChildren vars add child draw strings
		Input string:
			Set to .m_value, EXCEPT
				Bool converted to "1/0"
				File text replace \ with /
				Vector expand single elem to 3 repeated
*/

#include "StdAfx.h"
#include "PropertyItem.h"
#include "PropertyCtrl.h"
#include "InPlaceEdit.h"
#include "InPlaceComboBox.h"
#include "InPlaceButton.h"
#include "FillSliderCtrl.h"
#include "SplineCtrl.h"
#include "ColorGradientCtrl.h"
#include "SliderCtrlEx.h"

#include "EquipPackDialog.h"
#include "SelectEAXPresetDlg.h"

#include "ShaderEnum.h"
#include "ShadersDialog.h"
#include "CustomColorDialog.h"

// AI
#include "AIDialog.h"
#include "AICharactersDialog.h"
#include "AIAnchorsDialog.h"
#include "AI\AIBehaviorLibrary.h"
#include "AI\AIGoalLibrary.h"

// Smart Objects
#include "SmartObjectClassDialog.h"
#include "SmartObjectStateDialog.h"
#include "SmartObjectPatternDialog.h"
#include "SmartObjectActionDialog.h"
#include "SmartObjectHelperDialog.h"
#include "SmartObjectEventDialog.h"
#include "SmartObjectTemplateDialog.h"

#include "Material/MaterialManager.h"

#include "SelectGameTokenDialog.h"
#include "GameTokens/GameTokenManager.h"
#include "UIEnumsDatabase.h"
#include "SelectSequenceDialog.h"
#include "SelectMissionObjectiveDialog.h"
#include "GenericSelectItemDialog.h"

#include <ITimer.h>

//////////////////////////////////////////////////////////////////////////
#define CMD_ADD_CHILD_ITEM 100
#define CMD_ADD_ITEM 101
#define CMD_DELETE_ITEM 102

#define BUTTON_WIDTH (16)
#define NUMBER_CTRL_WIDTH 60

//////////////////////////////////////////////////////////////////////////
//! Undo object for Variable in property control..
class CUndoVariableChange : public IUndoObject
{
public:
	CUndoVariableChange( IVariable *var,const char *undoDescription )
	{
		// Stores the current state of this object.
		assert( var != 0 );
		m_undoDescription = undoDescription;
		m_var = var;
		m_undo = m_var->Clone(false);
	}
protected:
	virtual int GetSize() { 
		int size = sizeof(*this);
		//if (m_var)
			//size += m_var->GetSize();
		if (m_undo)
			size += m_undo->GetSize();
		if (m_redo)
			size += m_redo->GetSize();
		return size;
	}
	virtual const char* GetDescription() { return m_undoDescription; };
	virtual void Undo( bool bUndo )
	{
		if (bUndo)
		{
			m_redo = m_var->Clone(false);
		}
		m_var->CopyValue( m_undo );
	}
	virtual void Redo()
	{
		if (m_redo)
			m_var->CopyValue(m_redo);
	}

private:
	CString m_undoDescription;
	TSmartPtr<IVariable> m_undo;
	TSmartPtr<IVariable> m_redo;
	TSmartPtr<IVariable> m_var;
};

//////////////////////////////////////////////////////////////////////////
namespace {
struct {
	int dataType;
	const char *name;
	PropertyType type;
	int image;
} s_propertyTypeNames[] =
{
	{ IVariable::DT_SIMPLE,"Bool",ePropertyBool,2 },
	{ IVariable::DT_SIMPLE,"Int",ePropertyInt,0 },
	{ IVariable::DT_SIMPLE,"Float",ePropertyFloat,0 },
	{ IVariable::DT_SIMPLE,"Vector",ePropertyVector,10 },
	{ IVariable::DT_SIMPLE,"String",ePropertyString,3 },
	{ IVariable::DT_PERCENT,"Float",ePropertyInt,13 },
	{ IVariable::DT_COLOR,"Color",ePropertyColor,1 },
	{ IVariable::DT_CURVE|IVariable::DT_PERCENT,"FloatCurve",ePropertyFloatCurve,13 },
	{ IVariable::DT_CURVE|IVariable::DT_COLOR,"ColorCurve",ePropertyColorCurve,1 },
	{ IVariable::DT_ANGLE,"Angle",ePropertyAngle,0 },
	{ IVariable::DT_FILE,"File",ePropertyFile,7 },
	{ IVariable::DT_TEXTURE,"Texture",ePropertyTexture,4 },
	{ IVariable::DT_SOUND,"Sound",ePropertySound,6 },
	{ IVariable::DT_OBJECT,"Model",ePropertyModel,5 },
	{ IVariable::DT_SIMPLE,"Selection",ePropertySelection,-1 },
	{ IVariable::DT_SIMPLE,"List",ePropertyList,-1 },
	{ IVariable::DT_SHADER,"Shader",ePropertyShader,9 },
	{ IVariable::DT_MATERIAL,"Material",ePropertyMaterial,14 },
	{ IVariable::DT_AI_BEHAVIOR,"AIBehavior",ePropertyAiBehavior,8 },
	{ IVariable::DT_AI_ANCHOR,"AIAnchor",ePropertyAiAnchor,8 },
	{ IVariable::DT_AI_CHARACTER,"AICharacter",ePropertyAiCharacter,8 },
	{ IVariable::DT_EQUIP,"Equip",ePropertyEquip,11 },
	{ IVariable::DT_REVERBPRESET,"ReverbPreset",ePropertyReverbPreset,11 },
	{ IVariable::DT_LOCAL_STRING,"LocalString",ePropertyLocalString,3 },
	{ IVariable::DT_SOCLASS,"Smart Object Class",ePropertySOClass,8 },
	{ IVariable::DT_SOCLASSES,"Smart Object Classes",ePropertySOClasses,8 },
	{ IVariable::DT_SOSTATE,"Smart Object State",ePropertySOState,8 },
	{ IVariable::DT_SOSTATES,"Smart Object States",ePropertySOStates,8 },
	{ IVariable::DT_SOSTATEPATTERN,"Smart Object State Pattern",ePropertySOStatePattern,8 },
	{ IVariable::DT_SOACTION,"AI Action",ePropertySOAction,8 },
	{ IVariable::DT_SOHELPER,"Smart Object Helper",ePropertySOHelper,8 },
	{ IVariable::DT_SONAVHELPER,"Smart Object Navigation Helper",ePropertySONavHelper,8 },
	{ IVariable::DT_SOEVENT,"Smart Object Event",ePropertySOEvent,8 },
	{ IVariable::DT_SOTEMPLATE,"Smart Object Template",ePropertySOTemplate,8 },
	{ IVariable::DT_VEEDHELPER,"Vehicle Helper",ePropertySelection,-1 },
	{ IVariable::DT_VEEDPART,"Vehicle Part",ePropertySelection,-1 },
  { IVariable::DT_VEEDCOMP,"Vehicle Component",ePropertySelection,-1 },
	{ IVariable::DT_GAMETOKEN,"Game Token",ePropertyGameToken, -1 },
	{ IVariable::DT_SEQUENCE,"Sequence",ePropertySequence, -1 },
	{ IVariable::DT_MISSIONOBJ,"Mission Objective",ePropertyMissionObj, -1 },
	{ IVariable::DT_USERITEMCB,"User",ePropertyUser, -1 }
};
static int NumPropertyTypes = sizeof(s_propertyTypeNames)/sizeof(s_propertyTypeNames[0]);

const char* DISPLAY_NAME_ATTR	= "DisplayName";
const char* VALUE_ATTR = "Value";
const char* TYPE_ATTR	=	"Type";
const char* TIP_ATTR	= "Tip";
const char* FILEFILTER_ATTR = "FileFilters";
};

//////////////////////////////////////////////////////////////////////////
// CPropertyItem implementation.
//////////////////////////////////////////////////////////////////////////
CPropertyItem::CPropertyItem( CPropertyCtrl* pCtrl )
{
	m_propertyCtrl = pCtrl;
	m_parent = 0;
	m_bExpandable = false;
	m_bExpanded = false;
	m_bEditChildren = false;
	m_bShowChildren = false;
	m_bSelected = false;
	m_bNoCategory = false;

	m_pStaticText = 0;
	m_cNumber = 0;
	m_cNumber1 = 0;
	m_cNumber2 = 0;
	m_cFillSlider = 0;
	m_cEdit = 0;
	m_cCombo = 0;
	m_cButton = 0;
	m_cButton2 = 0;
	m_cExpandButton = 0;
	m_cCheckBox = 0;
	m_cSpline = 0;
	m_cColorSpline = 0;
	m_cColorButton = 0;

	m_image = -1;
	m_bIgnoreChildsUpdate = false;
	m_value = "";
	m_type = ePropertyInvalid;
	
	m_modified = false;
	m_bMoveControls = false;
	m_lastModified = 0;
	m_valueMultiplier = 1;
	m_pEnumDBItem = 0;

	m_nHeight = 14;

	m_nCategoryPageId = -1;
}

CPropertyItem::~CPropertyItem()
{
	// just to make sure we dont double (or infinitely recurse...) delete
	AddRef();

	DestroyInPlaceControl();

	if (m_pVariable)
		ReleaseVariable();

	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->m_parent = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetXmlNode( XmlNodeRef &node )
{
	m_node = node;
	// No Undo while just creating properties.
	//GetIEditor()->SuspendUndo();
	ParseXmlNode();
	//GetIEditor()->ResumeUndo();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ParseXmlNode( bool bRecursive/* =true  */)
{
	if (!m_node->getAttr( DISPLAY_NAME_ATTR,m_name ))
	{
		m_name = m_node->getTag();
	}

	CString value;
	bool bHasValue=m_node->getAttr( VALUE_ATTR,value );

	CString type;
	m_node->getAttr( TYPE_ATTR,type);

	m_tip = "";
	m_node->getAttr( TIP_ATTR,m_tip );

	m_image = -1;
	m_type = ePropertyInvalid;
	for (int i = 0; i < NumPropertyTypes; i++)
	{
		if (stricmp(type,s_propertyTypeNames[i].name) == 0)
		{
			m_type = s_propertyTypeNames[i].type;
			m_image = s_propertyTypeNames[i].image;
			break;
		}
	}

	m_rangeMin = 0;
	m_rangeMax = 100;
	m_bHardMin = m_bHardMax = false;
	m_iInternalPrecision=2;																			// m.m.
	if (m_type == ePropertyFloat || m_type == ePropertyInt)
	{
		if (m_node->getAttr( "Min",m_rangeMin ))
			m_bHardMin = true;
		if (m_node->getAttr( "Max",m_rangeMax ))
			m_bHardMax = true;
		m_node->getAttr( "Precision",m_iInternalPrecision );			// m.m.
	}

	if (bHasValue)
		SetValue( value,false );

	m_bNoCategory = false;

	if (m_type == ePropertyVector && !m_propertyCtrl->IsExtenedUI())
	{
		bRecursive = false;

		m_childs.clear();
		Vec3 vec;
		m_node->getAttr( VALUE_ATTR,vec );
		// Create 3 sub elements.
		XmlNodeRef x = m_node->createNode( "X" );
		XmlNodeRef y = m_node->createNode( "Y" );
		XmlNodeRef z = m_node->createNode( "Z" );
		x->setAttr( TYPE_ATTR,"Float" );
		y->setAttr( TYPE_ATTR,"Float" );
		z->setAttr( TYPE_ATTR,"Float" );

		x->setAttr( VALUE_ATTR,vec.x );
		y->setAttr( VALUE_ATTR,vec.y );
		z->setAttr( VALUE_ATTR,vec.z );

		// Start ignoring all updates comming from childs. (Initializing childs).
		m_bIgnoreChildsUpdate = true;

		CPropertyItemPtr itemX = new CPropertyItem( m_propertyCtrl );
		itemX->SetXmlNode( x );
		AddChild( itemX );

		CPropertyItemPtr itemY = new CPropertyItem( m_propertyCtrl );
		itemY->SetXmlNode( y );
		AddChild( itemY );

		CPropertyItemPtr itemZ = new CPropertyItem( m_propertyCtrl );
		itemZ->SetXmlNode( z );
		AddChild( itemZ );
		
		m_bNoCategory = true;
		m_bExpandable = true;

		m_bIgnoreChildsUpdate = false;
	}
	else if (bRecursive)
	{
		// If recursive and not vector.

		m_bExpandable = false;
		// Create sub-nodes.
		for (int i=0; i < m_node->getChildCount(); i++)
		{
			m_bIgnoreChildsUpdate = true;

			XmlNodeRef child = m_node->getChild(i);
			CPropertyItemPtr item = new CPropertyItem( m_propertyCtrl );
			item->SetXmlNode( m_node->getChild(i) );
			AddChild( item );
			m_bExpandable = true;

			m_bIgnoreChildsUpdate = false;
		}
	}
	m_modified = false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetVariable( IVariable *var )
{
	if (var == m_pVariable)
		return;

	_smart_ptr<IVariable> pInputVar = var;

	// Release previous variable.
	if (m_pVariable)
		ReleaseVariable();

	m_pVariable = pInputVar;
	assert( m_pVariable != NULL );

	m_pVariable->AddOnSetCallback( functor(*this,&CPropertyItem::OnVariableChange) );

	m_tip = "";
	m_name = m_pVariable->GetHumanName();

	int dataType = m_pVariable->GetDataType();

	m_image = -1;
	m_type = ePropertyInvalid;
	int i;

	if (dataType != IVariable::DT_SIMPLE)
	{
		for (i = 0; i < NumPropertyTypes; i++)
		{
			if (dataType == s_propertyTypeNames[i].dataType)
			{
				m_type = s_propertyTypeNames[i].type;
				m_image = s_propertyTypeNames[i].image;
				break;
			}
		}
	}

	m_enumList = m_pVariable->GetEnumList();
	if (m_enumList != NULL)
	{
		m_type = ePropertySelection;
	}

	if (m_type == ePropertyInvalid)
	{
		switch(m_pVariable->GetType()) 
		{
		case IVariable::INT:
			m_type = ePropertyInt;
			break;
		case IVariable::BOOL:
			m_type = ePropertyBool;
			break;
		case IVariable::FLOAT:
			m_type = ePropertyFloat;
			break;
		case IVariable::VECTOR:
			m_type = ePropertyVector;
			break;
		case IVariable::STRING:
			m_type = ePropertyString;
			break;
		}
		for (i = 0; i < NumPropertyTypes; i++)
		{
			if (m_type == s_propertyTypeNames[i].type)
			{
				m_image = s_propertyTypeNames[i].image;
				break;
			}
		}
	}

	m_valueMultiplier = 1;
	m_rangeMin = 0;
	m_rangeMax = 100;
	m_bHardMin = m_bHardMax = false;

	// Get variable limits.
	m_pVariable->GetLimits( m_rangeMin, m_rangeMax, m_bHardMin, m_bHardMax );

	// Check if value is percents.
	if (dataType == IVariable::DT_PERCENT)
	{
		// Scale all values by 100.
		m_valueMultiplier = 100;
	}
	else if (dataType == IVariable::DT_ANGLE)
	{
		// Scale radians to degrees.
		m_valueMultiplier = RAD2DEG(1);
		m_rangeMin = -360;
		m_rangeMax =  360;
	}
	else if (dataType == IVariable::DT_UIENUM)
	{
		m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum(m_name);
	}

	// Set precision to allow >= 1000 steps.
	m_iInternalPrecision = max(3 - int(log(m_rangeMax - m_rangeMin) / log(10.f)), 0);
	
	//////////////////////////////////////////////////////////////////////////
	VarToValue();

	m_bNoCategory = false;

	if (m_type == ePropertyVector && !m_propertyCtrl->IsExtenedUI())
	{
		m_bEditChildren = true;
		m_childs.clear();

		Vec3 vec;
		m_pVariable->Get( vec );
		IVariable *pVX = new CVariable<float>;
		pVX->SetName( "x" );
		pVX->Set( vec.x );
		IVariable *pVY = new CVariable<float>;
		pVY->SetName( "y" );
		pVY->Set( vec.y );
		IVariable *pVZ = new CVariable<float>;
		pVZ->SetName( "z" );
		pVZ->Set( vec.z );

		// Start ignoring all updates comming from childs. (Initializing childs).
		m_bIgnoreChildsUpdate = true;

		CPropertyItemPtr itemX = new CPropertyItem( m_propertyCtrl );
		itemX->SetVariable( pVX );
		AddChild( itemX );

		CPropertyItemPtr itemY = new CPropertyItem( m_propertyCtrl );
		itemY->SetVariable( pVY );
		AddChild( itemY );

		CPropertyItemPtr itemZ = new CPropertyItem( m_propertyCtrl );
		itemZ->SetVariable( pVZ );
		AddChild( itemZ );

		m_bNoCategory = true;
		m_bExpandable = true;

		m_bIgnoreChildsUpdate = false;
	}

	if (m_pVariable->NumChildVars() > 0)
	{
		if (m_type == ePropertyInvalid)
			m_type = ePropertyTable;
		m_bExpandable = true;
		if (m_pVariable->GetFlags() & IVariable::UI_SHOW_CHILDREN)
			m_bShowChildren = true;
		m_bIgnoreChildsUpdate = true;
		for (i = 0; i < m_pVariable->NumChildVars(); i++)
		{
			CPropertyItemPtr item = new CPropertyItem( m_propertyCtrl );
			item->SetVariable(m_pVariable->GetChildVar(i));
			AddChild( item );
		}
		m_bIgnoreChildsUpdate = false;
	}
	m_modified = false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReleaseVariable()
{
	if (m_pVariable)
	{
		// Unwire all from variable.
		m_pVariable->RemoveOnSetCallback(functor(*this,&CPropertyItem::OnVariableChange));
	}
	m_pVariable = 0;
}

//////////////////////////////////////////////////////////////////////////
//! Find item that reference specified property.
CPropertyItem* CPropertyItem::FindItemByVar( IVariable *pVar )
{
	if (m_pVariable == pVar)
		return this;
	for (int i = 0; i < m_childs.size(); i++)
	{
		CPropertyItem* pFound = m_childs[i]->FindItemByVar(pVar);
		if (pFound)
			return pFound;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CString CPropertyItem::GetFullName() const
{
	if (m_parent)
	{
		return m_parent->GetFullName() + "::" + m_name;
	}
	else
		return m_name;
}

//////////////////////////////////////////////////////////////////////////
CPropertyItem* CPropertyItem::FindItemByFullName( const CString &name )
{
	if (GetFullName() == name)
		return this;
	for (int i = 0; i < m_childs.size(); i++)
	{
		CPropertyItem* pFound = m_childs[i]->FindItemByFullName(name);
		if (pFound)
			return pFound;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::AddChild( CPropertyItem *item )
{
	assert(item);
	m_bExpandable = true;
	item->m_parent = this;
	m_childs.push_back(item);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RemoveChild( CPropertyItem *item )
{
	// Find item and erase it from childs array.
	for (int i = 0; i < m_childs.size(); i++)
	{
		if (item == m_childs[i])
		{
			m_childs.erase( m_childs.begin() + i );
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RemoveAllChildren()
{
  m_childs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnChildChanged( CPropertyItem *child )
{
	if (m_bIgnoreChildsUpdate)
		return;

	if (m_bEditChildren)
	{
		// Update parent.
		CString sValue;
		for (int i = 0; i < m_childs.size(); i++)
		{
			if (i>0)
				sValue += ", ";
			sValue += m_childs[i]->GetValue();
		}
		SetValue( sValue, false );
	}
	if (m_bEditChildren || m_bShowChildren)
		SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetSelected( bool selected )
{
	m_bSelected = selected;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetExpanded( bool expanded )
{
	if (IsDisabled())
	{
		m_bExpanded = false;
	}
	else
		m_bExpanded = expanded;
}

bool CPropertyItem::IsDisabled() const
{
	if (m_pVariable)
	{
		return m_pVariable->GetFlags() & IVariable::UI_DISABLED;
	}
	return false;
}

bool CPropertyItem::IsBold() const
{
	if (m_pVariable)
	{
		return m_pVariable->GetFlags() & IVariable::UI_BOLD;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CreateInPlaceControl( CWnd* pWndParent,CRect& ctrlRect )
{
	if (IsDisabled())
		return;

	m_bMoveControls = true;
	CRect nullRc(0,0,0,0);
	switch (m_type)
	{
		case ePropertyFloat:
		case ePropertyInt:
		case ePropertyAngle:
			{
				if (m_pEnumDBItem)
				{
					// Combo box.
					m_cCombo = new CInPlaceComboBox();
					m_cCombo->SetReadOnly( true );
					m_cCombo->Create( NULL,"",WS_CHILD|WS_VISIBLE, nullRc, pWndParent,2 );
					m_cCombo->SetUpdateCallback( functor(*this,&CPropertyItem::OnComboSelection) );

					for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
						m_cCombo->AddString( m_pEnumDBItem->strings[i] );
				}
				else
				{
					m_cNumber = new CNumberCtrl;
					m_cNumber->SetUpdateCallback( functor(*this,&CPropertyItem::OnNumberCtrlUpdate) );
					m_cNumber->EnableUndo( m_name + " Modified" );
					//m_cNumber->SetBeginUpdateCallback( functor(*this,OnNumberCtrlBeginUpdate) );
					//m_cNumber->SetEndUpdateCallback( functor(*this,OnNumberCtrlEndUpdate) );

					// (digits behind the comma, only used for floats)
					if(m_type == ePropertyFloat)
						m_cNumber->SetInternalPrecision( max(m_iInternalPrecision, 2) );

					// Only for integers.
					if (m_type == ePropertyInt)
						m_cNumber->SetInteger(true);
					m_cNumber->SetMultiplier( m_valueMultiplier );
					m_cNumber->SetRange( m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX );
					
					//m_cNumber->Create( pWndParent,rect,1,CNumberCtrl::LEFTARROW|CNumberCtrl::NOBORDER );
					m_cNumber->Create( pWndParent,nullRc,1,CNumberCtrl::NOBORDER|CNumberCtrl::LEFTALIGN );
					m_cNumber->SetLeftAlign( true );

					m_cFillSlider = new CFillSliderCtrl;
					m_cFillSlider->EnableUndo( m_name + " Modified" );
					m_cFillSlider->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
					m_cFillSlider->SetUpdateCallback( functor(*this,&CPropertyItem::OnFillSliderCtrlUpdate) );
					m_cFillSlider->SetRangeFloat( m_rangeMin/m_valueMultiplier, m_rangeMax/m_valueMultiplier );
				}
			}
			break;

		case ePropertyTable:
			if (!m_bEditChildren)
				break;

		case ePropertyString:
			if (m_pEnumDBItem)
			{
				// Combo box.
				m_cCombo = new CInPlaceComboBox();
				m_cCombo->SetReadOnly( true );
				m_cCombo->Create( NULL,"",WS_CHILD|WS_VISIBLE, nullRc, pWndParent,2 );
				m_cCombo->SetUpdateCallback( functor(*this,&CPropertyItem::OnComboSelection) );

				for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
					m_cCombo->AddString( m_pEnumDBItem->strings[i] );
				break;
			}
			// ... else, fall through to create edit box.

		case ePropertyVector:
			m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
			m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT, nullRc, pWndParent,2 );
			break;

		case ePropertyShader:
		case ePropertyMaterial:
		case ePropertyAiBehavior:
		case ePropertyAiAnchor:
		case ePropertyAiCharacter:
		case ePropertyEquip:
		case ePropertyReverbPreset:
		case ePropertySOClass:
		case ePropertySOClasses:
		case ePropertySOState:
		case ePropertySOStates:
		case ePropertySOStatePattern:
		case ePropertySOAction:
		case ePropertySOHelper:
		case ePropertySONavHelper:
		case ePropertySOEvent:
		case ePropertySOTemplate:
		case ePropertyGameToken:
		case ePropertyMissionObj:
		case ePropertySequence:
		case ePropertyUser:
		case ePropertyLocalString:
			m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
			m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT, nullRc, pWndParent,2 );
			if (m_type == ePropertyShader)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnShaderBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_type == ePropertyAiBehavior)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAIBehaviorBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_type == ePropertyAiAnchor)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAIAnchorBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_type == ePropertyAiCharacter)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAICharacterBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_type == ePropertyEquip)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnEquipBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_type == ePropertyReverbPreset)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnEAXPresetBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertyMaterial)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnMaterialBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );

				m_cButton2 = new CInPlaceButton( functor(*this,&CPropertyItem::OnMaterialPickSelectedButton) );
				m_cButton2->Create( "<",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,5 );
				m_cButton2->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			}
			else if (m_type == ePropertySOClass)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOClassBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOClasses)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOClassesBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOState)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStateBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOStates)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStatesBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOStatePattern)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStatePatternBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOAction)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOActionBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOHelper)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOHelperBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySONavHelper)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSONavHelperBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOEvent)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOEventBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySOTemplate)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOTemplateBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertyGameToken)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnGameTokenBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertySequence)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSequenceBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertyMissionObj)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnMissionObjButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertyUser)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnUserBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			else if (m_type == ePropertyLocalString)
			{
				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnLocalStringBrowseButton) );
				m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			}
			if (m_cButton)
			{
				m_cButton->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			}
			break;

		case ePropertySelection:
			{
				// Combo box.
				m_cCombo = new CInPlaceComboBox();
				m_cCombo->SetReadOnly( true );
				m_cCombo->Create( NULL,"",WS_CHILD|WS_VISIBLE, nullRc, pWndParent,2 );
				m_cCombo->SetUpdateCallback( functor(*this,&CPropertyItem::OnComboSelection) );

				if (m_enumList)
				{
					for (int i = 0; i < m_enumList->GetItemsCount(); i++)
					{
						m_cCombo->AddString( m_enumList->GetItemName(i) );
					}
				}
				else
					m_cCombo->SetReadOnly( false );
			}
			break;

		case ePropertyColor:
			{
				m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
				m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT, nullRc, pWndParent,2 );

				m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnColorBrowseButton) );
				m_cButton->Create( "",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
				m_cButton->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			}
			break;

		case ePropertyFloatCurve:
			{
				m_cSpline = new CSplineCtrl;
				m_cSpline->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
				m_cSpline->SetUpdateCallback( (CSplineCtrl::UpdateCallback const&)functor(*this,&CPropertyItem::ReceiveFromControl) );
				m_cSpline->SetTimeRange(0, 1);
				m_cSpline->SetValueRange(0, 1);
				m_cSpline->SetGrid(12, 12);
				m_cSpline->SetSpline( m_pVariable->GetSpline(true) );
			}
			break;

		case ePropertyColorCurve:
			{
				m_cColorSpline = new CColorGradientCtrl;
				m_cColorSpline->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
				m_cColorSpline->SetUpdateCallback( (CColorGradientCtrl::UpdateCallback const&)functor(*this,&CPropertyItem::ReceiveFromControl) );
				m_cColorSpline->SetTimeRange(0, 1);
				m_cColorSpline->SetSpline( m_pVariable->GetSpline(true) );
			}
			break;

		case ePropertyFile:
		case ePropertyTexture:
		case ePropertyModel:
		case ePropertySound:
			m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
			m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT, nullRc, pWndParent,2 );

			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnFileBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );

			// Use file browse icon.
			m_cButton->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			HICON hOpenIcon = (HICON)LoadImage( AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_FILE_BROWSE),IMAGE_ICON,16,15,LR_DEFAULTCOLOR|LR_SHARED );
			//m_cButton->SetIcon( CSize(16,15),IDI_FILE_BROWSE );
			m_cButton->SetIcon( CSize(16,15),hOpenIcon );
			m_cButton->SetBorderGap(0);
			break;
/*
		case ePropertyList:
			{
				AddButton( "Add", CWDelegate(this,(TDelegate)OnAddItemButton) );
			}
			break;
*/
	}
	
	MoveInPlaceControl( ctrlRect );
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CreateControls( CWnd* pWndParent,CRect& textRect,CRect& ctrlRect )
{
	m_bMoveControls = false;
	CRect nullRc(0,0,0,0);

	if (IsExpandable() && !m_propertyCtrl->IsCategory(this))
	{
		m_cExpandButton  = new CInPlaceColorButton( functor(*this,&CPropertyItem::OnExpandButton) );
		const char *sText = "+";
		if (IsExpanded())
			sText = "-";
		m_cExpandButton->Create( sText,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW,
			CRect(textRect.left-10,textRect.top+3,textRect.left,textRect.top+15),
			pWndParent,5 );
		m_cExpandButton->SetFont(m_propertyCtrl->GetFont());
	}
	// Create static text.
	if (m_type != ePropertyBool)
	{
		m_pStaticText = new CColorCtrl<CStatic>;
		m_pStaticText->SetTextColor( RGB(0,0,0) );
		m_pStaticText->Create( GetName(),WS_CHILD|WS_VISIBLE|SS_RIGHT|SS_ENDELLIPSIS,textRect,pWndParent );
		m_pStaticText->SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
	}

	switch (m_type)
	{
	case ePropertyBool:
		{
			m_cCheckBox = new CInPlaceCheckBox( functor(*this,&CPropertyItem::OnCheckBoxButton) );
			m_cCheckBox->Create( GetName(),WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_TABSTOP,ctrlRect,pWndParent,0 );
			m_cCheckBox->SetFont(m_propertyCtrl->GetFont());
		}
		break;
	case ePropertyFloat:
	case ePropertyInt:
	case ePropertyAngle:
		{
			m_cNumber = new CNumberCtrl;
			m_cNumber->SetUpdateCallback( functor(*this,&CPropertyItem::OnNumberCtrlUpdate) );
			m_cNumber->EnableUndo( m_name + " Modified" );

			// (digits behind the comma, only used for floats)
			if(m_type == ePropertyFloat)
				m_cNumber->SetInternalPrecision( max(m_iInternalPrecision, 2) );

			// Only for integers.
			if (m_type == ePropertyInt)
				m_cNumber->SetInteger(true);
			m_cNumber->SetMultiplier( m_valueMultiplier );
			m_cNumber->SetRange( m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX );

			m_cNumber->Create( pWndParent,nullRc,1 );
			m_cNumber->SetLeftAlign( true );

			/*
			CSliderCtrlEx *pSlider = new CSliderCtrlEx;
			//pSlider->EnableUndo( m_name + " Modified" );
			CRect rcSlider = ctrlRect;
			rcSlider.left += NUMBER_CTRL_WIDTH;
			pSlider->Create( WS_VISIBLE|WS_CHILD,rcSlider,pWndParent,2);
			*/
			m_cFillSlider = new CFillSliderCtrl;
			m_cFillSlider->SetFilledLook(false);
			m_cFillSlider->EnableUndo( m_name + " Modified" );
			m_cFillSlider->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
			m_cFillSlider->SetUpdateCallback( functor(*this,&CPropertyItem::OnFillSliderCtrlUpdate) );
			m_cFillSlider->SetRangeFloat( m_rangeMin/m_valueMultiplier,m_rangeMax/m_valueMultiplier );
		}
		break;

	case ePropertyVector:
		{
			CNumberCtrl** cNumber[3] = { &m_cNumber, &m_cNumber1, &m_cNumber2 };
			for (int a = 0; a < 3; a++)
			{
				CNumberCtrl* pNumber = *cNumber[a] = new CNumberCtrl;
				pNumber->Create( pWndParent,nullRc,1 );
				pNumber->SetUpdateCallback( functor(*this, &CPropertyItem::OnNumberCtrlUpdate) );
				pNumber->EnableUndo( m_name + " Modified" );
				pNumber->SetMultiplier( m_valueMultiplier );
				pNumber->SetRange( m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX );
			}
		}
		break;

	case ePropertyTable:
		if (!m_bEditChildren)
			break;
	case ePropertyString:
	//case ePropertyVector:
		m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
		m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT|WS_TABSTOP, nullRc, pWndParent,2 );
		break;

	case ePropertyShader:
	case ePropertyMaterial:
	case ePropertyAiBehavior:
	case ePropertyAiAnchor:
	case ePropertyAiCharacter:
	case ePropertyEquip:
	case ePropertyReverbPreset:
	case ePropertySOClass:
	case ePropertySOClasses:
	case ePropertySOState:
	case ePropertySOStates:
	case ePropertySOStatePattern:
	case ePropertySOAction:
	case ePropertySOHelper:
	case ePropertySONavHelper:
	case ePropertyGameToken:
	case ePropertySequence:
	case ePropertyUser:
	case ePropertyLocalString:
		m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
		m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT|WS_TABSTOP, nullRc, pWndParent,2 );

		if (m_type == ePropertyShader)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnShaderBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_type == ePropertyAiBehavior)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAIBehaviorBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_type == ePropertyAiAnchor)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAIAnchorBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_type == ePropertyAiCharacter)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnAICharacterBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_type == ePropertyEquip)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnEquipBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_type == ePropertyReverbPreset)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnEAXPresetBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertyMaterial)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnMaterialBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );

			m_cButton2 = new CInPlaceButton( functor(*this,&CPropertyItem::OnMaterialPickSelectedButton) );
			m_cButton2->Create( "<",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,5 );
			//m_cButton2->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			m_cButton2->SetXButtonStyle( BS_XT_WINXP_COMPAT,FALSE );
		}
		else if (m_type == ePropertySOClass)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOClassBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOClasses)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOClassesBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOState)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStateBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOStates)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStatesBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOStatePattern)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOStatePatternBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOAction)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOActionBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySOHelper)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSOHelperBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySONavHelper)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSONavHelperBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertyGameToken)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnGameTokenBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertySequence)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnSequenceBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertyUser)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnUserBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		else if (m_type == ePropertyLocalString)
		{
			m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnLocalStringBrowseButton) );
			m_cButton->Create( "...",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
		}
		if (m_cButton)
		{
			m_cButton->SetXButtonStyle( BS_XT_WINXP_COMPAT,FALSE );
		}
		break;

	case ePropertySelection:
		{
			// Combo box.
			m_cCombo = new CInPlaceComboBox();
			m_cCombo->SetReadOnly( true );
			m_cCombo->Create( NULL,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP, nullRc, pWndParent,2 );
			m_cCombo->SetUpdateCallback( functor(*this,&CPropertyItem::OnComboSelection) );

			if (m_enumList)
			{
				for (int i = 0; i < m_enumList->GetItemsCount(); i++)
				{
					m_cCombo->AddString( m_enumList->GetItemName(i) );
				}
			}
			else
				m_cCombo->SetReadOnly( false );
		}
		break;

	case ePropertyColor:
		{
			m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
			m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT|WS_TABSTOP, nullRc, pWndParent,2 );

			//m_cButton = new CInPlaceButton( functor(*this,OnColorBrowseButton) );
			//m_cButton->Create( "",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			//m_cButton->SetXButtonStyle( BS_XT_SEMIFLAT,FALSE );
			m_cColorButton = new CInPlaceColorButton( functor(*this,&CPropertyItem::OnColorBrowseButton) );
			m_cColorButton->Create( "",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_OWNERDRAW,nullRc,pWndParent,4 );
		}
		break;

	case ePropertyFloatCurve:
		{
			m_cSpline = new CSplineCtrl;
			m_cSpline->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
			m_cSpline->SetUpdateCallback( (CSplineCtrl::UpdateCallback const&)functor(*this,&CPropertyItem::ReceiveFromControl) );
			m_cSpline->SetTimeRange(0, 1);
			m_cSpline->SetValueRange(0, 1);
			m_cSpline->SetGrid(12, 12);
			m_cSpline->SetSpline( m_pVariable->GetSpline(true) );
		}
		break;

	case ePropertyColorCurve:
		{
			m_cColorSpline = new CColorGradientCtrl;
			m_cColorSpline->Create( WS_VISIBLE|WS_CHILD,nullRc,pWndParent,2);
			m_cColorSpline->SetUpdateCallback( (CColorGradientCtrl::UpdateCallback const&)functor(*this,&CPropertyItem::ReceiveFromControl) );
			m_cColorSpline->SetTimeRange(0, 1);
			m_cColorSpline->SetSpline( m_pVariable->GetSpline(true) );
		}
		break;

	case ePropertyFile:
	case ePropertyTexture:
	case ePropertyModel:
	case ePropertySound:
		m_cEdit = new CInPlaceEdit( m_value,functor(*this,&CPropertyItem::OnEditChanged) );
		m_cEdit->Create( WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT|WS_TABSTOP, nullRc, pWndParent,2 );

		m_cButton = new CInPlaceButton( functor(*this,&CPropertyItem::OnFileBrowseButton) );
		m_cButton->Create( "",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );

		// Use file browse icon.
		m_cButton->SetXButtonStyle( BS_XT_WINXP_COMPAT,FALSE );
		HICON hOpenIcon = (HICON)LoadImage( AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_FILE_BROWSE),IMAGE_ICON,16,15,LR_DEFAULTCOLOR|LR_SHARED );
		//m_cButton->SetIcon( CSize(16,15),IDI_FILE_BROWSE );
		m_cButton->SetIcon( CSize(16,15),hOpenIcon );
 		m_cButton->SetBorderGap(6);
		break;
		/*
		case ePropertyList:
		{
		AddButton( "Add", CWDelegate(this,(TDelegate)OnAddItemButton) );
		}
		break;
		*/
	}
	
	if (m_cEdit)
	{
		m_cEdit->ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	}
	if (m_cCombo)
	{
		m_cCombo->ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	}
	if (m_cButton && m_type != ePropertyColor)
	{
		m_cButton->SetXButtonStyle( BS_XT_WINXP_COMPAT,FALSE );
	}

	MoveInPlaceControl( ctrlRect );
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::DestroyInPlaceControl( bool bRecursive )
{
	/*
	if (!m_propertyCtrl->IsExtenedUI())
		ReceiveFromControl();
	*/

	SAFE_DELETE(m_pStaticText);
	SAFE_DELETE(m_cNumber);
	SAFE_DELETE(m_cNumber1);
	SAFE_DELETE(m_cNumber2);
	SAFE_DELETE(m_cFillSlider);
	SAFE_DELETE(m_cSpline);
	SAFE_DELETE(m_cColorSpline);
	SAFE_DELETE(m_cEdit);
	SAFE_DELETE(m_cCombo);
	SAFE_DELETE(m_cButton);
	SAFE_DELETE(m_cButton2);
	SAFE_DELETE(m_cExpandButton);
	SAFE_DELETE(m_cCheckBox);
	SAFE_DELETE(m_cColorButton);

	if (bRecursive)
	{
		int num = GetChildCount();
		for (int i = 0; i < num; i++)
		{
			GetChild(i)->DestroyInPlaceControl(bRecursive);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::MoveInPlaceControl( const CRect& rect )
{
	int nButtonWidth = BUTTON_WIDTH;
	if (m_propertyCtrl->IsExtenedUI())
		nButtonWidth += 10;

	CRect rc = rect;
	if (m_cButton)
	{
		if (m_type == ePropertyColor)
		{
			rc.right = rc.left + nButtonWidth + 2;
			m_cButton->MoveWindow( rc,FALSE );
			rc = rect;
			rc.left += nButtonWidth + 2 + 4;
		}
		else
		{
			rc.left = rc.right - nButtonWidth;
			m_cButton->MoveWindow( rc,FALSE );
			rc = rect;
			rc.right -= nButtonWidth;
		}
		m_cButton->SetFont(m_propertyCtrl->GetFont());
	}
	if (m_cColorButton)
	{
		rc.right = rc.left + nButtonWidth + 2;
		m_cColorButton->MoveWindow( rc,FALSE );
		rc = rect;
		rc.left += nButtonWidth + 2 + 4;
	}
	if (m_cButton2)
	{
		CRect brc(rc);
		brc.left = brc.right - nButtonWidth-2;
		m_cButton2->MoveWindow( brc,FALSE );
		rc.right -= nButtonWidth+4;
		m_cButton2->SetFont(m_propertyCtrl->GetFont());
	}

	if (m_cNumber)
	{
		CRect rcn = rc;
		if (m_cNumber1 && m_cNumber2)
		{
			int x = NUMBER_CTRL_WIDTH+4;
			CRect rc0,rc1,rc2;
			rc0 = CRect( rc.left,rc.top,rc.left+NUMBER_CTRL_WIDTH,rc.bottom );
			rc1 = CRect( rc.left+x,rc.top,rc.left+x+NUMBER_CTRL_WIDTH,rc.bottom );
			rc2 = CRect( rc.left+x*2,rc.top,rc.left+x*2+NUMBER_CTRL_WIDTH,rc.bottom );
			m_cNumber->MoveWindow( rc0,FALSE );
			m_cNumber1->MoveWindow( rc1,FALSE );
			m_cNumber2->MoveWindow( rc2,FALSE );
		}
		else
		{
			//rcn.right = rc.left + NUMBER_CTRL_WIDTH;
			if (rcn.Width() > NUMBER_CTRL_WIDTH)
				rcn.right = rc.left + NUMBER_CTRL_WIDTH;
			m_cNumber->MoveWindow( rcn,FALSE );
		}
		m_cNumber->SetFont(m_propertyCtrl->GetFont());
		if (m_cFillSlider)
		{
			CRect rcSlider = rc;
			rcSlider.left = rcn.right+1;
			m_cFillSlider->MoveWindow(rcSlider,FALSE);
		}
	}
	if (m_cEdit)
	{
		m_cEdit->MoveWindow( rc,FALSE );
		m_cEdit->SetFont(m_propertyCtrl->GetFont());
	}
	if (m_cCombo)
	{
		m_cCombo->MoveWindow( rc,FALSE );
		m_cCombo->SetFont(m_propertyCtrl->GetFont());
	}
	if (m_cSpline)
	{
		// Grow beyond current line.
		rc.bottom = rc.top + 48;
		rc.right -= 4;
		m_cSpline->MoveWindow( rc,FALSE );
	}
	if (m_cColorSpline)
	{
		// Grow beyond current line.
		rc.bottom = rc.top + 32;
		rc.right -= 4;
		m_cColorSpline->MoveWindow( rc,FALSE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetFocus()
{
	if (m_cNumber)
	{
		m_cNumber->SetFocus();
	}
	if (m_cEdit)
	{
		m_cEdit->SetFocus();
	}
	if (m_cCombo)
	{
		m_cCombo->SetFocus();
	}
	if (!m_cNumber && !m_cEdit && !m_cCombo)
		m_propertyCtrl->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReceiveFromControl()
{
	if (IsDisabled())
		return;
	if (m_cEdit)
	{
		CString str;
		m_cEdit->GetWindowText( str );
		if (!CUndo::IsRecording())
		{

			CUndo undo( GetName() + " Modified" );
			SetValue( str );
		}
		else
			SetValue( str );
	}
	if (m_cCombo)
	{
		if (!CUndo::IsRecording())
		{
			CUndo undo( GetName() + " Modified" );
			if (m_pEnumDBItem)
				SetValue( m_pEnumDBItem->NameToValue(m_cCombo->GetSelectedString()) );
			else
				SetValue( m_cCombo->GetSelectedString() );
		}
		else
		{
			if (m_pEnumDBItem)
				SetValue( m_pEnumDBItem->NameToValue(m_cCombo->GetSelectedString()) );
			else
				SetValue( m_cCombo->GetSelectedString() );
		}
	}
	if (m_cNumber)
	{
		if (m_cNumber1 && m_cNumber2)
		{
			CString val;
			val.Format( "%g,%g,%g",m_cNumber->GetValue(),m_cNumber1->GetValue(),m_cNumber2->GetValue() );
			SetValue( val );
		}
		else
			SetValue( m_cNumber->GetValueAsString() );
	}
	if (m_cSpline != 0 || m_cColorSpline != 0)
	{
    // Variable was already directly updated.
    //OnVariableChange(m_pVariable);
    m_modified = true;
    m_pVariable->Get(m_value);
    m_propertyCtrl->OnItemChange( this );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SendToControl()
{
	bool bInPlaceCtrl = m_propertyCtrl->IsExtenedUI();
	if (m_cButton)
	{
		if (m_type == ePropertyColor)
		{
			COLORREF clr = StringToColor(m_value);
			m_cButton->SetColorFace( clr );
			m_cButton->RedrawWindow();
			bInPlaceCtrl = true;
		}
		m_cButton->Invalidate();
	}
	if (m_cColorButton)
	{
		COLORREF clr = StringToColor(m_value);
		m_cColorButton->SetColor( clr );
		m_cColorButton->Invalidate();
		bInPlaceCtrl = true;
	}
	if (m_cCheckBox)
	{
		m_cCheckBox->SetCheck( GetBoolValue() ? BST_CHECKED : BST_UNCHECKED );
		m_cCheckBox->Invalidate();
	}
	if (m_cEdit)
	{
		m_cEdit->SetText(m_value);
		bInPlaceCtrl = true;
		m_cEdit->Invalidate();
	}
	if (m_cCombo)
	{
		if (m_type == ePropertyBool)
			m_cCombo->SetCurSel( GetBoolValue()?0:1 );
		else
		{
			if (m_pEnumDBItem)
				m_cCombo->SelectString(-1,m_pEnumDBItem->ValueToName(m_value));
			else
				m_cCombo->SelectString(-1,m_value);
		}
		bInPlaceCtrl = true;
		m_cCombo->Invalidate();
	}
	if (m_cNumber)
	{
		if (m_cNumber1 && m_cNumber2)
		{
			float x,y,z;
			sscanf( m_value,"%f,%f,%f",&x,&y,&z );
			m_cNumber->SetValue( x );
			m_cNumber1->SetValue( y );
			m_cNumber2->SetValue( z );
		}
		else
			m_cNumber->SetValue( atof(m_value) );
		bInPlaceCtrl = true;
		m_cNumber->Invalidate();
	}
	if (m_cFillSlider)
	{
		m_cFillSlider->SetValue( atof(m_value) );
		bInPlaceCtrl = true;
		m_cFillSlider->Invalidate();
	}
	if (!bInPlaceCtrl)
	{
		CRect rc;
		m_propertyCtrl->GetItemRect( this,rc );
		m_propertyCtrl->InvalidateRect( rc,TRUE );
		m_propertyCtrl->SetFocus();
	}
	if (m_cSpline)
	{
		m_cSpline->SetSpline( m_pVariable->GetSpline(true), TRUE );
	}
	if (m_cColorSpline)
	{
		m_cColorSpline->SetSpline( m_pVariable->GetSpline(true), TRUE );
	}
	CheckControlActiveColor();
}

//////////////////////////////////////////////////////////////////////////
bool CPropertyItem::HasDefaultValue( bool bChildren ) const
{
	if (!bChildren)
	{
		// In general, check if value is 0.
		if (m_pVariable)
		{
			switch(m_pVariable->GetType()) 
			{
			case IVariable::INT:
				{
					int v = 0;
					m_pVariable->Get(v);
					if (v != 0)
						return false;
				};
				break;
			case IVariable::BOOL:
				{
					bool v = 0;
					m_pVariable->Get(v);
					if (v != 0)
						return false;
				};
				break;
			case IVariable::FLOAT:
				{
					float v = 0;
					m_pVariable->Get(v);
					if (v != 0)
						return false;
				};
				break;
			case IVariable::VECTOR:
				{
					Vec3 v(0,0,0);
					m_pVariable->Get(v);
					if (!v.IsZero())
						return false;
				};
				break;
			case IVariable::STRING:
				{
					CString v;
					m_pVariable->Get(v);
					if (!v.IsEmpty())
						return false;
				};
				break;
			default:
				if (!m_value.IsEmpty())
					return false;
				break;
			}
		}
	}
	else
	{
		// Evaluate children.
		for (int i = 0; i < m_childs.size(); i++)
		{
			if (!m_childs[i]->HasDefaultValue(false) || !m_childs[i]->HasDefaultValue(true))
				return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CheckControlActiveColor()
{
	if (m_pStaticText)
	{
		COLORREF nTextColor = HasDefaultValue(false) ? RGB(0,0,0) : RGB(0,0,240);
		if (m_pStaticText->GetTextColor() != nTextColor)
			m_pStaticText->SetTextColor( nTextColor );
	}

	if (m_cExpandButton)
	{
		static COLORREF nDefColor = ::GetSysColor( COLOR_BTNFACE );
		static COLORREF nDefTextColor = ::GetSysColor( COLOR_BTNTEXT );
		static COLORREF nNondefColor = RGB( GetRValue(nDefColor)/2, GetGValue(nDefColor)/2, GetBValue(nDefColor) );
		static COLORREF nNondefTextColor = RGB( 255,255,0 );
		COLORREF nColor = HasDefaultValue(true) ? nDefColor : nNondefColor;
		if (m_cExpandButton->GetColor() != nColor)
		{
			m_cExpandButton->SetColor(nColor);
			if (nColor != nDefColor)
				m_cExpandButton->SetTextColor(nNondefTextColor);
			else
				m_cExpandButton->SetTextColor(nDefTextColor);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnComboSelection()
{
	ReceiveFromControl();
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::DrawValue( CDC *dc,CRect rect )
{
	// Setup text.
	dc->SetBkMode( TRANSPARENT );
	
	CString val = GetDrawValue();

	if (m_type == ePropertyBool)
	{
		CPoint p1( rect.left,rect.top+1 );
		int sz = rect.bottom - rect.top - 1;
		CRect rc(p1.x,p1.y,p1.x+sz,p1.y+sz );
		dc->DrawFrameControl( rc,DFC_BUTTON,DFCS_BUTTON3STATE );
		if (GetBoolValue())
			m_propertyCtrl->m_icons.Draw( dc,12,p1,ILD_TRANSPARENT );
		rect.left += 15;
	}
	else if (m_type == ePropertyFile || m_type == ePropertyTexture || 
						m_type == ePropertySound || m_type == ePropertyModel)
	{
		// Any file.
		// Check if file name fits into the designated rectangle.
		CSize textSize = dc->GetTextExtent( val, val.GetLength() );
		if (textSize.cx > rect.Width())
		{
			// Cut file name...
			if (m_type != ePropertySound || Path::GetExt(val).IsEmpty() == false)
				val = CString("...\\") + Path::GetFile(val);
		}
	}
	else if (m_type == ePropertyColor)
	{
		//CRect rc( CPoint(rect.right-BUTTON_WIDTH,rect.top),CSize(BUTTON_WIDTH,rect.bottom-rect.top) );
		CRect rc( CPoint(rect.left,rect.top+1),CSize(BUTTON_WIDTH+2,rect.bottom-rect.top-2) );
		//CPen pen( PS_SOLID,1,RGB(128,128,128));
		CPen pen( PS_SOLID,1,RGB(0,0,0));
		CBrush brush( StringToColor(val) );
		CPen *pOldPen = dc->SelectObject( &pen );
		CBrush *pOldBrush = dc->SelectObject( &brush );
		dc->Rectangle( rc );
		//COLORREF col = StringToColor(m_value);
		//rc.DeflateRect( 1,1 );
		//dc->FillSolidRect( rc,col );
		dc->SelectObject( pOldPen );
		dc->SelectObject( pOldBrush );
		rect.left = rect.left + BUTTON_WIDTH + 2 + 4;
	}
	else if (m_pVariable != 0 && m_pVariable->GetSpline(false) && m_pVariable->GetSpline(false)->GetKeyCount() > 0)
	{
		// Draw mini-curve or gradient.
		CPen* pOldPen = 0;

		ISplineInterpolator* pSpline = m_pVariable->GetSpline(true);
		int width = min(rect.Width()-1, 128);
		for (int x = 0; x < width; x++)
		{
			float time = float(x) / (width-1);
			ISplineInterpolator::ValueType val;
			pSpline->Interpolate(time, val);

			if (m_type == ePropertyColorCurve)
			{
				COLORREF col = RGB( pos_round(val[0]*255), pos_round(val[1]*255), pos_round(val[2]*255) );
				CPen pen(PS_SOLID, 1, col);
				if (!pOldPen)
					pOldPen = dc->SelectObject(&pen);
				else
					dc->SelectObject(&pen);
				dc->MoveTo( CPoint(rect.left+x, rect.bottom) );
				dc->LineTo( CPoint(rect.left+x, rect.top) );
			}
			else if (m_type == ePropertyFloatCurve)
			{
				CPoint point;
				point.x = rect.left + x;
				point.y = int_round((rect.bottom-1) * (1.f - val[0]) + (rect.top+1) * val[0]);
				if (x == 0)
					dc->MoveTo( point );
				else
					dc->LineTo( point );
			}

			if (pOldPen)
				dc->SelectObject(pOldPen);
		}

		// No text.
		return;
	}

	/*
	//////////////////////////////////////////////////////////////////////////
	// Draw filled bar like in CFillSliderCtrl.
	//////////////////////////////////////////////////////////////////////////
	if (m_type == ePropertyFloat || m_type == ePropertyInt || m_type == ePropertyAngle)
	{
		CRect rc = rect;
		rc.left += NUMBER_CTRL_WIDTH;
		rc.top += 1;
		rc.bottom -= 1;

		float value = atof(m_value);
		float min = m_rangeMin/m_valueMultiplier;
		float max = m_rangeMax/m_valueMultiplier;
		if (min == max)
			max = min + 1;
		float pos = (value-min) / fabs(max-min);
		int splitPos = rc.left + pos * rc.Width();

		// Paint filled rect.
		CRect fillRc = rc;
		fillRc.right = splitPos;
		dc->FillRect(fillRc,CBrush::FromHandle((HBRUSH)GetStockObject(LTGRAY_BRUSH)) );

		// Paint empty rect.
		CRect emptyRc = rc;
		emptyRc.left = splitPos+1;
		emptyRc.IntersectRect(emptyRc,rc);
		dc->FillRect(emptyRc,CBrush::FromHandle((HBRUSH)GetStockObject(WHITE_BRUSH)) );
	}
	*/

	CRect textRc;
	textRc = rect;
	::DrawTextEx( dc->GetSafeHdc(), val.GetBuffer(), val.GetLength(), textRc, DT_END_ELLIPSIS|DT_LEFT|DT_SINGLELINE|DT_VCENTER, NULL );
}

COLORREF CPropertyItem::StringToColor( const CString &value )
{
	float r,g,b;
	int res = 0;
	if (res != 3)
		res = sscanf( value,"%f,%f,%f",&r,&g,&b );
	if (res != 3)
		res = sscanf( value,"R:%f,G:%f,B:%f",&r,&g,&b );
	if (res != 3)
		res = sscanf( value,"R:%f G:%f B:%f",&r,&g,&b );
	if (res != 3)
		res = sscanf( value,"%f %f %f",&r,&g,&b );
	if (res != 3)
	{
		sscanf( value,"%f",&r );
		return r;
	}
	int ir = r;
	int ig = g;
	int ib = b;

	return RGB(ir,ig,ib);
}

bool CPropertyItem::GetBoolValue()
{
	if (stricmp(m_value,"true")==0 || atoi(m_value) != 0)
		return true;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
const char* CPropertyItem::GetValue() const
{
	return m_value;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetValue( const char* sValue,bool bRecordUndo,bool bForceModified )
{
	if (bRecordUndo && IsDisabled())
		return;

	_smart_ptr<CPropertyItem> holder = this; // Make sure we are not released during this function.

	CString value = sValue;
	
	switch (m_type)
	{
	case ePropertyBool:
		if (stricmp(value,"true")==0 || atof(value) != 0)
			value = "1";
		else
			value = "0";
		break;

	case ePropertyVector:
		if (value.Find(',') < 0)
			value = value + ", " + value + ", " + value;
		break;
	case ePropertyTexture:
	case ePropertySound:
	case ePropertyModel:
	case ePropertyMaterial:
		value.Replace( '\\','/' );
		break;
	}


	bool bModified = bForceModified || m_value.Compare(value) != 0;
	m_value = value;

	if (m_pVariable)
	{
		if (bModified)
		{
			float currTime = GetIEditor()->GetSystem()->GetITimer()->GetCurrTime();
			//if (currTime > m_lastModified+1) // At least one second between undo stores.
			{
				if (bRecordUndo && !CUndo::IsRecording())
				{
					if (!CUndo::IsSuspended())
					{
						CUndo undo( GetName() + " Modified" );
						undo.Record( new CUndoVariableChange(m_pVariable,"PropertyChange") );
					}
				}
				else if (bRecordUndo)
				{
					GetIEditor()->RecordUndo( new CUndoVariableChange(m_pVariable,"PropertyChange") );
				}
			}

			ValueToVar();
			m_lastModified = currTime;
		}
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		// DEPRICATED (For XmlNode).
		//////////////////////////////////////////////////////////////////////////
		if (m_node)
			m_node->setAttr( VALUE_ATTR,m_value );
		//CString xml = m_node->getXML();

		SendToControl();

		if (bModified)
		{
			m_modified = true;
			if (m_bEditChildren)
			{
				// Extract child components. 
				int pos = 0;
				for (int i = 0; i < m_childs.size(); i++)
				{
					CString elem = m_value.Tokenize(", ", pos);
					m_childs[i]->m_value = elem;
					m_childs[i]->SendToControl();
				}
			}
			
			if (m_parent)
				m_parent->OnChildChanged( this );
			// If Value changed mark document modified.
			// Notify parent that this Item have been modified.
			m_propertyCtrl->OnItemChange( this );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::VarToValue()
{
	assert( m_pVariable != 0 );

	if (m_type == ePropertyColor)
	{
		if (m_pVariable->GetType() == IVariable::VECTOR)
		{
			Vec3 v;
			m_pVariable->Get(v);
			int r = v.x*255;
			int g = v.y*255;
			int b = v.z*255;
			r = max(0,min(r,255));
			g = max(0,min(g,255));
			b = max(0,min(b,255));
			m_value.Format( "%d,%d,%d",r,g,b );
		}
		else
		{
			int col;
			m_pVariable->Get(col);
			m_value.Format( "%d,%d,%d",GetRValue((uint32)col),GetGValue((uint32)col),GetBValue((uint32)col) );
		}
		return;
	}

	m_value = m_pVariable->GetDisplayValue();

	if (m_type == ePropertySOHelper || m_type == ePropertySONavHelper)
	{
		// hide smart object class part
		int f = m_value.Find(':');
		if (f >= 0)
			m_value.Delete(0, f+1);
	}
}

CString CPropertyItem::GetDrawValue()
{
	CString value = m_value;

	if (m_pEnumDBItem)
		value = m_pEnumDBItem->ValueToName(value);

	if (m_valueMultiplier != 1.f)
	{
		float f = atof(m_value) * m_valueMultiplier;
		if (m_type == ePropertyInt)
			value.Format("%d", int_round(f));
		else
			value.Format("%g", f);
		if (m_valueMultiplier == 100)
			value += "%";
	}
	else if (m_type == ePropertyBool)
	{
		return GetBoolValue() ? "True" : "False";	
	}
	else if (m_type == ePropertyFloatCurve || m_type == ePropertyColorCurve)
	{
		// Don't display actual values in field.
		if (!value.IsEmpty())
			value = "[Curve]";
	}

	if (m_bShowChildren && !m_bExpanded)
	{
		// Show sub-values, skipping empty ones to avoid clutter.
		int iLast = 0;
		for (int i = 0; i < m_childs.size(); i++)
		{
			CString childVal = m_childs[i]->GetDrawValue();
			if ( childVal != "" && childVal != "0" && childVal != "0%" && childVal != "0,0,0" )
			{
				while (iLast <= i)
				{
					value += ", ";
					iLast++;
				}
				value += childVal;
			}
		}
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ValueToVar()
{
	assert( m_pVariable != NULL );

	_smart_ptr<CPropertyItem> holder = this; // Make sure we are not released during the call to variable Set.

	if (m_type == ePropertyColor)
	{
		COLORREF col = StringToColor(m_value);
		if (m_pVariable->GetType() == IVariable::VECTOR)
		{
			Vec3 v;
			v.x = GetRValue(col)/255.0f;
			v.y = GetGValue(col)/255.0f;
			v.z = GetBValue(col)/255.0f;
			m_pVariable->Set(v);
		}
		else
			m_pVariable->Set((int)col);
	}
	else if (m_type == ePropertySOHelper || m_type == ePropertySONavHelper)
	{
		// keep smart object class part

		CString oldValue;
		m_pVariable->Get(oldValue);
		int f = oldValue.Find(':');
		if (f >= 0)
			oldValue.Truncate(f+1);

		CString newValue(m_value);
		f = newValue.Find(':');
		if (f >= 0)
			newValue.Delete(0, f+1);

		m_pVariable->Set(oldValue + newValue);
	}
	else if (m_type != ePropertyInvalid)
	{
		m_pVariable->SetDisplayValue(m_value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnVariableChange( IVariable* pVar )
{
	assert(pVar != 0 && pVar == m_pVariable);

	// When variable changes, invalidate UI.
	m_modified = true;

	VarToValue();

  // update enum list
  if (m_type == ePropertySelection)  
    m_enumList = m_pVariable->GetEnumList();

	SendToControl();

	if (m_bEditChildren)
	{
		// Parse comma-separated values, set children.
		bool bPrevIgnore = m_bIgnoreChildsUpdate;
		m_bIgnoreChildsUpdate = true;

		int pos = 0;
		for (int i = 0; i < m_childs.size(); i++)
		{
			CString sElem = pos >= 0 ? m_value.Tokenize(", ", pos) : CString();
			m_childs[i]->SetValue(sElem,false);
		}
		m_bIgnoreChildsUpdate = bPrevIgnore;
	}  

	if (m_parent)
		m_parent->OnChildChanged( this );
	
	// If Value changed mark document modified.
	// Notify parent that this Item have been modified.
	// This may delete this control...
	m_propertyCtrl->OnItemChange( this );
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMouseWheel( UINT nFlags,short zDelta,CPoint point )
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (m_cCombo)
	{
		int sel = m_cCombo->GetCurSel();
		if (zDelta > 0)
		{
			sel++;
			if (m_cCombo->SetCurSel( sel ) == CB_ERR)
				m_cCombo->SetCurSel( 0 );
		}
		else
		{
			sel--;
			if (m_cCombo->SetCurSel( sel ) == CB_ERR)
				m_cCombo->SetCurSel( m_cCombo->GetCount()-1 );
		}
	}
	else if (m_cNumber)
	{
		if (zDelta > 0)
		{
			m_cNumber->SetValue( m_cNumber->GetValue() + m_cNumber->GetStep() );
		}
		else
		{
			m_cNumber->SetValue( m_cNumber->GetValue() - m_cNumber->GetStep() );
		}
		ReceiveFromControl();
	}
}


//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLButtonDblClk( UINT nFlags,CPoint point )
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (IsDisabled())
		return;

	if (m_type == ePropertyBool)
	{
		// Swap boolean value.
		if (GetBoolValue())
			SetValue( "0" );
		else
			SetValue( "1" );
	}
	else
	{
		// Simulate button click.
		if (m_cButton)
			m_cButton->Click();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLButtonDown( UINT nFlags,CPoint point )
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (m_type == ePropertyBool)
	{
		CRect rect;
		m_propertyCtrl->GetItemRect( this,rect );
		rect = m_propertyCtrl->GetItemValueRect( rect );

		CPoint p( rect.left-2,rect.top+1 );
		int sz = rect.bottom - rect.top;
		rect = CRect(p.x,p.y,p.x+sz,p.y+sz );

		if (rect.PtInRect(point))
		{
			// Swap boolean value.
			if (GetBoolValue())
				SetValue( "0" );
			else
				SetValue( "1" );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnCheckBoxButton()
{
	if (m_cCheckBox)
		if (m_cCheckBox->GetCheck() == BST_CHECKED)
			SetValue( "1" );
		else
			SetValue( "0" );
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnColorBrowseButton()
{
	/*
	COLORREF clr = StringToColor(m_value);
  if (GetIEditor()->SelectColor(clr,m_propertyCtrl))
  {
		int r,g,b;
		r = GetRValue(clr);
		g = GetGValue(clr);
		b = GetBValue(clr);
    //val.Format( "R:%d G:%d B:%d",r,g,b );
		CString val;
		val.Format( "%d,%d,%d",r,g,b );
		SetValue( val );
		m_propertyCtrl->Invalidate();
		//RedrawWindow( OwnerProperties->hWnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN );
  }
	*/
	if (IsDisabled())
		return;

	COLORREF orginalColor = StringToColor(m_value);
	COLORREF clr = orginalColor;
	CCustomColorDialog dlg( orginalColor,CC_FULLOPEN,m_propertyCtrl );
	dlg.SetColorChangeCallback( functor(*this,&CPropertyItem::OnColorChange) );
	if (dlg.DoModal() == IDOK)
	{
		clr = dlg.GetColor();
		if (clr != orginalColor)
		{
			int r,g,b;
			CString val;
			r = GetRValue(orginalColor);
			g = GetGValue(orginalColor);
			b = GetBValue(orginalColor);
			val.Format( "%d,%d,%d",r,g,b );
			SetValue( val,false );

			CUndo undo( CString(GetName()) + " Modified" );
			r = GetRValue(clr);
			g = GetGValue(clr);
			b = GetBValue(clr);
			val.Format( "%d,%d,%d",r,g,b );
			SetValue( val );
			m_propertyCtrl->InvalidateCtrl();
		}
	}
	else
	{
		if (StringToColor(m_value) != orginalColor)
		{
			int r,g,b;
			r = GetRValue(clr);
			g = GetGValue(clr);
			b = GetBValue(clr);
			CString val;
			val.Format( "%d,%d,%d",r,g,b );
			SetValue( val,false );
			m_propertyCtrl->InvalidateCtrl();
		}
	}
	if (m_cButton)
		m_cButton->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnColorChange( COLORREF clr )
{
	GetIEditor()->SuspendUndo();
	int r,g,b;
	r = GetRValue(clr);
	g = GetGValue(clr);
	b = GetBValue(clr);
	//val.Format( "R:%d G:%d B:%d",r,g,b );
	CString val;
	val.Format( "%d,%d,%d",r,g,b );
	SetValue( val );
	GetIEditor()->UpdateViews( eRedrawViewports );
	//GetIEditor()->Notify( eNotify_OnIdleUpdate );
	m_propertyCtrl->InvalidateCtrl();
	GetIEditor()->ResumeUndo();

	if (m_cButton)
		m_cButton->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnFileBrowseButton()
{
	ECustomFileType ftype = EFILE_TYPE_ANY;

	m_propertyCtrl->HideBitmapTooltip();

	if (m_type == ePropertyTexture)
	{
		// Filters for texture.
		ftype = EFILE_TYPE_TEXTURE;
	}
	else if (m_type == ePropertySound)
	{
		// Filters for sounds.
		ftype = EFILE_TYPE_SOUND;
	}
	else if (m_type == ePropertyModel)
	{
		// Filters for models.
		ftype = EFILE_TYPE_GEOMETRY;
	}
	CString startPath = Path::GetPath(m_value);

	/*
	if (ftype == EFILE_TYPE_ANY)
	{
		CString relativeFilename;
		if (CFileUtil::SelectFile( "All Files (*.*)|*.*",startPath,relativeFilename ))
		{
			SetValue( relativeFilename );
		}
	}
	else
	{
	*/
	CString relativeFilename = m_value;
	if (CFileUtil::SelectSingleFile( ftype,relativeFilename,"",startPath ))
	{
		SetValue( relativeFilename );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnShaderBrowseButton()
{
	CShadersDialog cShaders( m_value );
	if (cShaders.DoModal() == IDOK)
	{
		SetValue( cShaders.GetSelection() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReloadValues()
{
	if (m_node)
		ParseXmlNode(false);
	if (m_pVariable)
		SetVariable(m_pVariable);
	for (int i = 0; i < GetChildCount(); i++)
	{
		GetChild(i)->ReloadValues();
	}
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
CString CPropertyItem::GetTip() const
{
	if (!m_tip.IsEmpty())
		return m_tip;

	CString type;
	for (int i = 0; i < NumPropertyTypes; i++)
	{
		if (m_type == s_propertyTypeNames[i].type)
		{
			type = s_propertyTypeNames[i].name;
			break;
		}
	}

	CString tip;
	tip = CString("[")+type+"] ";
	tip += m_name + ": " + m_value;
	
	if (m_pVariable)
	{
		CString description = m_pVariable->GetDescription();
		if (!description.IsEmpty())
		{
			tip += CString("\r\n") + description;
		}
	}
	return tip;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnEditChanged()
{
	ReceiveFromControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnNumberCtrlUpdate( CNumberCtrl *ctrl )
{
	ReceiveFromControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnFillSliderCtrlUpdate( CSliderCtrlEx *ctrl )
{
	if (m_cFillSlider)
	{
		float fValue = m_cFillSlider->GetValue();

		float fStep = powf(10.f, -m_iInternalPrecision) / m_valueMultiplier;
		fValue = int_round(fValue / fStep) * fStep;

		CString val;
		val.Format("%g", fValue);
		SetValue( val );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnAIBehaviorBrowseButton()
{
	CAIDialog aiDlg(m_propertyCtrl);
	aiDlg.SetAIBehavior( m_value );
	if (aiDlg.DoModal() == IDOK)
	{
		SetValue( aiDlg.GetAIBehavior() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnAIAnchorBrowseButton()
{
	CAIAnchorsDialog aiDlg(m_propertyCtrl);
	aiDlg.SetAIAnchor(m_value);
	if (aiDlg.DoModal()==IDOK)
	{
		SetValue(aiDlg.GetAIAnchor());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnAICharacterBrowseButton()
{
	CAICharactersDialog aiDlg(m_propertyCtrl);
	aiDlg.SetAICharacter( m_value );
	if (aiDlg.DoModal() == IDOK)
	{
		SetValue( aiDlg.GetAICharacter() );
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnEquipBrowseButton()
{
	CEquipPackDialog EquipDlg(m_propertyCtrl);
	EquipDlg.SetCurrEquipPack(m_value);
	EquipDlg.SetEditMode(false);
	if (EquipDlg.DoModal()==IDOK)
	{
		SetValue(EquipDlg.GetCurrEquipPack());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnEAXPresetBrowseButton()
{
	CSelectEAXPresetDlg PresetDlg;
	PresetDlg.SetCurrPreset(m_value);
	if (PresetDlg.DoModal()==IDOK)
	{
		SetValue(PresetDlg.GetCurrPreset());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMaterialBrowseButton()
{
	// Open material browser dialog.
	CString name = GetValue();
	IDataBaseItem *pItem = GetIEditor()->GetMaterialManager()->FindItemByName(name);
	GetIEditor()->OpenDataBaseLibrary( EDB_TYPE_MATERIAL,pItem );
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMaterialPickSelectedButton()
{
	// Open material browser dialog.
	IDataBaseItem *pItem = GetIEditor()->GetMaterialManager()->GetSelectedItem();
	if (pItem)
		SetValue( pItem->GetName() );
	else
		SetValue( "" );
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOClassBrowseButton()
{
	CSmartObjectClassDialog soDlg(m_propertyCtrl);
	soDlg.SetSOClass(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOClass());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOClassesBrowseButton()
{
	CSmartObjectClassDialog soDlg(m_propertyCtrl, true);
	soDlg.SetSOClass(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOClass());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOStateBrowseButton()
{
	CSmartObjectStateDialog soDlg(m_propertyCtrl);
	soDlg.SetSOState(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOState());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOStatesBrowseButton()
{
	CSmartObjectStateDialog soDlg(m_propertyCtrl, true);
	soDlg.SetSOState(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOState());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOStatePatternBrowseButton()
{
	CString sPattern = m_value;
	CSmartObjectPatternDialog soPatternDlg(m_propertyCtrl);
	if ( m_value.Find('|') < 0 )
	{
		CSmartObjectStateDialog soDlg(m_propertyCtrl, true, true);
		soDlg.SetSOState(m_value);
		switch (soDlg.DoModal())
		{
		case IDOK:
			SetValue(soDlg.GetSOState());
			return;
		case IDCANCEL:
			return;
		case IDCONTINUE:
			sPattern = soDlg.GetSOState();
			break;
		}
	}
	soPatternDlg.SetPattern(sPattern);
	if (soPatternDlg.DoModal()==IDOK)
		SetValue(soPatternDlg.GetPattern());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOActionBrowseButton()
{
	CSmartObjectActionDialog soDlg(m_propertyCtrl);
	soDlg.SetSOAction(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOAction());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOHelperBrowseButton()
{
	CSmartObjectHelperDialog soDlg(m_propertyCtrl);
	CString value;
	m_pVariable->Get(value);
	int f = value.Find(':');
	if (f > 0)
	{
		soDlg.SetSOHelper(value.Left(f), value.Mid(f+1));
		if (soDlg.DoModal()==IDOK)
			SetValue(soDlg.GetSOHelper());
	}
	else
	{
		AfxMessageBox("This field can not be edited because it needs the smart object class.\nPlease, choose a smart object class first...");
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSONavHelperBrowseButton()
{
	CSmartObjectHelperDialog soDlg(m_propertyCtrl,false,true);
	CString value;
	m_pVariable->Get(value);
	int f = value.Find(':');
	if (f > 0)
	{
		soDlg.SetSOHelper(value.Left(f), value.Mid(f+1));
		if (soDlg.DoModal()==IDOK)
			SetValue(soDlg.GetSOHelper());
	}
	else
	{
		AfxMessageBox("This field can not be edited because it needs the smart object class.\nPlease, choose a smart object class first...");
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOEventBrowseButton()
{
	CSmartObjectEventDialog soDlg(m_propertyCtrl);
	soDlg.SetSOEvent(m_value);
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOEvent());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSOTemplateBrowseButton()
{
	CSmartObjectTemplateDialog soDlg(m_propertyCtrl);
	soDlg.SetSOTemplate( m_value );
	if (soDlg.DoModal()==IDOK)
		SetValue(soDlg.GetSOTemplate());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnGameTokenBrowseButton()
{
	CSelectGameTokenDialog gtDlg(m_propertyCtrl);
	gtDlg.PreSelectGameToken(GetValue());
	if (gtDlg.DoModal()==IDOK)
		SetValue(gtDlg.GetSelectedGameToken());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSequenceBrowseButton()
{
	CSelectSequenceDialog gtDlg(m_propertyCtrl);
	gtDlg.PreSelectItem(GetValue());
	if (gtDlg.DoModal()==IDOK)
		SetValue(gtDlg.GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMissionObjButton()
{
	CSelectMissionObjectiveDialog gtDlg(m_propertyCtrl);
	gtDlg.PreSelectItem(GetValue());
	if (gtDlg.DoModal()==IDOK)
		SetValue(gtDlg.GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnUserBrowseButton()
{
	IVariable::IGetCustomItems* pGetCustomItems = static_cast<IVariable::IGetCustomItems*> (m_pVariable->GetUserData());
	if (pGetCustomItems != 0)
	{
		std::vector<IVariable::IGetCustomItems::SItem> items;
		CString dlgTitle;
		// call the user supplied callback to fill-in items and get dialog title
		bool bShowIt = pGetCustomItems->GetItems(m_pVariable, items, dlgTitle);
		if (bShowIt) // if func didn't veto, show the dialog
		{
			CGenericSelectItemDialog gtDlg(m_propertyCtrl);
			if (pGetCustomItems->UseTree())
			{
				gtDlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
				const char* szSep = pGetCustomItems->GetTreeSeparator();
				if (szSep)
				{
					CString sep (szSep);
					gtDlg.SetTreeSeparator(sep);
				}
			}
			gtDlg.SetItems(items);
			if (dlgTitle.IsEmpty() == false)
				gtDlg.SetTitle(dlgTitle);
			gtDlg.PreSelectItem(GetValue());
			if (gtDlg.DoModal()==IDOK)
				SetValue(gtDlg.GetSelectedItem());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLocalStringBrowseButton()
{
	std::vector<IVariable::IGetCustomItems::SItem> items;
	ILocalizationManager* pMgr = gEnv->pSystem->GetLocalizationManager();
	if (!pMgr)
		return;
	int nCount = pMgr->GetLocalizedStringCount();
	if (nCount <= 0)
		return;
	items.reserve(nCount);
	IVariable::IGetCustomItems::SItem item;
	ILocalizationManager::SLocalizedInfo sInfo;
	for (int i=0; i<nCount; ++i)
	{
		if (pMgr->GetLocalizedInfoByIndex(i, sInfo))
		{
			item.desc = _T("English Text:\r\n");
			item.desc+= sInfo.sEnglish;
			item.name = sInfo.sKey;
			items.push_back(item);
		}
	}
	CString dlgTitle;
	CGenericSelectItemDialog gtDlg(m_propertyCtrl);
	const bool bUseTree = true;
	if (bUseTree)
	{
			gtDlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
			gtDlg.SetTreeSeparator("/");
	}
	gtDlg.SetItems(items);
	gtDlg.SetTitle(_T("Choose Localized String"));
	CString preselect = GetValue();
	if (!preselect.IsEmpty() && preselect.GetAt(0) == '@')
		preselect = preselect.Mid(1);
	gtDlg.PreSelectItem(preselect);
	if (gtDlg.DoModal()==IDOK)
	{
		preselect = "@";
		preselect+=gtDlg.GetSelectedItem();
		SetValue(preselect);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnExpandButton()
{
	m_propertyCtrl->Expand( this,!IsExpanded(),true );
}

//////////////////////////////////////////////////////////////////////////
int	CPropertyItem::GetHeight()
{
	if (m_propertyCtrl->IsExtenedUI())
	{
		//m_nHeight = 20;
		switch (m_type) {
		case ePropertyFloatCurve:
			//m_nHeight = 52;
			return 52;
			break;
		case ePropertyColorCurve:
			//m_nHeight = 36;
			return 36;
		}
		return 20;
	}
	return m_nHeight;
}
