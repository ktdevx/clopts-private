/*
 * Copyright (c) 2026 The CLOpts Authors
 * SPDX-License-Identifier: MIT
 *
 * HelpLayout: help, Usage line, version line and usage hint.
 *
 * Lists are written in two passes over the same row procedure: the left
 * columns are measured on a measure-mode output to find the widest one, then
 * every row is written and padded to it. The rows of a list are read in
 * place from the declaration (no allocation), through the public read
 * functions only.
 *
 * Nothing here is reserved: --help, --version and the help command are
 * ordinary options and commands, listed like any other one. They are created
 * by the helpers at the end of this file.
 */

#include "help/help_internal.h"

#include <assert.h>
#include <string.h>

/* ---- lists ------------------------------------------------------------- */

/* Indentation of every row of a list. */
#define HELP_LIST_INDENT 2
/* Gap between the widest left column of a list and the descriptions. */
#define HELP_LIST_GAP 2

/* A string "has text": it is neither NULL nor empty. */
static bool help_has_text(const char *s)
{
    return (s != NULL) && (s[0] != '\0');
}

/* Value name of option: its declared one, or VALUE when it has no text. */
static const char *help_value_name(const clopts_option_t *option)
{
    return help_has_text(clopts_option_value_name(option)) ? clopts_option_value_name(option) : "VALUE";
}

/* Writes "<V>" for option. */
static void help_write_value(clopts_help_out_t *out, const clopts_option_t *option)
{
    clopts_help_out_char(out, '<');
    clopts_help_out_str(out, help_value_name(option));
    clopts_help_out_char(out, '>');
}

/*
 * Writes the left column of option (4.1 to 4.6). The only procedure that
 * produces it: run on a measure-mode output it gives the width, run on a
 * write-mode output it writes the same bytes.
 */
