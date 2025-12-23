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

// libpd includes
extern "C" {
#include "z_libpd.h"
#include "pd_fileio.h"
}

static const char FromKernel[] = "kernel";

CKernel *CKernel::s_pThis = nullptr;

// Simple log buffer for SD card logging
static char s_LogBuffer[LOG_BUFFER_SIZE];
static unsigned s_nLogOffset = 0;

static void LogToBuffer(const char *pSource, const char *pMessage)
{
	if (s_nLogOffset >= LOG_BUFFER_SIZE - 256)
		return;  // Buffer full
	
	int n = snprintf(s_LogBuffer + s_nLogOffset, LOG_BUFFER_SIZE - s_nLogOffset,
	                 "[%s] %s\n", pSource, pMessage);
	if (n > 0)
		s_nLogOffset += n;
}

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
	m_I2CMaster (CMachineInfo::Get ()->GetDevice (DeviceI2CMaster), TRUE),
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
	m_AudioOutput (DEFAULT_AUDIO_OUTPUT),
	m_nSampleRate (DEFAULT_SAMPLE_RATE_HZ),
	m_pSoundDevice (nullptr),
	m_pMIDIDevice (nullptr),
	m_pPatch (nullptr)
{
	s_pThis = this;
	m_ActLED.Blink (5);
}

CKernel::~CKernel (void)
{
	delete m_pSoundDevice;
	s_pThis = nullptr;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == nullptr)
		{
			pTarget = &m_Screen;
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

void CKernel::WriteLogToSD (void)
{
	if (s_nLogOffset == 0)
		return;
	
	unsigned hFile = m_FileSystem.FileCreate (LOG_FILE_NAME);
	if (hFile != 0)
	{
		m_FileSystem.FileWrite (hFile, s_LogBuffer, s_nLogOffset);
		m_FileSystem.FileClose (hFile);
	}
}

// Helper macro to log to both logger and buffer
#define LOG(level, fmt, ...) do { \
	char _logbuf[256]; \
	snprintf(_logbuf, sizeof(_logbuf), fmt, ##__VA_ARGS__); \
	m_Logger.Write(FromKernel, level, "%s", _logbuf); \
	LogToBuffer(FromKernel, _logbuf); \
} while(0)

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
	// Create the appropriate sound device
	m_pSoundDevice = CAudioOutputFactory::Create (m_AudioOutput, &m_Interrupt,
	                                              &m_I2CMaster, m_nSampleRate);
	
	if (m_pSoundDevice == nullptr)
	{
		m_Logger.Write (FromKernel, LogError, "Failed to create audio device");
		return FALSE;
	}
	
	// Initialize based on device type
	boolean bOK = FALSE;
	
	switch (m_AudioOutput)
	{
	case AudioOutputPWM:
		bOK = static_cast<CPdSoundPWM*>(m_pSoundDevice)->Initialize();
		break;
	case AudioOutputI2S:
		bOK = static_cast<CPdSoundI2S*>(m_pSoundDevice)->Initialize();
		break;
	default:
		m_Logger.Write (FromKernel, LogError, "Unsupported audio output type");
		break;
	}
	
	return bOK;
}

boolean CKernel::LoadPatch (const char *pPatchName)
{
	m_Logger.Write (FromKernel, LogNotice, "Loading patch: %s", pPatchName);
	LogToBuffer(FromKernel, "Loading patch...");

	// libpd expects filename and directory separately
	const char *pDirectory = ".";
	
	// Find the filename part (after last '/')
	const char *pFilename = pPatchName;
	const char *pSlash = pPatchName;
	while (*pSlash)
	{
		if (*pSlash == '/')
			pFilename = pSlash + 1;
		pSlash++;
	}

	m_Logger.Write (FromKernel, LogDebug, "Opening patch: dir='%s' file='%s'", pDirectory, pFilename);
	
	// Open the patch in libpd (this will use newlib fopen -> _open)
	m_pPatch = libpd_openfile (pFilename, pDirectory);
	
	if (m_pPatch == nullptr)
	{
		m_Logger.Write (FromKernel, LogError, "libpd failed to open patch: %s", pPatchName);
		LogToBuffer(FromKernel, "ERROR: libpd_openfile failed");
		return FALSE;
	}

	m_Logger.Write (FromKernel, LogNotice, "Patch loaded successfully: %s", pPatchName);
	return TRUE;
}

boolean CKernel::FindAndLoadPatch (void)
{
	// Try to load the default patch first
	if (LoadPatch (DEFAULT_PATCH_NAME))
	{
		return TRUE;
	}

	// Look for any .pd file in the root directory
	m_Logger.Write (FromKernel, LogNotice, "Searching for .pd files...");

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
				// Check for .pd extension
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
	return FALSE;
}

