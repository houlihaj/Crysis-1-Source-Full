////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   propertyitem.h
//  Version:     v1.00
//  Created:     5/6/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __propertyitem_h__
#define __propertyitem_h__

#if _MSC_VER > 1000
#pragma once
#endif

//! All possible property types.
enum PropertyType
{
	ePropertyInvalid = 0,
	ePropertyTable = 1,
	ePropertyBool = 2,
	ePropertyInt,
	ePropertyFloat,
	ePropertyVector,
	ePropertyString,
	ePropertyColor,
	ePropertyAngle,
	ePropertyFloatCurve,
	ePropertyColorCurve,
	ePropertyFile,
	ePropertyTexture,
	ePropertySound,
	ePropertyModel,
	ePropertySelection,
	ePropertyList,
	ePropertyShader,
	ePropertyMaterial,
	ePropertyAiBehavior,
	ePropertyAiAnchor,
	ePropertyAiCharacter,
	ePropertyEquip,
	ePropertyReverbPreset,
	ePropertyLocalString,
	ePropertySOClass,
	ePropertySOClasses,
	ePropertySOState,
	ePropertySOStates,
	ePropertySOStatePattern,
	ePropertySOAction,
	ePropertySOHelper,
	ePropertySONavHelper,
	ePropertySOEvent,
	ePropertySOTemplate,
	ePropertyGameToken,
	ePropertySequence,
	ePropertyMissionObj,
	ePropertyUser
};

// forward declarations.
class CNumberCtrl;
class CPropertyCtrl;
class CInPlaceEdit;
class CInPlaceComboBox;
class CFillSliderCtrl;
class CSplineCtrl;
class CColorGradientCtrl;
class CSliderCtrlEx;
class CInPlaceColorButton;
struct IVariable;

class CInPlaceButton;
class CInPlaceCheckBox;

/** Item of CPropertyCtrl.
		Every property item reflects value of single XmlNode.
*/
class CPropertyItem : public CRefCountBase
{
public:
  // Variables.
  // Constructors.
  CPropertyItem( CPropertyCtrl* pCtrl );
	virtual ~CPropertyItem();

	//! Set xml node to this property item.
	virtual void SetXmlNode( XmlNodeRef &node );

	//! Set variable. 
	virtual void SetVariable( IVariable *var );

	//! Get Variable.
	IVariable* GetVariable() const { return m_pVariable; }

	/** Get type of property item.
	*/
	virtual int GetType() { return m_type; }
  
	/** Get name of property item.
	*/
	virtual CString GetName() const { return m_name; };

	/** Set name of property item.
	*/
	virtual void SetName( const char *sName ) { m_name = sName; };

	/** Called when item becomes selected.
	*/
	virtual void SetSelected( bool selected );

	/** Get if item is selected.
	*/
	bool IsSelected() const { return m_bSelected; };

	/** Get if item is currently expanded.
	*/
	bool IsExpanded() const { return m_bExpanded; };

	/** Get if item can be expanded (Have children).
	*/
	bool IsExpandable() const { return m_bExpandable; };

	/** Check if item cannot be category.
	*/
	bool IsNotCategory() const { return m_bNoCategory; };

	/** Check if item must be bold.
	*/
	bool IsBold() const;

	/** Check if item must be disabled.
	*/
	bool IsDisabled() const;

	/** Get height of this item.
	*/
	virtual int	GetHeight();
	
	/** Called by PropertyCtrl to draw value of this item.
	*/
	virtual void DrawValue( CDC *dc,CRect rect );

	/** Called by PropertyCtrl when item selected to creare in place editing control.
	*/
	virtual void CreateInPlaceControl( CWnd* pWndParent,CRect& rect );
	/** Called by PropertyCtrl when item deselected to destroy in place editing control.
	*/
	virtual void DestroyInPlaceControl( bool bRecursive=false );

	virtual void CreateControls( CWnd* pWndParent,CRect& textRect,CRect& ctrlRect );

