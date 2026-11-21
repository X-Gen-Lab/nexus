/**
 * \file            fdb_cfg.h
 * \brief           Private non-ECC 32-bit database storage profile
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_PRIVATE_FDB_CFG_H
#define NEXUS_PRIVATE_FDB_CFG_H
#include <assert.h>
#define FDB_USING_KVDB
#define FDB_USING_TSDB
#define FDB_USING_FAL_MODE
#define FDB_USING_TIMESTAMP_64BIT
#define FDB_WRITE_GRAN              32
#define FDB_KV_CACHE_TABLE_SIZE     0
#define FDB_SECTOR_CACHE_TABLE_SIZE 0
#define FDB_USING_READ_CACHE        0
#define FDB_PRINT(...)              ((void)0)
#define FDB_ASSERT(expression)      assert(expression)
#endif
