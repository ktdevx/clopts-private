/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 */

#include <naos/naos.h>

NAOS_DECLARE_SUITE(fixtures);
NAOS_DECLARE_SUITE(lookup);
NAOS_DECLARE_SUITE(dispatch);
NAOS_DECLARE_SUITE(long);
NAOS_DECLARE_SUITE(short);
NAOS_DECLARE_SUITE(values);
NAOS_DECLARE_SUITE(positional);
NAOS_DECLARE_SUITE(resolve);
NAOS_DECLARE_SUITE(errors);
NAOS_DECLARE_SUITE(result);
NAOS_DECLARE_SUITE(oom);

NAOS_MAIN(
    NAOS_ADD_SUITE(fixtures),
    NAOS_ADD_SUITE(lookup),
    NAOS_ADD_SUITE(dispatch),
    NAOS_ADD_SUITE(long),
    NAOS_ADD_SUITE(short),
    NAOS_ADD_SUITE(values),
    NAOS_ADD_SUITE(positional),
    NAOS_ADD_SUITE(resolve),
    NAOS_ADD_SUITE(errors),
    NAOS_ADD_SUITE(result),
    NAOS_ADD_SUITE(oom)
)
