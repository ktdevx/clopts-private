#include <naos/naos.h>

NAOS_DECLARE_SUITE(alloc);
NAOS_DECLARE_SUITE(size);
NAOS_DECLARE_SUITE(vec);
NAOS_DECLARE_SUITE(writer);

NAOS_MAIN(
    NAOS_ADD_SUITE(alloc),
    NAOS_ADD_SUITE(size),
    NAOS_ADD_SUITE(vec),
    NAOS_ADD_SUITE(writer)
)
