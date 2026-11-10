/* Good: OS-specific definitions count too. */
void clopts_set_allocator(const void *allocator)
{
    (void)allocator;
}

const void *clopts_allocator_get(void)
{
    return 0;
}
