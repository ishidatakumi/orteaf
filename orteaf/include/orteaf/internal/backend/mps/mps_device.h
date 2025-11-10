/**
 * @file mps_device.h
 * @brief MPS/Metal device discovery, selection, retain/release helpers.
 *
 * Thin wrappers over Metal's `MTLDevice` for obtaining the default device,
 * enumerating all devices, and managing retain/release of opaque handles.
 * When MPS is disabled or Objective-C is unavailable, functions exist but
 * behave as no-ops and return neutral values.
 */
#pragma once

#include <string>

#include "orteaf/internal/backend/mps/mps_size.h"

namespace orteaf::internal::backend::mps {

struct MPSDevice_st; using MPSDevice_t = MPSDevice_st*;
struct MPSDeviceArray_st; using MPSDeviceArray_t = MPSDeviceArray_st*;

static_assert(sizeof(MPSDevice_t) == sizeof(void*), "MPSDevice must be pointer-sized.");

/**
 * @brief Get the system default Metal device.
 * @return Opaque device handle, or nullptr when unavailable/disabled.
 */
MPSDevice_t get_device();

/**
 * @brief Get a Metal device by index from the system list.
 * @param device_id Zero-based device index
 * @return Opaque device handle, or nullptr if out of range/unavailable.
 */
MPSDevice_t get_device(MPSInt_t device_id);

/**
 * @brief Get the number of Metal devices available.
 * @return Device count; 0 when unavailable/disabled.
 */
int get_device_count();

/**
 * @brief Retain a device handle (increments reference count).
 * @param device Opaque device handle; nullptr is ignored.
 */
void device_retain(MPSDevice_t device);

/**
 * @brief Release a device handle (decrements reference count).
 * @param device Opaque device handle; nullptr is ignored.
 */
void device_release(MPSDevice_t device);

/**
 * @brief Get an array of all Metal devices.
 * @return Opaque array handle, or nullptr when unavailable/disabled.
 */
MPSDeviceArray_t get_device_array();

/**
 * @brief Human-readable device name (e.g., "Apple M4 Pro").
 */
std::string get_device_name(MPSDevice_t device);

/**
 * @brief Vendor hint string for architecture detection (typically "apple").
 */
std::string get_device_vendor(MPSDevice_t device);

/**
 * @brief Metal family hint (e.g., "m2", "m3", "m4") derived from device capabilities.
 */
std::string get_device_metal_family(MPSDevice_t device);


} // namespace orteaf::internal::backend::mps
