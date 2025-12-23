//
// kernel.cpp
//
// BarePD - Bare metal Pure Data for Raspberry Pi
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
//
// Based on Circle (https://github.com/rsta2/circle)
// Licensed under GPLv3
//
#include "kernel.h"
#include <circle/machineinfo.h>
#include <circle/util.h>
#include <assert.h>
#include <cstdlib>
#include <math.h>

// libpd includes
extern "C" {
#include "z_libpd.h"
#include "pd_fileio.h"
}

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = nullptr;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
	m_I2CMaster (CMachineInfo::Get ()->GetDevice (DeviceI2CMaster), TRUE),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_AudioOutput (AudioOutputI2S),  // Default to I2S for PCM5102A
	m_nSampleRate (DEFAULT_SAMPLE_RATE_HZ),
	m_bHeadless (FALSE),
	m_pSoundDevice (nullptr),
	m_pI2SDevice (nullptr),
	m_pMIDIDevice (nullptr),
	m_pPatch (nullptr)
{
	s_pThis = this;
	m_ActLED.Blink (5);
}

CKernel::~CKernel (void)
{
	delete m_pSoundDevice;
	delete m_pI2SDevice;
	s_pThis = nullptr;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	// Check for headless mode (skip video for lower latency)
	m_bHeadless = m_Options.GetAppOptionDecimal ("headless", 0) != 0;

	// Initialize serial first (always needed for logging in headless mode)
	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	// Only initialize screen if not headless
	if (bOK && !m_bHeadless)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == nullptr)
		{
			// Use serial in headless mode, screen otherwise
			pTarget = m_bHeadless ? (CDevice *)&m_Serial : (CDevice *)&m_Screen;
		}
		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_I2CMaster.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
	}

	return bOK;
}

void CKernel::ParseConfig (void)
{
	// Parse audio output type from cmdline.txt
	// Format: audio=pwm|i2s|usb
	const char *pAudioType = m_Options.GetAppOptionString ("audio", "pwm");
	m_AudioOutput = CAudioOutputFactory::ParseType (pAudioType);
	
	// Parse sample rate (optional)
	// Format: samplerate=44100|48000|96000
	unsigned nRate = m_Options.GetAppOptionDecimal ("samplerate", DEFAULT_SAMPLE_RATE_HZ);
	if (nRate >= 22050 && nRate <= 192000)
	{
		m_nSampleRate = nRate;
	}
	
	m_Logger.Write (FromKernel, LogNotice, "Audio config: %s @ %u Hz",
	                CAudioOutputFactory::GetTypeName (m_AudioOutput), m_nSampleRate);
}

boolean CKernel::SetupAudio (void)
{
	boolean bOK = FALSE;
	
	switch (m_AudioOutput)
	{
	case AudioOutputI2S:
		// I2S output for PCM5102A and similar DACs
		m_pI2SDevice = new CPdSoundI2S(&m_Interrupt, &m_I2CMaster, m_nSampleRate);
		if (m_pI2SDevice)
		{
			bOK = m_pI2SDevice->Initialize();
		}
		break;
		
	case AudioOutputPWM:
		// PWM output via 3.5mm jack (fallback)
		m_pSoundDevice = CAudioOutputFactory::Create (m_AudioOutput, &m_Interrupt,
		                                              &m_I2CMaster, m_nSampleRate);
		if (m_pSoundDevice)
		{
			bOK = static_cast<CPdSoundPWM*>(m_pSoundDevice)->Initialize();
		}
		break;
		
	default:
		m_Logger.Write (FromKernel, LogError, "Unsupported audio output type");
		break;
	}
	
	if (!bOK)
	{
		m_Logger.Write (FromKernel, LogError, "Failed to initialize audio device");
	}
	
	return bOK;
}

boolean CKernel::LoadPatch (const char *pPatchPath)
{
	m_Logger.Write (FromKernel, LogNotice, "Loading patch: %s", pPatchPath);

	// libpd expects filename and directory separately
	// Parse path to extract directory and filename
	char szDirectory[256] = ".";
	const char *pFilename = pPatchPath;
	
	// Find the last '/' to split directory and filename
	const char *pLastSlash = nullptr;
	for (const char *p = pPatchPath; *p; p++) {
		if (*p == '/') pLastSlash = p;
	}
	
	if (pLastSlash) {
		// Copy directory part
		unsigned nDirLen = pLastSlash - pPatchPath;
		if (nDirLen >= sizeof(szDirectory)) nDirLen = sizeof(szDirectory) - 1;
		memcpy(szDirectory, pPatchPath, nDirLen);
		szDirectory[nDirLen] = '\0';
		pFilename = pLastSlash + 1;
	}

	m_Logger.Write (FromKernel, LogDebug, "Opening patch: dir='%s' file='%s'", szDirectory, pFilename);
	
	// Open the patch in libpd
	m_pPatch = libpd_openfile (pFilename, szDirectory);
	
	if (m_pPatch == nullptr)
	{
		m_Logger.Write (FromKernel, LogError, "libpd failed to open patch: %s", pPatchPath);
		return FALSE;
	}

	m_Logger.Write (FromKernel, LogNotice, "Patch loaded successfully: %s", pPatchPath);
	return TRUE;
}

