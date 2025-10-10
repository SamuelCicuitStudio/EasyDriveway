/**************************************************************
 *  Project     : EasyDriveway
 *  File        : SetRole.h
 *  Purpose     : Compile-time role selection & helpers.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef SET_ROLE_H
#define SET_ROLE_H

#include <Arduino.h>

// -------------------------------------------------------------------
// ROLE SELECTION (exactly one defined)
// -------------------------------------------------------------------
// #define NVS_ROLE_ICM    // Interface Control Module
// #define NVS_ROLE_PMS    // Power Management Unit
// #define NVS_ROLE_SENS   // Sensor (production)
// #define NVS_ROLE_RELAY  // Relay  (production)
// #define NVS_ROLE_SEMU   // Sensor Emulator (multi-input, shared MAC)
 #define NVS_ROLE_REMU   // Relay  Emulator (multi-output, shared MAC)

// -------------------------------------------------------------------
// VALIDATION
// -------------------------------------------------------------------
#if (defined(NVS_ROLE_ICM)  + \
     defined(NVS_ROLE_PMS)  + \
     defined(NVS_ROLE_SENS) + \
     defined(NVS_ROLE_RELAY)+ \
     defined(NVS_ROLE_SEMU) + \
     defined(NVS_ROLE_REMU)) == 0
#  error "No role selected. Define exactly one of NVS_ROLE_* (ICM/PMS/SENS/RELAY/SEMU/REMU)."
#endif
#if (defined(NVS_ROLE_ICM)  + \
     defined(NVS_ROLE_PMS)  + \
     defined(NVS_ROLE_SENS) + \
     defined(NVS_ROLE_RELAY)+ \
     defined(NVS_ROLE_SEMU) + \
     defined(NVS_ROLE_REMU)) > 1
#  error "Multiple roles selected. Define ONLY one of NVS_ROLE_* (ICM/PMS/SENS/RELAY/SEMU/REMU)."
#endif

/**
 * @enum Role
 * @brief Enumerates all supported device roles.
 */
enum class Role : uint8_t { ICM, PMS, SENS, RELAY, SEMU, REMU };

/**
 * @brief Return the active role as an enum (constexpr-friendly).
 */
static inline constexpr Role ACTIVE_ROLE() {
#if   defined(NVS_ROLE_ICM)
    return Role::ICM;
#elif defined(NVS_ROLE_PMS)
    return Role::PMS;
#elif defined(NVS_ROLE_SENS)
    return Role::SENS;
#elif defined(NVS_ROLE_RELAY)
    return Role::RELAY;
#elif defined(NVS_ROLE_SEMU)
    return Role::SEMU;
#elif defined(NVS_ROLE_REMU)
    return Role::REMU;
#else
#   error "ACTIVE_ROLE() reached unreachable state."
#endif
}

/**
 * @brief Human-readable role string for logs/UI.
 * @return C-string name (e.g., "PMS").
 */
static inline const char* ROLE_NAME() {
    switch (ACTIVE_ROLE()) {
        case Role::ICM:   return "ICM";
        case Role::PMS:   return "PMS";
        case Role::SENS:  return "SENS";
        case Role::RELAY: return "RELAY";
        case Role::SEMU:  return "SEMU";
        case Role::REMU:  return "REMU";
        default:          return "UNKNOWN";
    }
}

/**
 * @brief Lowercase tag for file/log prefixes.
 * @return C-string base tag (e.g., "pms").
 */
static inline const char* ROLE_BASE_TAG() {
#if   defined(NVS_ROLE_ICM)
    return "icm";
#elif defined(NVS_ROLE_PMS)
    return "pms";
#elif defined(NVS_ROLE_SENS)
    return "sens";
#elif defined(NVS_ROLE_RELAY)
    return "rel";
#elif defined(NVS_ROLE_SEMU)
    return "semu";
#elif defined(NVS_ROLE_REMU)
    return "remu";
#else
    return "node";
#endif
}

#endif // SET_ROLE_H