static void help_write_option_left(clopts_help_out_t *out, const clopts_option_t *option)
{
    bool has_long = help_has_text(clopts_option_long_name(option));

    if (clopts_option_short_name(option) != '\0')
    {
        clopts_help_out_char(out, '-');
        clopts_help_out_char(out, clopts_option_short_name(option));
        if (has_long)
        {
            clopts_help_out_str(out, ", ");
        }
    }
    else
    {
        clopts_help_out_spaces(out, 4);
    }
    if (has_long)
    {
        clopts_help_out_str(out, "--");
        if ((clopts_option_kind_of(option) == CLOPTS_OPTION_FLAG) && clopts_option_negatable(option))
        {
            clopts_help_out_str(out, "[no-]");
        }
        clopts_help_out_str(out, clopts_option_long_name(option));
    }

    switch (clopts_option_kind_of(option))
    {
    case CLOPTS_OPTION_COUNT:
        clopts_help_out_str(out, "...");
        break;
    case CLOPTS_OPTION_VALUE:
        clopts_help_out_char(out, ' ');
        help_write_value(out, option);
        break;
    case CLOPTS_OPTION_LIST:
        clopts_help_out_char(out, ' ');
        help_write_value(out, option);
        if (clopts_option_delimiter(option) != '\0')
        {
            clopts_help_out_char(out, '[');
            clopts_help_out_char(out, clopts_option_delimiter(option));
            help_write_value(out, option);
            clopts_help_out_str(out, "...]");
        }
        else
        {
            clopts_help_out_str(out, "...");
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
typedef struct help_item
{
    const clopts_option_t *option;
    const clopts_positional_t *positional;
    const char *left;
    const char *help;
} help_item_t;

/* Where the declared rows of a list come from. */
typedef enum help_source_kind
{
    HELP_SOURCE_NONE,         /* no declared rows */
    HELP_SOURCE_OPTIONS,      /* the options of command, except those shadowed by shadow (when not NULL) */
    HELP_SOURCE_POSITIONALS,  /* the positionals of command */
    HELP_SOURCE_COMMANDS      /* the subcommands of program: name and summary as a fixed row */
} help_source_kind_t;

/*
 * The rows of one list, read in place without allocating: the declared rows
 * of the source in order, then the row_count rows at rows.
 */
typedef struct help_list
{
    help_source_kind_t kind;
    const clopts_program_t *program;
    const clopts_command_t *command;
    const clopts_command_t *shadow;
    const clopts_help_row_t *rows;
    size_t row_count;
} help_list_t;

/* True when command has an option with the long name l (exact). */
static bool help_has_long(const clopts_command_t *command, const char *l)
{
    size_t count = clopts_command_option_count(command);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const char *name = clopts_option_long_name(clopts_command_option_at(command, i));

        if ((name != NULL) && (strcmp(name, l) == 0))
        {
            return true;
        }
    }
    return false;
}

/* True when the long name l is taken in command: exactly, or as --no-<name> of a negatable FLAG. */
static bool help_long_taken(const clopts_command_t *command, const char *l)
{
    size_t count;
    size_t i;

    if (help_has_long(command, l))
    {
        return true;
    }
    if (strncmp(l, "no-", 3) != 0)
    {
        return false;
    }
    count = clopts_command_option_count(command);
    for (i = 0; i < count; i++)
    {
        const clopts_option_t *option = clopts_command_option_at(command, i);
        const char *name = clopts_option_long_name(option);

        if ((name != NULL) && clopts_option_negatable(option) && (strcmp(name, l + 3) == 0))
        {
            return true;
        }
    }
    return false;
}

/* True when the short name c is taken in command. */
static bool help_short_taken(const clopts_command_t *command, char c)
{
    size_t count = clopts_command_option_count(command);
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (clopts_option_short_name(clopts_command_option_at(command, i)) == c)
        {
            return true;
        }
    }
    return false;
}

/*
 * True when --no-<l> is taken in command: by an option named "no-<l>", or as
 * the negation of a negatable FLAG named l.
 */
static bool help_negation_taken(const clopts_command_t *command, const char *l)
{
    size_t count = clopts_command_option_count(command);
    size_t i;

    for (i = 0; i < count; i++)
    {
        const clopts_option_t *option = clopts_command_option_at(command, i);
        const char *name = clopts_option_long_name(option);

        if (name == NULL)
        {
            continue;
        }
        if ((strncmp(name, "no-", 3) == 0) && (strcmp(name + 3, l) == 0))
        {
            return true;
        }
        if (clopts_option_negatable(option) && (strcmp(name, l) == 0))
        {
            return true;
        }
    }
    return false;
}

/*
 * True when the global option option is shadowed in command: every form it
 * can be given in (its long name, --no-<long> when it is negatable, its short
 * name) is found in command first, so it cannot be given there.
 */
static bool help_shadowed(const clopts_option_t *option, const clopts_command_t *command)
{
    const char *l = clopts_option_long_name(option);
    const char s = clopts_option_short_name(option);

    if ((l != NULL) && !help_long_taken(command, l))
    {
        return false;
    }
    if ((l != NULL) && clopts_option_negatable(option) && !help_negation_taken(command, l))
    {
        return false;
    }
    if ((s != '\0') && !help_short_taken(command, s))
    {
        return false;
    }
    return true;
}

/* Number of declared elements of the source of list, before any filter. */
static size_t help_source_len(const help_list_t *list)
{
    switch (list->kind)
    {
    case HELP_SOURCE_OPTIONS:
        return clopts_command_option_count(list->command);
    case HELP_SOURCE_POSITIONALS:
        return clopts_command_positional_count(list->command);
    case HELP_SOURCE_COMMANDS:
        return clopts_program_command_count(list->program);
    case HELP_SOURCE_NONE:
    default:
        return 0;
    }
}

/* True when the element index of the source of list is listed. */
static bool help_source_listed(const help_list_t *list, size_t index)
{
    if ((list->kind != HELP_SOURCE_OPTIONS) || (list->shadow == NULL))
    {
        return true;
    }
    return !help_shadowed(clopts_command_option_at(list->command, index), list->shadow);
}

/* Number of declared rows of list. */
static size_t help_list_declared(const help_list_t *list)
{
    size_t len = help_source_len(list);
    size_t count = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (help_source_listed(list, i))
        {
            count++;
        }
    }
    return count;
}

