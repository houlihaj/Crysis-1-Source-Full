////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FaceController.h
//  Version:     v1.00
//  Created:     7/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FaceEffector.h"
#include "FaceEffectorLibrary.h"

//////////////////////////////////////////////////////////////////////////
float CFacialEffCtrl::Evaluate( float fInput )
{
	switch (m_type)
	{
	case CTRL_LINEAR:
		return fInput*m_fWeight;
	case CTRL_SPLINE:
		{
			float fOut = 0;
			interpolate( fInput,fOut );
			//fOut=fInput;
			return fOut;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffCtrl::SetType( ControlType t )
{
	if (t == m_type)
		return;
	
	if (t == CTRL_SPLINE)
	{
		m_type = t;
		InsertKeyFloat(-1,-m_fWeight);
		InsertKeyFloat(1,m_fWeight);
	}
	else if (t == CTRL_LINEAR)
	{
		clear();
		m_type = t;
	}
};

//////////////////////////////////////////////////////////////////////////
void CFacialEffCtrl::SetConstantWeight( float fWeight )
{
	m_fWeight = fWeight;
	if (m_type == CTRL_SPLINE)
	{
		InsertKeyFloat(-1,-fWeight);
		InsertKeyFloat(1,fWeight);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffCtrl::SetConstantBalance( float fBalance )
{
	m_fBalance = fBalance;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffCtrl::SerializeSpline( XmlNodeRef &node,bool bLoading )
{
	if (bLoading)
	{
		clear();

		const char* attribute = node->getAttr("Keys");
		const char* position = attribute;
		for (;;)
		{
			char* end;

			// Skip to the next digit.
			for (; *position && !(*position >= '0' && *position <= '9') && *position != '.' && *position != '-'; ++position);
			if (!*position)
				break;

			// Read in the time.
			float time = float(strtod(position, &end));
			if (position == end || !end)
				break;
			position = end;

			// Skip to the next digit.
			for (; *position && !(*position >= '0' && *position <= '9') && *position != '.' && *position != '-'; ++position);
			if (!*position)
				break;

			// Read in the value.
			float value = float(strtod(position, &end));
			if (position == end || !end)
				break;
			position = end;

			// Insert the key we have created.
			InsertKeyFloat(time, value);
		}
	}
	else
	{
		string keystr;
		string s;
		for (int i = 0; i < num_keys(); i++)
		{
			s.Format("%g:%g,",key(i).time,key(i).value );
			keystr += s;
		}
		node->setAttr( "Keys",keystr );
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffCtrl::Serialize( CFacialEffectorsLibrary *pLibrary,XmlNodeRef &node,bool bLoading )
{
	// MichaelS - Don't serialize m_fBalance - currently this is only used in the preview mode in the facial editor.

	if (bLoading)
	{
		int nType = CTRL_LINEAR;
		m_fWeight = 0;
		node->getAttr( "Type",nType );
		node->getAttr( "Weight",m_fWeight );
		m_type = (ControlType)nType;
		if (m_type == CTRL_SPLINE)
			SerializeSpline( node,bLoading );
		const char *sEffectorName = node->getAttr("Effector");
		m_pEffector = (CFacialEffector*)pLibrary->Find( sEffectorName );
	}
	else
	{
		if (m_type != CTRL_LINEAR)
			node->setAttr( "Type",m_type );
		if (m_fWeight != 0)
			node->setAttr( "Weight",m_fWeight );
		if (m_type == CTRL_SPLINE)
			SerializeSpline( node,bLoading );
		node->setAttr( "Effector",m_pEffector->GetName() );
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CFacialEffector::GetSubEffector( int nIndex )
{
	assert( nIndex >= 0 && nIndex < (int)m_subEffectors.size() );
	IFacialEffCtrl *pCtrl = m_subEffectors[nIndex];
	if (pCtrl)
		return pCtrl->GetEffector();
	return 0;
};

//////////////////////////////////////////////////////////////////////////
IFacialEffCtrl* CFacialEffector::GetSubEffCtrl( int nIndex )
{
	assert( nIndex >= 0 && nIndex < (int)m_subEffectors.size() );
	return m_subEffectors[nIndex];
}

//////////////////////////////////////////////////////////////////////////
IFacialEffCtrl* CFacialEffector::AddSubEffector( IFacialEffector *pEffector )
{
	CFacialEffCtrl *pCtrl = new CFacialEffCtrl;
	pCtrl->SetCEffector( (CFacialEffector*)pEffector );
	m_subEffectors.push_back(pCtrl);
	return pCtrl;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffector::RemoveSubEffector( IFacialEffector *pEffector )
{
	int nSize = m_subEffectors.size();
	for (int i = 0; i < nSize; i++)
	{
		if (m_subEffectors[i]->GetCEffector() == pEffector)
		{
			m_subEffectors.erase( m_subEffectors.begin() + i );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffector::RemoveAllSubEffectors()
{
	m_subEffectors.clear();
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffector::SetName( const char *sNewName )
{
	if (m_pLibrary)
		m_pLibrary->RenameEffector( this,sNewName );
  m_name = sNewName;
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffector::Serialize( XmlNodeRef &node,bool bLoading )
{

}

//////////////////////////////////////////////////////////////////////////
// CFacialEffectorAttachment Implementation
//////////////////////////////////////////////////////////////////////////
void CFacialEffectorAttachment::SetParamString( EFacialEffectorParam param,const char *str )
{
	switch (param)
	{
	case EFE_PARAM_BONE_NAME:
		m_sAttacment = str;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialEffectorAttachment::GetParamString( EFacialEffectorParam param )
{
	switch (param)
	{
	case EFE_PARAM_BONE_NAME:
		return m_sAttacment;
		break;
	}
	return CFacialEffector::GetParamString(param);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorAttachment::SetParamVec3( EFacialEffectorParam param,Vec3 vValue )
{
	switch (param)
	{
	case EFE_PARAM_BONE_ROT_AXIS:
		m_vAngles = vValue;
		break;
	case EFE_PARAM_BONE_POS_AXIS:
		m_vOffset = vValue;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3 CFacialEffectorAttachment::GetParamVec3( EFacialEffectorParam param )
{
	switch (param)
	{
	case EFE_PARAM_BONE_ROT_AXIS:
		return m_vAngles;
		break;
	case EFE_PARAM_BONE_POS_AXIS:
		return m_vOffset;
		break;
	}
	return Vec3(0,0,0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialEffectorAttachment::Serialize( XmlNodeRef &node,bool bLoading )
{
	if (bLoading)
	{
		m_sAttacment = node->getAttr( "Attachment" );
		node->getAttr( "PosOffset",m_vOffset );
		node->getAttr( "RotOffset",m_vAngles );
	}
	else
	{
		node->setAttr( "Attachment",m_sAttacment );
		node->setAttr( "PosOffset",m_vOffset );
		node->setAttr( "RotOffset",m_vAngles );
	}
}
