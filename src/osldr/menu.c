// EFI Boot Menu UI
// Copyright (c) 1998,2000,2018 MEG-OS project, All rights reserved.
// License: MIT
#include "osldr.h"

int printf(const char*, ...);
int snprintf(char*, size_t, const char*, ...);
int vsnprintf(char* buffer, size_t limit, const char* format, va_list args);
size_t strlen(const char*);
int putchar(char c);
int puts(const char*);
void* malloc(size_t);


size_t strwidth(const char* s) {
    size_t retval = 0;
    for(;*s;s++) {
        uint8_t c = *s;
        if(c<0x80) { // ASCII
            retval ++;
        } else if (c<0xC0) { // Trail bytes
            ;
        } else if (c<0xF0) { // Leading Byte Cx/Dx Ex
            retval += 2;
        }
    }
    return retval;
}


EFI_INPUT_KEY efi_wait_any_key(BOOLEAN reset, int ms) {
    EFI_INPUT_KEY retval = { 0, 0 };
    EFI_STATUS status;
    EFI_EVENT timer_event;
    EFI_EVENT events[2];
    UINTN index = 0;
    events[index++] = gST->ConIn->WaitForKey;
    if(ms >= 0) {
        status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &timer_event);
        status = gBS->SetTimer(timer_event, TimerRelative, ms * 10000);
        events[index++] = timer_event;
    }

    if(reset) {
        gST->ConIn->Reset(gST->ConIn, FALSE);
    }

    status = gBS->WaitForEvent(index, events, &index);
    if(!EFI_ERROR(status)) {
        if(index == 0) {
            EFI_INPUT_KEY key;
            status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            if(!EFI_ERROR(status)) {
                retval = key;
            }
        }
    }

    return retval;
}


#define MAX_SYSTEM_MENU_BUFFER	22
#define	MENU_BUFFER_POOL_SIZE	(80 * 25)

static menuitem system_menuitems[MAX_SYSTEM_MENU_BUFFER];
static menu_buffer system_menu_buffer;
static char* menu_string_pool;

menu_buffer* init_menu() {

    if(!menu_string_pool) {
        menu_string_pool = malloc(MENU_BUFFER_POOL_SIZE);
    }

    menu_buffer* result = &system_menu_buffer;

    result->items = system_menuitems;
    result->max_items = MAX_SYSTEM_MENU_BUFFER;
    result->item_count = 0;
    result->selected_index = 0;
    result->string_pool = menu_string_pool;
    result->pool_size = MENU_BUFFER_POOL_SIZE;
    result->pool_used = 0;

    return result;
}

EFI_STATUS menu_add_format(menu_buffer* buffer, uintptr_t menu_id, const char* format, ...) {
    if(buffer->item_count < buffer->max_items) {
        char* p;
        if(format) {
            va_list list;
            va_start(list, format);
            p = buffer->string_pool + buffer->pool_used;
            buffer->pool_used += 1 + vsnprintf(p, buffer->pool_size - buffer->pool_used - 1, format, list);
            va_end(list);
        } else {
            p = NULL;
        }
        menuitem item = { p, menu_id };
        buffer->items[buffer->item_count++] = item;
        return EFI_SUCCESS;
    } else {
        return EFI_BUFFER_TOO_SMALL;
    }
}

EFI_STATUS menu_add_separator(menu_buffer* buffer) {
    return menu_add_format(buffer, 0, NULL);
}

EFI_STATUS menu_add(menu_buffer* buffer, const char* label, uintptr_t menu_id) {
    if(label) {
        return menu_add_format(buffer, menu_id, "%s", label);
    } else {
        return menu_add_format(buffer, menu_id, NULL);
    }
}



void print_center(int rows, const char* message) {
    UINTN len = strwidth(message);
    UINTN con_cols, con_rows;
    cout->QueryMode(cout, cout->Mode->Mode, &con_cols, &con_rows);
    if(rows < 0) { rows += con_rows; }
    cout->SetCursorPosition(cout, (con_cols-len)/2, rows);
    printf("%s", message);
}


void draw_title_bar(const char* title) {
    uint32_t prev_color = cout->Mode->Attribute;
    const uint32_t title_color = 0x30;
    UINTN con_cols, con_rows;
    cout->QueryMode(cout, cout->Mode->Mode, &con_cols, &con_rows);
    cout->SetAttribute(cout, title_color);
    cout->SetCursorPosition(cout, 0, 0);
    for(int i=0; i<con_cols; i++) {
        putchar(' ');
    }
    print_center(0, title);
    cout->SetAttribute(cout, prev_color);
}


uintptr_t show_menu(menu_buffer* items, const char* title, const char* caption) {

    const int cur_left = 2;
    const int cur_top = 2;
    const int cur_left_item = 2;
    const int cur_padding = 1;

    int selected_index = items->selected_index;
    int redraw = 1;

    const uint32_t selected_item_color = 0x70;
    uint32_t regular_item_color = cout->Mode->Attribute;
    uintptr_t retVal = 0;

    cout->EnableCursor(cout, FALSE);
    cout->ClearScreen(cout);
    draw_title_bar(title);
    for(;;) {
        if(redraw) {
            int cur_y = cur_top;
            redraw = 0;

            if(caption) {
                cout->SetCursorPosition(cout, cur_left, cur_y++);
                cout->SetAttribute(cout, regular_item_color);
                puts(caption);
                cur_y += cur_padding;
            }

            for(int i=0; i<items->item_count; i++, cur_y++) {
                if(items->items[i].label) {
                    cout->SetCursorPosition(cout, cur_left_item, cur_y);
                    if(i == selected_index) {
                        cout->SetAttribute(cout, selected_item_color);
                        printf("  > %s  ", items->items[i].label);
                    } else {
                        cout->SetAttribute(cout, regular_item_color);
                        printf("    %s  ", items->items[i].label);
                    }
                }
            }
        }

        EFI_INPUT_KEY key = efi_wait_any_key(FALSE, -1);
        int cursor_move = 0;
        switch(key.UnicodeChar) {
            case 0x0D: // Enter
                items->selected_index = selected_index;
                retVal = items->items[selected_index].item_id;
                goto exit;
            case 'j': case 'J':
                cursor_move = 1;
                break;
            case 'k': case 'K':
                cursor_move = 2;
                break;
        }
        switch(key.ScanCode) {
            case 0x02: // CUR DOWN
            case 0x81: // VOL DOWN
                cursor_move = 1;
                break;
            case 0x01: // CUR UP
            case 0x80: // VOL UP
                cursor_move = 2;
                break;
            case 0x17: // ESC
                goto exit;
        }
        switch (cursor_move) {
            case 1:
                if (selected_index < items->item_count - 1) {
                    do {
                        selected_index++;
                    } while (!items->items[selected_index].label);
                } else {
                    selected_index = 0;
                }
                redraw = 1;
                break;

            case 2:
                if (selected_index) {
                    do {
                        selected_index--;
                    } while (!items->items[selected_index].label);
                } else {
                    selected_index = items->item_count - 1;
                }
                redraw = 1;
                break;
        }
    }
exit:
    cout->SetAttribute(cout, regular_item_color);
    return retVal;
}