/* Number of rows of list. */
static size_t help_list_count(const help_list_t *list)
{
    return help_list_declared(list) + list->row_count;
}

/* The item of element index of the source of list. */
static help_item_t help_source_item(const help_list_t *list, size_t index)
{
    help_item_t item = {NULL, NULL, NULL, NULL};

    switch (list->kind)
    {
    case HELP_SOURCE_OPTIONS:
        item.option = clopts_command_option_at(list->command, index);
        break;
    case HELP_SOURCE_POSITIONALS:
        item.positional = clopts_command_positional_at(list->command, index);
        break;
    case HELP_SOURCE_COMMANDS:
    {
        const clopts_command_t *command = clopts_program_command_at(list->program, index);

        item.left = clopts_command_name(command);
        item.help = clopts_command_summary(command);
        break;
    }
    case HELP_SOURCE_NONE:
    default:
        break;
    }
    return item;
}

/* Row index of list; requires index < help_list_count(list). */
static help_item_t help_list_item(const help_list_t *list, size_t index)
{
    help_item_t item = {NULL, NULL, NULL, NULL};
    size_t len = help_source_len(list);
    size_t listed = 0;
    size_t i;

    for (i = 0; i < len; i++)
    {
        if (help_source_listed(list, i))
        {
            if (listed == index)
            {
                return help_source_item(list, i);
            }
            listed++;
        }
    }
    {
        const clopts_help_row_t *row = &list->rows[index - listed];

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
static void help_write_item_left(clopts_help_out_t *out, const help_item_t *item)
{
    if (item->option != NULL)
    {
        help_write_option_left(out, item->option);
    }
    else if (item->positional != NULL)
    {
        clopts_help_out_char(out, '<');
        clopts_help_out_str(out, clopts_positional_name(item->positional));
        clopts_help_out_char(out, '>');
    }
    else if (item->left != NULL)
    {
        clopts_help_out_str(out, item->left);
    }
}

/* Display width of the left column of item, measured by help_write_item_left. */
static size_t help_item_left_width(const help_item_t *item)
{
    clopts_help_out_t measure;

    clopts_help_out_init(&measure, NULL, NULL);
    help_write_item_left(&measure, item);
    return measure.width;
}

/* Description of item; NULL or "" for none. */
static const char *help_item_help(const help_item_t *item)
{
    if (item->option != NULL)
    {
        return clopts_option_help(item->option);
    }
    if (item->positional != NULL)
    {
        return clopts_positional_help(item->positional);
    }
    return item->help;
}

/* The item has a description or a note (4.7 to 4.11). */
static bool help_item_has_rest(const help_item_t *item)
{
    const clopts_option_t *option = item->option;

    if (help_has_text(help_item_help(item)))
    {
        return true;
    }
    return (option != NULL) &&
           (clopts_option_required(option) || help_has_text(clopts_option_default(option)) || help_has_text(clopts_option_env(option)));
}

/* Writes a separating space unless nothing has been written yet. */
static void help_write_separator(clopts_help_out_t *out, bool *first)
{
    if (!*first)
    {
        clopts_help_out_char(out, ' ');
    }
    *first = false;
}

/* Writes the description and the notes of item, in the fixed order (4.12). */
static void help_write_item_rest(clopts_help_out_t *out, const help_item_t *item)
{
    const clopts_option_t *option = item->option;
    const char *help = help_item_help(item);
    bool first = true;

    if (help_has_text(help))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, help);
    }
    if (option == NULL)
    {
        return;
    }
    if (clopts_option_required(option))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "(required)");
    }
    if (help_has_text(clopts_option_default(option)))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "(default: ");
        clopts_help_out_str(out, clopts_option_default(option));
        clopts_help_out_char(out, ')');
    }
    if (help_has_text(clopts_option_env(option)))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "[env: ");
        clopts_help_out_str(out, clopts_option_env(option));
        clopts_help_out_char(out, ']');
    }
}

