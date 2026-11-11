#include "domain/rle.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lifegame::domain
{

namespace
{

constexpr int kMaxWidth = kMaxBoardWidth;
constexpr int kMaxHeight = kMaxBoardHeight;

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
    {
        text.remove_suffix(1);
    }
    return text;
}

RleErrorInfo makeError(RleError code, std::string_view source, std::size_t offset)
{
    RleErrorInfo info;
    info.code = code;
    info.position = offset;

    const std::size_t limit = std::min(offset, source.size());
    for (std::size_t i = 0; i < limit; ++i)
    {
        if (source[i] == '\n')
        {
            ++info.line;
            info.column = 1;
        }
        else
        {
            ++info.column;
        }
    }
    return info;
}

struct BodyLine
{
    std::size_t bodyStart = 0;
    std::size_t sourceStart = 0;
};

std::size_t sourceOffsetForBody(const std::vector<BodyLine>& lines, std::size_t bodyIndex)
{
    if (lines.empty())
    {
        return 0;
    }
    const auto it = std::upper_bound(lines.begin(),
                                     lines.end(),
                                     bodyIndex,
                                     [](std::size_t index, const BodyLine& line) {
                                         return index < line.bodyStart;
                                     });
    if (it == lines.begin())
    {
        return lines.front().sourceStart;
    }
    const BodyLine& line = *(it - 1);
    return line.sourceStart + (bodyIndex - line.bodyStart);
}

bool startsWithHeaderKey(std::string_view line)
{
    return !line.empty() && (line.front() == 'x' || line.front() == 'X') &&
           line.find('=') != std::string_view::npos;
}

bool parseNonNegativeInt(std::string_view text, int* value)
{
    if (text.empty())
    {
        return false;
    }

    int parsed = 0;
    const char* begin = text.data();
    const char* end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed < 0 || parsed > 1'000'000)
    {
        return false;
    }
    *value = parsed;
    return true;
}

struct Header
{
    int width = 0;
    int height = 0;
    bool hasWidth = false;
    bool hasHeight = false;
    std::string rule;
};

RleErrorInfo parseHeader(std::string_view line, Header* header)
{
    std::size_t start = 0;
    while (start <= line.size())
    {
        const std::size_t comma = line.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? line.size() : comma;
        const std::string_view token = trim(line.substr(start, end - start));

        if (!token.empty())
        {
            const std::size_t equals = token.find('=');
            if (equals == std::string_view::npos)
            {
                return {RleError::InvalidHeader, start};
            }

            const std::string_view key = trim(token.substr(0, equals));
            const std::string_view value = trim(token.substr(equals + 1));

            if (key == "x" || key == "X")
            {
                if (!parseNonNegativeInt(value, &header->width))
                {
                    return {RleError::InvalidHeader, start};
                }
                header->hasWidth = true;
            }
            else if (key == "y" || key == "Y")
            {
                if (!parseNonNegativeInt(value, &header->height))
                {
                    return {RleError::InvalidHeader, start};
                }
                header->hasHeight = true;
            }
            else if (key == "rule" || key == "Rule")
            {
                if (!value.empty())
                {
                    const RuleParseResult parsed = Rule::parse(value);
                    if (parsed.error != RuleParseError::None)
                    {
                        return {RleError::InvalidRule, start};
                    }
                    header->rule = parsed.rule.notation();
                }
            }
        }

        if (comma == std::string_view::npos)
        {
            break;
        }
        start = comma + 1;
    }

    return {};
}

std::size_t appendUnsigned(std::string* output, std::size_t value)
{
    char buffer[32];
    const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    const std::size_t digits = static_cast<std::size_t>(result.ptr - buffer);
    output->append(buffer, digits);
    return digits;
}

std::size_t appendRun(std::string* output, int count, char tag)
{
    if (count <= 0)
    {
        return 0;
    }
    std::size_t length = 1;
    if (count > 1)
    {
        length += appendUnsigned(output, static_cast<std::size_t>(count));
    }
    output->push_back(tag);
    return length;
}

void wrapLine(std::string* output, std::size_t* lineLength)
{
    if (*lineLength >= 70)
    {
        output->push_back('\n');
        *lineLength = 0;
    }
}

