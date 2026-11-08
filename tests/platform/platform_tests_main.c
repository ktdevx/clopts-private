#include <naos/naos.h>

NAOS_DECLARE_SUITE(split);
NAOS_DECLARE_SUITE(argv);
NAOS_DECLARE_SUITE(env);
NAOS_DECLARE_SUITE(oom);

#ifdef _WIN32
/* Windows-only suites; their files are built only on Windows (see CMakeLists.txt). */
NAOS_DECLARE_SUITE(convert);
NAOS_DECLARE_SUITE(crt);

NAOS_MAIN(
    NAOS_ADD_SUITE(split),
    NAOS_ADD_SUITE(argv),
    NAOS_ADD_SUITE(env),
    NAOS_ADD_SUITE(oom),
    NAOS_ADD_SUITE(convert),
    NAOS_ADD_SUITE(crt)
)
#else
NAOS_MAIN(
    NAOS_ADD_SUITE(split),
    NAOS_ADD_SUITE(argv),
    NAOS_ADD_SUITE(env),
    NAOS_ADD_SUITE(oom)
)
#endif
