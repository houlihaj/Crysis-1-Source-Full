#include "stdafx.h"
#include "VehicleXMLSaver.h"

#include <stack>
#include <IReadWriteXMLSink.h>

#include "VehicleData.h"

/*
* Implementation of IWriteXMLSource, writes IVehicleData structure
*/
class CVehicleDataSaver : public IWriteXMLSource
{
public:
  CVehicleDataSaver();
  CVehicleDataSaver( IVariablePtr pNode );

  // IWriteXMLSource
  virtual void AddRef();
  virtual void Release();

  virtual IWriteXMLSourcePtr BeginTable( const char * name );
  virtual IWriteXMLSourcePtr BeginTableAt( int elem );
  virtual bool HaveValue( const char * name );
  virtual bool GetValue( const char * name, TValue& value, const XmlNodeRef& definition );
  virtual bool EndTableAt( int elem );
  virtual bool EndTable( const char * name );

  virtual IWriteXMLSourcePtr BeginArray( const char * name, size_t * numElems, const XmlNodeRef& definition );
  virtual bool HaveElemAt( int elem );
  virtual bool GetAt( int elem, TValue& value, const XmlNodeRef& definition );
  virtual bool EndArray( const char * name );

  virtual bool Complete();
  // ~IWriteXMLSource

private:
  int m_nRefs;  
  std::stack<IVariablePtr> m_nodeStack;

  IVariablePtr CurNode() { assert(!m_nodeStack.empty()); return m_nodeStack.top(); }

};

TYPEDEF_AUTOPTR(CVehicleDataSaver);
typedef CVehicleDataSaver_AutoPtr CVehicleDataSaverPtr;


void CVehicleDataSaver::AddRef()
{
  ++m_nRefs;
}

void CVehicleDataSaver::Release()
{
  if (0 == --m_nRefs)
    delete this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginTable( const char * name )
{
  IVariablePtr childNode = GetChildVar( CurNode(), name );
  if (!childNode)
    return NULL;
  m_nodeStack.push( childNode );
  return this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginTableAt( int elem )
{
  IVariablePtr childNode = CurNode()->GetChildVar(elem-1);
  if (!childNode)
    return NULL;
  m_nodeStack.push( childNode );
  return this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginArray( const char * name, size_t * numElems, const XmlNodeRef& definition )
{
  IVariablePtr childNode = GetChildVar( CurNode(), name);
  if (!childNode)
    return NULL;
  *numElems = childNode->NumChildVars();
  m_nodeStack.push( childNode );
  return this;
}

bool CVehicleDataSaver::EndTable( const char * name )
{
  m_nodeStack.pop();
  return true;
}

bool CVehicleDataSaver::EndTableAt( int elem )
{
  return EndTable( NULL );
}

bool CVehicleDataSaver::EndArray( const char * name )
{
  return EndTable(name);
}

namespace
{
  #define VAL_MIN 0.0002f
  #define VAL_PRECISION 4.0f

  template <class T> void ClampValue( T& val )
  {  
  }

  //! rounding
  template <> void ClampValue( float& val )
  {
    const static float coeff = pow(10.0f, VAL_PRECISION);
    val = ( (float)((long)(val * coeff + sgn(val)*0.5f)) ) / coeff;
    
    if (abs(val) <= VAL_MIN) 
      val = 0;        
  }
  template <> void ClampValue( Vec3& val )
  {
    for (int i=0; i<3; ++i)
      ClampValue<float>(val[i]);
  }

  struct CGetValueVisitor
  {
  public:
    CGetValueVisitor( IVariablePtr pNode, const char * name ) : m_pNode(pNode), m_name(name), m_ok(false) {}
    
    void Visit( const char*& value )
    {
      static CString sVal;
      if (IVariable* child = GetChildVar( m_pNode, m_name ))
      {        
        child->Get( sVal );  
        value = sVal;
        m_ok = true;
      }      
    }    
    template <class T> void Visit( T& value )
    {
      if (IVariable* child = GetChildVar( m_pNode, m_name ))
      { 
        child->Get( value );            
        ClampValue( value );
        m_ok = true;
      }      
    }    
    bool Ok()
    {
      return m_ok;
    }
  private:
    bool m_ok;
    IVariablePtr m_pNode;
    const char * m_name;
  };

  struct CGetAtVisitor
  {
  public:
    CGetAtVisitor( IVariablePtr pNode, int elem ) : m_pNode(pNode), m_elem(elem), m_ok(false) {}

    template <class T> void Visit( T& value )
    {
      if (IVariable* child = m_pNode->GetChildVar( m_elem-1 ))
      {
        child->Get( value );  
        ClampValue( value );
        m_ok = true;
      }          
    }
    void Visit( const char*& value )
    {
      static CString sVal;
      if (IVariable* child = m_pNode->GetChildVar( m_elem-1 ))
      {        
        child->Get( sVal );  
        value = sVal;
        m_ok = true;
      }          
    }
    bool Ok()
    {
      return m_ok;
    }
  private:
    bool m_ok;
    IVariablePtr m_pNode;
    int m_elem;
  };

}

bool CVehicleDataSaver::GetValue( const char * name, TValue& value, const XmlNodeRef& definition )
{
  CGetValueVisitor visitor( CurNode(), name );
  value.Visit( visitor );
  return visitor.Ok();
}

bool CVehicleDataSaver::GetAt( int elem, TValue& value, const XmlNodeRef& definition )
{
  CGetAtVisitor visitor( CurNode(), elem );
  value.Visit( visitor );
  return visitor.Ok();
}

bool CVehicleDataSaver::HaveElemAt( int elem )
{  
  if (elem <= CurNode()->NumChildVars())
    return true;
  return false;
}

bool CVehicleDataSaver::HaveValue( const char * name )
{ 
  if (GetChildVar( CurNode(), name ))    
    return true;
  return false;
}

bool CVehicleDataSaver::Complete()
{
  return true;
}

CVehicleDataSaver::CVehicleDataSaver( IVariablePtr pNode ) : m_nRefs(0)
{
  m_nodeStack.push( pNode );
}

XmlNodeRef VehicleDataSave( const char * definitionFile, IVehicleData* pData )
{
  CVehicleDataSaverPtr pSaver( new CVehicleDataSaver(pData->GetRoot()) );
  return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->CreateXMLFromSource( definitionFile, &*pSaver );
}

bool VehicleDataSave( const char * definitionFile, const char * dataFile, IVehicleData* pData )
{
  CVehicleDataSaverPtr pSaver( new CVehicleDataSaver(pData->GetRoot()) );
  return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->WriteXML( definitionFile, dataFile, &*pSaver );
}

