//
// pdsounddevice.cpp
//
// BarePD - Sound device that bridges libpd with Circle's audio
//
#include "pdsounddevice.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <assert.h>

extern "C" {
#include "z_libpd.h"
}

static const char FromPdSound[] = "pdsound";

#ifdef USE_I2S
CPdSoundDevice::CPdSoundDevice (CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster)
:	CI2SSoundBaseDevice (pInterrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE, pI2CMaster, DAC_I2C_ADDRESS),
#else
CPdSoundDevice::CPdSoundDevice (CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster)
:	CPWMSoundBaseDevice (pInterrupt, SAMPLE_RATE, CHUNK_SIZE),
#endif
	m_pInBuffer (nullptr),
	m_pOutBuffer (nullptr),
	m_nInChannels (0),
	m_nOutChannels (2)
{
}

CPdSoundDevice::~CPdSoundDevice (void)
{
	delete[] m_pInBuffer;
	delete[] m_pOutBuffer;
}

boolean CPdSoundDevice::Initialize (void)
{
	// Get channel count from hardware
	m_nOutChannels = GetHWTXChannels ();
	if (m_nOutChannels == 0)
	{
		m_nOutChannels = 2;  // Default stereo
	}
	
	// Allocate buffers for libpd
	// libpd processes in blocks of 64 samples (DEFDACBLKSIZE)
	// We need buffers that can hold CHUNK_SIZE frames
	unsigned nBlockSize = libpd_blocksize();
	unsigned nBufferFrames = CHUNK_SIZE;
	
	m_pInBuffer = new float[nBufferFrames * m_nInChannels];
	m_pOutBuffer = new float[nBufferFrames * m_nOutChannels];
	
	if (m_pInBuffer == nullptr || m_pOutBuffer == nullptr)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to allocate audio buffers");
		return FALSE;
	}
	
	// Clear buffers
	memset(m_pInBuffer, 0, nBufferFrames * m_nInChannels * sizeof(float));
	memset(m_pOutBuffer, 0, nBufferFrames * m_nOutChannels * sizeof(float));
	
	// Initialize libpd audio
	if (libpd_init_audio(m_nInChannels, m_nOutChannels, SAMPLE_RATE) != 0)
	{
		CLogger::Get()->Write(FromPdSound, LogError, "Failed to init libpd audio");
		return FALSE;
	}
	
	CLogger::Get()->Write(FromPdSound, LogNotice, 
		"Audio initialized: %u Hz, %u out channels, block size %u",
		SAMPLE_RATE, m_nOutChannels, nBlockSize);
	
	return TRUE;
}

unsigned CPdSoundDevice::GetChunk (s16 *pBuffer, unsigned nChunkSize)
{
	assert(pBuffer != nullptr);
	
	unsigned nChannels = GetHWTXChannels();
	unsigned nFrames = nChunkSize / nChannels;
	
	// Calculate number of pd ticks needed
	// libpd processes 64 frames per tick
	unsigned nBlockSize = libpd_blocksize();
	unsigned nTicks = nFrames / nBlockSize;
	
	if (nTicks == 0)
	{
		nTicks = 1;
	}
	
	// Ensure we have enough buffer space
	unsigned nProcessFrames = nTicks * nBlockSize;
	
	// Clear input buffer (no audio input for now)
	memset(m_pInBuffer, 0, nProcessFrames * m_nInChannels * sizeof(float));
	
	// Process audio through libpd
	libpd_process_float(nTicks, m_pInBuffer, m_pOutBuffer);
	
	// Convert float output to s16 and copy to hardware buffer
	// libpd outputs interleaved audio in range [-1.0, 1.0]
	const float scale = 32767.0f;
	unsigned nSamplesOut = nProcessFrames * nChannels;
	
	for (unsigned i = 0; i < nSamplesOut && i < nChunkSize; i++)
	{
		float sample = m_pOutBuffer[i];
		
		// Soft clip to prevent harsh distortion
		if (sample > 1.0f) sample = 1.0f;
		if (sample < -1.0f) sample = -1.0f;
		
		pBuffer[i] = (s16)(sample * scale);
	}
	
	// Fill remainder with silence if needed
	for (unsigned i = nSamplesOut; i < nChunkSize; i++)
	{
		pBuffer[i] = 0;
	}
	
	return nChunkSize;
}

