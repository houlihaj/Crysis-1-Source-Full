#ifndef __ANIMATIONGRAPHTEMPLATE_H__
#define __ANIMATIONGRAPHTEMPLATE_H__

#pragma once

class CAnimationGraphTemplate : public _reference_target_t
{
public:
	bool Init( XmlNodeRef root );
	bool ProcessTemplate( XmlNodeRef templ ) const;

private:
	bool ComputeExprAttr( string& out, XmlNodeRef templ, const string& expr ) const;
	bool LoadFromXml( XmlNodeRef node, bool checkParamTag );

	enum EOp
	{
		eOp_BeginChild,
		eOp_EndChild,
		eOp_AlterChild,
		eOp_EndAlterChild,
		eOp_ConstantAttr,
		eOp_ParamAttr,
		eOp_ExprAttr,
		eOp_OverrideParentAttr,
		eOp_Noop,
	};

	struct SOp
	{
		SOp( EOp what = eOp_Noop, string name = "", string expr = "" ) { this->what = what; this->name = name; this->expr = expr; }
		EOp what;
		string name;
		string expr;
	};

	struct SParam
	{
		string type;
	};

	class CChild;

	string m_debugName;
	mutable string m_stateName;
	std::vector<SOp> m_ops;
	std::map<string, SParam> m_params;
};

typedef _smart_ptr<CAnimationGraphTemplate> CAnimationGraphTemplatePtr;

#endif
