// SPDX-FileCopyrightText: 2021 Foundation Devices, Inc. <hello@foundationdevices.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/builtin.h"
#include "py/objarray.h"

#include "lib/quirc_internal.h"

/// package: foundation.qr

#ifndef CONFIG_QUIRC_QR_MAX_HOR_RES
#define CONFIG_QUIRC_QR_MAX_HOR_RES (128)
#endif

#ifndef CONFIG_QUIRC_QR_MAX_VER_RES
#define CONFIG_QUIRC_QR_MAX_VER_RES (128)
#endif

#define CONFIG_QUIRC_FLOOD_FILL_VARS (QUIRC_VARS_NUMOF(CONFIG_QUIRC_QR_MAX_VER_RES))

#ifndef CONFIG_QR_DEBUG
#define CONFIG_QR_DEBUG 0
#endif

#if CONFIG_QR_DEBUG
#include <stdio.h>

#define DEBUG(str, ...) printf("[quirc] %s: " str, __func__, ##__VA_ARGS__)
#else
#define DEBUG(str, ...) \
    do {                \
    } while (0)
#endif

typedef struct quirc_flood_fill_vars quirc_flood_fill_vars_t;
typedef struct quirc quirc_t;
typedef struct quirc_code quirc_code_t;
typedef struct quirc_data quirc_data_t;

static quirc_flood_fill_vars_t _flood_fill_vars[CONFIG_QUIRC_FLOOD_FILL_VARS];
static quirc_t                 _quirc;
static quirc_code_t            _code;
static quirc_data_t            _data;
static uint8_t                 _framebuffer[CONFIG_QUIRC_QR_MAX_HOR_RES * CONFIG_QUIRC_QR_MAX_VER_RES];

/// def init(hor_res: int, ver_res: int) -> None:
///     """
///     Initialize QR context.
///     """
STATIC mp_obj_t mod_quirc_init(mp_obj_t hor_res_obj, mp_obj_t ver_res_obj) {
    mp_uint_t hor_res = mp_obj_get_int(hor_res_obj);
    mp_uint_t ver_res = mp_obj_get_int(ver_res_obj);

    if (hor_res > CONFIG_QUIRC_QR_MAX_HOR_RES || ver_res > CONFIG_QUIRC_QR_MAX_VER_RES) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("Maximum resolution for the QR scanner is %ux%u"),
                          CONFIG_QUIRC_QR_MAX_HOR_RES, CONFIG_QUIRC_QR_MAX_VER_RES);
    }

    if (quirc_init(&_quirc, hor_res, ver_res, _framebuffer, _flood_fill_vars, CONFIG_QUIRC_FLOOD_FILL_VARS) < 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Invalid buffer size for this decoder"));
        return mp_const_none;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(quirc_init_obj, mod_quirc_init);

/// def scan() -> str:
///     '''
///     Scan QR codes in an image.
///
///     Returns a list of data found in the QR codes.
///     '''
STATIC mp_obj_t mod_quirc_scan(void) {
    // Prepare to decode
    quirc_begin(&_quirc, NULL, NULL);
    // This triggers the decoding of the image we just gave quirc
    quirc_end(&_quirc);

    // Let's see if we got any results
    int num_codes = quirc_count(&_quirc);
    DEBUG("num_codes=%d\n", num_codes);
    if (num_codes == 0) {
        DEBUG("No codes found\n");
        return mp_const_none;
    }

    // Extract the first code found only, even if multiple were found
    quirc_extract(&_quirc, 0, &_code);
    DEBUG("quirc_extract() done\n");

    // Decoding stage
    quirc_decode_error_t err = quirc_decode(&_code, &_data);
    if (err) {
        DEBUG("ERROR: Decode failed: %s\n", quirc_strerror(err));
        return mp_const_none;
    }

    DEBUG("Data: %s\n", _data.payload);

    vstr_t vstr;
    vstr_init(&vstr, _data.payload_len + 1);
    vstr_add_strn(&vstr, (const char*)_data.payload, _data.payload_len);  // Can append to vstr if necessary

    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(quirc_scan_obj, mod_quirc_scan);

STATIC mp_obj_array_t quirc_framebuffer_obj = {
    .base     = {&mp_type_bytearray},
    .typecode = BYTEARRAY_TYPECODE,
    .free     = 0,
    .len      = sizeof(_framebuffer),
    .items    = _framebuffer,
};

STATIC const mp_rom_map_elem_t quirc_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_quirc) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&quirc_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&quirc_scan_obj) },
    { MP_ROM_QSTR(MP_QSTR_framebuffer), MP_ROM_PTR(&quirc_framebuffer_obj) }
};
STATIC MP_DEFINE_CONST_DICT(quirc_module_globals, quirc_module_globals_table);

const mp_obj_module_t quirc_user_cmodule = {
    .base    = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&quirc_module_globals,
};


#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    MP_REGISTER_MODULE(MP_QSTR_quirc, quirc_user_cmodule);
#else
    MP_REGISTER_MODULE(MP_QSTR_quirc, quirc_user_cmodule, MODULE_QUIRC_ENABLED);
#endif
