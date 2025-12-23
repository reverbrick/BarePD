//
// kernel.h
//
// BarePD - Bare metal Pure Data for Raspberry Pi
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// Based on Circle (https://github.com/rsta2/circle)
// Licensed under GPLv3
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbmidi.h>
#include <circle/i2cmaster.h>
#include <circle/sched/scheduler.h>
#include <circle/types.h>
#include <circle/fs/fat/fatfs.h>
#include <SDCard/emmc.h>

#include "pdsounddevice.h"
#include "pd_fudi.h"

// Default patch filename
#define DEFAULT_PATCH_NAME      "main.pd"
#define MAX_PATCH_SIZE          (256 * 1024) // 256KB max patch size

// Default audio settings
#define DEFAULT_AUDIO_OUTPUT    AudioOutputI2S
#define DEFAULT_SAMPLE_RATE_HZ  48000

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);

private:
	// Configuration
	void ParseConfig (void);
	
	// Patch loading
	boolean LoadPatch (const char *pPatchName);
	boolean FindAndLoadPatch (void);

	// Audio setup
	boolean SetupAudio (void);

	// MIDI handlers
	static void MIDIPacketHandler (unsigned nCable, u8 *pPacket, unsigned nLength);
	static void USBDeviceRemovedHandler (CDevice *pDevice, void *pContext);

	// FUDI processing
	void ProcessFudi (void);
	void ProcessFudiSerial (CDevice *pSerial);
	static void FudiOutputHandler (const char *pMessage);
	
	// Pd message hooks for FUDI output
	static void PdFloatHook (const char *recv, float x);
	static void PdBangHook (const char *recv);
	static void PdSymbolHook (const char *recv, const char *sym);

private:
	// Core Circle components
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CScheduler		m_Scheduler;

	// USB and I2C
	CUSBHCIDevice		m_USBHCI;
	CI2CMaster		m_I2CMaster;

	// SD Card and filesystem
	CEMMCDevice		m_EMMC;
	CFATFileSystem		m_FileSystem;

	// Audio configuration
	TAudioOutputType	m_AudioOutput;
	unsigned		m_nSampleRate;
	boolean			m_bHeadless;		// Skip video for lower latency
	
	// Sound devices
	CSoundBaseDevice	*m_pSoundDevice;	// For PWM output
	CPdSoundI2S		*m_pI2SDevice;		// For I2S output (PCM5102A)

	// USB MIDI
	CUSBMIDIDevice		*m_pMIDIDevice;

	// FUDI remote control (via UART serial - GPIO 14/15)
	// Note: USB CDC Gadget not available on Pi 3B (no OTG support)
	CFudiParser		m_FudiParser;		// FUDI protocol parser
	boolean			m_bFudiEnabled;		// FUDI over serial enabled

	// Loaded patch handle
	void			*m_pPatch;

	static CKernel		*s_pThis;
};

#endif
