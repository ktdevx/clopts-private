#include <naos/naos.h>

NAOS_DECLARE_SUITE(out);
NAOS_DECLARE_SUITE(layout);
NAOS_DECLARE_SUITE(option_row);
NAOS_DECLARE_SUITE(message);

NAOS_MAIN(
    NAOS_ADD_SUITE(out),
    NAOS_ADD_SUITE(layout),
    NAOS_ADD_SUITE(option_row),
    NAOS_ADD_SUITE(message)
)
