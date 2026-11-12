#pragma once

/**
 * @file rle.h
 * @brief Strict bounded RLE codec and typed errors.
 */

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "domain/grid.h"
#include "domain/pattern.h"
#include "domain/rule.h"

namespace lifegame::domain
{

/// @brief Stable RLE error codes; the domain never returns user-facing prose.
enum class RleError
{
    None = 0,           ///< No error.
    EmptyBody,          ///< The body carries no cells.
    InvalidHeader,      ///< The x/y or rule header is malformed.
    InvalidCharacter,   ///< A character outside the RLE alphabet.
    RunTooLong,         ///< A run count is out of range.
    TooLarge,           ///< The decoded area or the input exceeds the size limits.
    InvalidRule,        ///< The rule header is not valid B/S notation.
    MissingTerminator   ///< The body ends without the '!' terminator.
};

/// @brief Stable spelling of an error code for logs.
///
/// @param error Error code.
/// @return Static name; "Unknown" for a value outside the enumeration.
constexpr const char* rleErrorName(RleError error) noexcept
{
    switch (error)
    {
    case RleError::None:
        return "None";
    case RleError::EmptyBody:
        return "EmptyBody";
    case RleError::InvalidHeader:
        return "InvalidHeader";
    case RleError::InvalidCharacter:
        return "InvalidCharacter";
    case RleError::RunTooLong:
        return "RunTooLong";
    case RleError::TooLarge:
        return "TooLarge";
    case RleError::InvalidRule:
        return "InvalidRule";
    case RleError::MissingTerminator:
        return "MissingTerminator";
    }
    return "Unknown";
}

/// @brief Machine-readable RLE failure with source location.
struct RleErrorInfo
{
    RleError code = RleError::None; ///< Stable error code.
    std::size_t position = 0;       ///< Original byte offset (0-based).
    int line = 1;                   ///< 1-based source line.
    int column = 1;                 ///< 1-based source column.
};

/// @brief Successfully decoded RLE document.
struct DecodedDocument
{
    Pattern pattern;          ///< Decoded cells and dimensions.
    std::string title;        ///< Optional `#N` header.
    std::string description;  ///< Optional `#C` header (first line).
    std::string rule;         ///< Canonical B/S notation, empty when absent.
};

/// @brief Result of decodeRle(): a document or a typed error.
struct RleDecodeResult
{
    std::optional<DecodedDocument> document; ///< Decoded document on success.
    RleErrorInfo error;                      ///< Error details on failure.
};

/**
 * @brief Decode RLE text.
 *
 * The parser is intentionally strict and bounded: a malformed or hostile file
 * can never make the column/row counters overflow or make the decoder write
 * outside its buffers.
 *
 * @param text RLE source text.
 * @return Decoded document or a typed error with line/column.
 */
RleDecodeResult decodeRle(std::string_view text);

/**
 * @brief Encode a board as RLE.
 * @param board Board to encode.
 * @param rule Rule written into the header.
 * @param title Optional `#N` title.
 * @return Complete RLE text with header and terminating '!'.
 */
std::string encodeRle(const Grid& board, const Rule& rule, std::string_view title = {});

} // namespace lifegame::domain
