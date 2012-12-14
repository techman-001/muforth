/*
 * This file is part of muFORTH: http://muforth.nimblemachines.com/
 *
 * Copyright (c) 2002-2012 David Frech. All rights reserved, and all wrongs
 * reversed. (See the file COPYRIGHT for details.)
 */

/*
 * OSX support for USB devices.
 */

#include "muforth.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

static void add_number_match(CFMutableDictionaryRef matching,
                             CFStringRef keyRef,
                             SInt32 value)
{
    CFNumberRef numberRef;

    /* Create a CFNumber for the given key and set the value in the dictionary */
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    CFDictionarySetValue(matching, keyRef, numberRef);
    CFRelease(numberRef);
}

#ifdef WITH_USB_DEVICE
/*
 * usb-find-device (vendor-id product-id -- handle -1 | 0)
 */
void mu_usb_find_device()
{
    CFMutableDictionaryRef matching;
    io_service_t ioService;
    IOCFPlugInInterface **pluginInterface;
    IOUSBDeviceInterface **deviceInterface;
    IOReturn ior;
    SInt32 score;      /* unused */

    /* Match instances of IOUSBDevice and its subclasses */
    matching = IOServiceMatching(kIOUSBDeviceClassName);
    if (matching == NULL) return abort_zmsg("IOServiceMatching returned NULL");

    /* Match device's VendorID & ProductID. */
    add_number_match(matching, CFSTR(kUSBVendorID), ST1);
    add_number_match(matching, CFSTR(kUSBProductID), TOP);

    /* Look up our service. We have to release it when we're done. */
    ioService = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (ioService == 0)
    {
        DROP(1);
        TOP = 0;
        return;
    }

    ior = IOCreatePlugInInterfaceForService(ioService,
            kIOUSBDeviceUserClientTypeID,
            kIOCFPlugInInterfaceID,
            &pluginInterface,
            &score);

    if ((ior != kIOReturnSuccess) || (pluginInterface == NULL))
        return abort_zmsg("IOCreatePlugInInterfaceForService failed");

    /* Use the plugin interface to retrieve the device interface. */
    ior = (*pluginInterface)->QueryInterface(pluginInterface,
            CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
            (LPVOID*) &deviceInterface);

    /* We're done with the plugin interface. */
    (*pluginInterface)->Release(pluginInterface);

    if ((ior != kIOReturnSuccess) || (deviceInterface == NULL))
        return abort_zmsg("QueryInterface failed");

    /* Return deviceInterface as a handle for further operations on device. */
    ST1 = (addr)deviceInterface;
    TOP = -1;
}

/*
 * usb-close-device (devhandle)
 */
void mu_usb_close_device()
{
    IOUSBDeviceInterface **deviceInterface = (IOUSBDeviceInterface **)TOP;

    /* We're done with the device. */
    (*deviceInterface)->Release(deviceInterface);
    DROP(1);
}

/*
 * usb-device-request (bmRequestType bRequest wValue wIndex wLength 'buffer device)
 */
void mu_usb_device_request()
{
    IOUSBDevRequest tr;
    IOReturn ior;
    IOUSBDeviceInterface **dev = (IOUSBDeviceInterface **)TOP;

    tr.bmRequestType = SP[6];
    tr.bRequest = SP[5];
    tr.wValue = SP[4];
    tr.wIndex = ST3;
    tr.wLength = ST2;
    tr.pData = (void *)ST1;
    DROP(7);
    ior = (*dev)->DeviceRequest(dev, &tr);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("DeviceRequest failed");
}
#endif

/*
 * usb-find-device (vendor-id product-id -- dev -1 | 0)
 *
 * Match vid and pid, and try to open interface 0.
 */
