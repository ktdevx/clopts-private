/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Suite decl_positional: the checks of a positional alone, made by
 * clopts_positional_create() and the positional setters (positional.c).
 * None of these tests creates a program or a command. The order of the
 * positionals is checked when one is attached (decl suite).
 */

#include "decl/decl_internal.h"
#include "test_decl.h"

#include <naos/naos.h>

/* ---- helpers ----------------------------------------------------------- */

/* Checks that call returns CLOPTS_ERROR_NONE. */
#define EXPECT_OK(call) NAOS_CHECK_INT_EQ((call), CLOPTS_ERROR_NONE)

/* An object whose address an out pointer holds before a call; never read or written. */
static char sentinel_positional_storage;
#define SENTINEL_POSITIONAL ((clopts_positional_t *)&sentinel_positional_storage)

/* A new unattached positional; the test stops when it cannot be made. */
static clopts_positional_t *make_positional(const char *name)
{
    clopts_positional_t *positional = test_decl_positional(name);

    NAOS_REQUIRE_NOT_NULL(positional);
    return positional;
}

/* ---- clopts_positional_create ------------------------------------------- */

NAOS_TEST(positional_name_invalid)
{
    clopts_positional_t *positional;

    positional = SENTINEL_POSITIONAL;
    NAOS_CHECK_INT_EQ(clopts_positional_create(NULL, &positional), CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(positional);

    positional = SENTINEL_POSITIONAL;
    NAOS_CHECK_INT_EQ(clopts_positional_create("", &positional), CLOPTS_ERROR_INVALID_NAME);
    NAOS_CHECK_NULL(positional);

    positional = NULL;
    EXPECT_OK(clopts_positional_create("FILE", &positional));
    NAOS_REQUIRE_NOT_NULL(positional);
    NAOS_CHECK_STR_EQ(clopts_positional_get_name(positional), "FILE");
    clopts_positional_destroy(positional);
}

/* ---- positional setters ---------------------------------------------- */

/* The setters of an unattached positional check nothing: the order is checked at attach. */
NAOS_TEST(positional_setters_before_attach_never_fail)
{
    clopts_positional_t *positional = make_positional("file");

    clopts_positional_set_variadic(positional, true);
    clopts_positional_set_required(positional, true);
    clopts_positional_set_variadic(positional, false);
    clopts_positional_set_required(positional, false);
    EXPECT_OK(clopts_positional_set_help(positional, "File."));
    EXPECT_OK(clopts_positional_set_help(positional, NULL));
    NAOS_CHECK_FALSE(clopts_positional_is_required(positional));
    NAOS_CHECK_FALSE(clopts_positional_is_variadic(positional));
    NAOS_CHECK_NULL(clopts_positional_get_help(positional));
    clopts_positional_destroy(positional);
}

NAOS_SUITE(decl_positional,
    NAOS_ADD_TEST(positional_name_invalid),
    NAOS_ADD_TEST(positional_setters_before_attach_never_fail)
)
