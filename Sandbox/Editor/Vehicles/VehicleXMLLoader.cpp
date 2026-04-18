#include "stdafx.h"
#include "VehicleXMLLoader.h"

#include <stack>
#include <IReadWriteXMLSink.h>

#include "VehicleData.h"


namespace 
{
  void SetExtendedProperties( IVariablePtr node, const XmlNodeRef& def, IReadXMLSink* pSink )
  {
    const char* nodeName = node->GetName();

    // set limits if present, otherwise use default ones
    float min, max;
    node->GetLimits(min, max);
    def->getAttr("min", min);
    def->getAttr("max", max);    
    node->SetLimits(min, max);

    // set description
    CString desc;
    if (def->getAttr("desc", desc))
      node->SetDescription(desc);    

    // handle extendable array type.
    int ext = 0;
    def->getAttr("extendable", ext);
    
    if (ext == 1 && !pSink->IsCreationMode() 
      && strcmp(nodeName, def->getAttr("name"))==0 ) // don't set it for array content
    {
      node->SetDataType(IVariable::DT_EXTARRAY);  

      IVariablePtr child = CreateArrayEntry(def);
      node->SetUserData(child);
    }
        
    else if (0 == strcmp(nodeName, "filename"))
    {
      node->SetDataType(IVariable::DT_FILE);
    }
    else if (0 == strcmp(nodeName, "sound"))
    {
      node->SetDataType(IVariable::DT_SOUND);
    }
   
  }

  template<class T> IVariable* CreateVar( T value, const XmlNodeRef& def )
  {     
    return new CVariable<T>;
  }

  IVariable* CreateVar( const char* value, const XmlNodeRef& def )
  {
    // enums and lists are only used for string variables 

    XmlNodeRef enumNode = def->findChild("Enum");
    
    if (0 != enumNode || def->haveAttr("list"))
    {
      CVariableEnum<CString>* varEnum = new CVariableEnum<CString>;
            
      if (0 != enumNode)
      {      
        for (int i=0; i<enumNode->getChildCount(); ++i)
        {
          const char* val = enumNode->getChild(i)->getContent();
          varEnum->AddEnumItem( val, val );
        }      
      }
      else 
      {
        // add empty and current value as default
        varEnum->AddEnumItem("", "");
        varEnum->AddEnumItem(value, value);

        const char* listType = def->getAttr("list");
        if (0 == strcmp(listType, "Helper"))
          varEnum->SetDataType(IVariable::DT_VEEDHELPER);
        else if (0 == strcmp(listType, "Part"))
          varEnum->SetDataType(IVariable::DT_VEEDPART);
        else if (0 == strcmp(listType, "Component"))
          varEnum->SetDataType(IVariable::DT_VEEDCOMP);
      }
      return varEnum;
    }
    else 
      return new CVariable<CString>;
  }


  void CreateDefaultChildren( CVariableArray* arr, const XmlNodeRef& def )
  {
    // this fills a table with its children that are defined in def
    // so afterwards we have the complete data for exposing in UI    
  }

}

/*
* Implementation of IReadXMLSink, creates IVehicleData structure
*/
class CVehicleDataLoader : public IReadXMLSink
{
public:
  CVehicleDataLoader();

  IVehicleData* GetVehicleData() { return m_pData; }

  // IReadXMLSink
  virtual void AddRef();
  virtual void Release();

  virtual IReadXMLSinkPtr BeginTable( const char * name, const XmlNodeRef& definition );
  virtual IReadXMLSinkPtr BeginTableAt( int elem, const XmlNodeRef& definition );
  virtual bool SetValue( const char * name, const TValue& value, const XmlNodeRef& definition );
  virtual bool EndTableAt( int elem );
  virtual bool EndTable( const char * name );

  virtual IReadXMLSinkPtr BeginArray( const char * name, const XmlNodeRef& definition  );
  virtual bool SetAt( int elem, const TValue& value, const XmlNodeRef& definition);
  virtual bool EndArray( const char * name );

  virtual bool Complete();

  virtual bool IsCreationMode(){ return m_creationNode != 0; }
  virtual XmlNodeRef GetCreationNode(){ return m_creationNode; }
  virtual void SetCreationNode(XmlNodeRef definition){ m_creationNode = definition; }
  // ~IReadXMLSink

private:
  int m_nRefs;  
  IVehicleData* m_pData;
  std::stack<IVariablePtr> m_nodeStack;
  XmlNodeRef m_creationNode;

  IVariablePtr CurNode() { assert(!m_nodeStack.empty()); return m_nodeStack.top(); }
  
  class SSetValueVisitor
  {
  public:
    SSetValueVisitor( IVariablePtr parent, const char * name, const XmlNodeRef& definition, IReadXMLSink* sink) 
    : m_parent(parent)
    , m_name(name)
    , m_def(definition)
    , m_sink(sink)
    {}

    template <class T> void Visit( const T& value )
    {       
      IVariablePtr node( CreateVar(value, m_def) );
      DoVisit( value, node );
    }    
  private:		
    template <class T> void DoVisit( const T& value, IVariablePtr node )
    {
      node->SetName( m_name );
      node->Set( value );      

      SetExtendedProperties( node, m_def, m_sink );

      m_parent->AddChildVar( node );
    }

    IVariablePtr m_parent;
    const char * m_name;
    const XmlNodeRef& m_def;
    IReadXMLSink* m_sink;
  };
  class SSetValueAtVisitor
  {
  public:
    SSetValueAtVisitor( IVariablePtr parent, int elem, const XmlNodeRef& definition, IReadXMLSink* sink ) 
      : m_parent(parent)
      , m_elem(elem)
      , m_def(definition)
      , m_sink(sink)
    {}

