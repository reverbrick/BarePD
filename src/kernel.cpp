//
// kernel.cpp
//
// BarePD - Bare metal Pure Data for Raspberry Pi
// Based on Circle (https://github.com/rsta2/circle)
//
#include "kernel.h"
#include <circle/machineinfo.h>
#include <circle/util.h>
#include <assert.h>

// libpd includes
extern "C" {
#include "z_libpd.h"
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

	// Open the patch file
	unsigned hFile = m_FileSystem.FileOpen (pPatchName);
	if (hFile == 0)
	{
		m_Logger.Write (FromKernel, LogWarning, "Cannot open patch file: %s", pPatchName);
		return FALSE;
	}

	// Allocate buffer for patch content
	static char patchBuffer[MAX_PATCH_SIZE];
	unsigned nBytesRead = 0;
	unsigned nTotalRead = 0;

	// Read the entire file
	while ((nBytesRead = m_FileSystem.FileRead (hFile, patchBuffer + nTotalRead, 
	                                            MAX_PATCH_SIZE - nTotalRead - 1)) > 0)
	{
		nTotalRead += nBytesRead;
		if (nTotalRead >= MAX_PATCH_SIZE - 1)
		{
			m_Logger.Write (FromKernel, LogWarning, "Patch file too large (max %u bytes)", MAX_PATCH_SIZE);
			break;
		}
	}

	m_FileSystem.FileClose (hFile);

	if (nTotalRead == 0)
	{
		m_Logger.Write (FromKernel, LogWarning, "Empty patch file: %s", pPatchName);
		return FALSE;
	}

	// Null-terminate the buffer
	patchBuffer[nTotalRead] = '\0';

	m_Logger.Write (FromKernel, LogNotice, "Read %u bytes from patch file", nTotalRead);

	// Extract directory and filename from patch path
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

	// Open the patch in libpd
	m_pPatch = libpd_openfile (pFilename, pDirectory);
	
	if (m_pPatch == nullptr)
	{
		m_Logger.Write (FromKernel, LogError, "libpd failed to open patch: %s", pPatchName);
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
	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "========================================");
	m_Logger.Write (FromKernel, LogNotice, "  BarePD - Bare Metal Pure Data");
	m_Logger.Write (FromKernel, LogNotice, "  https://github.com/reverbrick/parepd");
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

	// Parse configuration
	ParseConfig ();

	// Initialize libpd
	m_Logger.Write (FromKernel, LogNotice, "Initializing libpd...");
	
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

	// Initialize libpd
	if (libpd_init() != 0)
	{
		m_Logger.Write (FromKernel, LogWarning, "libpd already initialized");
	}

	// Setup audio output
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
	
	// Start sound device
	if (!m_pSoundDevice->Start ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot start audio device");
		return ShutdownHalt;
	}

	m_Logger.Write (FromKernel, LogNotice, "");
	m_Logger.Write (FromKernel, LogNotice, "BarePD is running!");
	m_Logger.Write (FromKernel, LogNotice, "Audio output: %s", CAudioOutputFactory::GetTypeName(m_AudioOutput));
	m_Logger.Write (FromKernel, LogNotice, "Connect USB MIDI to send notes to the patch.");
	m_Logger.Write (FromKernel, LogNotice, "");

	// Main loop
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
