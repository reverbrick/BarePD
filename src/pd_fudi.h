//
// pd_fudi.h
//
// BarePD - FUDI (Fast Universal Digital Interface) protocol parser
// Enables remote control of Pure Data patches via serial
//
// Protocol format: receiver message args;
// Examples:
//   freq 440;           -> libpd_float("freq", 440)
//   trigger bang;       -> libpd_bang("trigger")
//   pd dsp 1;           -> libpd_message("pd", "dsp", 1, [1])
//   osc freq 440 amp 0.5; -> libpd_message("osc", "freq", ...)
//
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
// Licensed under GPLv3
//

#ifndef _pd_fudi_h
#define _pd_fudi_h

#include <circle/types.h>

// Maximum message length
#define FUDI_MAX_MESSAGE_LEN    256
#define FUDI_MAX_ATOMS          32

// FUDI output callback type
typedef void (*TFudiOutputCallback)(const char *pMessage);

class CFudiParser
{
public:
    CFudiParser(void);
    ~CFudiParser(void);

    /// Process incoming bytes, returns true if a complete message was parsed
    /// Call repeatedly with incoming serial data
    boolean ProcessByte(char c);
    
    /// Process a buffer of bytes
    /// Returns number of complete messages parsed
    unsigned ProcessBuffer(const char *pBuffer, unsigned nLength);
    
    /// Set callback for Pd output (messages from Pd to send back)
    void SetOutputCallback(TFudiOutputCallback pCallback);
    
    /// Send a message back (formats as FUDI and calls callback)
    void SendFloat(const char *pReceiver, float fValue);
    void SendBang(const char *pReceiver);
    void SendSymbol(const char *pReceiver, const char *pSymbol);
    void SendMessage(const char *pReceiver, const char *pMessage);

    /// Get statistics
    unsigned GetMessagesReceived(void) const { return m_nMessagesReceived; }
    unsigned GetParseErrors(void) const { return m_nParseErrors; }

private:
    /// Parse and execute a complete FUDI message
    boolean ParseMessage(void);
    
    /// Parse a single atom (float or symbol)
    boolean ParseAtom(const char *pAtom, boolean *pbIsFloat, float *pfValue);

    // Input buffer
    char m_Buffer[FUDI_MAX_MESSAGE_LEN];
    unsigned m_nBufferPos;
    
    // Output callback
    TFudiOutputCallback m_pOutputCallback;
    
    // Statistics
    unsigned m_nMessagesReceived;
    unsigned m_nParseErrors;
};

#endif

