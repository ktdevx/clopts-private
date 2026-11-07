/*
 * Warning-free C99 apart from the variable length array below, which the
 * CLOpts compiler options must reject.
 */
int main(int argc, char **argv)
{
    int n = argc + 1;
    int buf[n];
    int i;

    (void)argv;
    for (i = 0; i < n; ++i)
    {
        buf[i] = i;
    }
    return buf[n - 1] - (n - 1);
}
