//
// pdsounddevice.cpp
//
// BarePD - Sound device implementations for PWM and I2S (PCM5102A) audio
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// Licensed under GPLv3
//
#include "pdsounddevice.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

extern "C" {
#include "z_libpd.h"
}

static const char FromPdSound[] = "pdsound";

//
// Helper function to process libpd audio
//
static void ProcessPdAudio (float *pInBuffer, float *pOutBuffer,
                            s16 *pHWBuffer, unsigned nChunkSize,
                            unsigned nInChannels, unsigned nOutChannels)
{
	unsigned nFrames = nChunkSize / nOutChannels;
	
	// Calculate number of pd ticks needed (libpd processes 64 frames per tick)
	unsigned nBlockSize = libpd_blocksize();
	unsigned nTicks = nFrames / nBlockSize;
	if (nTicks == 0) nTicks = 1;
	
	unsigned nProcessFrames = nTicks * nBlockSize;
	
	// Clear input buffer
	if (pInBuffer && nInChannels > 0)
	{
		memset(pInBuffer, 0, nProcessFrames * nInChannels * sizeof(float));
	}
	
	// Process audio through libpd
	libpd_process_float(nTicks, pInBuffer, pOutBuffer);
	
	// Convert float output to s16
	const float scale = 32767.0f;
	unsigned nSamplesOut = nProcessFrames * nOutChannels;
	
	for (unsigned i = 0; i < nSamplesOut && i < nChunkSize; i++)
	{
		float sample = pOutBuffer[i];
		
		// Soft clip
		if (sample > 1.0f) sample = 1.0f;
		if (sample < -1.0f) sample = -1.0f;
		
		pHWBuffer[i] = (s16)(sample * scale);
	}
	
	// Fill remainder with silence
	for (unsigned i = nSamplesOut; i < nChunkSize; i++)
	{
		pHWBuffer[i] = 0;
	}
}

// ============================================================================
// PWM Sound Device Implementation
// ============================================================================

CPdSoundPWM::CPdSoundPWM (CInterruptSystem *pInterrupt, unsigned nSampleRate, unsigned nChunkSize)
:	CPWMSoundBaseDevice (pInterrupt, nSampleRate, nChunkSize),
	m_pInBuffer (nullptr),
	m_pOutBuffer (nullptr),
	m_nInChannels (0),
	m_nOutChannels (2),
	m_nSampleRate (nSampleRate)
{
}

CPdSoundPWM::~CPdSoundPWM (void)
{
	delete[] m_pInBuffer;
	delete[] m_pOutBuffer;
}

boolean CPdSoundPWM::Initialize (void)
{
	m_nOutChannels = GetHWTXChannels();
	if (m_nOutChannels == 0) m_nOutChannels = 2;
	
	unsigned nBufferFrames = DEFAULT_CHUNK_SIZE;
	
	m_pInBuffer = new float[nBufferFrames * (m_nInChannels > 0 ? m_nInChannels : 1)];
	m_pOutBuffer = new float[nBufferFrames * m_nOutChannels];
	
	if (!m_pInBuffer || !m_pOutBuffer)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to allocate PWM audio buffers");
		return FALSE;
	}
	
	memset(m_pOutBuffer, 0, nBufferFrames * m_nOutChannels * sizeof(float));
	
	if (libpd_init_audio(m_nInChannels, m_nOutChannels, m_nSampleRate) != 0)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to init libpd audio (PWM)");
		return FALSE;
	}
	
	CLogger::Get()->Write(FromPdSound, LogNotice, 
		"PWM audio: %u Hz, %u channels", m_nSampleRate, m_nOutChannels);
	
	return TRUE;
}

unsigned CPdSoundPWM::GetChunk (s16 *pBuffer, unsigned nChunkSize)
{
	ProcessPdAudio(m_pInBuffer, m_pOutBuffer, pBuffer, nChunkSize,
	               m_nInChannels, m_nOutChannels);
	return nChunkSize;
}

// ============================================================================
// I2S Sound Device Implementation (PCM5102A and similar DACs)
// ============================================================================

