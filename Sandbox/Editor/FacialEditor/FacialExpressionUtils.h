////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FacialExpressionUtils.h
//  Version:     v1.00
//  Created:     27/10/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FACIALEXPRESSIONUTILS_H__
#define __FACIALEXPRESSIONUTILS_H__

#include "VectorSet.h"

struct IFacialEffectorsLibrary;
struct IFacialEffector;

namespace FacialExpressionUtils
{
	template <typename T> class ControlledItemIterator
	{
	public:
		typedef T ItemController;
		typedef typename ItemController::Cursor Cursor;
		typedef typename ItemController::Item Item;

		ControlledItemIterator(): controller(0), cursor() {}
		ControlledItemIterator(const ControlledItemIterator& other): controller(other.controller), cursor(other.cursor) {}
		ControlledItemIterator(ItemController& controller, const Cursor& cursor): controller(&controller), cursor(cursor) {}

		Item operator*() {return GetControlledItem(*controller, cursor);}
		ControlledItemIterator& operator++() {cursor = AdvanceCursor(*controller, cursor); return *this;}
		ControlledItemIterator operator++(int) {ControlledItemIterator copy = *this; ++*this; return copy;}
		bool operator==(const ControlledItemIterator& other) {return controller == other.controller && cursor == other.cursor;}
		bool operator!=(const ControlledItemIterator& other) {return controller != other.controller || cursor != other.cursor;}

	private:
		ItemController* controller;
		Cursor cursor;
	};

	class FacialExpressionLibraryOrphanFinder
	{
		template <typename T> friend class ControlledItemIterator;
		class Entry
		{
		public:
			Entry(IFacialEffector* pEffector): pEffector(pEffector), count(0) {}
			bool operator<(const Entry& other) const {return pEffector < other.pEffector;}
			IFacialEffector* pEffector;
			int count;
		};
	public:
		typedef VectorSet<Entry>::iterator Cursor;
		typedef IFacialEffector* Item;

		typedef ControlledItemIterator<FacialExpressionLibraryOrphanFinder> Iterator;

		FacialExpressionLibraryOrphanFinder(IFacialEffectorsLibrary* pLibrary);
		Iterator Begin();
		Iterator End();

		IFacialEffector* GetExpression(VectorSet<Entry>::iterator cursor);
		VectorSet<Entry>::iterator GetNextOrphanIndex(VectorSet<Entry>::iterator cursor);

	private:
		void FindOrphans();
		VectorSet<Entry>::iterator SkipNonOrphans(VectorSet<Entry>::iterator cursor);

		IFacialEffectorsLibrary* m_pLibrary;
		bool m_bDirty;

		typedef VectorSet<Entry> EntryContainer;
		EntryContainer m_entries;
	};
	inline IFacialEffector* GetControlledItem(FacialExpressionLibraryOrphanFinder& controller, FacialExpressionLibraryOrphanFinder::Cursor cursor) {return controller.GetExpression(cursor);}
	inline FacialExpressionLibraryOrphanFinder::Cursor AdvanceCursor(FacialExpressionLibraryOrphanFinder& controller, FacialExpressionLibraryOrphanFinder::Cursor cursor) {return controller.GetNextOrphanIndex(cursor);}

	IFacialEffector* GetGarbageFacialExpressionFolder(IFacialEffectorsLibrary* pLibrary, bool createIfNecessary = true);
	int AddOrphansToGarbageFacialExpressionFolder(IFacialEffectorsLibrary* pLibrary);
	int DeleteOrphans(IFacialEffectorsLibrary* pLibrary);
	void RemoveFromGarbage(IFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffector);
	bool IsExpressionInGarbage(IFacialEffectorsLibrary* pLibrary, IFacialEffector* pEffector);
}

#endif //__FACIALEXPRESSIONUTILS_H__
