#include "driver.h"
#include "ramdisk.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, LoadSetting)
#pragma alloc_text (PAGE, InitDeviceExtension)
#pragma alloc_text (PAGE, AssignRamdiskDeviceName)
#pragma alloc_text (PAGE, IsValidIoParams)
#endif

NTSTATUS RegisterRamdiskDeviceName(WDFDEVICE device, PWDFDEVICE_INIT devinit)
{
    NTSTATUS status = 0;

    //assign device name. 
    //If name is not unique, we can't install ramdisk more than 1 instance.
    DECLARE_CONST_UNICODE_STRING(nt_name, NT_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(dos_name, DOS_DEVICE_NAME);

    status = WdfDeviceInitAssignName(devinit, &nt_name);
    if(NT_SUCCESS(status))
        status = WdfDeviceCreateSymbolicLink(device, &dos_name);
    else
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceInitAssignName() failed [%!STATUS!]", status);
    
    if (!NT_SUCCESS(status))
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceCreateSymbolicLink() failed [%!STATUS!]", status);
    return status;
}

NTSTATUS InitDeviceExtension(PDEVICE_EXTENSION devext)
{
    NTSTATUS status = 0;
    SMOKYDISK_SETTING setting = {0};
    
    LoadSetting(&setting);

    devext->DiskMemory = ExAllocatePoolWithTag(PagedPool, setting.DiskSize.QuadPart, MY_POOLTAG);
    if (NULL == devext->DiskMemory)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ExAllocatePoolWithTag() failed");
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    devext->DiskSize.QuadPart = setting.DiskSize.QuadPart;
    RtlCopyMemory(&devext->Geometry, &setting.Geometry, sizeof(DISK_GEOMETRY));

    return status;
}

void LoadSetting(PSMOKYDISK_SETTING setting)
{
//todo: read registry to load settings
    setting->DiskSize.QuadPart = DEFAULT_DISK_SIZE;
    setting->Geometry.TracksPerCylinder = TRACKS_PER_CYLINDER;
    setting->Geometry.SectorsPerTrack = SECTORS_PER_TRACK;
    setting->Geometry.BytesPerSector = BYTES_PER_SECTOR;

    //DiskSize == BytesPerSector * SectorsPerTrack * TracksPerCylinder * Cylinders
    //Here we determine Cylinders by DiskTotalSize.
    setting->Geometry.Cylinders.QuadPart = 
                    setting->DiskSize.QuadPart 
                            / setting->Geometry.TracksPerCylinder 
                            / setting->Geometry.SectorsPerTrack 
                            / setting->Geometry.BytesPerSector;
}

BOOLEAN IsValidIoParams(
    IN PDEVICE_EXTENSION DevExt,
    IN LARGE_INTEGER ByteOffset,
    IN size_t Length)
{
//  1.end of IO range should not exceed total size range.
//  2.begin of IO range should be fit in disk.
//  3.IO offset should be positive number.

    LARGE_INTEGER begin = ByteOffset;
    LARGE_INTEGER end = ByteOffset;
    end.QuadPart = end.QuadPart + Length;

    if (ByteOffset.QuadPart < 0)
        return FALSE;
    if (begin.QuadPart < 0 || begin.QuadPart > (DevExt->DiskSize.QuadPart-1))
        return FALSE;
    if (end.QuadPart <= 0 || end.QuadPart > (DevExt->DiskSize.QuadPart - 1))
        return FALSE;

    return TRUE;
}

