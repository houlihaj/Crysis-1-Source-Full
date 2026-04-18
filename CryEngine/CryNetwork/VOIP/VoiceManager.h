/*************************************************************************
 Crytek Source File.
 Copyright (C), Crytek Studios, 2001-2005.
 -------------------------------------------------------------------------
 $Id$
 $DateTime$
 Description:  manages voice codecs
 -------------------------------------------------------------------------
 History:
 - 28/11/2005   : Created by Craig Tiller
*************************************************************************/
#ifndef __VOICEMANAGER_H__
#define __VOICEMANAGER_H__

#pragma once

struct IVoiceEncoder;
struct IVoiceDecoder;

typedef IVoiceEncoder*(*VoiceEncoderFactory)();
typedef IVoiceDecoder*(*VoiceDecoderFactory)();

class CVoiceManager
{
public:
	static IVoiceEncoder * CreateEncoder( const char * name );
	static IVoiceDecoder * CreateDecoder( const char * name );
};

class CAutoRegCodec
{
public:
	CAutoRegCodec( const char * name, VoiceEncoderFactory, VoiceDecoderFactory );

	IVoiceEncoder * CreateEncoder() const
	{
		return m_encoderFactory();
	}
	IVoiceDecoder * CreateDecoder() const
	{
		return m_decoderFactory();
	}

	static const CAutoRegCodec * FindCodec( const char * name );

private:
	static CAutoRegCodec * m_root;
	CAutoRegCodec * m_next;
	const char * m_name;
	VoiceEncoderFactory m_encoderFactory;
	VoiceDecoderFactory m_decoderFactory;
};

template <class T_Enc, class T_Dec>
class CAutoRegCodecT : public CAutoRegCodec
{
public:
	CAutoRegCodecT( const char * name ) : CAutoRegCodec( name, CreateEncoder, CreateDecoder )
	{
	}

private:
	static IVoiceEncoder * CreateEncoder()
	{
		return new T_Enc();
	}
	static IVoiceDecoder * CreateDecoder()
	{
		return new T_Dec();
	}
};

#define REGISTER_CODEC( name, encoder, decoder ) CAutoRegCodecT<encoder,decoder> codec_##name(#name)

#endif