CPdSoundI2S::CPdSoundI2S (CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster,
                          u8 ucI2CAddress, unsigned nSampleRate, unsigned nChunkSize)
:	CI2SSoundBaseDevice (pInterrupt, nSampleRate, nChunkSize, FALSE, pI2CMaster, ucI2CAddress),
	m_pInBuffer (nullptr),
	m_pOutBuffer (nullptr),
	m_nInChannels (0),
	m_nOutChannels (2),
	m_nSampleRate (nSampleRate)
{
}

CPdSoundI2S::~CPdSoundI2S (void)
{
	delete[] m_pInBuffer;
	delete[] m_pOutBuffer;
}

boolean CPdSoundI2S::Initialize (void)
{
	m_nOutChannels = GetHWTXChannels();
	if (m_nOutChannels == 0) m_nOutChannels = 2;
	
	unsigned nBufferFrames = DEFAULT_CHUNK_SIZE;
	
	m_pInBuffer = new float[nBufferFrames * (m_nInChannels > 0 ? m_nInChannels : 1)];
	m_pOutBuffer = new float[nBufferFrames * m_nOutChannels];
	
	if (!m_pInBuffer || !m_pOutBuffer)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to allocate I2S audio buffers");
		return FALSE;
	}
	
	memset(m_pOutBuffer, 0, nBufferFrames * m_nOutChannels * sizeof(float));
	
	if (libpd_init_audio(m_nInChannels, m_nOutChannels, m_nSampleRate) != 0)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to init libpd audio (I2S)");
		return FALSE;
	}
	
	CLogger::Get()->Write(FromPdSound, LogNotice, 
		"I2S audio (PCM5102A): %u Hz, %u channels", m_nSampleRate, m_nOutChannels);
	
	return TRUE;
}

unsigned CPdSoundI2S::GetChunk (s16 *pBuffer, unsigned nChunkSize)
{
	ProcessPdAudio(m_pInBuffer, m_pOutBuffer, pBuffer, nChunkSize,
	               m_nInChannels, m_nOutChannels);
	return nChunkSize;
}

// ============================================================================
// Audio Output Factory
// ============================================================================

CSoundBaseDevice *CAudioOutputFactory::Create (TAudioOutputType eType,
                                               CInterruptSystem *pInterrupt,
                                               CI2CMaster *pI2CMaster,
                                               unsigned nSampleRate)
{
	CSoundBaseDevice *pDevice = nullptr;
	
	switch (eType)
	{
	case AudioOutputPWM:
		CLogger::Get()->Write(FromPdSound, LogNotice, "Creating PWM audio output (3.5mm jack)");
		pDevice = new CPdSoundPWM(pInterrupt, nSampleRate);
		break;
		
	case AudioOutputI2S:
		CLogger::Get()->Write(FromPdSound, LogNotice, "Creating I2S audio output (PCM5102A compatible)");
		// PCM5102A doesn't need I2C control, so ucI2CAddress = 0
		pDevice = new CPdSoundI2S(pInterrupt, pI2CMaster, 0, nSampleRate);
		break;
		
	default:
		CLogger::Get()->Write(FromPdSound, LogWarning, "Unknown audio type, using PWM");
		pDevice = new CPdSoundPWM(pInterrupt, nSampleRate);
		break;
	}
	
	return pDevice;
}

TAudioOutputType CAudioOutputFactory::ParseType (const char *pName)
{
	if (pName == nullptr)
		return AudioOutputPWM;
	
	// Compare case-insensitive
	if (pName[0] == 'p' || pName[0] == 'P')
		return AudioOutputPWM;
	if (pName[0] == 'i' || pName[0] == 'I')
		return AudioOutputI2S;
	if (pName[0] == 'h' || pName[0] == 'H')
		return AudioOutputHDMI;
	
	return AudioOutputPWM;
}

const char *CAudioOutputFactory::GetTypeName (TAudioOutputType eType)
{
	switch (eType)
	{
	case AudioOutputPWM:  return "PWM (3.5mm jack)";
	case AudioOutputI2S:  return "I2S (PCM5102A)";
	case AudioOutputHDMI: return "HDMI";
	default:              return "Unknown";
	}
}
