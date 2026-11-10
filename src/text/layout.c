/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * TextLayout: help and Usage line.
 *
 * Lists are written in two passes over the same row procedure: the left
 * columns are measured on a measure-mode output to find the widest one, then
 * every row is written and padded to it. The rows of a list are read in
 * place from the declaration (no allocation), through the public read
 * functions only.
 *
 * Nothing here is reserved: --help, --version and the help command are
 * ordinary options and commands that the program declares, listed like any
 * other one.
 */

#include "text/text_internal.h"

#include <assert.h>
#include <string.h>

/* ---- lists ------------------------------------------------------------- */

/* Indentation of every row of a list. */
#define TEXT_LIST_INDENT 2
/* Gap between the widest left column of a list and the descriptions. */
#define TEXT_LIST_GAP 2

/* A string "has text": it is neither NULL nor empty. */
static bool text_has_text(const char *s)
{
    return (s != NULL) && (s[0] != '\0');
}

/* Value name of option: its declared one, or VALUE when it has no text. */
static const char *text_value_name(const clopts_option_t *option)
{
    return text_has_text(clopts_option_get_value_name(option)) ? clopts_option_get_value_name(option) : "VALUE";
}

/* Writes "<V>" for option. */
static void text_write_value(clopts_text_out_t *out, const clopts_option_t *option)
{
    clopts_text_out_char(out, '<');
    clopts_text_out_str(out, text_value_name(option));
    clopts_text_out_char(out, '>');
}

/*
 * Writes the left column of option (4.1 to 4.6). The only procedure that
 * produces it: run on a measure-mode output it gives the width, run on a
 * write-mode output it writes the same bytes.
 */
static void text_write_option_left(clopts_text_out_t *out, const clopts_option_t *option)
{
    bool has_long = text_has_text(clopts_option_get_long_name(option));

    if (clopts_option_get_short_name(option) != '\0')
    {
        clopts_text_out_char(out, '-');
        clopts_text_out_char(out, clopts_option_get_short_name(option));
        if (has_long)
        {
            clopts_text_out_str(out, ", ");
        }
    }
    else
    {
        clopts_text_out_spaces(out, 4);
    }
    if (has_long)
    {
        clopts_text_out_str(out, "--");
        if ((clopts_option_get_kind(option) == CLOPTS_OPTION_FLAG) && clopts_option_is_negatable(option))
        {
            clopts_text_out_str(out, "[no-]");
        }
        clopts_text_out_str(out, clopts_option_get_long_name(option));
    }

    switch (clopts_option_get_kind(option))
    {
    case CLOPTS_OPTION_COUNT:
        clopts_text_out_str(out, "...");
        break;
    case CLOPTS_OPTION_VALUE:
        clopts_text_out_char(out, ' ');
        text_write_value(out, option);
        break;
    case CLOPTS_OPTION_LIST:
        clopts_text_out_char(out, ' ');
        text_write_value(out, option);
        if (clopts_option_get_delimiter(option) != '\0')
        {
            clopts_text_out_char(out, '[');
            clopts_text_out_char(out, clopts_option_get_delimiter(option));
            text_write_value(out, option);
            clopts_text_out_str(out, "...]");
        }
        else
        {
            clopts_text_out_str(out, "...");
        }
        break;
    case CLOPTS_OPTION_FLAG:
    default:
        break;
    }
}

/*
 * One row as it is written. Exactly one kind is used, checked in this order:
 *   - option != NULL: the left column and the notes of the option row;
 *     positional, left and help are not read.
 *   - positional != NULL: the left column "<NAME>" and the help of the
 *     positional; left and help are not read.
 *   - otherwise a fixed row: left is written as is, help is the description
 *     (NULL or "" for none).
 */
typedef struct text_item
{
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *left;
    const char *help;
} text_item_t;

