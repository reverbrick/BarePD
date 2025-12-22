//
// kernel.h
//
// BarePD - Bare metal Pure Data for Raspberry Pi
// Based on Circle (https://github.com/rsta2/circle)
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

// Default patch filename
#define DEFAULT_PATCH_NAME      "main.pd"
#define PATCH_DIRECTORY         ""          // Root of SD card
#define MAX_PATCH_SIZE          (256 * 1024) // 256KB max patch size

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
	// Patch loading
	boolean LoadPatch (const char *pPatchName);
	boolean FindAndLoadPatch (void);

	// MIDI handlers
	static void MIDIPacketHandler (unsigned nCable, u8 *pPacket, unsigned nLength);
	static void USBDeviceRemovedHandler (CDevice *pDevice, void *pContext);

private:
	// Core Circle components
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		m_Screen;
	CSerialDevice		m_Serial;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CScheduler		m_Scheduler;

	// USB and I2C
	CUSBHCIDevice		m_USBHCI;
	CI2CMaster		m_I2CMaster;

	// SD Card and filesystem
	CEMMCDevice		m_EMMC;
	CFATFileSystem		m_FileSystem;

	// PD Sound device
	CPdSoundDevice		*m_pPdSound;

	// USB MIDI
	CUSBMIDIDevice		*m_pMIDIDevice;

	// Loaded patch handle
	void			*m_pPatch;

	static CKernel		*s_pThis;
};

#endif
