// EFI Hello World

#include "efi.h"

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE image, IN EFI_SYSTEM_TABLE *st)
{
	st->ConOut->OutputString(st->ConOut, L"Hello, world!\r\n");
	for (;;)
		;
}
