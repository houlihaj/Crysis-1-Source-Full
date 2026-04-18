#ifndef __SIMPLESTREAMDEFS_H__
#define __SIMPLESTREAMDEFS_H__

#pragma once

struct SStreamRecord
{
	static const size_t KEY_SIZE = 32;
	static const size_t VALUE_SIZE = 96;

	SStreamRecord( const char * key, const char * value )
	{
		memset(this->key, 0, KEY_SIZE);
		memset(this->value, 0, VALUE_SIZE);
		strcpy(this->key, key);
		strcpy(this->value, value);
	}

	SStreamRecord( const char * key )
	{
		memset(this->key, 0, KEY_SIZE);
		memset(this->value, 0, VALUE_SIZE);
		strcpy(this->key, key);
	}

	SStreamRecord()
	{
		memset(this->key, 0, KEY_SIZE);
		memset(this->value, 0, VALUE_SIZE);
	}

	char key[KEY_SIZE];
	char value[VALUE_SIZE];
};



#endif