	/** Move in place control to new position.
	*/
	virtual void MoveInPlaceControl( const CRect& rect );

	/** Set Focus to inplace control.
	*/
	virtual void SetFocus();

	/** Set data from InPlace control to Item value.
	*/
	virtual void SetData( CWnd* pWndInPlaceControl ){};

	//////////////////////////////////////////////////////////////////////////
	// Mouse notifications.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnLButtonDown( UINT nFlags,CPoint point );
	virtual void OnRButtonDown( UINT nFlags,CPoint point ) {};
	virtual void OnLButtonDblClk( UINT nFlags,CPoint point );
	virtual void OnMouseWheel( UINT nFlags,short zDelta,CPoint point );

	/** Changes value of item.
	*/
	virtual void SetValue( const char* sValue,bool bRecordUndo=true,bool bForceModified=false );

	/** Returns current value of property item.
	*/
	virtual const char* GetValue() const;

	/** Get Item's XML node.
	*/
	XmlNodeRef&	GetXmlNode() { return m_node; };

	//////////////////////////////////////////////////////////////////////////
	//! Get description of this item.
	CString GetTip() const;

	//! Return image index of this property.
	int GetImage() const { return m_image; };

	//! Return true if this property item is modified.
	bool IsModified() const { return m_modified; }

	bool HasDefaultValue( bool bChildren = false ) const;

	//////////////////////////////////////////////////////////////////////////
	// Childs.
	//////////////////////////////////////////////////////////////////////////
	//! Expand child nodes.
	virtual void SetExpanded( bool expanded );
	
	//! Reload Value from Xml Node (hierarchicaly reload children also).
	virtual void ReloadValues();

	//! Get number of child nodes.
	int GetChildCount() const { return m_childs.size(); };
	//! Get Child by id.
	CPropertyItem* GetChild( int index ) const { return m_childs[index]; };

	//! Parent of this item.
	CPropertyItem* GetParent() const { return m_parent; };

	//! Add Child item.
	void AddChild( CPropertyItem *item );

	//! Delete child item.
	void RemoveChild( CPropertyItem *item );

  //! Delete all child items
  void RemoveAllChildren();

	//! Find item that reference specified property.
	CPropertyItem* FindItemByVar( IVariable *pVar );
	//! Get full name, including names of all parents.
	virtual CString GetFullName() const;
	//! Find item by full specified item.
	CPropertyItem* FindItemByFullName( const CString &name );

	void ReceiveFromControl();

protected:
	//////////////////////////////////////////////////////////////////////////
	// Private methods.
	//////////////////////////////////////////////////////////////////////////
  void SendToControl();
	void CheckControlActiveColor();

	void OnChildChanged( CPropertyItem *child );

	void OnEditChanged();
	void OnNumberCtrlUpdate( CNumberCtrl *ctrl );
	void OnFillSliderCtrlUpdate( CSliderCtrlEx *ctrl );
	void OnNumberCtrlBeginUpdate( CNumberCtrl *ctrl ) {};
	void OnNumberCtrlEndUpdate( CNumberCtrl *ctrl ) {};
	void OnSplineCtrlUpdate( CSplineCtrl* ctrl );

	void OnComboSelection();

	void OnCheckBoxButton();
	void OnColorBrowseButton();
	void OnColorChange( COLORREF col );
	void OnFileBrowseButton();
	void OnShaderBrowseButton();
	void OnAIBehaviorBrowseButton();
	void OnAIAnchorBrowseButton();
	void OnAICharacterBrowseButton();
	void OnEquipBrowseButton();
	void OnEAXPresetBrowseButton();
	void OnMaterialBrowseButton();
	void OnMaterialPickSelectedButton();
	void OnGameTokenBrowseButton();
	void OnSequenceBrowseButton();
	void OnMissionObjButton();
	void OnUserBrowseButton();
	void OnLocalStringBrowseButton();
	void OnExpandButton();

