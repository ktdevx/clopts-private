/*
 * Valid C99 that only triggers warnings: an unused variable and a comparison
 * between signed and unsigned integers. Builds with plain warnings, fails
 * when warnings are errors.
 */
int main(int argc, char **argv)
{
    unsigned int limit = 2u;
    int unused = 0;

    (void)argv;
    if (argc < limit)
    {
        return 0;
    }
    return 0;
}
