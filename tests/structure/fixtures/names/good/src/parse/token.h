/* Good: an operation of an enumeration stays with it, in a header of another name. */
typedef enum clopts_token_kind
{
    CLOPTS_TOKEN_WORD
} clopts_token_kind_t;

typedef struct clopts_token
{
    clopts_token_kind_t kind;
} clopts_token_t;

const char *clopts_token_kind_name(clopts_token_kind_t kind);
int clopts_token_is_word(const clopts_token_t *token);
