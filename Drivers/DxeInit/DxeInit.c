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

STATIC VOID
lkapi_event_init(VOID** event)
{
  EFI_STATUS Status = gBS->CreateEvent(0, 0, NULL, NULL, (EFI_EVENT*)event);
  ASSERT_EFI_ERROR (Status);
}

STATIC VOID
lkapi_event_destroy(VOID* event)
{
  EFI_STATUS Status = gBS->CloseEvent((EFI_EVENT)event);
  ASSERT_EFI_ERROR (Status);
}

STATIC VOID
lkapi_event_wait(VOID** event)
{
  UINTN Index;
  EFI_STATUS Status = gBS->WaitForEvent(1, (EFI_EVENT*)event, &Index);
  ASSERT_EFI_ERROR (Status);
}

STATIC VOID
lkapi_event_signal(VOID* event)
{
  EFI_STATUS Status = gBS->SignalEvent((EFI_EVENT)event);
  ASSERT_EFI_ERROR (Status);
}

EFI_STATUS
EFIAPI
DxeInitInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  lkapi_t* LKApi = GetLKApi();

  PcdSet32 (PcdGicDistributorBase, LKApi->int_get_dist_base());
  PcdSet32 (PcdGicRedistributorsBase, LKApi->int_get_redist_base());
  PcdSet32 (PcdGicInterruptInterfaceBase, LKApi->int_get_cpu_base());

  LKApi->event_init = lkapi_event_init;
  LKApi->event_destroy = lkapi_event_destroy;
  LKApi->event_wait = lkapi_event_wait;
  LKApi->event_signal = lkapi_event_signal;

  return EFI_SUCCESS;
}