/* Where the declared rows of a list come from. */
typedef enum text_source_kind
{
    TEXT_SOURCE_NONE,         /* no declared rows */
    TEXT_SOURCE_OPTIONS,      /* the options of scope, except those shadowed by shadow (when not NULL) */
    TEXT_SOURCE_POSITIONALS,  /* the positionals of scope */
    TEXT_SOURCE_COMMANDS      /* the subcommands of program: name and summary as a fixed row */
} text_source_kind_t;

/*
 * The rows of one list, read in place without allocating: the declared rows
 * of the source in order, then the row_count rows at rows.
 */
typedef struct text_list
{
    text_source_kind_t kind;
    const clopts_program_t *program;
    const clopts_scope_t *scope;
    const clopts_scope_t *shadow;
    const clopts_text_row_t *rows;
    size_t row_count;
} text_list_t;

/* True when scope has an option with the long name l (exact). */
static bool text_has_long(const clopts_scope_t *scope, const char *l)
{
    size_t count = clopts_scope_get_option_count(scope);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const char *name = clopts_option_get_long_name(clopts_scope_get_option_at(scope, i));

        if ((name != NULL) && (strcmp(name, l) == 0))
        {
            return true;
        }
    }
    return false;
}

/* Number of declared elements of the source of list, before any filter. */
static size_t text_source_len(const text_list_t *list)
{
    switch (list->kind)
    {
    case TEXT_SOURCE_OPTIONS:
        return clopts_scope_get_option_count(list->scope);
    case TEXT_SOURCE_POSITIONALS:
        return clopts_scope_get_positional_count(list->scope);
    case TEXT_SOURCE_COMMANDS:
        return clopts_program_get_command_count(list->program);
    case TEXT_SOURCE_NONE:
    default:
        return 0;
    }
}

/* True when the element index of the source of list is listed. */
static bool text_source_listed(const text_list_t *list, size_t index)
{
    if ((list->kind != TEXT_SOURCE_OPTIONS) || (list->shadow == NULL))
    {
        return true;
    }
    return !clopts_scope_shadows_option(list->shadow, clopts_scope_get_option_at(list->scope, index));
}

/* Number of declared rows of list. */
static size_t text_list_declared(const text_list_t *list)
{
    size_t len = text_source_len(list);
    size_t count = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (text_source_listed(list, i))
        {
            count++;
        }
    }
    return count;
}

/* Number of rows of list. */
static size_t text_list_count(const text_list_t *list)
{
    return text_list_declared(list) + list->row_count;
}

/* The item of element index of the source of list. */
static text_item_t text_source_item(const text_list_t *list, size_t index)
{
    text_item_t item = {NULL, NULL, NULL, NULL};

    switch (list->kind)
    {
    case TEXT_SOURCE_OPTIONS:
        item.option = clopts_scope_get_option_at(list->scope, index);
        break;
    case TEXT_SOURCE_POSITIONALS:
        item.positional = clopts_scope_get_positional_at(list->scope, index);
        break;
    case TEXT_SOURCE_COMMANDS:
    {
        const clopts_command_t *command = clopts_program_get_command_at(list->program, index);

        item.left = clopts_command_get_name(command);
        item.help = clopts_command_get_summary(command);
        break;
    }
    case TEXT_SOURCE_NONE:
    default:
        break;
    }
    return item;
}

/* Row index of list; requires index < text_list_count(list). */
static text_item_t text_list_item(const text_list_t *list, size_t index)
{
    text_item_t item = {NULL, NULL, NULL, NULL};
    size_t len = text_source_len(list);
    size_t listed = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (text_source_listed(list, i))
        {
            if (listed == index)
            {
                return text_source_item(list, i);
            }
            listed++;
        }
    }
    {
        const clopts_text_row_t *row = &list->rows[index - listed];

        item.option = row->option;
        item.left = row->left;
        item.help = row->help;
    }
    return item;
}

