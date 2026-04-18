#ifndef __ANIMATIONGRAPHTESTER_H__
#define __ANIMATIONGRAPHTESTER_H__

#pragma once

class CAnimationGraphDialog;

class CAnimationGraphTester : public CXTPTaskPanel
{
public:
	void Init( CAnimationGraphDialog * pParent );
	void Reload();

	void OnCommand(int cmd);

private:
	CAnimationGraphDialog * m_pParent;
	CXTPTaskPanelGroup * m_pGroup;

	void AddVerb( CString name, int id );
};

#endif
