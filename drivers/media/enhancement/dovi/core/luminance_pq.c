/* Convert decoder luminance into a saturated 12-bit PQ code. */
#include "luminance_pq.h"
#include "pq12_tables.h"
#include "pq12_impl.h"

typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_u64[(sizeof(unsigned long long) == 8) ? 1 : -1];

unsigned int dovi_luminance_to_pq12(unsigned int value)
{
    return linear_to_pq12(value, dovi_pq12_positions, dovi_pq12_slopes, dovi_pq12_intercepts,
                          4095u);
}