void mu_usb_find_device()
{
    CFMutableDictionaryRef matching;
    io_service_t ioService;
    IOCFPlugInInterface **pluginInterface;
    IOUSBInterfaceInterface190 **intfInterface;
    IOReturn ior;
    SInt32 score;      /* unused */

    /* Match instances of IOUSBInterface and its subclasses */
    matching = IOServiceMatching(kIOUSBInterfaceClassName);
    if (matching == NULL) return abort_zmsg("IOServiceMatching returned NULL");

    /*
     * Match interface's VendorID, ProductID, InterfaceNumber, and
     * ConfigurationValue.
     */
    add_number_match(matching, CFSTR(kUSBVendorID), ST1);
    add_number_match(matching, CFSTR(kUSBProductID), TOP);
    add_number_match(matching, CFSTR(kUSBInterfaceNumber), 0);
    add_number_match(matching, CFSTR(kUSBConfigurationValue), 1);

    /* Look up our service. We have to release it when we're done. */
    ioService = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
    if (ioService == 0)
    {
        DROP(1);
        TOP = 0;
        return;
    }

    ior = IOCreatePlugInInterfaceForService(ioService,
            kIOUSBInterfaceUserClientTypeID,
            kIOCFPlugInInterfaceID,
            &pluginInterface,
            &score);

    if ((ior != kIOReturnSuccess) || (pluginInterface == NULL))
        return abort_zmsg("IOCreatePlugInInterfaceForService failed");

    /* Use the plugin interface to retrieve the device interface. */
    ior = (*pluginInterface)->QueryInterface(pluginInterface,
            CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID190),
            (LPVOID*) &intfInterface);

    /* We're done with the plugin interface. */
    (*pluginInterface)->Release(pluginInterface);

    if ((ior != kIOReturnSuccess) || (intfInterface == NULL))
        return abort_zmsg("QueryInterface failed");

    /*
     * Open the interface. This will instantiate all of its pipes
     * (endpoints).
     */
    ior = (*intfInterface)->USBInterfaceOpen(intfInterface);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("USBInterfaceOpen failed");

    /*
     * Return intfInterface as a handle for further operations on the USB
     * interface.
     */
    ST1 = (addr)intfInterface;
    TOP = -1;
}

/*
 * usb-close (dev)
 */
void mu_usb_close()
{
    IOUSBInterfaceInterface190 **intfInterface = (IOUSBInterfaceInterface190 **)TOP;
    IOReturn ior;

    /* We're done with the interface. Close it and then release it. */
    ior = (*intfInterface)->USBInterfaceClose(intfInterface);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("USBInterfaceClose failed");

    (*intfInterface)->Release(intfInterface);
    DROP(1);
}

/*
 * usb-control (bmRequestType bRequest wValue wIndex wLength 'buffer dev - count)
 */
void mu_usb_control()
{
    IOUSBDevRequestTO tr;
    IOReturn ior;
    IOUSBInterfaceInterface190 **intf = (IOUSBInterfaceInterface190 **)TOP;

    tr.bmRequestType = SP[6];
    tr.bRequest = SP[5];
    tr.wValue = SP[4];
    tr.wIndex = ST3;
    tr.wLength = ST2;
    tr.pData = (void *)ST1;
    tr.noDataTimeout = 1000;
    tr.completionTimeout = 4000;
    DROP(6);

    ior = (*intf)->ControlRequestTO(intf, 0, &tr);
    if (ior != kIOReturnSuccess)
    {
        TOP = 0;    /* count of bytes transferred */
        return abort_zmsg("ControlRequest failed");
    }
    TOP = tr.wLenDone;
}

#ifdef WITH_USB_EXTRAS
/*
 * usb-get-pipe-properties (pipe# dev -- direction number transferType maxpacketsize interval)
 */
void mu_usb_get_pipe_properties()
{
    IOUSBInterfaceInterface190 **intf = (IOUSBInterfaceInterface190 **)TOP;
    IOReturn ior;
    UInt16 max_packet_size;
    UInt8 direction;
    UInt8 number;
    UInt8 transfer_type;
    UInt8 interval;

    ior = (*intf)->GetPipeProperties(intf, ST1, &direction, &number, &transfer_type,
                                     &max_packet_size, &interval);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("GetPipeProperties failed");

    DROP(-3);
    SP[4] = direction;
    ST3 = number;
    ST2 = transfer_type;
    ST1 = max_packet_size;
    TOP = interval;
}
#endif

/*
 * usb-read ( 'buffer size pipe# dev -- #read)
 */
void mu_usb_read()
{
    IOUSBInterfaceInterface190 **intf = (IOUSBInterfaceInterface190 **)TOP;
    IOReturn ior;
    UInt32 size = ST2;
    UInt8  pipe = ST1;

    /* data timeout of 100ms; completion timeout of 400ms */
    ior = (*intf)->ReadPipeTO(intf, pipe, (void *)ST3, &size, 100, 400);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("ReadPipe failed");

    DROP(3);
    TOP = size;     /* count of bytes actually read */
}

/*
 * usb-write ( 'buffer size pipe# dev)
 */
void mu_usb_write()
{
    IOUSBInterfaceInterface190 **intf = (IOUSBInterfaceInterface190 **)TOP;
    IOReturn ior;
    UInt8  pipe = ST1;

    /* data timeout of 100ms; completion timeout of 400ms */
    ior = (*intf)->WritePipeTO(intf, pipe, (void *)ST3, ST2, 100, 400);
    if (ior != kIOReturnSuccess)
        return abort_zmsg("WritePipe failed");

    DROP(4);
}