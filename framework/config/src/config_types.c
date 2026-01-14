/**
 * \file            config_types.c
 * \brief           Config Manager Basic Data Type Operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Implements basic data type storage and retrieval functions
 *                  for int32, uint32, int64, float, and bool types.
 */

#include "config/config.h"
#include "config_store.h"
#include "config_callback.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* 32-bit Signed Integer Operations                                          */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_i32(const char* key, int32_t value) {
    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    int32_t old_value = 0;
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(int32_t);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_I32) {
            if (config_store_get(key, NULL, &old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_I32, &value, sizeof(int32_t),
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_I32,
                               has_old_value ? &old_value : NULL, sizeof(int32_t),
                               &value, sizeof(int32_t));
    }

    return status;
}

config_status_t
config_get_i32(const char* key, int32_t* value, int32_t default_val) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_I32) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(int32_t);
    status = config_store_get(key, NULL, value, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    return status;
}

/*---------------------------------------------------------------------------*/
/* 32-bit Unsigned Integer Operations                                        */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_u32(const char* key, uint32_t value) {
    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    uint32_t old_value = 0;
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(uint32_t);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_U32) {
            if (config_store_get(key, NULL, &old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_U32, &value, sizeof(uint32_t),
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_U32,
                               has_old_value ? &old_value : NULL, sizeof(uint32_t),
                               &value, sizeof(uint32_t));
    }

    return status;
}

config_status_t
config_get_u32(const char* key, uint32_t* value, uint32_t default_val) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_U32) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(uint32_t);
    status = config_store_get(key, NULL, value, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    return status;
}

/*---------------------------------------------------------------------------*/
/* 64-bit Signed Integer Operations                                          */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_i64(const char* key, int64_t value) {
    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    int64_t old_value = 0;
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(int64_t);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_I64) {
            if (config_store_get(key, NULL, &old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_I64, &value, sizeof(int64_t),
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_I64,
                               has_old_value ? &old_value : NULL, sizeof(int64_t),
                               &value, sizeof(int64_t));
    }

    return status;
}

config_status_t
config_get_i64(const char* key, int64_t* value, int64_t default_val) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_I64) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(int64_t);
    status = config_store_get(key, NULL, value, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    return status;
}

/*---------------------------------------------------------------------------*/
/* Float Operations                                                          */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_float(const char* key, float value) {
    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    float old_value = 0.0f;
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(float);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_FLOAT) {
            if (config_store_get(key, NULL, &old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_FLOAT, &value, sizeof(float),
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_FLOAT,
                               has_old_value ? &old_value : NULL, sizeof(float),
                               &value, sizeof(float));
    }

    return status;
}

config_status_t
config_get_float(const char* key, float* value, float default_val) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_FLOAT) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(float);
    status = config_store_get(key, NULL, value, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    return status;
}

/*---------------------------------------------------------------------------*/
/* Boolean Operations                                                        */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_bool(const char* key, bool value) {
    if (key == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    bool old_value = false;
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(bool);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_BOOL) {
            if (config_store_get(key, NULL, &old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_BOOL, &value, sizeof(bool),
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_BOOL,
                               has_old_value ? &old_value : NULL, sizeof(bool),
                               &value, sizeof(bool));
    }

    return status;
}

config_status_t
config_get_bool(const char* key, bool* value, bool default_val) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status == CONFIG_ERROR_NOT_FOUND) {
        *value = default_val;
        return CONFIG_OK;
    }

    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_BOOL) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = sizeof(bool);
    status = config_store_get(key, NULL, value, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    return status;
}


/*---------------------------------------------------------------------------*/
/* String Operations                                                         */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_str(const char* key, const char* value) {
    if (key == NULL || value == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    char old_value[CONFIG_MAX_MAX_VALUE_SIZE];
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(old_value);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_STRING) {
            if (config_store_get(key, NULL, old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    /* Store string including null terminator */
    size_t len = strlen(value) + 1;
    config_status_t status = config_store_set(key, CONFIG_TYPE_STRING, value, len,
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_STRING,
                               has_old_value ? old_value : NULL, old_size,
                               value, len);
    }

    return status;
}

config_status_t
config_get_str(const char* key, char* buffer, size_t buf_size) {
    if (key == NULL || buffer == NULL || buf_size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_STRING) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = buf_size;
    status = config_store_get(key, NULL, buffer, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    if (status != CONFIG_OK) {
        return status;
    }

    /* Ensure null termination */
    buffer[buf_size - 1] = '\0';

    return CONFIG_OK;
}

config_status_t
config_get_str_len(const char* key, size_t* len) {
    if (key == NULL || len == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    size_t size;
    config_status_t status = config_store_get_size(key, CONFIG_DEFAULT_NAMESPACE_ID, &size);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type */
    config_type_t type;
    status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    if (status != CONFIG_OK) {
        return status;
    }

    if (type != CONFIG_TYPE_STRING) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    /* Return length excluding null terminator */
    *len = (size > 0) ? (size - 1) : 0;

    return CONFIG_OK;
}

/*---------------------------------------------------------------------------*/
/* Binary Blob Operations                                                    */
/*---------------------------------------------------------------------------*/

config_status_t
config_set_blob(const char* key, const void* data, size_t size) {
    if (key == NULL || data == NULL || size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* Get old value if exists for callback notification */
    uint8_t old_value[CONFIG_MAX_MAX_VALUE_SIZE];
    bool has_old_value = false;
    config_type_t old_type;
    size_t old_size = sizeof(old_value);
    
    if (config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &old_type) == CONFIG_OK) {
        if (old_type == CONFIG_TYPE_BLOB) {
            if (config_store_get(key, NULL, old_value, &old_size, NULL,
                                 CONFIG_DEFAULT_NAMESPACE_ID) == CONFIG_OK) {
                has_old_value = true;
            }
        }
    }

    config_status_t status = config_store_set(key, CONFIG_TYPE_BLOB, data, size,
                            CONFIG_FLAG_NONE, CONFIG_DEFAULT_NAMESPACE_ID);

    /* Notify callbacks if set was successful */
    if (status == CONFIG_OK) {
        config_callback_notify(key, CONFIG_TYPE_BLOB,
                               has_old_value ? old_value : NULL, old_size,
                               data, size);
    }

    return status;
}

config_status_t
config_get_blob(const char* key, void* buffer, size_t buf_size, size_t* actual_size) {
    if (key == NULL || buffer == NULL || buf_size == 0) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    /* First check if key exists and get its type */
    config_type_t type;
    config_status_t status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    
    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type before attempting to read */
    if (type != CONFIG_TYPE_BLOB) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    size_t size = buf_size;
    status = config_store_get(key, NULL, buffer, &size, NULL,
                               CONFIG_DEFAULT_NAMESPACE_ID);

    if (status != CONFIG_OK) {
        return status;
    }

    if (actual_size != NULL) {
        *actual_size = size;
    }

    return CONFIG_OK;
}

config_status_t
config_get_blob_len(const char* key, size_t* len) {
    if (key == NULL || len == NULL) {
        return CONFIG_ERROR_INVALID_PARAM;
    }

    config_status_t status = config_store_get_size(key, CONFIG_DEFAULT_NAMESPACE_ID, len);
    if (status != CONFIG_OK) {
        return status;
    }

    /* Verify type */
    config_type_t type;
    status = config_store_get_type(key, CONFIG_DEFAULT_NAMESPACE_ID, &type);
    if (status != CONFIG_OK) {
        return status;
    }

    if (type != CONFIG_TYPE_BLOB) {
        return CONFIG_ERROR_TYPE_MISMATCH;
    }

    return CONFIG_OK;
}
