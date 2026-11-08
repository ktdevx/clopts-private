/*
 * HelpLayout: help, Usage line, version line and usage hint.
 *
 * Lists are written in two passes over the same row procedure: the left
 * columns are measured on a measure-mode output to find the widest one, then
 * every row is written and padded to it. The rows of a list are read in
 * place from the declaration (no allocation).
 *
 * clopts_write_usage, clopts_write_version and clopts_write_usage_hint are
 * still skeletons: they return CLOPTS_WRITE_UNAVAILABLE without calling
 * write_fn.
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

/* What the elements of the vec of a list are. */
typedef enum help_vec_kind
{
    HELP_VEC_OPTIONS,      /* clopts_option_t * */
    HELP_VEC_POSITIONALS,  /* clopts_positional_t * */
    HELP_VEC_COMMANDS      /* clopts_command_t *: name and summary as a fixed row */
} help_vec_kind_t;

/*
 * The rows of one list, read in place without allocating: the elements of
 * vec in order (vec NULL: none), then the row_count rows at rows.
 */
typedef struct help_list
{
    const clopts_core_vec_t *vec;
    help_vec_kind_t kind;
    const clopts_help_row_t *rows;
    size_t row_count;
} help_list_t;

/* Number of elements of the vec of list. */
static size_t help_list_vec_len(const help_list_t *list)
{
    return (list->vec != NULL) ? list->vec->len : 0;
}

/* Number of rows of list. */
static size_t help_list_count(const help_list_t *list)
{
    return help_list_vec_len(list) + list->row_count;
}

