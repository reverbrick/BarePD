//
// pd_fudi.cpp
//
// BarePD - FUDI protocol parser implementation
// Copyright (C) 2024 Daniel Górny <PlayableElectronics>
// Licensed under GPLv3
//

#include "pd_fudi.h"
#include <circle/logger.h>
#include <circle/util.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

extern "C" {
#include "z_libpd.h"
}

static const char FromFudi[] = "fudi";

CFudiParser::CFudiParser(void)
:   m_nBufferPos(0),
    m_pOutputCallback(nullptr),
    m_nMessagesReceived(0),
    m_nParseErrors(0)
{
    memset(m_Buffer, 0, sizeof(m_Buffer));
}

CFudiParser::~CFudiParser(void)
{
}

boolean CFudiParser::ProcessByte(char c)
{
    // Ignore carriage returns
    if (c == '\r')
        return FALSE;
    
    // Semicolon or newline terminates message
    if (c == ';' || c == '\n')
    {
        if (m_nBufferPos > 0)
        {
            m_Buffer[m_nBufferPos] = '\0';
            boolean bResult = ParseMessage();
            m_nBufferPos = 0;
            return bResult;
        }
        return FALSE;
    }
    
    // Add to buffer if there's room
    if (m_nBufferPos < FUDI_MAX_MESSAGE_LEN - 1)
    {
        m_Buffer[m_nBufferPos++] = c;
    }
    else
    {
        // Buffer overflow - reset
        CLogger::Get()->Write(FromFudi, LogWarning, "Message too long, discarding");
        m_nBufferPos = 0;
        m_nParseErrors++;
    }
    
    return FALSE;
}

unsigned CFudiParser::ProcessBuffer(const char *pBuffer, unsigned nLength)
{
    unsigned nMessages = 0;
    
    for (unsigned i = 0; i < nLength; i++)
    {
        if (ProcessByte(pBuffer[i]))
        {
            nMessages++;
        }
    }
    
    return nMessages;
}

boolean CFudiParser::ParseAtom(const char *pAtom, boolean *pbIsFloat, float *pfValue)
{
    if (!pAtom || !*pAtom)
        return FALSE;
    
    // Try to parse as float
    char *pEnd;
    float fVal = strtof(pAtom, &pEnd);
    
    if (pEnd != pAtom && *pEnd == '\0')
    {
        // Successfully parsed as float
        *pbIsFloat = TRUE;
        *pfValue = fVal;
        return TRUE;
    }
    
    // It's a symbol
    *pbIsFloat = FALSE;
    return TRUE;
}

