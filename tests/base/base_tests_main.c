/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include <naos/naos.h>

NAOS_DECLARE_SUITE(mem);
NAOS_DECLARE_SUITE(size);
NAOS_DECLARE_SUITE(vec);
NAOS_DECLARE_SUITE(utf8);

NAOS_MAIN(
    NAOS_ADD_SUITE(mem),
    NAOS_ADD_SUITE(size),
    NAOS_ADD_SUITE(vec),
    NAOS_ADD_SUITE(utf8)
)
