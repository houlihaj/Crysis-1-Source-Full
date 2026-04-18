
// utility call to support loading from "TextureFallbackFile.dat"
class CTextureFallbackLoader
{
public:

	// call once to load the fallback file
	// Returns
	//   success
	bool InitAndLoad()
	{
		if(!m_FallbackData.empty())
		{
			assert(0);					// call Free before doing this again
			return false;
		}

		char *szFilename = "Bin32/rc/TextureFallbackFile.dat";

		FILE *in = iSystem->GetIPak()->FOpen(szFilename,"rb");

		if(!in)
		{
			CryLog("CTextureFallbackLoader file not found '%s'",szFilename);
			return false;
		}
		
		CryLog("CTextureFallbackLoader open '%s'",szFilename);

		uint8 header[8];

		if(iSystem->GetIPak()->FReadRaw(header,8,1,in)!=1)
		{
			iSystem->GetIPak()->FClose(in);
			return false;
		}

		// header
		char szFileID[5];

		sprintf(szFileID,"tfs%c",m_iVersion);				// 4 byte CryTextureFallbackSystem 

		if(strncmp((char *)header,szFileID,4)!=0)
		{
			iSystem->GetIPak()->FClose(in);
			return false;
		}

		uint32 entries = ReadValueT<uint32>(header+4);

		m_FallbackData.resize(entries*(8+4));					// index buffer size

		if(iSystem->GetIPak()->FReadRaw(&m_FallbackData[0],m_FallbackData.size(),1,in)!=1)
		{
			iSystem->GetIPak()->FClose(in);
			return false;
		}

		uint8 *pMem = &m_FallbackData[0];
		uint32 dwDataSize=0;

		// index block
		for(uint32 dwI=0;dwI<entries;++dwI)
		{
			uint64 key = ReadValueT<uint64>(pMem);			pMem+=8;
			uint32 dwSize = ReadValueT<uint32>(pMem);		pMem+=4;

			SPosAndSize PosAndSize;

			PosAndSize.pos=dwDataSize;
			PosAndSize.size=dwSize;

			m_Index[key]=PosAndSize;

			dwDataSize+=dwSize;
		}

		m_FallbackData.resize(dwDataSize);						// data block size

		if(iSystem->GetIPak()->FReadRaw(&m_FallbackData[0],m_FallbackData.size(),1,in)!=1)
		{
			iSystem->GetIPak()->FClose(in);
			return false;
		}
		
		CryLog("CTextureFallbackLoader %d entries",m_Index.size());

		// data block

		iSystem->GetIPak()->FClose(in);
		return true;
	}

	// to free memory after last GetEntry() call
	void Free()
	{
		m_FallbackData.clear();
		m_Index.clear();
	}

	// call InitAndLoad() before
	// Very fast lookup
	// Returns
	//   true=exists, false=entry with that name does not exist
	bool GetEntry( const char *szFilename, uint8 * &pMem, uint32 &dwSize )
	{
		pMem=0;dwSize=0;

		uint64 key = ComputeCRC(szFilename);

		std::map<uint64,SPosAndSize>::const_iterator it = m_Index.find(key);

		if(it==m_Index.end())
			return false;

		const SPosAndSize &rPosAndSize = it->second;

		dwSize = rPosAndSize.size;

		uint32 dwOffset = rPosAndSize.pos;

		pMem = &m_FallbackData[dwOffset];
		return true;
	}

	// for statistics
	uint32 GetFallbackBlockSize() const
	{
		return (uint32)m_FallbackData.size();
	}

	// Arguments:
	//   szFilename - must not be 0
	uint64 ComputeCRC( const char *szFilename )
	{
		string sUnifiedPathName = szFilename;
		uint32 dwNameLen = (uint32)strlen(szFilename);

		// unify path to get reliable CRC
		{
			string::iterator it, end=sUnifiedPathName.end();
			uint32 dwI=0, dwEnd=(uint32)sUnifiedPathName.size();

			for(it=sUnifiedPathName.begin();it!=end;++it,++dwI)
			{
				if(*it=='\\')*it='/';
				else
					if(*it=='.')dwEnd=dwI;
					else
						*it = (char)tolower((int)*it);
			}

			sUnifiedPathName.resize(dwEnd);
		}

		return CRC_64(sUnifiedPathName.c_str(), dwNameLen);
	}

	private: // --------------------------------------------------------------------------------------------

	struct SPosAndSize
	{
		uint32			pos;
		uint32			size;
	};

	std::vector<uint8>							m_FallbackData;			//
	std::map<uint64,SPosAndSize>		m_Index;						// m_Index[key] = entry offset in memory (key,size,data...)

	static const int								m_iVersion=2;				// increase to prevent loading of files with old formats

	// -----------------------------------------------------------------------------------------------------

	// Calculate CRC64 of buffer.
	static uint64 CRC_64( const char *buf,unsigned int len )
	{
		const uint64 POLY64REV = 0xd800000000000000ULL;
		static uint64 Table[256];
		uint64 code = 0;
		static int init = 0;

		if (!init) {
			int i;
			init = 1;
			for (i = 0; i <= 255; i++) {
				int j;
				uint64 part = i;
				for (j = 0; j < 8; j++) {
					if (part & 1)
						part = (part >> 1) ^ POLY64REV;
					else
						part >>= 1;
				}
				Table[i] = part;
			}
		}
		while (len--)
		{
			uint64 temp1 = code >> 8;
			uint64 temp2 = Table[(code ^ (uint64)*buf) & 0xff];
			code = temp1 ^ temp2;
			buf += 1;
		}
		return code;
	}

	//
	template <class T> T ReadValueT( uint8 *pMem )
	{
		uint32 dwSize = sizeof(T);

		T ret=0;

		for(uint32 dwI=0;dwI<dwSize;++dwI)
			ret |= ((T)pMem[dwSize-1-dwI])<<(dwI*8);

		return ret;
	}
};

