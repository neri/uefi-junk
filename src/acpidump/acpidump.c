// ACPI DUMP Ulitity (test)
// Copyright (c) 2018 MEG-OS project, All rights reserved.
// License: MIT
#include "efi.h"
#include "acpi.h"

#define INVALID_UNICHAR 0xFFFE
#define ZWNBSP          0xFEFF

CONST EFI_GUID efi_acpi_20_table_guid = EFI_ACPI_20_TABLE_GUID;


EFI_SYSTEM_TABLE* gST;
EFI_BOOT_SERVICES* gBS;
EFI_RUNTIME_SERVICES* gRT;
EFI_HANDLE* image;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* cout = NULL;

acpi_rsd_ptr_t* rsdp = NULL;
acpi_xsdt_t* xsdt = NULL;
int n_entries_xsdt = 0;


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

int putchar(unsigned char c) {
    static uint8_t state[2] = {0};
    uint32_t ch = INVALID_UNICHAR;

    if(c < 0x80) { // ASCII
        if(state[0] == 0){
            ch = c;
        } else {
            state[0] = 0;
        }
    } else if(c < 0xC0) { // Trail Bytes
        if(state[0] < 0xE0) { // 2bytes (Cx 8x .. Dx Bx)
            ch = ((state[0]&0x1F)<<6)|(c&0x3F);
            state[0] = 0;
        } else if (state[0] < 0xF0) { // 3bytes (Ex 8x 8x)
            if(state[1] == 0){
                state[1] = c;
                ch = ZWNBSP;
            } else {
                ch = ((state[0]&0x0F)<<12)|((state[1]&0x3F)<<6)|(c&0x3F);
                state[0] = 0;
            }
        }
    } else if(c >= 0xC2 && c <= 0xEF) { // Leading Byte
        state[0] = c;
        state[1] = 0;
        ch = ZWNBSP;
    } else { // invalid sequence
        state[0] = 0;
    }

    if(ch == INVALID_UNICHAR) ch ='?';

    switch(ch) {
        default:
        {
            CHAR16 box[] = { ch, 0 };
            EFI_STATUS status = cout->OutputString(cout, box);
            return status ? 0 : 1;
        }

        case '\n':
        {
            static CONST CHAR16* crlf = L"\r\n";
            EFI_STATUS status = cout->OutputString(cout, crlf);
            return status ? 0 : 1;
        }

        case ZWNBSP:
            return 0;
    }

}


EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE _image, IN EFI_SYSTEM_TABLE *st) {
    EFI_STATUS status;

    //	Init UEFI Environments
    gST = st;
    gBS = st->BootServices;
    gRT = st->RuntimeServices;
    image = _image;
    cout = gST->ConOut;

    rsdp = efi_find_config_table(st, &efi_acpi_20_table_guid);
    xsdt = (acpi_xsdt_t*)(rsdp->xsdtaddr);
    n_entries_xsdt = (xsdt->Header.length - 0x24) / sizeof(xsdt->Entry[0]);

    for (int i = 0; i < n_entries_xsdt; i++) {
        acpi_header_t *table = (acpi_header_t *)xsdt->Entry[i];
        printf("%p [%.4s]\n", table, table->signature);
    }

    return 0;
}