/* Writes the rows of list as one aligned list (10.1). */
static void help_write_rows(clopts_help_out_t *out, const help_list_t *list)
{
    size_t count = help_list_count(list);
    size_t max_width = 0;
    size_t i;

    /* Pass 1: the widest left column, measured. */
    for (i = 0; i < count; i++)
    {
        help_item_t item = help_list_item(list, i);
        size_t width = help_item_left_width(&item);

        if (width > max_width)
        {
            max_width = width;
        }
    }

    /* Pass 2: the rows, written and padded to max_width + HELP_LIST_GAP. */
    for (i = 0; i < count; i++)
    {
        help_item_t item = help_list_item(list, i);

        clopts_help_out_spaces(out, HELP_LIST_INDENT);
        help_write_item_left(out, &item);
        if (help_item_has_rest(&item))
        {
            clopts_help_out_spaces(out, (max_width - help_item_left_width(&item)) + HELP_LIST_GAP);
            help_write_item_rest(out, &item);
        }
        clopts_help_out_char(out, '\n');
    }
}

void clopts_help_write_list(clopts_help_out_t *out, const clopts_help_row_t *rows, size_t count)
{
    help_list_t list;

    assert(out != NULL);
    assert((count == 0) || (rows != NULL));

    list.kind = HELP_SOURCE_NONE;
    list.program = NULL;
    list.command = NULL;
    list.shadow = NULL;
    list.rows = rows;
    list.row_count = count;
    help_write_rows(out, &list);
}

/* ---- paragraphs -------------------------------------------------------- */

/* The program has subcommands. */
static bool help_has_subcommands(const clopts_program_t *program)
{
    return clopts_program_command_count(program) > 0;
}

/* Writes the notation of positional in the Usage line (5.2). */
static void help_write_positional_usage(clopts_help_out_t *out, const clopts_positional_t *positional)
{
    const bool required = clopts_positional_required(positional);

    if (!required)
    {
        clopts_help_out_char(out, '[');
    }
    clopts_help_out_char(out, '<');
    clopts_help_out_str(out, clopts_positional_name(positional));
    clopts_help_out_char(out, '>');
    if (clopts_positional_variadic(positional))
    {
        clopts_help_out_str(out, "...");
    }
    if (!required)
    {
        clopts_help_out_char(out, ']');
    }
}

/*
 * Writes the Usage line of command (the root for the program) and its '\n'
 * (5.1, 5.2, 5.4).
 */
static void help_write_usage_line(clopts_help_out_t *out, const clopts_program_t *program,
                                  const clopts_command_t *command)
{
    const clopts_command_t *root = clopts_program_root(program);
    size_t count;
    size_t i;

    clopts_help_out_str(out, "Usage: ");
    clopts_help_out_str(out, clopts_command_name(root));
    if (command != root)
    {
        clopts_help_out_char(out, ' ');
        clopts_help_out_str(out, clopts_command_name(command));
    }
    else if (help_has_subcommands(program))
    {
        clopts_help_out_str(out, " [<options>] <command> [<arguments>]\n");
        return;
    }
    clopts_help_out_str(out, " [<options>]");
    count = clopts_command_positional_count(command);
    for (i = 0; i < count; i++)
    {
        clopts_help_out_char(out, ' ');
        help_write_positional_usage(out, clopts_command_positional_at(command, i));
    }
    clopts_help_out_char(out, '\n');
}

/* Writes a paragraph of one line of text, after a blank line. */
static void help_write_text_paragraph(clopts_help_out_t *out, const char *text)
{
    clopts_help_out_char(out, '\n');
    clopts_help_out_str(out, text);
    clopts_help_out_char(out, '\n');
}

/* Writes a list paragraph with its heading, after a blank line, when the list has rows. */
static void help_write_list_paragraph(clopts_help_out_t *out, const char *heading, const help_list_t *list)
{
    if (help_list_count(list) == 0)
    {
        return;
    }
    clopts_help_out_char(out, '\n');
    clopts_help_out_str(out, heading);
    clopts_help_out_char(out, '\n');
    help_write_rows(out, list);
}

/* Sets list to the declared rows of kind, with no fixed row. */
static void help_list_init(help_list_t *list, help_source_kind_t kind, const clopts_program_t *program,
                           const clopts_command_t *command, const clopts_command_t *shadow)
{
    list->kind = kind;
    list->program = program;
    list->command = command;
    list->shadow = shadow;
    list->rows = NULL;
    list->row_count = 0;
}