/*
 * Writes the left column of item. The only procedure that produces it: run
 * on a measure-mode output it gives the width, run on a write-mode output it
 * writes the same bytes.
 */
static void text_write_item_left(clopts_text_out_t *out, const text_item_t *item)
{
    if (item->option != NULL)
    {
        text_write_option_left(out, item->option);
    }
    else if (item->positional != NULL)
    {
        clopts_text_out_char(out, '<');
        clopts_text_out_str(out, clopts_positional_get_name(item->positional));
        clopts_text_out_char(out, '>');
    }
    else if (item->left != NULL)
    {
        clopts_text_out_str(out, item->left);
    }
}

/* Display width of the left column of item, measured by text_write_item_left. */
static size_t text_item_left_width(const text_item_t *item)
{
    clopts_text_out_t measure;

    clopts_text_out_init(&measure, NULL, NULL);
    text_write_item_left(&measure, item);
    return measure.width;
}

/* Description of item; NULL or "" for none. */
static const char *text_item_help(const text_item_t *item)
{
    if (item->option != NULL)
    {
        return clopts_option_get_help(item->option);
    }
    if (item->positional != NULL)
    {
        return clopts_positional_get_help(item->positional);
    }
    return item->help;
}

/* The item has a description or a note (4.7 to 4.11). */
static bool text_item_has_rest(const text_item_t *item)
{
    const clopts_option_t *option = item->option;

    if (text_has_text(text_item_help(item)))
    {
        return true;
    }
    return (option != NULL) &&
           (clopts_option_is_required(option) || text_has_text(clopts_option_get_default(option)) || text_has_text(clopts_option_get_env(option)));
}

/* Writes a separating space unless nothing has been written yet. */
static void text_write_separator(clopts_text_out_t *out, bool *first)
{
    if (!*first)
    {
        clopts_text_out_char(out, ' ');
    }
    *first = false;
}

/* Writes the description and the notes of item, in the fixed order (4.12). */
static void text_write_item_rest(clopts_text_out_t *out, const text_item_t *item)
{
    const clopts_option_t *option = item->option;
    const char *help = text_item_help(item);
    bool first = true;

    if (text_has_text(help))
    {
        text_write_separator(out, &first);
        clopts_text_out_str(out, help);
    }
    if (option == NULL)
    {
        return;
    }
    if (clopts_option_is_required(option))
    {
        text_write_separator(out, &first);
        clopts_text_out_str(out, "(required)");
    }
    if (text_has_text(clopts_option_get_default(option)))
    {
        text_write_separator(out, &first);
        clopts_text_out_str(out, "(default: ");
        clopts_text_out_str(out, clopts_option_get_default(option));
        clopts_text_out_char(out, ')');
    }
    if (text_has_text(clopts_option_get_env(option)))
    {
        text_write_separator(out, &first);
        clopts_text_out_str(out, "[env: ");
        clopts_text_out_str(out, clopts_option_get_env(option));
        clopts_text_out_char(out, ']');
    }
}

/* Writes the rows of list as one aligned list (10.1). */
static void text_write_rows(clopts_text_out_t *out, const text_list_t *list)
{
    size_t count = text_list_count(list);
    size_t max_width = 0;
    size_t i;

    /* Pass 1: the widest left column, measured. */
    for (i = 0; i < count; i++)
    {
        text_item_t item = text_list_item(list, i);
        size_t width = text_item_left_width(&item);

        if (width > max_width)
        {
            max_width = width;
        }
    }

    /* Pass 2: the rows, written and padded to max_width + TEXT_LIST_GAP. */
    for (i = 0; i < count; i++)
    {
        text_item_t item = text_list_item(list, i);

        clopts_text_out_spaces(out, TEXT_LIST_INDENT);
        text_write_item_left(out, &item);
        if (text_item_has_rest(&item))
        {
            clopts_text_out_spaces(out, (max_width - text_item_left_width(&item)) + TEXT_LIST_GAP);
            text_write_item_rest(out, &item);
        }
        clopts_text_out_char(out, '\n');
    }
}