boolean CKernel::FindAndLoadPatch (void)
{
	// Note: Circle's FAT filesystem only supports root directory
	// Subdirectories are not supported
	
	// Try main.pd first
	m_Logger.Write (FromKernel, LogNotice, "Trying: %s", DEFAULT_PATCH_NAME);
	if (LoadPatch (DEFAULT_PATCH_NAME))
	{
		return TRUE;
	}

	// Look for any .pd file in the root directory
	m_Logger.Write (FromKernel, LogNotice, "Searching for .pd files in root...");

	TDirentry entry;
	TFindCurrentEntry currentEntry;

	if (m_FileSystem.RootFindFirst (&entry, &currentEntry))
	{
		do
		{
			// Check if it's a .pd file
			const char *pName = entry.chTitle;
			unsigned nLen = 0;
			while (pName[nLen]) nLen++;

			if (nLen > 3)
			{
				// Check for .pd extension (case insensitive)
				if (pName[nLen-3] == '.' && 
				    (pName[nLen-2] == 'p' || pName[nLen-2] == 'P') &&
				    (pName[nLen-1] == 'd' || pName[nLen-1] == 'D'))
				{
					m_Logger.Write (FromKernel, LogNotice, "Found patch: %s", pName);
					if (LoadPatch (pName))
					{
						return TRUE;
					}
				}
			}
		}
		while (m_FileSystem.RootFindNext (&entry, &currentEntry));
	}

	m_Logger.Write (FromKernel, LogWarning, "No .pd patch files found on SD card");
	m_Logger.Write (FromKernel, LogWarning, "Place 'main.pd' in /pd/ folder or SD root");
	return FALSE;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "========================================");
	m_Logger.Write (FromKernel, LogNotice, "  BarePD - Bare Metal Pure Data");
	m_Logger.Write (FromKernel, LogNotice, "  https://github.com/reverbrick/BarePD");
	m_Logger.Write (FromKernel, LogNotice, "========================================");
	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Mount SD card filesystem
	CDevice *pPartition = m_DeviceNameService.GetDevice ("emmc1-1", TRUE);
	if (pPartition == nullptr)
	{
		m_Logger.Write (FromKernel, LogError, "Partition not found");
		return ShutdownHalt;
	}

	if (!m_FileSystem.Mount (pPartition))
	{
		m_Logger.Write (FromKernel, LogError, "Cannot mount filesystem");
		return ShutdownHalt;
	}

	m_Logger.Write (FromKernel, LogNotice, "SD card mounted successfully");
	
	// Initialize file I/O bridge for libpd
	pd_fileio_init(&m_FileSystem);

	// Parse configuration
	ParseConfig ();

	// Initialize libpd
	m_Logger.Write (FromKernel, LogNotice, "Initializing libpd...");
	
	m_Logger.Write (FromKernel, LogDebug, "Setting up libpd hooks...");
	
	// Set up print hook to redirect pd output to logger
	libpd_set_printhook ([](const char *s) {
		if (s_pThis)
		{
			// Remove trailing newline for cleaner output
			size_t len = strlen(s);
			if (len > 0 && s[len-1] == '\n')
			{
				char buf[256];
				strncpy(buf, s, sizeof(buf)-1);
				buf[sizeof(buf)-1] = '\0';
				if (len < sizeof(buf))
					buf[len-1] = '\0';
				CLogger::Get()->Write("pd", LogNotice, "%s", buf);
			}
			else
			{
				CLogger::Get()->Write("pd", LogNotice, "%s", s);
			}
		}
	});

	// Set up bang hook for [send] messages with bang
	libpd_set_banghook ([](const char *recv) {
		CLogger::Get()->Write("pd", LogDebug, "[bang] -> %s", recv);
	});

	// Set up float hook for [send] messages with floats
	libpd_set_floathook ([](const char *recv, float x) {
		CLogger::Get()->Write("pd", LogDebug, "[float] -> %s: %f", recv, (double)x);
	});

	// Set up symbol hook for [send] messages with symbols
	libpd_set_symbolhook ([](const char *recv, const char *sym) {
		CLogger::Get()->Write("pd", LogDebug, "[symbol] -> %s: %s", recv, sym);
	});

	// Set up MIDI hooks for monitoring
	libpd_set_noteonhook ([](int ch, int pitch, int vel) {
		CLogger::Get()->Write("pd-midi", LogDebug, "Note %s ch=%d note=%d vel=%d",
		                      vel > 0 ? "ON" : "OFF", ch, pitch, vel);
	});

	libpd_set_controlchangehook ([](int ch, int cc, int val) {
		CLogger::Get()->Write("pd-midi", LogDebug, "CC ch=%d cc=%d val=%d", ch, cc, val);
	});

	// Initialize libpd
	m_Logger.Write (FromKernel, LogNotice, "Initializing libpd...");
	int initResult = libpd_init();
	if (initResult != 0)
	{
		m_Logger.Write (FromKernel, LogWarning, "libpd already initialized");
	}

	// Setup audio output
	m_Logger.Write (FromKernel, LogNotice, "Setting up audio output...");
	if (!SetupAudio ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot initialize audio output");
		return ShutdownHalt;
	}

	// Try to load a patch from SD card
	if (!FindAndLoadPatch ())
	{
		m_Logger.Write (FromKernel, LogWarning, "Running without a patch - audio will be silent");
		m_Logger.Write (FromKernel, LogWarning, "Place a 'main.pd' file on the SD card");
	}

	// Enable DSP
	m_Logger.Write (FromKernel, LogNotice, "Enabling DSP...");
	libpd_start_message(1);
	libpd_add_float(1.0f);
	libpd_finish_message("pd", "dsp");

	m_Logger.Write (FromKernel, LogNotice, "Starting audio output...");
	
	// Start sound device (different for I2S vs PWM)
	boolean bStarted = FALSE;
	if (m_AudioOutput == AudioOutputI2S && m_pI2SDevice)
	{
		bStarted = m_pI2SDevice->Start();
	}
	else if (m_pSoundDevice)
	{
		bStarted = m_pSoundDevice->Start();
	}
	
	if (!bStarted)
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot start audio device");
		return ShutdownHalt;
	}

	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "BarePD is running!");
	m_Logger.Write (FromKernel, LogNotice, "Audio output: %s", CAudioOutputFactory::GetTypeName(m_AudioOutput));
	m_Logger.Write (FromKernel, LogNotice, "Connect USB MIDI to send notes to the patch.");
	m_Logger.Write (FromKernel, LogNotice, "");

	// Main loop - optimized for lowest latency
	// No logging or screen updates during audio processing
	boolean bActive = TRUE;
	while (bActive)
	{
		// Audio processing - highest priority
		if (m_AudioOutput == AudioOutputI2S && m_pI2SDevice)
		{
			bActive = m_pI2SDevice->IsActive();
			m_pI2SDevice->Process();
		}
		else if (m_pSoundDevice)
		{
			bActive = m_pSoundDevice->IsActive();
		}
		else
		{
			bActive = FALSE;
		}
		
		// Check for MIDI device (only when USB state changes)
		if (m_pMIDIDevice == nullptr)
		{
			if (m_USBHCI.UpdatePlugAndPlay())
			{
				m_pMIDIDevice = (CUSBMIDIDevice *) m_DeviceNameService.GetDevice ("umidi1", FALSE);
				if (m_pMIDIDevice != nullptr)
				{
					m_pMIDIDevice->RegisterRemovedHandler (USBDeviceRemovedHandler);
					m_pMIDIDevice->RegisterPacketHandler (MIDIPacketHandler);
				}
			}
		}

		m_Scheduler.Yield();
	}

	// Cleanup
	if (m_pPatch != nullptr)
	{
		libpd_closefile (m_pPatch);
		m_pPatch = nullptr;
	}

	m_FileSystem.UnMount ();

	return ShutdownHalt;
}

