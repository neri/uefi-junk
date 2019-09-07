// A Sample Application to show BGRT on UEFI

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "efi.h"
#include "acpi.h"

#define	EFI_PRINT(s)	st->ConOut->OutputString(st->ConOut, L ## s)

CONST EFI_GUID EfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
CONST EFI_GUID efi_acpi_20_table_guid = EFI_ACPI_20_TABLE_GUID;

EFI_SYSTEM_TABLE *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
acpi_rsd_ptr_t* rsdp = NULL;
acpi_xsdt_t* xsdt = NULL;
int n_entries_xsdt = 0;


/*********************************************************************/

void* malloc(size_t n) {
    void* result = 0;
    EFI_STATUS status = gBS->AllocatePool(EfiLoaderData, n, &result);
    if(EFI_ERROR(status)){
        return 0;
    }
    return result;
}

void free(void* p) {
    if(p) {
        gBS->FreePool(p);
    }
}

static inline int IsEqualGUID(CONST EFI_GUID* guid1, CONST EFI_GUID* guid2) {
    uint64_t* p = (uint64_t*)guid1;
    uint64_t* q = (uint64_t*)guid2;
    return (p[0] == q[0]) && (p[1] == q[1]);
}

static void* efi_find_config_table(EFI_SYSTEM_TABLE *st, CONST EFI_GUID* guid) {
    for (int i = 0; i < st->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE* tab = st->ConfigurationTable + i;
        if (IsEqualGUID(&tab->VendorGuid, guid)) {
            return tab->VendorTable;
        }
    }
    return NULL;
}

static int is_equal_signature(const void* p1, const void* p2) {
    const uint32_t* _p1 = (const uint32_t*)p1;
    const uint32_t* _p2 = (const uint32_t*)p2;
    return (*_p1 == *_p2);
}

void* acpi_find_table(const char* signature) {
    if (!xsdt) return NULL;
    for (int i = 0; i < n_entries_xsdt; i++) {
        acpi_header_t *entry = (acpi_header_t *)xsdt->Entry[i];
        if (is_equal_signature(entry->signature, signature)) {
            return entry;
        }
    }
    return NULL;
}

void efi_blt_bmp(uint8_t *bmp, int offset_x, int offset_y) {
    int bmp_w = *((uint32_t*)(bmp + 18));
    int bmp_h = *((uint32_t*)(bmp + 22));
    int bmp_bpp = *((uint16_t*)(bmp + 28));
    int bmp_bpp8 = (bmp_bpp + 7) / 8;
    int bmp_delta = (bmp_bpp8 * bmp_w + 3) & 0xFFFFFFFC;
    const uint8_t* msdib = bmp + *((uint32_t*)(bmp + 10));

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt_buffer = malloc(bmp_w * bmp_h * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
    uint32_t *q = (uint32_t*)blt_buffer;
    UINTN blt_delta = bmp_w * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);

    switch (bmp_bpp) {
        case 24:
        case 32:
            for (int i = bmp_h - 1; i >= 0; i--) {
                const uint8_t* p = msdib + (i * bmp_delta);
                for (int j = 0; j < bmp_w; j++) {
                    uint32_t rgb = (p[j * bmp_bpp8 + 0]) + (p[j * bmp_bpp8 + 1] << 8) + (p[j * bmp_bpp8 + 2] << 16);
                    *q++ = rgb;
                }
            }
            break;
    }

    gop->Blt(gop, blt_buffer, EfiBltBufferToVideo, 0, 0, offset_x, offset_y, bmp_w, bmp_h, blt_delta);

    free(blt_buffer);
}

/*********************************************************************/

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE image, IN EFI_SYSTEM_TABLE *st)
{
    EFI_STATUS status;
    gST = st;
    gBS = st->BootServices;

    rsdp = efi_find_config_table(st, &efi_acpi_20_table_guid);
    if (!rsdp) {
        EFI_PRINT("Error: ACPI not found!\n");
        goto exit;
    }
    xsdt = (acpi_xsdt_t*)(rsdp->xsdtaddr);
    n_entries_xsdt = (xsdt->Header.length - 0x24) / sizeof(xsdt->Entry[0]);

    acpi_bgrt_t* bgrt = NULL;
    bgrt = acpi_find_table(ACPI_BGRT_SIGNATURE);
    if (!bgrt) {
        EFI_PRINT("Error: BGRT not found!\n");
        goto exit;
    }

    status = gBS->LocateProtocol(&EfiGraphicsOutputProtocolGuid, NULL, (void**)&gop);
    if (EFI_ERROR(status)) {
        EFI_PRINT("Error: GOP not found!\n");
        goto exit;
    }

    efi_blt_bmp((uint8_t *)bgrt->Image_Address, bgrt->Image_Offset_X, bgrt->Image_Offset_Y);

exit:
    // for (;;)
        ;
    return EFI_LOAD_ERROR;
}