TShutdownMode CKernel::Run (void)
{
	// Start logging to buffer
	LogToBuffer(FromKernel, "========================================");
	LogToBuffer(FromKernel, "  BarePD - Bare Metal Pure Data");
	LogToBuffer(FromKernel, "  https://github.com/reverbrick/BarePD");
	LogToBuffer(FromKernel, "========================================");
	LogToBuffer(FromKernel, "Compile time: " __DATE__ " " __TIME__);
	
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
		LogToBuffer(FromKernel, "ERROR: Partition not found");
		return ShutdownHalt;
	}

	if (!m_FileSystem.Mount (pPartition))
	{
		m_Logger.Write (FromKernel, LogError, "Cannot mount filesystem");
		LogToBuffer(FromKernel, "ERROR: Cannot mount filesystem");
		return ShutdownHalt;
	}

	m_Logger.Write (FromKernel, LogNotice, "SD card mounted successfully");
	LogToBuffer(FromKernel, "SD card mounted successfully");
	
	// Write early debug marker
	unsigned hDebug = m_FileSystem.FileCreate("DEBUG1.TXT");
	if (hDebug) {
		m_FileSystem.FileWrite(hDebug, "Mounted OK\n", 11);
		m_FileSystem.FileClose(hDebug);
	}
	
	// Initialize file I/O bridge for libpd
	pd_fileio_init(&m_FileSystem);
	LogToBuffer(FromKernel, "File I/O initialized");
	
	// Write second debug marker
	hDebug = m_FileSystem.FileCreate("DEBUG2.TXT");
	if (hDebug) {
		m_FileSystem.FileWrite(hDebug, "FileIO skipped\n", 15);
		m_FileSystem.FileClose(hDebug);
	}

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
	m_Logger.Write (FromKernel, LogNotice, "About to call libpd_init()...");
	LogToBuffer(FromKernel, "Calling libpd_init()...");
	m_Timer.MsDelay(100);  // Flush output before potential crash
	
	int initResult = libpd_init();
	
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "libpd_init() returned: %d", initResult);
		m_Logger.Write (FromKernel, LogNotice, "%s", buf);
		LogToBuffer(FromKernel, buf);
	}
	if (initResult != 0)
	{
		m_Logger.Write (FromKernel, LogWarning, "libpd already initialized");
	}

	// Setup audio output
	m_Logger.Write (FromKernel, LogDebug, "Setting up audio output...");
	LogToBuffer(FromKernel, "Setting up audio output...");
	if (!SetupAudio ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot initialize audio output");
		LogToBuffer(FromKernel, "ERROR: Cannot initialize audio output");
		WriteLogToSD();
		return ShutdownHalt;
	}
	LogToBuffer(FromKernel, "Audio output initialized");

	// Try to load a patch from SD card
	if (!FindAndLoadPatch ())
	{
		m_Logger.Write (FromKernel, LogWarning, "Running without a patch - audio will be silent");
		m_Logger.Write (FromKernel, LogWarning, "Place a 'main.pd' file on the SD card");
		LogToBuffer(FromKernel, "WARNING: No patch loaded");
	}
	else
	{
		LogToBuffer(FromKernel, "Patch loaded successfully");
	}

	// Enable DSP
	m_Logger.Write (FromKernel, LogNotice, "Enabling DSP...");
	LogToBuffer(FromKernel, "Enabling DSP...");
	libpd_start_message(1);
	libpd_add_float(1.0f);
	libpd_finish_message("pd", "dsp");
	LogToBuffer(FromKernel, "DSP enabled");

	m_Logger.Write (FromKernel, LogNotice, "Starting audio output...");
	LogToBuffer(FromKernel, "Starting audio output...");
	
	// Start sound device
	if (!m_pSoundDevice->Start ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot start audio device");
		LogToBuffer(FromKernel, "ERROR: Cannot start audio device");
		WriteLogToSD();
		return ShutdownHalt;
	}
	LogToBuffer(FromKernel, "Audio device started");

	{
		char buf[128];
		snprintf(buf, sizeof(buf), "BarePD running! Audio: %s", 
		         CAudioOutputFactory::GetTypeName(m_AudioOutput));
		LogToBuffer(FromKernel, buf);
	}
	
	// Write startup log to SD card
	WriteLogToSD();
	m_Logger.Write (FromKernel, LogNotice, "Startup log written to %s", LOG_FILE_NAME);

	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "BarePD is running!");
	m_Logger.Write (FromKernel, LogNotice, "Audio output: %s", CAudioOutputFactory::GetTypeName(m_AudioOutput));
	m_Logger.Write (FromKernel, LogNotice, "Connect USB MIDI to send notes to the patch.");
	m_Logger.Write (FromKernel, LogNotice, "");

	// Main loop
	unsigned nLastStatusTime = 0;
	for (unsigned nCount = 0; m_pSoundDevice->IsActive (); nCount++)
	{
		// Update USB devices (for MIDI and USB audio)
		boolean bUpdated = m_USBHCI.UpdatePlugAndPlay ();

		// Check for MIDI device
		if (m_pMIDIDevice == nullptr && bUpdated)
		{
			m_pMIDIDevice = (CUSBMIDIDevice *) m_DeviceNameService.GetDevice ("umidi1", FALSE);
			if (m_pMIDIDevice != nullptr)
			{
				m_pMIDIDevice->RegisterRemovedHandler (USBDeviceRemovedHandler);
				m_pMIDIDevice->RegisterPacketHandler (MIDIPacketHandler);
				m_Logger.Write (FromKernel, LogNotice, "USB MIDI device connected");
			}
		}

		// Periodic status update (every 10 seconds)
		unsigned nCurrentTime = m_Timer.GetUptime ();
		if (nCurrentTime - nLastStatusTime >= 10)
		{
			nLastStatusTime = nCurrentTime;
			m_Logger.Write (FromKernel, LogDebug, "Status: uptime=%us MIDI=%s",
			                nCurrentTime,
			                m_pMIDIDevice ? "connected" : "none");
		}

		m_Scheduler.Yield ();
		m_Screen.Rotor (0, nCount);
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
