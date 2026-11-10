/* Good: a struct and its operations; the enumeration next to it is not a struct. */
typedef enum clopts_token_kind
{
    CLOPTS_TOKEN_WORD
} clopts_token_kind_t;

typedef struct clopts_token
{
    clopts_token_kind_t kind;
} clopts_token_t;

int clopts_token_is_word(const clopts_token_t *token);
