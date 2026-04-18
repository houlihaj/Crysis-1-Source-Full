//---------------------------------------------------------------------------
// Copyright 2005 Crytek GmbH
// Created by: Michael Smith
//---------------------------------------------------------------------------
#ifndef __ANIMEVENTLISTVIEW_H__
#define __ANIMEVENTLISTVIEW_H__

class IAnimEventView;
class CObjectListCtrl;

IAnimEventView* AnimEventListView_Create(CObjectListCtrl* pListCtrl, const std::vector<string>& bones);

#endif //__ANIMEVENTLISTVIEW_H__