void clopts_text_write_list(clopts_text_out_t *out, const clopts_text_row_t *rows, size_t count)
{
    text_list_t list;

    assert(out != NULL);
    assert((count == 0) || (rows != NULL));

    list.kind = TEXT_SOURCE_NONE;
    list.program = NULL;
    list.scope = NULL;
    list.shadow = NULL;
    list.rows = rows;
    list.row_count = count;
    text_write_rows(out, &list);
}

/* ---- paragraphs -------------------------------------------------------- */

/* The program has subcommands. */
static bool text_has_subcommands(const clopts_program_t *program)
{
    return clopts_program_get_command_count(program) > 0;
}

/* Writes the notation of positional in the Usage line (5.2). */
static void text_write_positional_usage(clopts_text_out_t *out, const clopts_positional_t *positional)
{
    const bool required = clopts_positional_is_required(positional);

    if (!required)
    {
        clopts_text_out_char(out, '[');
    }
    clopts_text_out_char(out, '<');
    clopts_text_out_str(out, clopts_positional_get_name(positional));
    clopts_text_out_char(out, '>');
    if (clopts_positional_is_variadic(positional))
    {
        clopts_text_out_str(out, "...");
    }
    if (!required)
    {
        clopts_text_out_char(out, ']');
    }
}

/*
 * Writes the Usage line of command (NULL for the program) and its '\n'
 * (5.1, 5.2, 5.4).
 */
static void text_write_usage_line(clopts_text_out_t *out, const clopts_program_t *program,
                                  const clopts_command_t *command)
{
    const clopts_scope_t *scope =
        (command != NULL) ? clopts_command_get_scope(command) : clopts_program_get_scope(program);
    size_t count;
    size_t i;

    clopts_text_out_str(out, "Usage: ");
    clopts_text_out_str(out, clopts_program_get_name(program));
    if (command != NULL)
    {
        clopts_text_out_char(out, ' ');
        clopts_text_out_str(out, clopts_command_get_name(command));
    }
    else if (text_has_subcommands(program))
    {
        clopts_text_out_str(out, " [<options>] <command> [<arguments>]\n");
        return;
    }
    clopts_text_out_str(out, " [<options>]");
    count = clopts_scope_get_positional_count(scope);
    for (i = 0; i < count; i++)
    {
        clopts_text_out_char(out, ' ');
        text_write_positional_usage(out, clopts_scope_get_positional_at(scope, i));
    }
    clopts_text_out_char(out, '\n');
}

/* Writes a paragraph of one line of text, after a blank line. */
static void text_write_text_paragraph(clopts_text_out_t *out, const char *text)
{
    clopts_text_out_char(out, '\n');
    clopts_text_out_str(out, text);
    clopts_text_out_char(out, '\n');
}

/* Writes a list paragraph with its heading, after a blank line, when the list has rows. */
static void text_write_list_paragraph(clopts_text_out_t *out, const char *heading, const text_list_t *list)
{
    if (text_list_count(list) == 0)
    {
        return;
    }
    clopts_text_out_char(out, '\n');
    clopts_text_out_str(out, heading);
    clopts_text_out_char(out, '\n');
    text_write_rows(out, list);
}

/* Sets list to the declared rows of kind in scope (or program), with no fixed row. */
static void text_list_init(text_list_t *list, text_source_kind_t kind, const clopts_program_t *program,
                           const clopts_scope_t *scope, const clopts_scope_t *shadow)
{
    list->kind = kind;
    list->program = program;
    list->scope = scope;
    list->shadow = shadow;
    list->rows = NULL;
    list->row_count = 0;
}

