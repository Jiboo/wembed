#pragma once

//#define VERBOSE

/**
 * FAST_MATH will break conformance with webassembly spec, but will use native
 * floating points operations. Probably safe if you don't care about +/-NaN,
 * +/-0, and the significand bits of a NaN.
 */
//#define FAST_MATH


#include "wembed/module.hpp"
#include "wembed/context.hpp"
