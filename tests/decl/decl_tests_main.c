/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include <naos/naos.h>

NAOS_DECLARE_SUITE(build);
NAOS_DECLARE_SUITE(decl);
NAOS_DECLARE_SUITE(decl_error);
NAOS_DECLARE_SUITE(decl_option);
NAOS_DECLARE_SUITE(decl_positional);

NAOS_MAIN(
    NAOS_ADD_SUITE(build),
    NAOS_ADD_SUITE(decl),
    NAOS_ADD_SUITE(decl_error),
    NAOS_ADD_SUITE(decl_option),
    NAOS_ADD_SUITE(decl_positional)
)
