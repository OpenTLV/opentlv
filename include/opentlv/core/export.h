// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marek Cingel

#ifndef OPENTLV_CORE_EXPORT_H
#define OPENTLV_CORE_EXPORT_H

/** @file
 * @brief Visibility of public Core functions.
 * @ingroup core
 */

/** Direct and static builds use the compiler's default visibility.
 * A shared-library build may supply its import or export annotation here.
 */
#ifndef OPENTLV_CORE_API
#define OPENTLV_CORE_API
#endif

#endif /* OPENTLV_CORE_EXPORT_H */
