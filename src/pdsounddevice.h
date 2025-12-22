//
// pdsounddevice.h
//
// BarePD - Sound device that bridges libpd with Circle's audio
//
#ifndef _pdsounddevice_h
#define _pdsounddevice_h

#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/interrupt.h>
#include <circle/i2cmaster.h>

// Audio configuration
#define SAMPLE_RATE      48000
#define CHUNK_SIZE       (384 * 4)  // Audio buffer size in frames

// Choose sound output: PWM, I2S, or HDMI
// Uncomment the desired output
#define USE_PWM
//#define USE_I2S
//#define USE_HDMI

#ifdef USE_I2S
  // I2S DAC I2C address (0 = no I2C control)
  #define DAC_I2C_ADDRESS  0x4C  // PCM5102A typically doesn't need I2C
  #define SOUND_BASE_CLASS CI2SSoundBaseDevice
#elif defined(USE_HDMI)
  #include <circle/sound/hdmisoundbasedevice.h>
  #define SOUND_BASE_CLASS CHDMISoundBaseDevice
#else
  // Default: PWM output (3.5mm jack on Pi)
  #define SOUND_BASE_CLASS CPWMSoundBaseDevice
#endif

class CPdSoundDevice : public SOUND_BASE_CLASS
{
public:
#ifdef USE_I2S
	CPdSoundDevice (CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster);
#else
	CPdSoundDevice (CInterruptSystem *pInterrupt, CI2CMaster *pI2CMaster = nullptr);
#endif
	~CPdSoundDevice (void);

	boolean Initialize (void);

protected:
	// Override GetChunk to generate audio via libpd
	unsigned GetChunk (s16 *pBuffer, unsigned nChunkSize) override;

private:
	// Audio buffers for libpd processing
	float *m_pInBuffer;
	float *m_pOutBuffer;
	
	// Number of channels
	unsigned m_nInChannels;
	unsigned m_nOutChannels;
};

#endif

