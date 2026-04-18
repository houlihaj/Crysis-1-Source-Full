#include "StdAfx.h"
#include "ICompressionPolicy.h"
#include "ArithModel.h"
#include "AdaptiveFloat.h"

class CAdaptiveOrientationPolicy
{
public:
	bool Load( XmlNodeRef node, const string& filename )
	{
		return 
			m_axis[0].Load(node, filename, "Params") &&
			m_axis[1].Load(node, filename, "Params") &&
			m_axis[2].Load(node, filename, "Params");
	}

	bool ReadMemento( CByteInputStream& in ) const
	{
		for (int i=0; i<3; i++)
			m_axis[i].ReadMemento(in);
		m_halfSphere = in.GetTyped<bool>();
		return true;
	}

	bool WriteMemento( CByteOutputStream& out ) const
	{
		for (int i=0; i<3; i++)
			m_axis[i].WriteMemento(out);
		out.PutTyped<bool>() = m_halfSphere;
		return true;
	}

	void NoMemento() const
	{
		for (int i=0; i<3; i++)
			m_axis[i].NoMemento();
		m_halfSphere = false;
	}

	bool ReadValue( CCommInputStream& in, Quat& value, CArithModel * pModel, uint32 age ) const
	{
		bool ok = true;
		for (int i=0; ok && i<3; i++)
			ok &= m_axis[i].ReadValue(in, value.v[i], age);
		if (!ok)
			return false;

		uint8 changedHalfSphere = in.DecodeShift(4);
		if(changedHalfSphere >= 15)
		{
			m_halfSphere = !m_halfSphere;
			in.UpdateShift(4, 15, 1);
		}
		else
			in.UpdateShift(4, 0, 15);

		if(1 - value.v.x*value.v.x - value.v.y*value.v.y - value.v.z*value.v.z > 0)
			value.w = sqrtf(1 - value.v.x*value.v.x - value.v.y*value.v.y - value.v.z*value.v.z);
		else
			value.w = 0;
		if(!m_halfSphere)
			value.w *= -1;
		if(!(fabsf(value.GetLength() - 1.0f) < 0.001f))
			value.Normalize();

		return ok;
	}
	bool WriteValue( CCommOutputStream& out, Quat value, CArithModel * pModel, uint32 age ) const
	{
		if(fabsf(value.GetLength() - 1.0f) > 0.001f)
			value.Normalize();

		for (int i=0; i<3; i++)
			m_axis[i].WriteValue(out, value.v[i], age);

		uint8		changedHalfSphere = 0;
		bool   newHalfSphere = (value.w >= 0);
		if(newHalfSphere != m_halfSphere)
			changedHalfSphere = 1;
		if(changedHalfSphere)
		{
			out.EncodeShift(4, 15, 1);
			m_halfSphere = newHalfSphere;
		}
		else
			out.EncodeShift(4, 0, 15);

		return true;
	}

	template <class T>
	bool ReadValue( CCommInputStream& in, T& value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}
	template <class T>
	bool WriteValue( CCommOutputStream& out, T value, CArithModel * pModel, uint32 age ) const
	{
		NetWarning("AdaptiveOrientationPolicy: not implemented for generic types");
		return false;
	}

	void GetMemoryStatistics(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CAdaptiveOrientationPolicy");
		pSizer->Add(*this);
	}

private:
	CAdaptiveFloat m_axis[3];
	mutable bool m_halfSphere;
};

REGISTER_COMPRESSION_POLICY(CAdaptiveOrientationPolicy, "AdaptiveOrientation");