boolean CFudiParser::ParseMessage(void)
{
    // Tokenize the message
    char *pTokens[FUDI_MAX_ATOMS];
    unsigned nTokens = 0;
    
    // Make a working copy
    char szWork[FUDI_MAX_MESSAGE_LEN];
    strncpy(szWork, m_Buffer, sizeof(szWork) - 1);
    szWork[sizeof(szWork) - 1] = '\0';
    
    // Split by whitespace
    char *pSave = nullptr;
    char *pTok = strtok_r(szWork, " \t", &pSave);
    while (pTok && nTokens < FUDI_MAX_ATOMS)
    {
        pTokens[nTokens++] = pTok;
        pTok = strtok_r(nullptr, " \t", &pSave);
    }
    
    if (nTokens == 0)
        return FALSE;
    
    // First token is the receiver
    const char *pReceiver = pTokens[0];
    
    // Handle special case: single token = bang
    if (nTokens == 1)
    {
        // Just receiver name = send bang
        if (libpd_bang(pReceiver) == 0)
        {
            m_nMessagesReceived++;
            CLogger::Get()->Write(FromFudi, LogDebug, "bang -> %s", pReceiver);
            return TRUE;
        }
        else
        {
            CLogger::Get()->Write(FromFudi, LogWarning, "Unknown receiver: %s", pReceiver);
            m_nParseErrors++;
            return FALSE;
        }
    }
    
    // Second token might be a message type or a value
    const char *pSecond = pTokens[1];
    boolean bIsFloat;
    float fValue;
    
    // Check if second token is "bang"
    if (strcmp(pSecond, "bang") == 0)
    {
        if (libpd_bang(pReceiver) == 0)
        {
            m_nMessagesReceived++;
            CLogger::Get()->Write(FromFudi, LogDebug, "bang -> %s", pReceiver);
            return TRUE;
        }
        m_nParseErrors++;
        return FALSE;
    }
    
    // Two tokens: receiver + float
    if (nTokens == 2)
    {
        if (ParseAtom(pSecond, &bIsFloat, &fValue) && bIsFloat)
        {
            if (libpd_float(pReceiver, fValue) == 0)
            {
                m_nMessagesReceived++;
                CLogger::Get()->Write(FromFudi, LogDebug, "%.2f -> %s", (double)fValue, pReceiver);
                return TRUE;
            }
        }
        else
        {
            // It's a symbol
            if (libpd_symbol(pReceiver, pSecond) == 0)
            {
                m_nMessagesReceived++;
                CLogger::Get()->Write(FromFudi, LogDebug, "%s -> %s", pSecond, pReceiver);
                return TRUE;
            }
        }
        m_nParseErrors++;
        return FALSE;
    }
    
    // Three or more tokens: receiver + message + args
    // Format: receiver message arg1 arg2 ...
    const char *pMessage = pSecond;
    
    // Build argument list
    int nArgs = nTokens - 2;
    if (nArgs > 0)
    {
        if (libpd_start_message(nArgs) != 0)
        {
            m_nParseErrors++;
            return FALSE;
        }
        
        for (int i = 0; i < nArgs; i++)
        {
            const char *pArg = pTokens[2 + i];
            if (ParseAtom(pArg, &bIsFloat, &fValue))
            {
                if (bIsFloat)
                {
                    libpd_add_float(fValue);
                }
                else
                {
                    libpd_add_symbol(pArg);
                }
            }
        }
        
        if (libpd_finish_message(pReceiver, pMessage) == 0)
        {
            m_nMessagesReceived++;
            CLogger::Get()->Write(FromFudi, LogDebug, "%s %s [...] -> %s", 
                                  pMessage, nArgs > 0 ? pTokens[2] : "", pReceiver);
            return TRUE;
        }
    }
    
    m_nParseErrors++;
    return FALSE;
}

void CFudiParser::SetOutputCallback(TFudiOutputCallback pCallback)
{
    m_pOutputCallback = pCallback;
}

void CFudiParser::SendFloat(const char *pReceiver, float fValue)
{
    if (m_pOutputCallback)
    {
        char szMsg[FUDI_MAX_MESSAGE_LEN];
        snprintf(szMsg, sizeof(szMsg), "%s %g;\n", pReceiver, (double)fValue);
        m_pOutputCallback(szMsg);
    }
}

void CFudiParser::SendBang(const char *pReceiver)
{
    if (m_pOutputCallback)
    {
        char szMsg[FUDI_MAX_MESSAGE_LEN];
        snprintf(szMsg, sizeof(szMsg), "%s bang;\n", pReceiver);
        m_pOutputCallback(szMsg);
    }
}

void CFudiParser::SendSymbol(const char *pReceiver, const char *pSymbol)
{
    if (m_pOutputCallback)
    {
        char szMsg[FUDI_MAX_MESSAGE_LEN];
        snprintf(szMsg, sizeof(szMsg), "%s %s;\n", pReceiver, pSymbol);
        m_pOutputCallback(szMsg);
    }
}

void CFudiParser::SendMessage(const char *pReceiver, const char *pMessage)
{
    if (m_pOutputCallback)
    {
        char szMsg[FUDI_MAX_MESSAGE_LEN];
        snprintf(szMsg, sizeof(szMsg), "%s %s;\n", pReceiver, pMessage);
        m_pOutputCallback(szMsg);
    }
}

