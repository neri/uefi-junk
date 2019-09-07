// MEG-OS Boot Loader for EFI
// Copyright (c) 2018 MEG-OS project, All rights reserved.
// License: MIT
#include "osldr.h"
#include "acpi.h"
#include "rsrc.h"


#define RES_X_MIN 800
#define RES_Y_MIN 600

#define	GOP_STANDARD_RGB	PixelBlueGreenRedReserved8BitPerColor

#define	OS_INDICATIONS_SUPPORTED_NAME	L"OsIndicationsSupported"
#define	OS_INDICATIONS_NAME	L"OsIndications"

#ifdef EFI_VENDOR_NAME
#define EFI_VENDOR_PATH "\\EFI\\" EFI_VENDOR_NAME "\\"
#else
#define EFI_VENDOR_PATH "\\EFI\\MEGOS\\"
#endif
CONST CHAR16* KERNEL_PATH = L"" EFI_VENDOR_PATH "BOOT" EFI_SUFFIX ".EFI";
CONST CHAR16* cp932_bin_path = L"" EFI_VENDOR_PATH "CP932.BIN";
CONST CHAR16* cp932_fnt_path = L"" EFI_VENDOR_PATH "CP932.FNT";
CONST CHAR16* SHELL_PATH = L"\\SHELL" EFI_SUFFIX ".EFI";

CONST EFI_GUID EfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
CONST EFI_GUID EfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
CONST EFI_GUID EfiGlovalVariableGuid = EFI_GLOBAL_VARIABLE;
CONST EFI_GUID EfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
CONST EFI_GUID EfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
CONST EFI_GUID EfiEdidActiveProtocolGuid = EFI_EDID_ACTIVE_PROTOCOL_GUID;
CONST EFI_GUID EfiEdidDiscoveredProtocolGuid = EFI_EDID_DISCOVERED_PROTOCOL_GUID;
CONST EFI_GUID EfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
CONST EFI_GUID EfiDevicePathToTextProtocolGuid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
CONST EFI_GUID efi_acpi_20_table_guid = EFI_ACPI_20_TABLE_GUID;


int printf(const char*, ...);
int snprintf(char*, size_t, const char*, ...);
int puts(const char*);
void* malloc(size_t);
void free(void*);
void* memset(void *, int, size_t);

typedef enum {
    menu_item_start_normally = 1,
    menu_item_option,
    menu_item_sysinfo,
    menu_item_chgres,
    menu_item_device,
    menu_item_reset,
    menu_item_shutdown,
    menu_item_bios,
    menu_item_shell,
    menu_item_max
} menu_ids;


EFI_SYSTEM_TABLE* gST;
EFI_BOOT_SERVICES* gBS;
EFI_RUNTIME_SERVICES* gRT;
EFI_HANDLE* image;

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* cout = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
int edid_x = 0, edid_y = 0;
EFI_FILE_HANDLE sysdrv = NULL;

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


CHAR16 *devicePathToText(EFI_DEVICE_PATH_PROTOCOL* path) {
    static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* dp2tp = NULL;
    if(!dp2tp) {
        gBS->LocateProtocol(&EfiDevicePathToTextProtocolGuid, NULL, (void**)&dp2tp);
    }
    if(dp2tp) {
        return dp2tp->ConvertDevicePathToText(path, TRUE, TRUE);
    } else {
        return NULL;
    }
}
EFI_STATUS exec(CONST CHAR16* path);


BOOLEAN rsrc_ja_enabled = FALSE;

