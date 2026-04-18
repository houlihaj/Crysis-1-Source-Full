////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialEffectorsLibrary.h
//  Version:     v1.00
//  Created:     7/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FacialEffectorsLibrary_h__
#define __FacialEffectorsLibrary_h__
#pragma once

#include "FaceEffector.h"

class CFacialAnimation;

//////////////////////////////////////////////////////////////////////////
class CFacialEffectorsLibrary : public IFacialEffectorsLibrary
{
public:
	CFacialEffectorsLibrary( CFacialAnimation *pFaceAnim );
	~CFacialEffectorsLibrary();

	//////////////////////////////////////////////////////////////////////////
	// IFacialEffectorsLibrary interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void AddRef() { ++m_nRefCounter; }
	virtual void Release() { if (--m_nRefCounter <= 0) delete this; }

	virtual void SetName( const char *name ) { m_name = name; };
	virtual const char* GetName() { return m_name.c_str(); };

	virtual IFacialEffector* Find( const char *sName );
	virtual IFacialEffector* GetRoot();

	virtual void VisitEffectors(IFacialEffectorsLibraryEffectorVisitor* pVisitor);
	
	IFacialEffector* CreateEffector( EFacialEffectorType nType,const char *sName );
	virtual void RemoveEffector(IFacialEffector* pEffector);

	virtual void Serialize( XmlNodeRef &node,bool bLoading );
	virtual void SerializeEffector( IFacialEffector *pEffector,XmlNodeRef &node,bool bLoading );
	//////////////////////////////////////////////////////////////////////////

	int GetLastIndex() const { return m_nLastIndex; }
	void RenameEffector( CFacialEffector *pEffector,const string &newstr );
	
	CFacialEffector* GetEffectorFromIndex( int nIndex );

	void MergeLibrary(IFacialEffectorsLibrary* pMergeLibrary, const Functor1wRet<const char*, MergeCollisionAction>& collisionStrategy);

	int GetEffectorCount() const { return m_nameToEffectorMap.size(); }

private:
	void NewRoot();
	void SerializeEffectorSelf( CFacialEffector *pEffector,XmlNodeRef &node,bool bLoading );
	void SerializeEffectorSubCtrls( CFacialEffector *pEffector,XmlNodeRef &node,bool bLoading );
	void SetEffectorIndex( CFacialEffector *pEffector );
	void ReplaceEffector(CFacialEffector* pOriginalEffector, CFacialEffector* pNewEffector);

private:
	friend class CFacialModel;

	int m_nRefCounter;
	string m_name;

	CFacialAnimation *m_pFacialAnim;

	typedef std::map<string,_smart_ptr<CFacialEffector>,stl::less_stricmp<const char*> > NameToEffectorMap;
	NameToEffectorMap m_nameToEffectorMap;

	_smart_ptr<CFacialEffector> m_pRoot;

	std::vector<_smart_ptr<CFacialEffector> > m_indexToEffector;

	int m_nLastIndex;
};

#endif // __FacialEffectorsLibrary_h__
