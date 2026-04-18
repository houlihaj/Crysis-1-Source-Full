#include "StdAfx.h"
#include "AnimationGraphTemplate.h"
#include <stack>

class CAnimationGraphTemplate::CChild
{
public:
	CChild( CAnimationGraphTemplate * pTempl, const string& name, bool bAlter ) : m_pTempl(pTempl), m_bAlter(bAlter)
	{
		EOp op = m_bAlter? eOp_AlterChild : eOp_BeginChild;
		m_pTempl->m_ops.push_back( SOp(op, name) );
	}

	~CChild()
	{
		EOp op = m_bAlter? eOp_EndAlterChild : eOp_EndChild;
		m_pTempl->m_ops.push_back( SOp(op) );
	}

private:
	bool m_bAlter;
	CAnimationGraphTemplate * m_pTempl;
};

bool CAnimationGraphTemplate::Init( XmlNodeRef root )
{
	m_ops.resize(0);
	m_params.clear();
	return LoadFromXml( root, true );
}

bool CAnimationGraphTemplate::LoadFromXml( XmlNodeRef node, bool checkParamTag )
{
	bool ok = true;

	m_debugName = node->getAttr("name");

	int nChildren = node->getChildCount();
	for (int i=0; i<nChildren; i++)
	{
		XmlNodeRef childNode = node->getChild(i);
		if (checkParamTag && 0 == strcmp("Param", childNode->getTag()))
		{
			SParam param;
			string name;
			if (!childNode->haveAttr("name"))
			{
				GameWarning("No name for parameter [in template %s]", m_debugName.c_str());
				ok = false;
				continue;
			}
			else
			{
				name = childNode->getAttr("name");
			}
			if (!childNode->haveAttr("type"))
			{
				GameWarning("No type for param '%s'; defaulting to string [in template %s]", name.c_str(), m_debugName.c_str());
				param.type = "string";
				ok = false;
			}
			else
			{
				param.type = childNode->getAttr("type");
			}
			if (!m_params.insert( std::make_pair(name, param) ).second)
			{
				GameWarning("Duplicate param '%s' [in template %s]", name.c_str(), m_debugName.c_str());
				ok = false;
			}
		}
		else if (0 == strcmp("OverrideAttrs", childNode->getTag()))
		{
			for (int a=0; a<childNode->getNumAttributes(); a++)
			{
				const char * name, * value;
				childNode->getAttributeByIndex( a, &name, &value );
				m_ops.push_back( SOp(eOp_OverrideParentAttr, name, value) );
			}
			continue;
		}

		CChild child( this, childNode->getTag(), 0 == strcmp("SelectWhen", childNode->getTag()) );
		ok &= LoadFromXml(childNode, false);

		for (int i=0; i<childNode->getNumAttributes(); i++)
		{
			const char * name, * value;
			childNode->getAttributeByIndex( i, &name, &value );
			EOp op = eOp_ConstantAttr;
			if (strchr(value, '$'))
			{
				op = eOp_ParamAttr;
				for (const char * p = value; *p; ++p)
				{
					switch (*p)
					{
					case ' ':
					case '*':
					case '/':
					case '+':
					case '-':
						op = eOp_ExprAttr;
					}
				}
				if (op == eOp_ParamAttr && m_params.find(value+1) == m_params.end())
				{
					GameWarning("No such parameter '%s' [in template %s]", value, m_debugName.c_str());
					ok = false;
				}
			}
			m_ops.push_back( SOp(op, name, value + (op == eOp_ParamAttr)) );
		}
	}

	return ok;
}

