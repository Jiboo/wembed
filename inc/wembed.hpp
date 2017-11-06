#pragma once

//#define WEMBED_VERBOSE

/**
 * FAST_MATH will break conformance with webassembly spec, but will use native
 * floating points operations. Probably safe if you don't care about +/-NaN,
 * +/-0, and the significand bits of a NaN.
 */
//#define WEMBED_FAST_MATH

/**
 * Used to prefix exported name with "__wexport_", so that we avoid collision with
 * libc/m reserved names, see https://bugs.llvm.org/show_bug.cgi?id=34941
 */
//#define WEMBED_PREFIX_EXPORTED_FUNC

#include "wembed/module.hpp"
#include "wembed/context.hpp"