    template <class T> void Visit( const T& value )
    {	
      IVariablePtr node( CreateVar(value, m_def) ); 
      DoVisit( value, node );
    }    
  private:
    template <class T> void DoVisit( const T& value, IVariablePtr node )
    {
      CString s;
      //s.Format(_T("%d"), m_elem);
      s = m_def->getAttr("elementName");

      node->SetName( s );
      node->Set( value );

      SetExtendedProperties( node, m_def, m_sink );
     
      m_parent->AddChildVar( node );
    }

    IVariablePtr m_parent;
    int m_elem;
    const XmlNodeRef& m_def;
    IReadXMLSink* m_sink;
  };
};

TYPEDEF_AUTOPTR(CVehicleDataLoader);
typedef CVehicleDataLoader_AutoPtr CVehicleDataLoaderPtr;


CVehicleDataLoader::CVehicleDataLoader() 
: m_nRefs(0)
, m_creationNode(0)
{	
  m_pData = new CVehicleData;
  m_nodeStack.push( m_pData->GetRoot() );  
}

void CVehicleDataLoader::AddRef()
{
  ++m_nRefs;
}

void CVehicleDataLoader::Release()
{
  if (0 == --m_nRefs)
    delete this;
}

IReadXMLSinkPtr CVehicleDataLoader::BeginTable( const char * name, const XmlNodeRef& definition )
{  
  CVariableArray* arr = new CVariableArray;
  arr->SetName( name );
  m_nodeStack.push( arr );

  // create default children here
  CreateDefaultChildren(arr, definition);

  return this;
}

IReadXMLSinkPtr CVehicleDataLoader::BeginTableAt( int elem, const XmlNodeRef& definition )
{
  CVariableArray* arr = new CVariableArray;
  
  CString s;
  //s.Format(_T("%d"), elem);    
  s = definition->getAttr("elementName");

  arr->SetName( s );
  m_nodeStack.push( arr );  

  // create default children here
  CreateDefaultChildren(arr, definition);

  return this;
}

bool CVehicleDataLoader::SetValue( const char * name, const TValue& value, const XmlNodeRef& definition )
{
  SSetValueVisitor visitor(CurNode(), name, definition, this);
  value.Visit( visitor );   
  
  return true;
}

bool CVehicleDataLoader::EndTable( const char * name )
{
  IVariablePtr newNode = CurNode();	
  m_nodeStack.pop();
  CurNode()->AddChildVar( newNode ); // add child
  return true;
}

bool CVehicleDataLoader::EndTableAt( int elem )
{
  return EndTable( 0 );
}

IReadXMLSinkPtr CVehicleDataLoader::BeginArray( const char * name, const XmlNodeRef& definition )
{
  IVariablePtr arr = new CVariableArray;
  arr->SetName( name );
  
  SetExtendedProperties( arr, definition, this );

  m_nodeStack.push( arr );  
  return this;
}

bool CVehicleDataLoader::SetAt( int elem, const TValue& value, const XmlNodeRef& definition )
{
  SSetValueAtVisitor visitor(CurNode(), elem, definition, this);
  value.Visit( visitor );    
  
  return true;
}

bool CVehicleDataLoader::EndArray( const char * name )
{
  return EndTable( name );
}

bool CVehicleDataLoader::Complete()
{
  return true;
}



IVehicleData* VehicleDataLoad( const char * definitionFile, const char * dataFile, bool bFillDefaults  )
{
  XmlNodeRef definition = GetISystem()->LoadXmlFile(definitionFile);
  if (!definition){
    Warning("VehicleDataLoad: unable to load definition file %s", definitionFile);
    return 0;
  }
  return VehicleDataLoad( definition, dataFile, bFillDefaults );
}

IVehicleData* VehicleDataLoad( const XmlNodeRef& definition, const char * dataFile, bool bFillDefaults )
{
  CVehicleDataLoaderPtr pLoader( new CVehicleDataLoader );  
  if (GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->ReadXML( definition, dataFile, &*pLoader )){
    return pLoader->GetVehicleData();
  }
  else {
    return NULL;  
  }  
}

IVehicleData* VehicleDataLoad( const XmlNodeRef& definition, const XmlNodeRef& data, bool bFillDefaults )
{
  CVehicleDataLoaderPtr pLoader( new CVehicleDataLoader );  
  if (GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->ReadXML( definition, data, &*pLoader )){
    return pLoader->GetVehicleData();
  }
  else {
    return NULL;  
  }  
}

// creates structure according to definition element
IVariablePtr CreateArrayEntry( const XmlNodeRef& definition )
{
  CVehicleDataLoaderPtr pLoader( new CVehicleDataLoader );  
  pLoader->SetCreationNode(definition);

  if (GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->ReadXML( definition, XmlNodeRef(0), &*pLoader ))
  {
    IVariablePtr arr = pLoader->GetVehicleData()->GetRoot();

    //DumpVariable(arr);
    
    if (arr->NumChildVars() && arr->GetChildVar(0)->NumChildVars()
      && 0 == strcmp(arr->GetChildVar(0)->GetChildVar(0)->GetName(), definition->getAttr("elementName")))
    {
      // return the actual array entry.. it's on 2nd level cause the loader 
      // always creates an empty top-level array
      return arr->GetChildVar(0)->GetChildVar(0);      
    }
    else
    {
      arr->SetName(definition->getAttr("elementName")); // fixme
      return arr;
    }
  }
  else {
    return NULL;  
  }  
}

//
// end of CVehicleDataLoader implementation


