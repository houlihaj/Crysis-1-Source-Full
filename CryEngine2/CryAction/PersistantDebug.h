#ifndef __PERSISTANTDEBUG_H__
#define __PERSISTANTDEBUG_H__

#pragma once

#include <list>
#include <map>

#include "IGameFramework.h"

class CPersistantDebug : public IPersistantDebug
{
public:
	void Begin( const char * name, bool clear );
	void AddSphere( const Vec3& pos, float radius, ColorF clr, float timeout );
	void AddDirection( const Vec3& pos, float radius, const Vec3& dir, ColorF clr, float timeout );
	void AddLine( const Vec3& pos1, const Vec3& pos2, ColorF clr, float timeout );
	void AddPlanarDisc( const Vec3& pos, float innerRadius, float outerRadius, ColorF clr, float timeout );
	void Add2DText ( const char * text, float size, ColorF clr, float timeout );
	void Add2DLine( float x1, float y1, float x2, float y2, ColorF clr, float timeout );
	void AddText ( float x, float y, float size, ColorF clr, float timeout, const char * fmt, ... );
	void AddQuat(const Vec3& pos, const Quat& q, float r, ColorF clr, float timeout);

	void Update( float frameTime );
	void PostUpdate ( float frameTime );
	void Reset();

	CPersistantDebug();

private:

	IFFont *m_pDefaultFont;

	enum EObjType
	{
		eOT_Sphere,
		eOT_Arrow,
		eOT_Line,
		eOT_Disc,
		eOT_Text,
		eOT_Line2D,
		eOT_Quat,
	};
	struct SObj
	{
		EObjType obj;
		ColorF clr;
		float timeRemaining;
		float totalTime;
		float radius;
		float radius2;
		Vec3 pos;
		Vec3 dir;
		Quat q;
		string text;
	};

	struct STextObj2D
	{
		string text;
		ColorF clr;
    float size;
		float timeRemaining;
		float totalTime;
	};

	typedef std::list<SObj> ListObj;
	typedef std::map<string, ListObj> MapListObj;
	typedef std::list<STextObj2D> ListObjText2D;   // 2D objects need a separate pass, so we put it into another list
	MapListObj m_objects;
	MapListObj::iterator m_current;
	ListObjText2D m_2DTexts;

};

#endif
