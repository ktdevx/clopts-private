/*
 * HelpLayout: help, Usage line, version line and usage hint.
 *
 * Lists are written in two passes over the same row procedure: the left
 * columns are measured on a measure-mode output to find the widest one, then
 * every row is written and padded to it.
 *
 * Skeleton: every public function returns CLOPTS_WRITE_UNAVAILABLE without
 * calling write_fn.
 */

#include "help/help_internal.h"

#include <assert.h>

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
    return help_has_text(option->value_name) ? option->value_name : "VALUE";
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
    bool has_long = help_has_text(option->long_name);

    if (option->short_name != '\0')
    {
        clopts_help_out_char(out, '-');
        clopts_help_out_char(out, option->short_name);
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
        if ((option->kind == CLOPTS_PARSER_FLAG) && option->negatable)
        {
            clopts_help_out_str(out, "[no-]");
        }
        clopts_help_out_str(out, option->long_name);
    }

    switch (option->kind)
    {
    case CLOPTS_PARSER_COUNT:
        clopts_help_out_str(out, "...");
        break;
    case CLOPTS_PARSER_VALUE:
        clopts_help_out_char(out, ' ');
        help_write_value(out, option);
        break;
    case CLOPTS_PARSER_LIST:
        clopts_help_out_char(out, ' ');
        help_write_value(out, option);
        if (option->delimiter != '\0')
        {
            clopts_help_out_char(out, '[');
            clopts_help_out_char(out, option->delimiter);
            help_write_value(out, option);
            clopts_help_out_str(out, "...]");
        }
        else
        {
            clopts_help_out_str(out, "...");
        }
        break;
    case CLOPTS_PARSER_FLAG:
    default:
        break;
    }
}

/* Writes the left column of row. */
static void help_write_row_left(clopts_help_out_t *out, const clopts_help_row_t *row)
{
    if (row->option != NULL)
    {
        help_write_option_left(out, row->option);
    }
    else if (row->left != NULL)
    {
        clopts_help_out_str(out, row->left);
    }
}

/* Display width of the left column of row, measured by help_write_row_left. */
static size_t help_row_left_width(const clopts_help_row_t *row)
{
    clopts_help_out_t measure;

    clopts_help_out_init(&measure, NULL, NULL);
    help_write_row_left(&measure, row);
    return measure.width;
}

/* Description of row; NULL or "" for none. */
static const char *help_row_help(const clopts_help_row_t *row)
{
    return (row->option != NULL) ? row->option->help : row->help;
}

/* The row has a description or a note (4.7 to 4.11). */
static bool help_row_has_rest(const clopts_help_row_t *row)
{
    const clopts_option_t *option = row->option;

    if (help_has_text(help_row_help(row)))
    {
        return true;
    }
    return (option != NULL) &&
           (option->required || help_has_text(option->default_value) || help_has_text(option->env));
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

/* Writes the description and the notes of row, in the fixed order (4.12). */
static void help_write_row_rest(clopts_help_out_t *out, const clopts_help_row_t *row)
{
    const clopts_option_t *option = row->option;
    const char *help = help_row_help(row);
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
    if (option->required)
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "(required)");
    }
    if (help_has_text(option->default_value))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "(default: ");
        clopts_help_out_str(out, option->default_value);
        clopts_help_out_char(out, ')');
    }
    if (help_has_text(option->env))
    {
        help_write_separator(out, &first);
        clopts_help_out_str(out, "[env: ");
        clopts_help_out_str(out, option->env);
        clopts_help_out_char(out, ']');
    }
}

void clopts_help_write_list(clopts_help_out_t *out, const clopts_help_row_t *rows, size_t count)
{
    size_t max_width = 0;
    size_t i;

    assert(out != NULL);
    assert((count == 0) || (rows != NULL));

    /* Pass 1: the widest left column, measured. */
    for (i = 0; i < count; i++)
    {
        size_t width = help_row_left_width(&rows[i]);

        if (width > max_width)
        {
            max_width = width;
        }
    }

    /* Pass 2: the rows, written and padded to max_width + HELP_LIST_GAP. */
    for (i = 0; i < count; i++)
    {
        const clopts_help_row_t *row = &rows[i];

        clopts_help_out_spaces(out, HELP_LIST_INDENT);
        help_write_row_left(out, row);
        if (help_row_has_rest(row))
        {
            clopts_help_out_spaces(out, (max_width - help_row_left_width(row)) + HELP_LIST_GAP);
            help_write_row_rest(out, row);
        }
        clopts_help_out_char(out, '\n');
    }
}

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_usage(const clopts_program_t *program, const clopts_command_t *command,
                                         clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_version(const clopts_program_t *program, clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}

clopts_write_status_t clopts_write_usage_hint(const clopts_program_t *program, const clopts_command_t *command,
                                              clopts_write_fn write_fn, void *ctx)
{
    (void)program;
    (void)command;
    (void)write_fn;
    (void)ctx;
    return CLOPTS_WRITE_UNAVAILABLE;
}
