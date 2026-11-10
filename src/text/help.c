/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * Help: the help text and its Usage line (write.c calls them).
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

#include "text/help.h"

#include <assert.h>
#include <string.h>

/* ---- lists ------------------------------------------------------------- */

/* Indentation of every row of a list. */
#define LIST_INDENT 2
/* Gap between the widest left column of a list and the descriptions. */
#define LIST_GAP 2

/* A string "has text": it is neither NULL nor empty. */
static bool has_text(const char *s)
{
    return (s != NULL) && (s[0] != '\0');
}

/* Value name of option: its declared one, or VALUE when it has no text. */
static const char *value_name(const clopts_option_t *option)
{
    return has_text(clopts_option_get_value_name(option)) ? clopts_option_get_value_name(option) : "VALUE";
}

/* Writes "<V>" for option. */
static void write_value(clopts_out_t *out, const clopts_option_t *option)
{
    clopts_out_char(out, '<');
    clopts_out_str(out, value_name(option));
    clopts_out_char(out, '>');
}

/*
 * Writes the left column of option (4.1 to 4.6). The only procedure that
 * produces it: run on a measure-mode output it gives the width, run on a
 * write-mode output it writes the same bytes.
 */
static void write_option_left(clopts_out_t *out, const clopts_option_t *option)
{
    bool has_long = has_text(clopts_option_get_long_name(option));

    if (clopts_option_get_short_name(option) != '\0')
    {
        clopts_out_char(out, '-');
        clopts_out_char(out, clopts_option_get_short_name(option));
        if (has_long)
        {
            clopts_out_str(out, ", ");
        }
    }
    else
    {
        clopts_out_spaces(out, 4);
    }
    if (has_long)
    {
        clopts_out_str(out, "--");
        if ((clopts_option_get_kind(option) == CLOPTS_OPTION_FLAG) && clopts_option_is_negatable(option))
        {
            clopts_out_str(out, "[no-]");
        }
        clopts_out_str(out, clopts_option_get_long_name(option));
    }

    switch (clopts_option_get_kind(option))
    {
    case CLOPTS_OPTION_COUNT:
        clopts_out_str(out, "...");
        break;
    case CLOPTS_OPTION_VALUE:
        clopts_out_char(out, ' ');
        write_value(out, option);
        break;
    case CLOPTS_OPTION_LIST:
        clopts_out_char(out, ' ');
        write_value(out, option);
        if (clopts_option_get_delimiter(option) != '\0')
        {
            clopts_out_char(out, '[');
            clopts_out_char(out, clopts_option_get_delimiter(option));
            write_value(out, option);
            clopts_out_str(out, "...]");
        }
        else
        {
            clopts_out_str(out, "...");
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
typedef struct item
{
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *left;
    const char *help;
} item_t;

/* Where the declared rows of a list come from. */
typedef enum source_kind
{
    TEXT_SOURCE_OPTIONS,      /* the options of scope, except those shadowed by shadow (when not NULL) */
    TEXT_SOURCE_POSITIONALS,  /* the positionals of scope */
    TEXT_SOURCE_COMMANDS      /* the subcommands of program: name and summary as a fixed row */
} source_kind_t;

/* The rows of one list, read in place from the declaration without allocating. */
typedef struct list
{
    source_kind_t kind;
    const clopts_program_t *program;
    const clopts_scope_t *scope;
    const clopts_scope_t *shadow;
} list_t;

/* True when scope has an option with the long name l (exact). */
static bool has_long(const clopts_scope_t *scope, const char *l)
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
static size_t source_len(const list_t *list)
{
    switch (list->kind)
    {
    case TEXT_SOURCE_OPTIONS:
        return clopts_scope_get_option_count(list->scope);
    case TEXT_SOURCE_POSITIONALS:
        return clopts_scope_get_positional_count(list->scope);
    case TEXT_SOURCE_COMMANDS:
        return clopts_program_get_command_count(list->program);
    default:
        return 0;
    }
}

/* True when the element index of the source of list is listed. */
static bool source_listed(const list_t *list, size_t index)
{
    if ((list->kind != TEXT_SOURCE_OPTIONS) || (list->shadow == NULL))
    {
        return true;
    }
    return !clopts_scope_shadows_option(list->shadow, clopts_scope_get_option_at(list->scope, index));
}

/* Number of declared rows of list. */
static size_t list_declared(const list_t *list)
{
    size_t len = source_len(list);
    size_t count = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (source_listed(list, i))
        {
            count++;
        }
    }
    return count;
}

/* Number of rows of list. */
static size_t list_count(const list_t *list)
{
    return list_declared(list);
}

/* The item of element index of the source of list. */
static item_t source_item(const list_t *list, size_t index)
{
    item_t item = {NULL, NULL, NULL, NULL};

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
    default:
        break;
    }
    return item;
}

/* Row index of list; requires index < list_count(list). */
static item_t list_item(const list_t *list, size_t index)
{
    item_t item = {NULL, NULL, NULL, NULL};
    size_t len = source_len(list);
    size_t listed = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (source_listed(list, i))
        {
            if (listed == index)
            {
                return source_item(list, i);
            }
            listed++;
        }
    }
    assert(false && "index must be less than list_count()");
    return item;
}