void CKernel::MIDIPacketHandler (unsigned nCable, u8 *pPacket, unsigned nLength)
{
	if (nLength < 3)
		return;

	u8 ucStatus  = pPacket[0];
	u8 ucChannel = ucStatus & 0x0F;
	u8 ucType    = ucStatus >> 4;
	u8 ucData1   = pPacket[1];
	u8 ucData2   = pPacket[2];

	// Forward MIDI to libpd
	switch (ucType)
	{
	case 0x8:  // Note Off
		libpd_noteon(ucChannel, ucData1, 0);
		break;
	case 0x9:  // Note On
		libpd_noteon(ucChannel, ucData1, ucData2);
		break;
	case 0xB:  // Control Change
		libpd_controlchange(ucChannel, ucData1, ucData2);
		break;
	case 0xC:  // Program Change
		libpd_programchange(ucChannel, ucData1);
		break;
	case 0xE:  // Pitch Bend
		{
			int value = ((ucData2 << 7) | ucData1) - 8192;
			libpd_pitchbend(ucChannel, value);
		}
		break;
	}
}

void CKernel::USBDeviceRemovedHandler (CDevice *pDevice, void *pContext)
{
	if (s_pThis && s_pThis->m_pMIDIDevice == (CUSBMIDIDevice *) pDevice)
	{
		CLogger::Get()->Write(FromKernel, LogNotice, "USB MIDI device removed");
		s_pThis->m_pMIDIDevice = nullptr;
	}
}
