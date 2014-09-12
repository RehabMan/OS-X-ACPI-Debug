/*
 * Copyright (c) 2013 RehabMan. All rights reserved.
 *
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your 
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along 
 *  with this program; if not, write to the Free Software Foundation, Inc., 
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <IOKit/IOCommandGate.h>
#include "ACPIDebug.h"

//REVIEW: avoids problem with Xcode 5.1.0 where -dead_strip eliminates these required symbols
#include <libkern/OSKextLib.h>
void* _org_rehabman_dontstrip_[] =
{
    (void*)&OSKextGetCurrentIdentifier,
    (void*)&OSKextGetCurrentLoadTag,
    (void*)&OSKextGetCurrentVersionString,
};

OSDefineMetaClassAndStructors(org_rehabman_ACPIDebug, IOService)

/******************************************************************************
 * ACPIDebug::init
 ******************************************************************************/
bool ACPIDebug::init(OSDictionary *dict)
{
    DEBUG_LOG("ACPIDebug::init: Initializing\n");
    
    bool result = super::init(dict);
    m_pDevice = NULL;
    m_pWorkLoop = NULL;
    m_pTimer = NULL;
    m_pCmdGate = NULL;
    
    m_nPollingInterval = 100;
	if (OSNumber* num = OSDynamicCast(OSNumber, dict->getObject("PollingInterval")))
    {
		m_nPollingInterval = (int)num->unsigned32BitValue();
        //setProperty("PollingInterval", _wakedelay, 32);
    }
    
    return result;
}

/******************************************************************************
 * ACPIDebug::probe
 ******************************************************************************/
IOService *ACPIDebug::probe(IOService *provider, SInt32 *score)
{
    DEBUG_LOG("ACPIDebug::probe: Probing\n");
    
    IOService *result = super::probe(provider, score);
    IOACPIPlatformDevice* pDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
    
    // check for proper DSDT methods
    if (kIOReturnSuccess != pDevice->validateObject("COUN") ||
        kIOReturnSuccess != pDevice->validateObject("FTCH"))
    {
        DEBUG_LOG("ACPIDebug::probe: DSDT methods COUN or FTCH not available\n");
        return NULL;
    }
    
    return result;
}

/******************************************************************************
 * ACPIDebug::start
 ******************************************************************************/
bool ACPIDebug::start(IOService *provider)
{
    DEBUG_LOG("ACPIDebug::start: called\n");
    
    m_pDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (NULL == m_pDevice || !super::start(provider))
        return false;

    // need a work loop to send timer events to
    m_pWorkLoop = getWorkLoop();
    if (NULL == m_pWorkLoop)
        return false;
    m_pWorkLoop->retain();

    // need a timer to kick off every second
    m_pTimer = IOTimerEventSource::timerEventSource(this,
        OSMemberFunctionCast(IOTimerEventSource::Action, this, &ACPIDebug::OnTimerEvent));
    if (NULL == m_pTimer)
        return false;
	if (kIOReturnSuccess != m_pWorkLoop->addEventSource(m_pTimer))
        return false;

    // command gate to route setProperties through workloop
    m_pCmdGate = IOCommandGate::commandGate(this);
    if (m_pCmdGate)
        m_pWorkLoop->addEventSource(m_pCmdGate);
    
	IOLog("ACPIDebug: Version 0.1.2 starting\n");
    
    // call it once
    OnTimerEvent();
    
	this->registerService(0);
    return true;
}

/******************************************************************************
 * ACPIDebug::message (handling Notify)
 ******************************************************************************/
IOReturn ACPIDebug::message(UInt32 type, IOService * provider, void * argument)
{
	if (type == kIOACPIMessageDeviceNotification)
	{
        DEBUG_LOG("ACPIDebug::message(%d, %p, %p)\n", type, provider, argument);
        PrintTraces();
	}
	return super::message(type, provider, argument);
}

/******************************************************************************
 * ACPIDebug::PrintTraces
 ******************************************************************************/
void ACPIDebug::PrintTraces()
{
    for (;;)
    {
        // see if there are any trace items in the RING
        UInt32 count = 0;
        if (kIOReturnSuccess != m_pDevice->evaluateInteger("COUN", &count))
        {
            IOLog("ACPIDebug: evaluateObject of COUN method failed\n");
            break;
            
        }
        if (!count)
            break;
        
        // gather the next item from RING and print it
        OSObject* debug;
        if (kIOReturnSuccess == m_pDevice->evaluateObject("FTCH", &debug) &&
            NULL != debug)
        {
            static char buf[2048];
            // got a valid object, format and print it...
            FormatDebugString(debug, buf, sizeof(buf)/sizeof(buf[0]));
            IOLog("ACPIDebug: %s\n", buf);
            debug->release();
        }
    }
}