/* Writes the Arguments: paragraph of command, if it has positionals (2.3). */
static void help_write_arguments(clopts_help_out_t *out, const clopts_command_t *command)
{
    help_list_t list;

    help_list_init(&list, HELP_SOURCE_POSITIONALS, NULL, command, NULL);
    help_write_list_paragraph(out, "Arguments:", &list);
}

/* Writes the Options: paragraph of command, if it has options (1.4, 2.4, 3.3). */
static void help_write_options(clopts_help_out_t *out, const clopts_command_t *command)
{
    help_list_t list;

    help_list_init(&list, HELP_SOURCE_OPTIONS, NULL, command, NULL);
    help_write_list_paragraph(out, "Options:", &list);
}

/* Writes the help of a program with subcommands (1.1 to 1.6). */
static void help_write_global_help(clopts_help_out_t *out, const clopts_program_t *program)
{
    const clopts_command_t *root = clopts_program_root(program);
    const char *description = clopts_command_description(root);
    help_list_t commands;

    help_write_usage_line(out, program, root);
    if (help_has_text(description))
    {
        help_write_text_paragraph(out, description);
    }

    help_list_init(&commands, HELP_SOURCE_COMMANDS, program, NULL, NULL);
    help_write_list_paragraph(out, "Commands:", &commands);

    help_write_options(out, root);

    /* Only when the program has a global --help to point to. */
    if (help_has_long(root, "help"))
    {
        clopts_help_out_str(out, "\nRun '");
        clopts_help_out_str(out, clopts_command_name(root));
        clopts_help_out_str(out, " <command> --help' for help on a command.\n");
    }
}

/* Writes the help of a subcommand (2.1 to 2.6). */
static void help_write_command_help(clopts_help_out_t *out, const clopts_program_t *program,
                                    const clopts_command_t *command)
{
    const char *description = clopts_command_description(command);
    const char *summary = clopts_command_summary(command);
    help_list_t globals;

    help_write_usage_line(out, program, command);
    if (help_has_text(description))
    {
        help_write_text_paragraph(out, description);
    }
    else if (help_has_text(summary))
    {
        help_write_text_paragraph(out, summary);
    }
    help_write_arguments(out, command);
    help_write_options(out, command);

    help_list_init(&globals, HELP_SOURCE_OPTIONS, NULL, clopts_program_root(program), command);
    help_write_list_paragraph(out, "Global Options:", &globals);
}

/* Writes the help of a program without subcommands (3.1 to 3.3). */
static void help_write_single_help(clopts_help_out_t *out, const clopts_program_t *program)
{
    const clopts_command_t *root = clopts_program_root(program);
    const char *description = clopts_command_description(root);

    help_write_usage_line(out, program, root);
    if (help_has_text(description))
    {
        help_write_text_paragraph(out, description);
    }
    help_write_arguments(out, root);
    help_write_options(out, root);
}

/* ---- public functions -------------------------------------------------- */

bool clopts_help_has_command(const clopts_program_t *program, const clopts_command_t *command)
{
    size_t count = clopts_program_command_count(program);
    size_t i;

    if (command == clopts_program_root(program))
    {
        return true;
    }
    for (i = 0; i < count; i++)
    {
        if (clopts_program_command_at(program, i) == command)
        {
            return true;
        }
    }
    return false;
}

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || clopts_help_has_command(program, command));

    if (command == NULL)
    {
        command = clopts_program_root(program);
    }

    clopts_help_out_init(&out, write_fn, ctx);
    if (command != clopts_program_root(program))
    {
        help_write_command_help(&out, program, command);
    }
    else if (help_has_subcommands(program))
    {
        help_write_global_help(&out, program);
    }
    else
    {
        help_write_single_help(&out, program);
    }
    return clopts_help_out_status(&out);
}

clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                         clopts_write_fn write_fn, void *ctx)
{
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || clopts_help_has_command(program, command));

    if (command == NULL)
    {
        command = clopts_program_root(program);
    }

    clopts_help_out_init(&out, write_fn, ctx);
    help_write_usage_line(&out, program, command);
    return clopts_help_out_status(&out);
}