/*
 * Writes the left column of item. The only procedure that produces it: run
 * on a measure-mode output it gives the width, run on a write-mode output it
 * writes the same bytes.
 */
static void write_item_left(clopts_out_t *out, const item_t *item)
{
    if (item->option != NULL)
    {
        write_option_left(out, item->option);
    }
    else if (item->positional != NULL)
    {
        clopts_out_char(out, '<');
        clopts_out_str(out, clopts_positional_get_name(item->positional));
        clopts_out_char(out, '>');
    }
    else if (item->left != NULL)
    {
        clopts_out_str(out, item->left);
    }
}

/* Display width of the left column of item, measured by write_item_left. */
static size_t item_left_width(const item_t *item)
{
    clopts_out_t measure;

    clopts_out_init(&measure, NULL, NULL);
    write_item_left(&measure, item);
    return measure.width;
}

/* Description of item; NULL or "" for none. */
static const char *item_help(const item_t *item)
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
static bool item_has_rest(const item_t *item)
{
    const clopts_option_t *option = item->option;

    if (has_text(item_help(item)))
    {
        return true;
    }
    return (option != NULL) &&
           (clopts_option_is_required(option) || has_text(clopts_option_get_default(option)) || has_text(clopts_option_get_env(option)));
}

/* Writes a separating space unless nothing has been written yet. */
static void write_separator(clopts_out_t *out, bool *first)
{
    if (!*first)
    {
        clopts_out_char(out, ' ');
    }
    *first = false;
}

/* Writes the description and the notes of item, in the fixed order (4.12). */
static void write_item_rest(clopts_out_t *out, const item_t *item)
{
    const clopts_option_t *option = item->option;
    const char *help = item_help(item);
    bool first = true;

    if (has_text(help))
    {
        write_separator(out, &first);
        clopts_out_str(out, help);
    }
    if (option == NULL)
    {
        return;
    }
    if (clopts_option_is_required(option))
    {
        write_separator(out, &first);
        clopts_out_str(out, "(required)");
    }
    if (has_text(clopts_option_get_default(option)))
    {
        write_separator(out, &first);
        clopts_out_str(out, "(default: ");
        clopts_out_str(out, clopts_option_get_default(option));
        clopts_out_char(out, ')');
    }
    if (has_text(clopts_option_get_env(option)))
    {
        write_separator(out, &first);
        clopts_out_str(out, "[env: ");
        clopts_out_str(out, clopts_option_get_env(option));
        clopts_out_char(out, ']');
    }
}

/* Writes the rows of list as one aligned list (10.1). */
static void write_rows(clopts_out_t *out, const list_t *list)
{
    size_t count = list_count(list);
    size_t max_width = 0;
    size_t i;

    /* Pass 1: the widest left column, measured. */
    for (i = 0; i < count; i++)
    {
        item_t item = list_item(list, i);
        size_t width = item_left_width(&item);

        if (width > max_width)
        {
            max_width = width;
        }
    }

    /* Pass 2: the rows, written and padded to max_width + LIST_GAP. */
    for (i = 0; i < count; i++)
    {
        item_t item = list_item(list, i);

        clopts_out_spaces(out, LIST_INDENT);
        write_item_left(out, &item);
        if (item_has_rest(&item))
        {
            clopts_out_spaces(out, (max_width - item_left_width(&item)) + LIST_GAP);
            write_item_rest(out, &item);
        }
        clopts_out_char(out, '\n');
    }
}

/* ---- paragraphs -------------------------------------------------------- */

/* The program has subcommands. */
static bool has_subcommands(const clopts_program_t *program)
{
    return clopts_program_get_command_count(program) > 0;
}

/* Writes the notation of positional in the Usage line (5.2). */
static void write_positional_usage(clopts_out_t *out, const clopts_positional_t *positional)
{
    const bool required = clopts_positional_is_required(positional);

    if (!required)
    {
        clopts_out_char(out, '[');
    }
    clopts_out_char(out, '<');
    clopts_out_str(out, clopts_positional_get_name(positional));
    clopts_out_char(out, '>');
    if (clopts_positional_is_variadic(positional))
    {
        clopts_out_str(out, "...");
    }
    if (!required)
    {
        clopts_out_char(out, ']');
    }
}

/*
 * Writes the Usage line of command (NULL for the program) and its '\n'
 * (5.1, 5.2, 5.4).
 */
