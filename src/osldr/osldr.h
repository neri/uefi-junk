// Include file for MEG-OS Loader
// Copyright (c) 2018 MEG-OS project, All rights reserved.
// License: MIT
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "efi.h"

#define	INVALID_UNICHAR	0xFFFE
#define	ZWNBSP	0xFEFF

#if defined(__x86_64__)
#define EFI_SUFFIX	"X64"
#elif defined(__i386__)
#define EFI_SUFFIX	"IA32"
#elif defined(__arm__)
#define EFI_SUFFIX	"ARM"
#elif defined(__aarch64__)
#define EFI_SUFFIX	"AA64"
#endif

extern EFI_SYSTEM_TABLE* gST;
extern EFI_BOOT_SERVICES* gBS;
extern EFI_RUNTIME_SERVICES* gRT;
extern EFI_HANDLE* image;

extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* cout;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;

typedef struct {
	void* base;
	size_t size;
} base_and_size;

typedef struct {
	const char* label;
	uintptr_t item_id;
} menuitem;

typedef struct {
	menuitem* items;
	char* string_pool;
	int item_count, max_items, selected_index, pool_size, pool_used;
} menu_buffer;

EFI_STATUS cp932_tbl_init(base_and_size);
EFI_STATUS cp932_font_init(base_and_size);
EFIAPI EFI_STATUS ATOP_init(IN EFI_GRAPHICS_OUTPUT_PROTOCOL* gop, OUT EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL** result);

EFI_INPUT_KEY efi_wait_any_key(BOOLEAN, int);
menu_buffer* init_menu();
uintptr_t show_menu(menu_buffer* items, const char* title, const char* caption);
EFI_STATUS menu_add(menu_buffer* buffer, const char* label, uintptr_t menu_id);
EFI_STATUS menu_add_separator(menu_buffer* buffer);
EFI_STATUS menu_add_format(menu_buffer* buffer, uintptr_t menu_id, const char* format, ...);

size_t strwidth(const char* s);
void print_center(int rows, const char* message);
void draw_title_bar(const char* title);
