/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro to access HAL configuration options.
 *
 * This macro is used to access various HAL configuration options defined in config.h
 * It should be used instead of directly accessing the HAL_CONFIG_ prefixed options.
 *
 * @param x The configuration option to access, without the HAL_CONFIG_ prefix.
 */
#define HAL_CONFIG(x) HAL_CONFIG_##x

/**
 * @brief The minimum supported chip revision.
 */
#define HAL_CONFIG_CHIP_SUPPORT_MIN_REV 100

#ifdef __cplusplus
}
#endif
