/**
 * @file AABB.c
 * @ingroup AABB
 * @defgroup AABB
 * @author Michael Fitzmayer
 * @copyright "THE BEER-WARE LICENCE" (Revision 42)
 */

#include <stdbool.h>
#include "AABB.h"

bool BoxesAreIntersecting(const AABB stBoxA, const AABB stBoxB)
{
    double dAx = stBoxB.dLeft - stBoxA.dRight;
    double dAy = stBoxB.dTop  - stBoxA.dBottom;
    double dBx = stBoxA.dLeft - stBoxB.dRight;
    double dBy = stBoxA.dTop  - stBoxB.dBottom;

    if (dAx > 0.0 || dAy > 0.0) return 0;
    if (dBx > 0.0 || dBy > 0.0) return 0;

    return 1;
}
