/** @file
  PEGATRON BIOS Team Training 
  Level 1 - PCIe
**/

#include "OemPcie.h"



static UINTN   Argc;
static CHAR16  **Argv;

/**

  This function parse application ARG.

  @return Status
**/
static
EFI_STATUS
GetArg (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters;            

  Status = gBS->HandleProtocol (
                  gImageHandle,                               //Handle
                  &gEfiShellParametersProtocolGuid,           
                  (VOID **)&ShellParameters                   //**Interface
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Argc = ShellParameters->Argc;
  Argv = ShellParameters->Argv;
  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
	EFI_STATUS						Status = EFI_SUCCESS;
  UINT32  						Index;
  Index = 0;
	UINTN							Bus = 0;
	UINTN							Device = 0; 
	UINTN							Function = 0;
	UINTN							PcieBase = 0xE0000000;
	UINTN							PcieDevice = 0;
	UINT32				 			Value;
  Value = 0;


	
	Print(L"Input CMD: Pcie BusNum DevNum FuncNum\n");

	Status = GetArg ();
	if (Argc < 4) {
    	Print (L"Error: less parameter - Please Input Correct CMD\n");
    	Status = EFI_INVALID_PARAMETER;
    	return Status;
	}

	
	Bus = StrHexToUintn(Argv[1]);
	Device = StrHexToUintn(Argv[2]);
	Function = StrHexToUintn(Argv[3]);

	if(Bus > 255 || Device > 31 || Function > 7){
    	Print (L"Error: Bus, Device, Function over limit\n");
    	Status = EFI_INVALID_PARAMETER;
    	return Status;		
	}
	
	Print (L"Bus: 0x%x, Device: 0x%x, Function: 0x%x\n", Bus, Device, Function);
	PcieDevice = PcieBase | ((Bus << 20) | (Device << 15) | (Function << 12));
	Print (L"Pcie Device Address 0x%X\n", PcieDevice);

	Value = MmioRead32(PcieDevice);
	if(Value == 0 || Value ==0xFFFFFFFF){
		Print (L"Pcie Device not present, 0x%X\n", Value);
	}
	else{
		Print (L"Pcie Device ID: 0x%X\n", Value);
	}
	
	return Status;
}
