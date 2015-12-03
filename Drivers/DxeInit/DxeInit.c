/** @file
  Timer Architecture Protocol driver of the ARM flavor

  Copyright (c) 2011-2013 ARM Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <LittleKernel.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_EVENT             EfiExitBootServicesEvent = (EFI_EVENT)NULL;

STATIC VOID
lkapi_event_init(VOID** event)
{
  ASSERT(gBS->CreateEvent(0, 0, NULL, NULL, (EFI_EVENT*)event)==EFI_SUCCESS);
}

STATIC VOID
lkapi_event_destroy(VOID* event)
{
  ASSERT(gBS->CloseEvent((EFI_EVENT)event)==EFI_SUCCESS);
}

STATIC VOID
lkapi_event_wait(VOID** event)
{
  UINTN Index;
  ASSERT(gBS->WaitForEvent(1, (EFI_EVENT*)event, &Index)==EFI_SUCCESS);
}

STATIC VOID
lkapi_event_signal(VOID* event)
{
  ASSERT(gBS->SignalEvent((EFI_EVENT)event)==EFI_SUCCESS);
}

VOID
EFIAPI
ExitBootServicesEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  lkapi_t* LKApi = GetLKApi();
  LKApi->platform_uninit();
}

EFI_STATUS
EFIAPI
DxeInitInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS Status;
  lkapi_t* LKApi = GetLKApi();

  PcdSet32 (PcdGicDistributorBase, LKApi->int_get_dist_base());
  PcdSet32 (PcdGicRedistributorsBase, LKApi->int_get_redist_base());
  PcdSet32 (PcdGicInterruptInterfaceBase, LKApi->int_get_cpu_base());

  LKApi->event_init = lkapi_event_init;
  LKApi->event_destroy = lkapi_event_destroy;
  LKApi->event_wait = lkapi_event_wait;
  LKApi->event_signal = lkapi_event_signal;

  // Register for an ExitBootServicesEvent
  Status = gBS->CreateEvent (EVT_SIGNAL_EXIT_BOOT_SERVICES, TPL_NOTIFY, ExitBootServicesEvent, NULL, &EfiExitBootServicesEvent);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
