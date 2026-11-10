/* Good: a struct and its operations in one header. */
typedef struct clopts_vec
{
    int len;
} clopts_vec_t;

int clopts_vec_push(clopts_vec_t *v);
int clopts_vec(void);
static int not_checked(void);
