#pragma once

/**
 * @file pattern_library.h
 * @brief 内置图案目录（只含 ID 与形状）。
 */

#include <string_view>
#include <vector>

#include "domain/pattern.h"

namespace lifegame::domain
{

/// @brief 内置图案（只有稳定 ID 与形状）。
///
/// 显示名称与描述属于视图层。
/// @return 按显示顺序排列的内置图案目录。
const std::vector<Pattern>& builtinPatterns();

/// @brief 按 ID 查找内置图案。
/// @param id 稳定的图案标识。
/// @return 指向内置目录的指针；找不到时返回 nullptr。
const Pattern* findBuiltinPattern(std::string_view id);

} // namespace lifegame::domain
