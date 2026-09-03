#ifndef UTIL_MATH_H_
#define UTIL_MATH_H_

#include <stdint.h>

/*
 * Floor division: like q = a / b, but rounds toward negative infinity
 * instead of toward zero. Assumes b > 0.
 *
 * Standard C '/' truncates toward zero, which is wrong for converting
 * world block coordinates to chunk coordinates when coordinates can be
 * negative - e.g. floorDiv(-1, 16) must be -1, not 0.
 */
static inline int32_t floorDiv(int32_t a, int32_t b)
{
    int32_t q = a / b;
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        q -= 1;
    return q;
}

/*
 * Floor modulo: like r = a % b, but always returns a result in
 * [0, b), matching floorDiv (a == floorDiv(a,b)*b + floorMod(a,b)).
 * Assumes b > 0.
 */
static inline int32_t floorMod(int32_t a, int32_t b)
{
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0)))
        r += b;
    return r;
}

#endif /* UTIL_MATH_H_ */