const char* get_string(rsrc id) {
    if(id >= rsrc_max) return NULL;

    const char *retval = NULL;

    if(rsrc_ja_enabled) {
        retval = rsrc_ja[id];
    }

    if(!retval) {
        retval = rsrc_default[id];
    }

    return retval;
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

void* malloc(size_t n) {
    void* result = 0;
    EFI_STATUS status = gST->BootServices->AllocatePool(EfiLoaderData, n, &result);
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

#define EDID_LENGTH 0x80

int validate_edid(size_t size, void* _edid) {
    if (size < EDID_LENGTH) return 0;
    if (!_edid) return 0;
    uint64_t edid_signature = 0x00FFFFFFFFFFFF00;
    if (*((uint64_t*)_edid) != edid_signature) return 0;

    uint8_t* edid = (uint8_t*)_edid;
    uint8_t sum = 0;
    for (int i = 0; i < EDID_LENGTH; i++) {
        sum += edid[i];
    }
    if (sum != 0) return 0;

    return 1;
}

EFI_STATUS init_gop(EFI_HANDLE* image) {
    EFI_STATUS status;

    UINTN handleCount = 0;
    EFI_HANDLE* handleBuffer = NULL;
    status = gBS->LocateHandleBuffer(ByProtocol, &EfiGraphicsOutputProtocolGuid, NULL, &handleCount, &handleBuffer);
    if(EFI_ERROR(status)) {
        return EFI_NOT_FOUND;
    } else {
        gBS->OpenProtocol(handleBuffer[0], &EfiGraphicsOutputProtocolGuid, (void*)&gop, image, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if(EFI_ERROR(status)) {
            return EFI_NOT_FOUND;
        }
    }

    EFI_EDID_ACTIVE_PROTOCOL* edid1;
    status = gBS->LocateProtocol(&EfiEdidActiveProtocolGuid, NULL, (void**)&edid1);
    if(!EFI_ERROR(status) && validate_edid(edid1->SizeOfEdid, edid1->Edid)) {
        edid_x = ((edid1->Edid[58]&0xF0)<<4) + edid1->Edid[56];
        edid_y = ((edid1->Edid[61]&0xF0)<<4) + edid1->Edid[59];
    }else{
        EFI_EDID_DISCOVERED_PROTOCOL* edid2;
        status = gBS->LocateProtocol(&EfiEdidDiscoveredProtocolGuid, NULL, (void**)&edid2);
        if(!EFI_ERROR(status) && validate_edid(edid2->SizeOfEdid, edid2->Edid)) {
            edid_x = ((edid2->Edid[58]&0xF0)<<4) + edid2->Edid[56];
            edid_y = ((edid2->Edid[61]&0xF0)<<4) + edid2->Edid[59];
        }
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* mode = gop->Mode;
    uint32_t mode_to_be = -1;
    if(edid_x>0 && edid_y>0) {
        for(int i=0; i<mode->MaxMode;i++) {
            UINTN sizeOfInfo;
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
            gop->QueryMode(gop, i, &sizeOfInfo, &info);
            if(info->HorizontalResolution == edid_x && info->VerticalResolution == edid_y) {
                mode_to_be = i;
            }
        }
        if(mode->Mode != mode_to_be) {
            gop->SetMode(gop, mode_to_be);
        }
    }

    return EFI_SUCCESS;
}


void efi_console_control(BOOLEAN textMode) {
    cout->EnableCursor(cout, FALSE);
    EFI_STATUS status;
    EFI_CONSOLE_CONTROL_PROTOCOL* efi_cc = NULL;
    EFI_CONSOLE_CONTROL_SCREEN_MODE mode_to_be = textMode ? EfiConsoleControlScreenText : EfiConsoleControlScreenGraphics;
    status = gBS->LocateProtocol(&EfiConsoleControlProtocolGuid, NULL, (void**)&efi_cc);
    if (!EFI_ERROR(status)) {
        efi_cc->SetMode(efi_cc, mode_to_be);
    }
}


void gop_cls(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop) {
    if(gop){
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL color = { 0, 0, 0, 0 };
        gop->Blt(gop, &color, EfiBltVideoFill, 0, 0, 0, 0, gop->Mode->Info->HorizontalResolution, gop->Mode->Info->VerticalResolution, 0);
    } else {
        cout->ClearScreen(cout);
    }
}

void change_resolution_menu() {

    menu_buffer* items = init_menu();
    menu_add(items, get_string(rsrc_return_to_previous), 0);
    menu_add(items, NULL, 0);

    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* mode = gop->Mode;
    uint32_t maxmode = mode->MaxMode;
    for(int i=0; i<maxmode; i++) {
        UINTN sizeOfInfo;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        gop->QueryMode(gop, i, &sizeOfInfo, &info);

        BOOLEAN white = TRUE;
        if (info->HorizontalResolution == edid_x && info->VerticalResolution == edid_y) {
            white = TRUE;
        } else if (
            info->HorizontalResolution < RES_X_MIN ||
            info->VerticalResolution < RES_Y_MIN ||
            (info->HorizontalResolution & 7) != 0 ||
            (info->VerticalResolution & 7) != 0
        ) {
            white = FALSE;
        }
        if (info->PixelFormat != GOP_STANDARD_RGB)
            white = FALSE;
        if (white){
            if(mode->Mode == i) {
                items->selected_index = items->item_count;
            }
            EFI_STATUS status = menu_add_format(items, i+1, "%4d x %4d (%d)", info->HorizontalResolution, info->VerticalResolution, i);
            if(EFI_ERROR(status)) {
                break;
            }
        }
    }

    uintptr_t menuresult;
    do {
        menuresult = show_menu(items, get_string(rsrc_display_settings), NULL);

        if(menuresult) {
            gop->SetMode(gop, menuresult-1);
            cout->Reset(cout, TRUE);
        }
    }while(menuresult);

}


void system_info() {

    static char caption[1024];
    uint32_t uver = gST->Hdr.Revision;

#if defined(__x86_64__)
    const char *arch = "amd64";
#elif defined(__aarch64__)
    const char *arch = "aa64";
#elif defined(__i386__)
    const char *arch = "i386";
#elif defined(__arm__)
    const char *arch = "arm";
#endif

    snprintf(caption, 1023, "UEFI ver %d.%d (%S %08x)\n  Arch: %s (%zd bit)\n",
     (int)(uver >> 16), (int)(uver & 0xFFFF), gST->FirmwareVendor, gST->FirmwareRevision, arch, 8 * sizeof(void *));

    menu_buffer* items = init_menu();
    menu_add(items, get_string(rsrc_return_to_previous), 0);
    menu_add(items, NULL, 0);

    uintptr_t menuresult;
    do {
        menuresult = show_menu(items, get_string(rsrc_system_info), caption);

        if(menuresult) {
            gop->SetMode(gop, menuresult-1);
            cout->Reset(cout, TRUE);
        }
    }while(menuresult);

}


void device_menu() {
    // menu_buffer* items = init_menu();
    // menu_add(items, get_string(rsrc_return_to_previous), 0);
    // menu_add_separator(items);

    UINTN count;
    EFI_HANDLE *fshandles = NULL;
    EFI_STATUS status = gBS->LocateHandleBuffer(ByProtocol, &EfiSimpleFileSystemProtocolGuid, NULL, &count, &fshandles);

    cout->ClearScreen(cout);
    printf("%zd filesystems:\n", count);
    for(UINTN i=0; i<count; i++) {
        EFI_HANDLE fsh = fshandles[i];
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        EFI_FILE_PROTOCOL *file = NULL;
        EFI_DEVICE_PATH_PROTOCOL *dp = NULL;

        status = gBS->HandleProtocol(fsh, &EfiSimpleFileSystemProtocolGuid, (void**)&fs);
        if(EFI_ERROR(status)) {
            printf("Error opening filesystem: %zx", status);
            continue;
        }
        status = gBS->HandleProtocol(fsh, &EfiDevicePathProtocolGuid, (void**)&dp);
        if(EFI_ERROR(status)) {
            printf("Error opening devicepath: %zx", status);
            continue;
        }

        CHAR16* p = devicePathToText(dp);
        printf("%S\n", p);

        EFI_GUID EfiFileSystemInfoGuid = EFI_FILE_SYSTEM_INFO_ID;
        status = fs->OpenVolume(fs, &file);
        if(!EFI_ERROR(status)) {
            EFI_FILE_SYSTEM_INFO *fsinfo = NULL;
            UINTN size = 0;
            status = file->GetInfo(file, &EfiFileSystemInfoGuid, &size, fsinfo);
            fsinfo = malloc(size);
            status = file->GetInfo(file, &EfiFileSystemInfoGuid, &size, fsinfo);
            if(!EFI_ERROR(status)) {
                printf("fs %llu %llu %u\n", fsinfo->VolumeSize, fsinfo->FreeSpace, fsinfo->BlockSize);
                printf("[%S]\n", fsinfo->VolumeLabel);
            } else {
                printf("GetInfo: Error(%zx)", status);
            }
        }

    }
    efi_wait_any_key(TRUE, -1);

    // uintptr_t menuresult = show_menu(items, "Device", NULL);

}


void option_menu() {

    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    for(;;) {
        menu_buffer* items = init_menu();
        menu_add(items, get_string(rsrc_return_to_previous), 0);
        menu_add_separator(items);
        if(gop) {
            menu_add(items, get_string(rsrc_display_settings), menu_item_chgres);
            menu_add_separator(items);
        }
        menu_add(items, get_string(rsrc_other_devices), menu_item_device);
        menu_add(items, get_string(rsrc_system_info), menu_item_sysinfo);
        menu_add(items, get_string(rsrc_shell), menu_item_shell);
        menu_add_separator(items);
        {
            uint32_t attributes = 0;
            uint64_t os_indications_supported = 0;
            UINTN data_size = sizeof(uint64_t);
            gRT->GetVariable(OS_INDICATIONS_SUPPORTED_NAME, &EfiGlovalVariableGuid, &attributes, &data_size, &os_indications_supported);
            if(os_indications_supported & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                menu_add(items, get_string(rsrc_boot_to_fw_ui), menu_item_bios );
            }
        }
        menu_add(items, get_string(rsrc_reset), menu_item_reset);
        menu_add(items, get_string(rsrc_shutdown), menu_item_shutdown);

        uintptr_t menuresult = show_menu(items, get_string(rsrc_advanced_option), NULL);

        switch(menuresult) {
            case menu_item_sysinfo:
                system_info();
                break;

            case menu_item_chgres:
                change_resolution_menu();
                break;

            case menu_item_device:
                device_menu();
                break;

            case menu_item_reset:
                gRT->ResetSystem(EfiResetCold, 0, 0, NULL);
                break;

            case menu_item_shutdown:
                gRT->ResetSystem(EfiResetShutdown, 0, 0, NULL);
                break;

            case menu_item_bios:
            {
                uint32_t attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
                uint64_t os_indications = EFI_OS_INDICATIONS_BOOT_TO_FW_UI;
                UINTN data_size = sizeof(os_indications);
                EFI_STATUS status = gRT->SetVariable(OS_INDICATIONS_NAME, &EfiGlovalVariableGuid, attributes, data_size, &os_indications);
                if(!EFI_ERROR(status)) {
                    gRT->ResetSystem(EfiResetWarm, 0, 0, NULL);
                }
            }
                break;

            case menu_item_shell:
            {
                exec(SHELL_PATH);
            }
                break;

            default:
                return;
        }
    }
}


EFI_STATUS efi_get_file_content(IN EFI_FILE_HANDLE fs, IN CONST CHAR16* path, OUT base_and_size* result) {
    EFI_STATUS status;
    EFI_FILE_HANDLE handle = NULL;
    void* buff = NULL;
    uint64_t fsize = UINT64_MAX;

    //  Open file
    status = fs->Open(fs, &handle, path, EFI_FILE_MODE_READ, 0);
    if(EFI_ERROR(status)) return status;

    //  Get file size
    status = handle->SetPosition(handle, fsize);
    if(EFI_ERROR(status)) goto error;
    status = handle->GetPosition(handle, &fsize);
    if(EFI_ERROR(status)) goto error;
    status = handle->SetPosition(handle, 0);
    if(EFI_ERROR(status)) goto error;

    //  Allocate memory
    if ((sizeof(UINTN) < sizeof(uint64_t)) && fsize > UINT32_MAX) {
        status = EFI_OUT_OF_RESOURCES;
        goto error;
    }
    buff = malloc(fsize);
    if(!buff){
        status = EFI_OUT_OF_RESOURCES;
        goto error;
    }

    //  Read
    UINTN read_count = fsize;
    status = handle->Read(handle, &read_count, buff);
    if(EFI_ERROR(status)) goto error;
    status = handle->Close(handle);
    if(EFI_ERROR(status)) goto error;

    result->base = buff;
    result->size = read_count;

    return EFI_SUCCESS;

error:
    if(buff) {
        free(buff);
    }
    if(handle) {
        handle->Close(handle);
    }
    return status;
}

EFI_STATUS exec(CONST CHAR16* path) {
    EFI_STATUS status;
    // cout->ClearScreen(cout);
    EFI_HANDLE child = NULL;
    EFI_DEVICE_PATH_PROTOCOL* dpath = NULL;
    base_and_size exe_ptr;
    status = efi_get_file_content(sysdrv, path, &exe_ptr);
    if(!EFI_ERROR(status)) {
        status = gBS->LoadImage(FALSE, image, dpath, exe_ptr.base, exe_ptr.size, &child);
    }
    if(!EFI_ERROR(status)) {
        EFI_LOADED_IMAGE_PROTOCOL* li = NULL;
        EFI_LOADED_IMAGE_PROTOCOL* li2 = NULL;
        status = gBS->HandleProtocol(child, &EfiLoadedImageProtocolGuid, (void**)&li);
        if(!EFI_ERROR(status)) {
            li->SystemTable->ConOut = cout;
        }
        status = gBS->HandleProtocol(image, &EfiLoadedImageProtocolGuid, (void**)&li2);
        if(!EFI_ERROR(status)) {
            li->DeviceHandle = li2->DeviceHandle; // TODO:
        }
    }
    if(!EFI_ERROR(status)) {
        status = gBS->StartImage(child, NULL, NULL);
    }
    if(EFI_ERROR(status)) {
        // cout->ClearScreen(cout);
        // draw_title_bar(get_string(rsrc_load_error_title));
        printf("\n\n  %s\n\n  %s: %zx\n\n  %s\n", get_string(rsrc_load_error), get_string(rsrc_error_code),status, get_string(rsrc_press_any_key));
        efi_wait_any_key(TRUE, -1);
    }

    return status;
}


void efi_blt_bmp(uint8_t *bmp, int offset_x, int offset_y) {
    int bmp_w = *((uint32_t *)(bmp + 18));
    int bmp_h = *((uint32_t *)(bmp + 22));
    int bmp_bpp = *((uint16_t *)(bmp + 28));
    int bmp_bpp8 = (bmp_bpp + 7) / 8;
    int bmp_delta = (bmp_bpp8 * bmp_w + 3) & 0xFFFFFFFC;
    const uint8_t *msdib = bmp + *((uint32_t *)(bmp + 10));

    UINTN blt_delta = bmp_w * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt_buffer = malloc(blt_delta * bmp_h);
    uint32_t *q = (uint32_t *)blt_buffer;

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


EFI_STATUS start_os() {
    cout->ClearScreen(cout);
    acpi_bgrt_t* bgrt = NULL;
    if (gop) bgrt = acpi_find_table(ACPI_BGRT_SIGNATURE);
    if (bgrt) {
        efi_blt_bmp((uint8_t *)bgrt->Image_Address, bgrt->Image_Offset_X, bgrt->Image_Offset_Y);
    }
    return exec(KERNEL_PATH);
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

    //	Prepare filesystem
    {
        EFI_LOADED_IMAGE_PROTOCOL* li;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
        status = gBS->HandleProtocol(image, &EfiLoadedImageProtocolGuid, (void**)&li);
        if(EFI_ERROR(status)) return EFI_LOAD_ERROR;
        status = gBS->HandleProtocol(li->DeviceHandle, &EfiSimpleFileSystemProtocolGuid, (void**)&fs);
        if(EFI_ERROR(status)) return EFI_LOAD_ERROR;
        status = fs->OpenVolume(fs, &sysdrv);
        if(EFI_ERROR(status)) return EFI_LOAD_ERROR;
    }

    //	Init Screen
    init_gop(image);
    efi_console_control(!gop);
    if(gop) {

        base_and_size cp932_bin_ptr;
        status = efi_get_file_content(sysdrv, cp932_bin_path, &cp932_bin_ptr);
        if(EFI_ERROR(status)) {
            printf("ERROR: can't read %S (%zx)\n", cp932_bin_path, status);
            goto cp932_exit;
        }
        cp932_tbl_init(cp932_bin_ptr);

        base_and_size cp932_fnt_ptr;
        status = efi_get_file_content(sysdrv, cp932_fnt_path, &cp932_fnt_ptr);
        if(EFI_ERROR(status)) {
            printf("ERROR: can't read %S (%zx)\n", cp932_fnt_path, status);
            goto cp932_exit;
        }
        cp932_font_init(cp932_fnt_ptr);

        rsrc_ja_enabled = TRUE;

cp932_exit:

        ATOP_init(gop, &cout);
    }


    BOOLEAN menu_flag = FALSE;
    // cout->SetAttribute(cout, 0x17);
    cout->ClearScreen(cout);
    acpi_bgrt_t* bgrt = NULL;
    if (gop) bgrt = acpi_find_table(ACPI_BGRT_SIGNATURE);
    if (bgrt) {
        efi_blt_bmp((uint8_t *)bgrt->Image_Address, bgrt->Image_Offset_X, bgrt->Image_Offset_Y);
    } else {
        print_center(-5, get_string(rsrc_starting));
    }
    for(int t = 2; t > 0; t--) {
        char buffer[256];
        snprintf(buffer, 256, get_string(rsrc_press_esc_to_menu), t);
        print_center(-2, buffer);
        EFI_INPUT_KEY key = efi_wait_any_key(FALSE, 1000);
        if(key.ScanCode == 0x17 || key.UnicodeChar == 0x20){
            menu_flag = TRUE;
            break;
        }
    }

    if(!menu_flag) start_os();

    for(;;) {
        menu_buffer* items = init_menu();
        menu_add(items, get_string(rsrc_start_normally), menu_item_start_normally);
        menu_add_separator(items);
        menu_add(items, get_string(rsrc_advanced_option), menu_item_option);

        uintptr_t menuresult = show_menu(items, get_string(rsrc_main_title), get_string(rsrc_choose_an_option));

        switch(menuresult) {
            case menu_item_start_normally:
                start_os();
                break;
            case menu_item_option:
                option_menu();
                break;
        }
    }

// error:
    efi_console_control(TRUE);
    efi_wait_any_key(TRUE, -1);

// exit:
    efi_console_control(TRUE);
    return EFI_LOAD_ERROR;
}
