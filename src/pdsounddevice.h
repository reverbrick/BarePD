//
// pdsounddevice.h
//
// BarePD - Sound device that bridges libpd with Circle's audio
// Supports PWM (3.5mm) and I2S (PCM5102A) audio outputs
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// Licensed under GPLv3
//
#ifndef _pdsounddevice_h
#define _pdsounddevice_h

#include <circle/sound/soundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/interrupt.h>
#include <circle/i2cmaster.h>

// Audio configuration
#define DEFAULT_SAMPLE_RATE     48000
#define DEFAULT_CHUNK_SIZE      (384 * 4)  // Audio buffer size in frames

// Maximum channels supported
#define MAX_AUDIO_CHANNELS      8

/// Audio output type selection
enum TAudioOutputType
{
	AudioOutputPWM,         ///< PWM output via 3.5mm jack (default)
	AudioOutputI2S,         ///< I2S output for DACs like PCM5102A
	AudioOutputHDMI,        ///< HDMI audio output (future)
	AudioOutputUnknown
};

//
// PWM Sound Device (3.5mm jack)
//
class CPdSoundPWM : public CPWMSoundBaseDevice
{
public:
	CPdSoundPWM (CInterruptSystem *pInterrupt,
	             unsigned nSampleRate = DEFAULT_SAMPLE_RATE,
	             unsigned nChunkSize = DEFAULT_CHUNK_SIZE);
	~CPdSoundPWM (void);

	boolean Initialize (void);
	unsigned GetOutputChannels (void) const { return m_nOutChannels; }

protected:
	unsigned GetChunk (s16 *pBuffer, unsigned nChunkSize) override;

private:
	float *m_pInBuffer;
	float *m_pOutBuffer;
	unsigned m_nInChannels;
	unsigned m_nOutChannels;
	unsigned m_nSampleRate;
};

//
// I2S Sound Device (PCM5102A and other I2S DACs)
//
class CPdSoundI2S : public CI2SSoundBaseDevice
{
public:
	/// \param pInterrupt    Interrupt system
	/// \param pI2CMaster    I2C master (optional, for DACs with I2C control)
	/// \param ucI2CAddress  I2C address (0 = no I2C control, for PCM5102A)
	/// \param nSampleRate   Sample rate in Hz
	/// \param nChunkSize    Buffer size in frames
	CPdSoundI2S (CInterruptSystem *pInterrupt,
	             CI2CMaster *pI2CMaster = nullptr,
	             u8 ucI2CAddress = 0,
	             unsigned nSampleRate = DEFAULT_SAMPLE_RATE,
	             unsigned nChunkSize = DEFAULT_CHUNK_SIZE);
	~CPdSoundI2S (void);

	boolean Initialize (void);
	unsigned GetOutputChannels (void) const { return m_nOutChannels; }

protected:
	unsigned GetChunk (s16 *pBuffer, unsigned nChunkSize) override;

private:
	float *m_pInBuffer;
	float *m_pOutBuffer;
	unsigned m_nInChannels;
	unsigned m_nOutChannels;
	unsigned m_nSampleRate;
};

//
// Audio output factory - creates the appropriate sound device
//
class CAudioOutputFactory
{
public:
	/// Create a sound device based on type
	/// \param eType         Output type (PWM, I2S)
	/// \param pInterrupt    Interrupt system
	/// \param pI2CMaster    I2C master (for I2S DACs with I2C control)
	/// \param nSampleRate   Sample rate
	/// \return Pointer to sound device, or nullptr on failure
	static CSoundBaseDevice *Create (TAudioOutputType eType,
	                                 CInterruptSystem *pInterrupt,
	                                 CI2CMaster *pI2CMaster = nullptr,
	                                 unsigned nSampleRate = DEFAULT_SAMPLE_RATE);

	/// Parse audio output type from string
	/// \param pName  String name ("pwm", "i2s")
	/// \return Output type enum
	static TAudioOutputType ParseType (const char *pName);

	/// Get string name for output type
	static const char *GetTypeName (TAudioOutputType eType);
};

#endif
