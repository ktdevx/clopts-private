/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include <naos/naos.h>

NAOS_DECLARE_SUITE(alloc);
NAOS_DECLARE_SUITE(str);
NAOS_DECLARE_SUITE(size);
NAOS_DECLARE_SUITE(vec);
NAOS_DECLARE_SUITE(utf8);

NAOS_MAIN(
    NAOS_ADD_SUITE(alloc),
    NAOS_ADD_SUITE(str),
    NAOS_ADD_SUITE(size),
    NAOS_ADD_SUITE(vec),
    NAOS_ADD_SUITE(utf8)
)