clopts_write_status_t clopts_write_version(const clopts_program_t *program, const char *version,
                                           clopts_write_fn write_fn, void *ctx)
{
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);

    if (version == NULL)
    {
        return CLOPTS_WRITE_UNAVAILABLE;
    }

    clopts_help_out_init(&out, write_fn, ctx);
    clopts_help_out_str(&out, clopts_command_name(clopts_program_root(program)));
    clopts_help_out_char(&out, ' ');
    clopts_help_out_str(&out, version);
    clopts_help_out_char(&out, '\n');
    return clopts_help_out_status(&out);
}

clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program, const clopts_command_t *command,
                                              clopts_write_fn write_fn, void *ctx)
{
    const clopts_command_t *root;
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || clopts_help_has_command(program, command));

    root = clopts_program_root(program);
    clopts_help_out_init(&out, write_fn, ctx);
    clopts_help_out_str(&out, "Run '");
    clopts_help_out_str(&out, clopts_command_name(root));
    if ((command != NULL) && (command != root))
    {
        clopts_help_out_char(&out, ' ');
        clopts_help_out_str(&out, clopts_command_name(command));
    }
    clopts_help_out_str(&out, " --help' for usage.\n");
    return clopts_help_out_status(&out);
}

/* ---- declarations for the help texts ----------------------------------- */

/*
 * Creates a terminal FLAG named l / s with help and attaches it to command.
 * *out (when out is not NULL) gets it, or NULL on failure.
 */
static clopts_decl_error_t help_add_terminal(clopts_command_t *command, const char *l, char s, const char *help,
                                             const clopts_option_t **out)
{
    clopts_option_t *option = NULL;
    clopts_decl_error_t error;

    if (out != NULL)
    {
        *out = NULL;
    }
    assert(command != NULL);
    error = clopts_option_create(CLOPTS_OPTION_FLAG, l, s, &option);
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_option_set_terminal(option, true);
    }
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_option_set_help(option, help);
    }
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_command_add_option(command, option);
    }
    if (error != CLOPTS_DECL_NONE)
    {
        clopts_option_destroy(option);
        return error;
    }
    if (out != NULL)
    {
        *out = option;
    }
    return CLOPTS_DECL_NONE;
}

clopts_decl_error_t clopts_help_add_help_option(clopts_command_t *command, const clopts_option_t **out)
{
    return help_add_terminal(command, "help", 'h', "Show this help", out);
}

clopts_decl_error_t clopts_help_add_version_option(clopts_command_t *command, const clopts_option_t **out)
{
    return help_add_terminal(command, "version", '\0', "Show version information", out);
}

clopts_decl_error_t clopts_help_command_create(clopts_command_t **out)
{
    clopts_command_t *command = NULL;
    clopts_positional_t *positional = NULL;
    clopts_decl_error_t error;

    assert(out != NULL);
    *out = NULL;
    error = clopts_command_create("help", &command);
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_command_set_summary(command, "Show help for a command");
    }
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_positional_create("COMMAND", &positional);
    }
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_positional_set_help(positional, "Command to show help for");
    }
    if (error == CLOPTS_DECL_NONE)
    {
        error = clopts_command_add_positional(command, positional);
        if (error == CLOPTS_DECL_NONE)
        {
            positional = NULL;
        }
    }
    if (error != CLOPTS_DECL_NONE)
    {
        clopts_positional_destroy(positional);
        clopts_command_destroy(command);
        return error;
    }
    *out = command;
    return CLOPTS_DECL_NONE;
}

bool clopts_help_target(const clopts_program_t *program, const clopts_result_t *result,
                        const clopts_command_t **target)
{
    const char *name;

    assert(program != NULL);
    assert(target != NULL);

    *target = NULL;
    if (clopts_result_status(result) != CLOPTS_OK)
    {
        return false;
    }
    if (clopts_result_positional_count(result) == 0)
    {
        return true;
    }
    name = clopts_result_positional_at(result, 0);
    *target = clopts_program_find_command(program, name);
    return *target != NULL;
}
