#pragma once

/**
 * @file pattern_library.h
 * @brief Built-in pattern catalog (IDs and shapes only).
 */

#include <string_view>
#include <vector>

#include "domain/pattern.h"

namespace lifegame::domain
{

/// @brief Built-in patterns (stable IDs and shapes only).
///
/// Display names and descriptions live in the view layer.
///
/// @return Built-in pattern catalog in display order.
const std::vector<Pattern>& builtinPatterns();

/**
 * @brief Find a built-in pattern by ID.
 * @param id Stable pattern identifier.
 * @return Pointer into the built-in catalog, or nullptr when missing.
 */
const Pattern* findBuiltinPattern(std::string_view id);

} // namespace lifegame::domain