std::string sanitizeTitle(std::string_view title)
{
    std::string result(title);
    for (char& ch : result)
    {
        if (ch == '\n' || ch == '\r')
        {
            ch = ' ';
        }
    }
    return result;
}

} // namespace

RleDecodeResult decodeRle(std::string_view text)
{
    Header header;
    std::string title;
    std::string description;
    std::string body;
    std::vector<BodyLine> bodyLines;
    bool sawHeader = false;

    std::size_t lineStart = 0;
    while (lineStart <= text.size())
    {
        const std::size_t lineEnd = text.find('\n', lineStart);
        const std::size_t end = lineEnd == std::string_view::npos ? text.size() : lineEnd;
        std::string_view line = text.substr(lineStart, end - lineStart);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }

        const std::string_view clean = trim(line);
        if (!clean.empty())
        {
            if (clean.front() == '#')
            {
                if (clean.size() >= 3 && (clean[1] == 'N' || clean[1] == 'n') && clean[2] == ' ')
                {
                    title = std::string(trim(clean.substr(3)));
                }
                else if (clean.size() >= 3 && (clean[1] == 'C' || clean[1] == 'c') && clean[2] == ' ')
                {
                    if (!description.empty())
                    {
                        description.push_back(' ');
                    }
                    description += std::string(trim(clean.substr(3)));
                }
            }
            else if (!sawHeader && startsWithHeaderKey(clean))
            {
                sawHeader = true;
                const RleErrorInfo error = parseHeader(clean, &header);
                if (error.code != RleError::None)
                {
                    const std::size_t cleanOffset =
                        static_cast<std::size_t>(clean.data() - text.data());
                    return {std::nullopt, makeError(error.code, text, cleanOffset + error.position)};
                }
            }
            else
            {
                const std::size_t cleanOffset =
                    static_cast<std::size_t>(clean.data() - text.data());
                const std::size_t bodyStart = body.size();
                body.append(clean);
                body.push_back('\n');
                bodyLines.push_back({bodyStart, cleanOffset});
            }
        }

        if (lineEnd == std::string_view::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }

    if (body.empty())
    {
        return {std::nullopt, makeError(RleError::EmptyBody, text, 0)};
    }

    if (header.hasWidth && header.width > kMaxWidth)
    {
        return {std::nullopt, makeError(RleError::TooLarge, text, 0)};
    }
    if (header.hasHeight && header.height > kMaxHeight)
    {
        return {std::nullopt, makeError(RleError::TooLarge, text, 0)};
    }

    std::vector<std::vector<Cell>> rows(1);
    std::int64_t row = 0;
    std::int64_t col = 0;
    std::int64_t run = 0;
    std::int64_t maxCol = 0;
    bool finished = false;

    for (std::size_t i = 0; i < body.size() && !finished; ++i)
    {
        const char ch = body[i];
        if (ch >= '0' && ch <= '9')
        {
            run = run * 10 + (ch - '0');
            if (run > static_cast<std::int64_t>(kMaxWidth) * kMaxHeight)
            {
                return {std::nullopt, makeError(RleError::RunTooLong, text, sourceOffsetForBody(bodyLines, i))};
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0)
        {
            continue;
        }

        const std::int64_t count = run > 0 ? run : 1;
        run = 0;

        switch (ch)
        {
        case 'b':
        case 'B':
            if (col + count > kMaxWidth)
            {
                return {std::nullopt, makeError(RleError::TooLarge, text, sourceOffsetForBody(bodyLines, i))};
            }
            col += count;
            maxCol = std::max(maxCol, col);
            break;

        case 'o':
        case 'O':
        {
            if (row >= kMaxHeight || col + count > kMaxWidth)
            {
                return {std::nullopt, makeError(RleError::TooLarge, text, sourceOffsetForBody(bodyLines, i))};
            }
            if (static_cast<std::int64_t>(rows.size()) <= row)
            {
                rows.resize(static_cast<std::size_t>(row) + 1);
            }
            std::vector<Cell>& target = rows[static_cast<std::size_t>(row)];
            if (static_cast<std::int64_t>(target.size()) < col + count)
            {
                target.resize(static_cast<std::size_t>(col + count), kDead);
            }
            for (std::int64_t offset = 0; offset < count; ++offset)
            {
                target[static_cast<std::size_t>(col + offset)] = kAlive;
            }
            col += count;
            maxCol = std::max(maxCol, col);
            break;
        }

        case '$':
            row += count;
            col = 0;
            if (row >= kMaxHeight)
            {
                return {std::nullopt, makeError(RleError::TooLarge, text, sourceOffsetForBody(bodyLines, i))};
            }
            if (static_cast<std::int64_t>(rows.size()) <= row)
            {
                rows.resize(static_cast<std::size_t>(row) + 1);
            }
            break;

        case '!':
            finished = true;
            break;

        default:
            return {std::nullopt, makeError(RleError::InvalidCharacter, text, sourceOffsetForBody(bodyLines, i))};
        }
    }

    if (!finished)
    {
        const std::size_t offset =
            body.empty() ? 0 : sourceOffsetForBody(bodyLines, body.size() - 1);
        return {std::nullopt, makeError(RleError::MissingTerminator, text, offset)};
    }

    const int contentWidth = std::max(1, static_cast<int>(maxCol));
    const int contentHeight = std::max(1, static_cast<int>(rows.size()));
    const int width = std::max(header.hasWidth ? header.width : 0, contentWidth);
    const int height = std::max(header.hasHeight ? header.height : 0, contentHeight);

    if (width > kMaxWidth || height > kMaxHeight)
    {
        return {std::nullopt, makeError(RleError::TooLarge, text, 0)};
    }

    DecodedDocument document;
    document.title = std::move(title);
    document.description = std::move(description);
    document.rule = std::move(header.rule);

    std::vector<Cell> cells(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kDead);
    const int copyRows = std::min(height, static_cast<int>(rows.size()));
    for (int r = 0; r < copyRows; ++r)
    {
        const int copyCols = std::min(width, static_cast<int>(rows[static_cast<std::size_t>(r)].size()));
        for (int c = 0; c < copyCols; ++c)
        {
            cells[static_cast<std::size_t>(r) * static_cast<std::size_t>(width) + static_cast<std::size_t>(c)] =
                rows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
        }
    }
    std::optional<Pattern> pattern =
        Pattern::fromCells("rle", width, height, std::move(cells));
    if (!pattern.has_value())
    {
        return {std::nullopt, makeError(RleError::InvalidHeader, text, 0)};
    }
    document.pattern = std::move(*pattern);

    return {std::move(document), {}};
}

std::string encodeRle(const Grid& board, const Rule& rule, std::string_view title)
{
    const int width = board.width();
    const int height = board.height();
    if (width <= 0 || height <= 0)
    {
        return "!";
    }

    int lastLiveRow = -1;
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            if (board.cell(row, col))
            {
                lastLiveRow = row;
                break;
            }
        }
    }

    std::string output;
    output.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) / 2u + 256u);
    if (!title.empty())
    {
        output += "#N ";
        output += sanitizeTitle(title);
        output.push_back('\n');
    }
    output += "x = ";
    appendUnsigned(&output, static_cast<std::size_t>(width));
    output += ", y = ";
    appendUnsigned(&output, static_cast<std::size_t>(height));
    output += ", rule = ";
    output += rule.notation();
    output.push_back('\n');

    std::size_t lineLength = 0;
    for (int row = 0; row <= lastLiveRow; ++row)
    {
        int lastInRow = -1;
        for (int col = 0; col < width; ++col)
        {
            if (board.cell(row, col))
            {
                lastInRow = col;
            }
        }

        int col = 0;
        while (col <= lastInRow)
        {
            const bool alive = board.cell(row, col);
            int run = 1;
            while (col + run <= lastInRow && board.cell(row, col + run) == alive)
            {
                ++run;
            }

            wrapLine(&output, &lineLength);
            lineLength += appendRun(&output, run, alive ? 'o' : 'b');
            col += run;
        }

        if (row < lastLiveRow)
        {
            output.push_back('$');
            ++lineLength;
        }
    }

    output.push_back('!');
    output.push_back('\n');
    return output;
}

} // namespace lifegame::domain