static void write_usage_line(clopts_out_t *out, const clopts_program_t *program,
                                  const clopts_command_t *command)
{
    const clopts_scope_t *scope =
        (command != NULL) ? clopts_command_get_scope(command) : clopts_program_get_scope(program);
    size_t count;
    size_t i;

    clopts_out_str(out, "Usage: ");
    clopts_out_str(out, clopts_program_get_name(program));
    if (command != NULL)
    {
        clopts_out_char(out, ' ');
        clopts_out_str(out, clopts_command_get_name(command));
    }
    else if (has_subcommands(program))
    {
        clopts_out_str(out, " [<options>] <command> [<arguments>]\n");
        return;
    }
    clopts_out_str(out, " [<options>]");
    count = clopts_scope_get_positional_count(scope);
    for (i = 0; i < count; i++)
    {
        clopts_out_char(out, ' ');
        write_positional_usage(out, clopts_scope_get_positional_at(scope, i));
    }
    clopts_out_char(out, '\n');
}

/* Writes a paragraph of one line of text, after a blank line. */
static void write_text_paragraph(clopts_out_t *out, const char *text)
{
    clopts_out_char(out, '\n');
    clopts_out_str(out, text);
    clopts_out_char(out, '\n');
}

/* Writes a list paragraph with its heading, after a blank line, when the list has rows. */
static void write_list_paragraph(clopts_out_t *out, const char *heading, const list_t *list)
{
    if (list_count(list) == 0)
    {
        return;
    }
    clopts_out_char(out, '\n');
    clopts_out_str(out, heading);
    clopts_out_char(out, '\n');
    write_rows(out, list);
}

/* Sets list to the declared rows of kind in scope (or program), with no fixed row. */
static void list_init(list_t *list, source_kind_t kind, const clopts_program_t *program,
                           const clopts_scope_t *scope, const clopts_scope_t *shadow)
{
    list->kind = kind;
    list->program = program;
    list->scope = scope;
    list->shadow = shadow;
}

/* Writes the Arguments: paragraph of scope, if it has positionals (2.3). */
static void write_arguments(clopts_out_t *out, const clopts_scope_t *scope)
{
    list_t list;

    list_init(&list, TEXT_SOURCE_POSITIONALS, NULL, scope, NULL);
    write_list_paragraph(out, "Arguments:", &list);
}

/* Writes the Options: paragraph of scope, if it has options (1.4, 2.4, 3.3). */
static void write_options(clopts_out_t *out, const clopts_scope_t *scope)
{
    list_t list;

    list_init(&list, TEXT_SOURCE_OPTIONS, NULL, scope, NULL);
    write_list_paragraph(out, "Options:", &list);
}

/* Writes the help of a program with subcommands (1.1 to 1.6). */
static void write_global_help(clopts_out_t *out, const clopts_program_t *program)
{
    const clopts_scope_t *global = clopts_program_get_scope(program);
    const char *description = clopts_program_get_description(program);
    list_t commands;

    write_usage_line(out, program, NULL);
    if (has_text(description))
    {
        write_text_paragraph(out, description);
    }

    list_init(&commands, TEXT_SOURCE_COMMANDS, program, NULL, NULL);
    write_list_paragraph(out, "Commands:", &commands);

    write_options(out, global);

    /* Only when the program has a global --help to point to. */
    if (has_long(global, "help"))
    {
        clopts_out_str(out, "\nRun '");
        clopts_out_str(out, clopts_program_get_name(program));
        clopts_out_str(out, " <command> --help' for help on a command.\n");
    }
}

/* Writes the help of a subcommand (2.1 to 2.6). */
static void write_command_help(clopts_out_t *out, const clopts_program_t *program,
                                    const clopts_command_t *command)
{
    const char *description = clopts_command_get_description(command);
    const char *summary = clopts_command_get_summary(command);
    list_t globals;

    write_usage_line(out, program, command);
    if (has_text(description))
    {
        write_text_paragraph(out, description);
    }
    else if (has_text(summary))
    {
        write_text_paragraph(out, summary);
    }
    write_arguments(out, clopts_command_get_scope(command));
    write_options(out, clopts_command_get_scope(command));

    list_init(&globals, TEXT_SOURCE_OPTIONS, NULL, clopts_program_get_scope(program),
                   clopts_command_get_scope(command));
    write_list_paragraph(out, "Global Options:", &globals);
}

/* Writes the help of a program without subcommands (3.1 to 3.3). */
static void write_single_help(clopts_out_t *out, const clopts_program_t *program)
{
    const char *description = clopts_program_get_description(program);

    write_usage_line(out, program, NULL);
    if (has_text(description))
    {
        write_text_paragraph(out, description);
    }
    write_arguments(out, clopts_program_get_scope(program));
    write_options(out, clopts_program_get_scope(program));
}

/* ---- entry points ------------------------------------------------------ */

void clopts_help_write(clopts_out_t *out, const clopts_program_t *program, const clopts_command_t *command)
{
    assert((out != NULL) && (program != NULL));

    if (command != NULL)
    {
        write_command_help(out, program, command);
    }
    else if (has_subcommands(program))
    {
        write_global_help(out, program);
    }
    else
    {
        write_single_help(out, program);
    }
}

void clopts_help_write_usage(clopts_out_t *out, const clopts_program_t *program, const clopts_command_t *command)
{
    assert((out != NULL) && (program != NULL));
    write_usage_line(out, program, command);
}