	void OnSOClassBrowseButton();
	void OnSOClassesBrowseButton();
	void OnSOStateBrowseButton();
	void OnSOStatesBrowseButton();
	void OnSOStatePatternBrowseButton();
	void OnSOActionBrowseButton();
	void OnSOHelperBrowseButton();
	void OnSONavHelperBrowseButton();
	void OnSOEventBrowseButton();
	void OnSOTemplateBrowseButton();

	void ParseXmlNode( bool bRecursive=true );

	//! String to color.
	COLORREF StringToColor( const CString &value );
	//! String to boolean.
	bool GetBoolValue();

	//! Convert variable value to value string.
	void VarToValue();
	CString GetDrawValue();

	//! Convert from value to variable.
	void ValueToVar();

	//! Release used variable. 
	void ReleaseVariable();
	//! Callback called when variable change.
	void OnVariableChange( IVariable* var );
	
private:
	CString m_name;
	PropertyType m_type;

	CString m_value;

	//////////////////////////////////////////////////////////////////////////
	// Flags for this property item.
	//////////////////////////////////////////////////////////////////////////
	//! True if item selected.
	unsigned int m_bSelected : 1;
	//! True if item currently expanded
	unsigned int m_bExpanded : 1;
	//! True if item can be expanded
	unsigned int m_bExpandable : 1;
	//! True if children can be edited in parent
	unsigned int m_bEditChildren : 1;
	//! True if children displayed in parent field.
	unsigned int m_bShowChildren : 1;
	//! True if item can not be category.
	unsigned int m_bNoCategory : 1;
	//! If tru ignore update that comes from childs.
	unsigned int m_bIgnoreChildsUpdate : 1;
	//! If can move in place controls.
	unsigned int m_bMoveControls : 1;
	//! True if item modified.
	unsigned int m_modified : 1;

	// Used for number controls.
	float m_rangeMin;
	float m_rangeMax;
	bool  m_bHardMin, m_bHardMax; // Values really limited by this range.
	int		m_iInternalPrecision;		//!< m.m. internal precision (digits behind the comma, only used for floats)
	int   m_nHeight;

	// Xml node.
	XmlNodeRef m_node;

	//! Pointer to the variable for this item.
	TSmartPtr<IVariable> m_pVariable;

	//////////////////////////////////////////////////////////////////////////
	// InPlace controls.
	CColorCtrl<CStatic>* m_pStaticText;
	CNumberCtrl* m_cNumber;
	CNumberCtrl* m_cNumber1;
	CNumberCtrl* m_cNumber2;
	CFillSliderCtrl *m_cFillSlider;
	CInPlaceEdit* m_cEdit;
	CSplineCtrl* m_cSpline;
	CColorGradientCtrl* m_cColorSpline;
	CInPlaceComboBox* m_cCombo;
	CInPlaceButton* m_cButton;
	CInPlaceButton* m_cButton2;
	CInPlaceColorButton* m_cExpandButton;
	CInPlaceCheckBox* m_cCheckBox;
	CInPlaceColorButton* m_cColorButton;
	//////////////////////////////////////////////////////////////////////////

	//! Owner property control.
	CPropertyCtrl* m_propertyCtrl;

	//! Parent item.
	CPropertyItem* m_parent;

	// Enum.
	IVarEnumListPtr m_enumList;
	CUIEnumsDatabase_SEnum* m_pEnumDBItem;

	CString m_tip;
	int m_image;

	//! Last modified time in seconds.
	float m_lastModified;

	float m_valueMultiplier;

	// Childs.
	typedef std::vector<TSmartPtr<CPropertyItem> > Childs;
	Childs m_childs;

	//////////////////////////////////////////////////////////////////////////
	friend class CPropertyCtrlEx;
	int m_nCategoryPageId;
};

typedef _smart_ptr<CPropertyItem> CPropertyItemPtr;

#endif // __propertyitem_h__