/******************************************************************************
 * ACPIDebug::OnTimerEvent
 ******************************************************************************/
IOReturn ACPIDebug::OnTimerEvent()
{
    //REVIEW: timer may now be unecessary
    PrintTraces();

    if (NULL != m_pTimer)
    {
        m_pTimer->cancelTimeout();
        m_pTimer->setTimeoutMS(m_nPollingInterval);
    }
    
    return kIOReturnSuccess;
}

/******************************************************************************
 * FormatDebugString
 ******************************************************************************/
size_t ACPIDebug::FormatDebugString(OSObject* debug, char* buf, size_t buf_size)
{
    // determine type of object
    // for integer: just print the value
    // for string: print string in quotes
    // for array: print comma separated contents { }
    // for buffer: print comma separated bytes { }
    //REVIEW: are there other types?
    //REVIEW: how to allow formatting?
    
    size_t left = buf_size;
    int n;
    if (OSNumber* num = OSDynamicCast(OSNumber, debug))
    {
        n = snprintf(buf, left, "0x%llx", num->unsigned64BitValue());
        left -= n; buf += n;
    }
    else if (OSString* str = OSDynamicCast(OSString, debug))
    {
        n = snprintf(buf, left, "\"%s\"", str->getCStringNoCopy());
        left -= n; buf += n;
    }
    else if (OSData* data = OSDynamicCast(OSData, debug))
    {
        n = snprintf(buf, buf_size, "{ ");
        left -= n; buf += n;
        int count = data->getLength();
        const uint8_t* p = static_cast<const uint8_t*>(data->getBytesNoCopy());
        for (int i = 0; i < count; i++)
        {
            n = snprintf(buf, left, "0x%02x, ", p[i]);
            left -= n; buf += n;
        }
        n = snprintf(buf, left, "}");
        left -= n; buf += n;
    }
    else if (OSArray* arr = OSDynamicCast(OSArray, debug))
    {
        n = snprintf(buf, buf_size, "{ ");
        left -= n; buf += n;
        int count = arr->getCount();
        for (int i = 0; i < count; i++)
        {
            size_t n = FormatDebugString(arr->getObject(i), buf, left);
            left -= n; buf += n;
            n = snprintf(buf, left, ", ");
            left -= n; buf += n;
        }
        n = snprintf(buf, left, "}");
        left -= n; buf += n;
    }
    else
    {
        size_t n = snprintf(buf, buf_size, "!!unknown type!!");
        left -= n; buf += n;
    }
    
    return buf_size - left;
}

/******************************************************************************
 * ACPIDebug::stop
 ******************************************************************************/
void ACPIDebug::stop(IOService *provider)
{
	DEBUG_LOG("ACPIDebug::stop: called\n");
    
    if (NULL != m_pTimer)
    {
        m_pTimer->cancelTimeout();
        m_pWorkLoop->removeEventSource(m_pTimer);
        m_pTimer->release();
        m_pTimer = NULL;
    }
    if (NULL != m_pCmdGate)
    {
        m_pWorkLoop->removeEventSource(m_pCmdGate);
        m_pCmdGate->release();
        m_pCmdGate = NULL;
    }
    
    if (NULL != m_pWorkLoop)
    {
        m_pWorkLoop->release();
        m_pWorkLoop = NULL;
    }
	
    super::stop(provider);
}

/******************************************************************************
 * ACPIDebug::setProperties
 ******************************************************************************/
IOReturn ACPIDebug::setProperties(OSObject* props)
{
    if (m_pCmdGate)
    {
        // syncronize through workloop...
        IOReturn result = m_pCmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &ACPIDebug::setPropertiesGated), props);
        if (kIOReturnSuccess != result)
            return result;
    }
    return kIOReturnSuccess;
}

/******************************************************************************
 * ACPIDebug::setPropertiesGated
 ******************************************************************************/
IOReturn ACPIDebug::setPropertiesGated(OSObject* props)
{
    OSDictionary* dict = OSDynamicCast(OSDictionary, props);
    if (!dict)
        return kIOReturnSuccess;
    
    for (int i = 0; i < 10; i++)
    {
        const char kTriggerFormat[] = "dbg%d";
        const char kTargetFormat[] = "DBG%d";
        char buf[16];
        snprintf(buf, sizeof(buf), kTriggerFormat, i);
        if (OSNumber* num = OSDynamicCast(OSNumber, dict->getObject(buf)))
        {
            snprintf(buf, sizeof(buf), kTargetFormat, i);
            IOReturn result = m_pDevice->evaluateObject(buf, NULL, (OSObject**)&num, 1);
            if (kIOReturnSuccess != result)
            {
                IOLog("ACPIDebug evaluateObject(\"%s\") failed (%x)\n", buf, result);
            }
            //num->release();
        }
    }
    
    return kIOReturnSuccess;
}