/* Writes the Arguments: paragraph of scope, if it has positionals (2.3). */
static void text_write_arguments(clopts_text_out_t *out, const clopts_scope_t *scope)
{
    text_list_t list;

    text_list_init(&list, TEXT_SOURCE_POSITIONALS, NULL, scope, NULL);
    text_write_list_paragraph(out, "Arguments:", &list);
}

/* Writes the Options: paragraph of scope, if it has options (1.4, 2.4, 3.3). */
static void text_write_options(clopts_text_out_t *out, const clopts_scope_t *scope)
{
    text_list_t list;

    text_list_init(&list, TEXT_SOURCE_OPTIONS, NULL, scope, NULL);
    text_write_list_paragraph(out, "Options:", &list);
}

/* Writes the help of a program with subcommands (1.1 to 1.6). */
static void text_write_global_help(clopts_text_out_t *out, const clopts_program_t *program)
{
    const clopts_scope_t *global = clopts_program_get_scope(program);
    const char *description = clopts_program_get_description(program);
    text_list_t commands;

    text_write_usage_line(out, program, NULL);
    if (text_has_text(description))
    {
        text_write_text_paragraph(out, description);
    }

    text_list_init(&commands, TEXT_SOURCE_COMMANDS, program, NULL, NULL);
    text_write_list_paragraph(out, "Commands:", &commands);

    text_write_options(out, global);

    /* Only when the program has a global --help to point to. */
    if (text_has_long(global, "help"))
    {
        clopts_text_out_str(out, "\nRun '");
        clopts_text_out_str(out, clopts_program_get_name(program));
        clopts_text_out_str(out, " <command> --help' for help on a command.\n");
    }
}

/* Writes the help of a subcommand (2.1 to 2.6). */
static void text_write_command_help(clopts_text_out_t *out, const clopts_program_t *program,
                                    const clopts_command_t *command)
{
    const char *description = clopts_command_get_description(command);
    const char *summary = clopts_command_get_summary(command);
    text_list_t globals;

    text_write_usage_line(out, program, command);
    if (text_has_text(description))
    {
        text_write_text_paragraph(out, description);
    }
    else if (text_has_text(summary))
    {
        text_write_text_paragraph(out, summary);
    }
    text_write_arguments(out, clopts_command_get_scope(command));
    text_write_options(out, clopts_command_get_scope(command));

    text_list_init(&globals, TEXT_SOURCE_OPTIONS, NULL, clopts_program_get_scope(program),
                   clopts_command_get_scope(command));
    text_write_list_paragraph(out, "Global Options:", &globals);
}

/* Writes the help of a program without subcommands (3.1 to 3.3). */
static void text_write_single_help(clopts_text_out_t *out, const clopts_program_t *program)
{
    const char *description = clopts_program_get_description(program);

    text_write_usage_line(out, program, NULL);
    if (text_has_text(description))
    {
        text_write_text_paragraph(out, description);
    }
    text_write_arguments(out, clopts_program_get_scope(program));
    text_write_options(out, clopts_program_get_scope(program));
}

/* ---- public functions -------------------------------------------------- */

bool clopts_text_has_command(const clopts_program_t *program, const clopts_command_t *command)
{
    size_t count = clopts_program_get_command_count(program);
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (clopts_program_get_command_at(program, i) == command)
        {
            return true;
        }
    }
    return false;
}

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    clopts_text_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || clopts_text_has_command(program, command));

    clopts_text_out_init(&out, write_fn, ctx);
    if (command != NULL)
    {
        text_write_command_help(&out, program, command);
    }
    else if (text_has_subcommands(program))
    {
        text_write_global_help(&out, program);
    }
    else
    {
        text_write_single_help(&out, program);
    }
    return clopts_text_out_status(&out);
}

clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                         clopts_write_fn write_fn, void *ctx)
{
    clopts_text_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || clopts_text_has_command(program, command));

    clopts_text_out_init(&out, write_fn, ctx);
    text_write_usage_line(&out, program, command);
    return clopts_text_out_status(&out);
}