bool CAnimationGraphTemplate::ProcessTemplate( XmlNodeRef templ ) const
{
	XmlNodeRef parent = templ->getParent();
	std::stack<XmlNodeRef> processing;
	processing.push(parent);

	m_stateName = parent->getAttr("id");

	bool inGame = true;
	if (parent->getAttr("includeInGame", inGame))
		if (!inGame)
			return true;

	bool ok = true;

	for (std::vector<SOp>::const_iterator iter = m_ops.begin(); iter != m_ops.end(); ++iter)
	{
		switch (iter->what)
		{
		case eOp_BeginChild:
			processing.push( processing.top()->createNode(iter->name.c_str()) );
			break;
		case eOp_EndChild:
			{
				assert(processing.size() > 1);
				XmlNodeRef node = processing.top();
				processing.pop();
				processing.top()->addChild( node );
			}
			break;
		case eOp_AlterChild:
			{
				XmlNodeRef node = processing.top()->findChild(iter->name.c_str());
				if (!node)
				{
					node = processing.top()->createNode(iter->name.c_str());
					processing.top()->addChild(node);
				}
				processing.push(node);
			}
			break;
		case eOp_EndAlterChild:
			assert(processing.size() > 1);
			processing.pop();
			break;
		case eOp_ConstantAttr:
			processing.top()->setAttr( iter->name.c_str(), iter->expr.c_str() );
			break;
		case eOp_ParamAttr:
			if (!templ->haveAttr(iter->expr.c_str()))
			{
				//GameWarning("No value set for template parameter '%s'", iter->expr.c_str());
				//ok = false;
			}
			else
			{
				processing.top()->setAttr( iter->name.c_str(), templ->getAttr(iter->expr.c_str()) );
			}
			break;
		case eOp_ExprAttr:
			{
				string value;
				if (!ComputeExprAttr( value, templ, iter->expr ))
				{
					GameWarning("Unable to evaluate value for template parameter '%s' (expression is '%s') [template %s, state %s]", iter->name.c_str(), iter->expr.c_str(), m_debugName.c_str(), m_stateName.c_str());
					ok = false;
				}
				else
					processing.top()->setAttr( iter->name.c_str(), value.c_str() );
			}
			break;
		case eOp_OverrideParentAttr:
			parent->setAttr(iter->name.c_str(), iter->expr.c_str());
			break;
		case eOp_Noop:
			break;
		}
	}

	assert(processing.size() == 1);

	return ok;
}

bool CAnimationGraphTemplate::ComputeExprAttr( string& out, XmlNodeRef templ, const string& expr_ ) const
{
	string expr = expr_ + " ";

	static const char * agTempVar = "__animation_graph_temporary_variable";

	out.resize(0);

	string lua;
	lua.reserve(expr.size());

	lua += agTempVar;
	lua += " = tostring( ";

	enum EState
	{
		COPYDATA,
		BUILDPARAM,
	};
	EState state = COPYDATA;

	string param;

	for (string::const_iterator iter = expr.begin(); iter != expr.end(); ++iter)
	{
		switch (state)
		{
		case COPYDATA:
			if (*iter == '$')
				state = BUILDPARAM;
			else
				lua += *iter;
			break;
		case BUILDPARAM:
			switch (*iter)
			{
			case ' ':
			case '+':
			case '-':
			case '*':
			case '/':
				{
					std::map<string, SParam>::const_iterator miter = m_params.find(param);
					string value = templ->getAttr(param);
					if (miter == m_params.end() || miter->second.type.empty())
					{
						GameWarning( "No such parameter '%s' [template %s, state %s]", param.c_str(), m_debugName.c_str(), m_stateName.c_str() );
						return false;
					}
					else if (value.empty())
					{
						if (miter->second.type == "string")
							lua += "''";
						else
							lua += "0";
					}
					else if (miter->second.type == "string")
						lua += "'" + value + "'";
					else
						lua += value;

					state = COPYDATA;
					lua += *iter;
					param.resize(0);
				}
				break;
			default:
				param += *iter;
			}
			break;
		}
	}

	lua += ")";

	IScriptSystem * pSS = gEnv->pScriptSystem;
	if (!pSS->ExecuteBuffer( lua.c_str(), lua.size(), "temp_anim_graph_stuff" ))
	{
		GameWarning("Failed to execute lua buffer '%s' [template %s, state %s]", lua.c_str(), m_debugName.c_str(), m_stateName.c_str());
		return false;
	}
	const char * pValue;
	if (!pSS->GetGlobalValue( agTempVar, pValue ))
	{
		GameWarning("Failed to fetch result of '%s' [template %s, state %s]", lua.c_str(), m_debugName.c_str(), m_stateName.c_str());
		return false;
	}

	out = pValue;

	return true;
}
