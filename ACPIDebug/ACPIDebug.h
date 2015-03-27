/*
 * Copyright (c) 2012 RehabMan. All rights reserved.
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

#ifndef __ACPIDebug__
#define __ACPIDebug__

#define ACPIDebug org_rehabman_ACPIDebug

#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOTimerEventSource.h>

#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

#ifdef DEBUG_MSG
#define DEBUG_LOG(args...)  IOLog(args)
#else
#define DEBUG_LOG(args...)
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class EXPORT ACPIDebug : public IOService
{
    typedef IOService super;
	OSDeclareDefaultStructors(org_rehabman_ACPIDebug)

public:
	virtual bool init(OSDictionary *dictionary = 0);
    virtual IOService *probe(IOService *provider, SInt32 *score);
    virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
    virtual IOReturn message( UInt32 type, IOService * provider, void * argument);
    virtual IOReturn setProperties(OSObject* props);
    
private:
	IOACPIPlatformDevice*   m_pDevice;
    IOWorkLoop*             m_pWorkLoop;
	IOTimerEventSource*     m_pTimer;
    int                     m_nPollingInterval;
    IOCommandGate*          m_pCmdGate;
    IOLock*                 m_pLock;
   
    IOReturn OnTimerEvent(void);
    void PrintTraces(void);
    static size_t FormatDebugString(OSObject* debug, char* buf, size_t buf_size);
    IOReturn setPropertiesGated(OSObject* props);
};

#endif // __ACPIDebug__