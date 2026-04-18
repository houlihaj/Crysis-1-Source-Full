#pragma once 


struct IEngineSettingsManager
{
public:
	virtual bool		HasKey(const string& key, bool bForcePull=false) = 0;

	virtual void		GetValueByRef(const string& key, string& value, bool bForcePull=false) = 0;
	virtual void		GetValueByRef(const string& key, int& value, bool bForcePull=false) = 0;

	virtual void		SetKey(const string& key, const char* value) = 0;
	virtual void		SetKey(const string& key, const string& value) = 0;
	virtual void		SetKey(const string& key, bool value) = 0;

	virtual bool		StoreData() = 0;
	virtual void		SetRootPath( const string& szRootPath ) = 0;
	virtual string	GetRootPath() = 0;
};
