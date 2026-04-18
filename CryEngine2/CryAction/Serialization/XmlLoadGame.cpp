#include "stdafx.h"
#include "XmlLoadGame.h"

#define XML_LOADGAME_USE_COMPRESSION   // must be in sync with XmlSaveGame.cpp
#undef  XML_LOADGAME_USE_COMPRESSION   // undef because does NOT work with aliases yet

class CXmlLoadGame::CSerializeCtx : public _reference_target_t
{
public:
	CSerializeCtx( XmlNodeRef node )
	{
		m_pSerializer = GetISystem()->GetXmlUtils()->CreateXmlSerializer();
		m_pReader = m_pSerializer->GetReader(node);
	}
	TSerialize GetTSerialize() { return TSerialize(m_pReader); }

private:
	_smart_ptr<IXmlSerializer> m_pSerializer;
	ISerialize *m_pReader;
};

struct CXmlLoadGame::Impl
{
	XmlNodeRef root;
	XmlNodeRef metadata;
	XmlNodeRef consoleVariables;
	string		 inputFile;

	std::vector<_smart_ptr<CSerializeCtx> > sections;
};

CXmlLoadGame::CXmlLoadGame() : m_pImpl(new Impl)
{
}

CXmlLoadGame::~CXmlLoadGame()
{
}

bool CXmlLoadGame::Init( const char * name )
{
#ifdef XML_LOADGAME_USE_COMPRESSION
	unsigned int nFileSizeBits = GetISystem()->GetCompressedFileSize(name);
	unsigned int nFileSize = nFileSizeBits/8; // Bits to Bytes.
	if (nFileSize <= 0)
		return false;

	char *pXmlData = new char[nFileSize+16];
	GetISystem()->ReadCompressedFile(name, pXmlData,nFileSizeBits);
	pXmlData[nFileSize] = 0;

	m_pImpl->root = GetISystem()->LoadXmlFromString(pXmlData);

	delete []pXmlData;
#else
	m_pImpl->root = GetISystem()->LoadXmlFile(name);
#endif

	if (!m_pImpl->root)
		return false;

	m_pImpl->inputFile = name;

	m_pImpl->metadata = m_pImpl->root->findChild("Metadata");
	if (!m_pImpl->metadata)
		return false;

	m_pImpl->consoleVariables = m_pImpl->root->findChild("ConsoleVariables");
	if (!m_pImpl->consoleVariables)
		return false;

	return true;
}

bool CXmlLoadGame::Init( const XmlNodeRef& root, const char * fileName )
{
	m_pImpl->root = root;
	if (!m_pImpl->root)
		return false;

	m_pImpl->inputFile = fileName;

	m_pImpl->metadata = m_pImpl->root->findChild("Metadata");
	if (!m_pImpl->metadata)
		return false;

	m_pImpl->consoleVariables = m_pImpl->root->findChild("ConsoleVariables");
	if (!m_pImpl->consoleVariables)
		return false;

	return true;
}

const char * CXmlLoadGame::GetMetadata( const char * tag )
{
	return m_pImpl->metadata->getAttr(tag);
}

bool CXmlLoadGame::GetMetadata( const char * tag, int& value )
{
	return m_pImpl->metadata->getAttr(tag, value);
}

bool CXmlLoadGame::HaveMetadata( const char * tag )
{
	return m_pImpl->metadata->haveAttr(tag);
}

bool CXmlLoadGame::GetConsoleVariable( ICVar * pVar )
{
	const char * value = m_pImpl->consoleVariables->getAttr(pVar->GetName());
	if (value[0])
	{
		pVar->Set( value );
		return true;
	}
	return false;
}

std::auto_ptr<TSerialize> CXmlLoadGame::GetSection( const char * section )
{
	XmlNodeRef node = m_pImpl->root->findChild(section);
	if (!node)
		return std::auto_ptr<TSerialize>();
	_smart_ptr<CSerializeCtx> pCtx = new CSerializeCtx(node);
	m_pImpl->sections.push_back( pCtx );
	return std::auto_ptr<TSerialize>( new TSerialize(pCtx->GetTSerialize()) );
}

bool CXmlLoadGame::HaveSection( const char * section )
{
	return m_pImpl->root->findChild(section) != 0;
}

void CXmlLoadGame::Complete()
{
	delete this;
}

const char *CXmlLoadGame::GetFileName() const
{
	if(m_pImpl.get())
		return m_pImpl.get()->inputFile;
	return NULL;
}