/* Row index of list; requires index < help_list_count(list). */
static help_item_t help_list_item(const help_list_t *list, size_t index)
{
    help_item_t item = {NULL, NULL, NULL, NULL};
    size_t vec_len = help_list_vec_len(list);

    if (index < vec_len)
    {
        const void *elem = clopts_core_vec_at(list->vec, index);

        switch (list->kind)
        {
        case HELP_VEC_OPTIONS:
            item.option = *(const clopts_option_t *const *)elem;
            break;
        case HELP_VEC_POSITIONALS:
            item.positional = *(const clopts_positional_t *const *)elem;
            break;
        case HELP_VEC_COMMANDS:
        default:
        {
            const clopts_command_t *command = *(const clopts_command_t *const *)elem;

            item.left = command->name;
            item.help = command->summary;
            break;
        }
        }
    }
    else
    {
        const clopts_help_row_t *row = &list->rows[index - vec_len];

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
        clopts_help_out_str(out, item->positional->name);
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
        return item->option->help;
    }
    if (item->positional != NULL)
    {
        return item->positional->help;
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

    list.vec = NULL;
    list.kind = HELP_VEC_OPTIONS;
    list.rows = rows;
    list.row_count = count;
    help_write_rows(out, &list);
}

/* ---- paragraphs -------------------------------------------------------- */

/* Reserved rows and their fixed descriptions. */
static const clopts_help_row_t help_row_help_command = {NULL, "help", "Show help for a command"};
static const clopts_help_row_t help_row_help_option = {NULL, "-h, --help", "Show this help"};
static const clopts_help_row_t help_row_version = {NULL, "    --version", "Show version information"};

/* The program has subcommands. */
static bool help_has_subcommands(const clopts_program_t *program)
{
    return program->commands.len > 0;
}

/* Writes the notation of positional in the Usage line (5.2). */
static void help_write_positional_usage(clopts_help_out_t *out, const clopts_positional_t *positional)
{
    if (!positional->required)
    {
        clopts_help_out_char(out, '[');
    }
    clopts_help_out_char(out, '<');
    clopts_help_out_str(out, positional->name);
    clopts_help_out_char(out, '>');
    if (positional->variadic)
    {
        clopts_help_out_str(out, "...");
    }
    if (!positional->required)
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
    size_t i;

    clopts_help_out_str(out, "Usage: ");
    clopts_help_out_str(out, program->root.name);
    if (command != &program->root)
    {
        clopts_help_out_char(out, ' ');
        clopts_help_out_str(out, command->name);
    }
    else if (help_has_subcommands(program))
    {
        clopts_help_out_str(out, " [<options>] <command> [<arguments>]\n");
        return;
    }
    clopts_help_out_str(out, " [<options>]");
    for (i = 0; i < command->positionals.len; i++)
    {
        clopts_help_out_char(out, ' ');
        help_write_positional_usage(out,
                                    *(const clopts_positional_t *const *)clopts_core_vec_at(&command->positionals, i));
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

/* Writes a list paragraph with its heading, after a blank line. */
static void help_write_list_paragraph(clopts_help_out_t *out, const char *heading, const help_list_t *list)
{
    clopts_help_out_char(out, '\n');
    clopts_help_out_str(out, heading);
    clopts_help_out_char(out, '\n');
    help_write_rows(out, list);
}

/* Writes the Arguments: paragraph of command, if it has positionals (2.3). */
static void help_write_arguments(clopts_help_out_t *out, const clopts_command_t *command)
{
    help_list_t list;

    if (command->positionals.len == 0)
    {
        return;
    }
    list.vec = &command->positionals;
    list.kind = HELP_VEC_POSITIONALS;
    list.rows = NULL;
    list.row_count = 0;
    help_write_list_paragraph(out, "Arguments:", &list);
}

/*
 * Writes the Options: paragraph: the options of command, -h, --help and,
 * with with_version, --version (1.4, 1.5, 2.4, 3.3).
 */
static void help_write_options(clopts_help_out_t *out, const clopts_command_t *command, bool with_version)
{
    clopts_help_row_t rows[2];
    help_list_t list;

    rows[0] = help_row_help_option;
    rows[1] = help_row_version;
    list.vec = &command->options;
    list.kind = HELP_VEC_OPTIONS;
    list.rows = rows;
    list.row_count = with_version ? 2 : 1;
    help_write_list_paragraph(out, "Options:", &list);
}

/* Writes the help of a program with subcommands (1.1 to 1.6, 3.4). */
static void help_write_global_help(clopts_help_out_t *out, const clopts_program_t *program)
{
    const clopts_command_t *root = &program->root;
    help_list_t commands;

    help_write_usage_line(out, program, root);
    if (help_has_text(root->description))
    {
        help_write_text_paragraph(out, root->description);
    }

    commands.vec = &program->commands;
    commands.kind = HELP_VEC_COMMANDS;
    commands.rows = &help_row_help_command;
    commands.row_count = 1;
    help_write_list_paragraph(out, "Commands:", &commands);

    help_write_options(out, root, program->version != NULL);

    clopts_help_out_str(out, "\nRun '");
    clopts_help_out_str(out, root->name);
    clopts_help_out_str(out, " <command> --help' for help on a command.\n");
}

/* Writes the help of a subcommand (2.1 to 2.6). */
static void help_write_command_help(clopts_help_out_t *out, const clopts_program_t *program,
                                    const clopts_command_t *command)
{
    const clopts_command_t *root = &program->root;
    help_list_t globals;

    help_write_usage_line(out, program, command);
    if (help_has_text(command->description))
    {
        help_write_text_paragraph(out, command->description);
    }
    else if (help_has_text(command->summary))
    {
        help_write_text_paragraph(out, command->summary);
    }
    help_write_arguments(out, command);
    help_write_options(out, command, false);

    globals.vec = &root->options;
    globals.kind = HELP_VEC_OPTIONS;
    globals.rows = &help_row_version;
    globals.row_count = (program->version != NULL) ? 1 : 0;
    if (help_list_count(&globals) > 0)
    {
        help_write_list_paragraph(out, "Global Options:", &globals);
    }
}

/* Writes the help of a program without subcommands (3.1 to 3.3). */
static void help_write_single_help(clopts_help_out_t *out, const clopts_program_t *program)
{
    const clopts_command_t *root = &program->root;

    help_write_usage_line(out, program, root);
    if (help_has_text(root->description))
    {
        help_write_text_paragraph(out, root->description);
    }
    help_write_arguments(out, root);
    help_write_options(out, root, program->version != NULL);
}

/* ---- public functions -------------------------------------------------- */

clopts_write_status_t clopts_write_help(const clopts_program_t *program, const clopts_command_t *command,
                                        clopts_write_fn write_fn, void *ctx)
{
    clopts_help_out_t out;

    assert(program != NULL);
    assert(write_fn != NULL);
    assert((command == NULL) || (command->program == program));

    if (command == NULL)
    {
        command = &program->root;
    }

    clopts_help_out_init(&out, write_fn, ctx);
    if (command != &program->root)
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
