#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "domain/grid.h"
#include "domain/history.h"
#include "domain/pattern_library.h"
#include "domain/rle.h"
#include "domain/rule.h"
#include "shared/app_settings.h"

namespace
{

int g_failures = 0;
int g_checks = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_checks;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #condition << std::endl;                       \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (false)

using Cells = std::vector<std::pair<int, int>>;

Cells liveCells(const lifegame::domain::Grid& board)
{
    Cells cells;
    for (int row = 0; row < board.height(); ++row)
    {
        for (int col = 0; col < board.width(); ++col)
        {
            if (board.cell(row, col))
            {
                cells.emplace_back(row, col);
            }
        }
    }
    return cells;
}

void placePattern(lifegame::domain::Grid* board,
                  const lifegame::domain::Pattern& pattern,
                  int topRow,
                  int topCol)
{
    for (int row = 0; row < pattern.height(); ++row)
    {
        for (int col = 0; col < pattern.width(); ++col)
        {
            if (pattern.alive(row, col))
            {
                board->setCell(topRow + row, topCol + col, true);
            }
        }
    }
}

static_assert(lifegame::domain::Rule().births(3));
static_assert(!lifegame::domain::Rule().births(2));
static_assert(lifegame::domain::Rule().survives(3));
static_assert(lifegame::domain::Rule(1u << 6, 1u << 2).births(6));
static_assert(!lifegame::domain::Rule(1u << 6, 1u << 2).survives(6));
static_assert(std::is_nothrow_move_constructible_v<lifegame::domain::Grid>);
static_assert(std::is_nothrow_move_assignable_v<lifegame::domain::Grid>);
static_assert(std::is_nothrow_move_constructible_v<lifegame::domain::Rule>);

constexpr lifegame::shared::AppSettings kClampedSettings =
    lifegame::shared::normalized(lifegame::shared::AppSettings{2000, 100, 5, 1, 500, true});
static_assert(kClampedSettings.jumpStep == 1000);
static_assert(kClampedSettings.sizeStep == 50);
static_assert(kClampedSettings.snapshotInterval == 10);
static_assert(kClampedSettings.density == 5);
static_assert(kClampedSettings.speed == 120);

void testRules()
{
    using lifegame::domain::Rule;
    using lifegame::domain::RuleParseError;

    const auto conway = Rule::parse("B3/S23");
    CHECK(conway.error == RuleParseError::None);
    CHECK(conway.rule.births(3));
    CHECK(!conway.rule.births(2));
    CHECK(conway.rule.survives(2));
    CHECK(conway.rule.survives(3));
    CHECK(!conway.rule.survives(4));
    CHECK(conway.rule.notation() == "B3/S23");

    const auto legacy = Rule::parse(" 3 / 23 ");
    CHECK(legacy.error == RuleParseError::None);
    CHECK(legacy.rule.notation() == "B3/S23");

    const auto lowercase = Rule::parse("b36/s23");
    CHECK(lowercase.error == RuleParseError::None);
    CHECK(lowercase.rule.notation() == "B36/S23");

    const auto emptySurvival = Rule::parse("B2/S");
    CHECK(emptySurvival.error == RuleParseError::None);
    CHECK(emptySurvival.rule.notation() == "B2/S");

    const auto emptyBirth = Rule::parse("B/S23");
    CHECK(emptyBirth.error == RuleParseError::None);
    CHECK(emptyBirth.rule.notation() == "B/S23");

    CHECK(Rule::parse("not-a-rule").error == RuleParseError::MissingSeparator);
    CHECK(Rule::parse("B9/S23").error == RuleParseError::InvalidCharacter);
    CHECK(Rule::parse("B3/S23/x").error == RuleParseError::InvalidCharacter);
    CHECK(Rule::parse("").error == RuleParseError::Empty);

    const auto& presets = lifegame::domain::builtinRulePresets();
    CHECK(presets.size() == 5);
    CHECK(lifegame::domain::findRulePreset("conway") != nullptr);
    CHECK(lifegame::domain::findRulePreset("missing") == nullptr);
    const auto* highlife = lifegame::domain::findRulePreset("highlife");
    CHECK(highlife != nullptr);
    if (highlife)
    {
        CHECK(highlife->rule.notation() == "B36/S23");
        CHECK(lifegame::domain::findRulePresetByNotation("B36/S23") == highlife);
    }
}

void testGridBasics()
{
    using lifegame::domain::Grid;

    Grid empty;
    CHECK(empty.width() == 0);
    CHECK(empty.height() == 0);
    CHECK(empty.population() == 0);
    CHECK(!empty.cell(0, 0));

    Grid clamped(1501, 1001);
    CHECK(clamped.width() == Grid::kMaxWidth);
    CHECK(clamped.height() == Grid::kMaxHeight);
    CHECK(clamped.activeCellStorage() == static_cast<std::size_t>(Grid::kMaxCells));

    Grid movedFrom(10, 8);
    Grid moved(std::move(movedFrom));
    movedFrom.resize(10, 8);
    movedFrom.setCell(0, 0, true);
    CHECK(movedFrom.cell(0, 0));
    CHECK(moved.width() == 10);
    CHECK(moved.height() == 8);

    Grid board(10, 8);
    CHECK(board.width() == 10);
    CHECK(board.height() == 8);
    CHECK(board.inBounds(0, 0));
    CHECK(board.inBounds(7, 9));
    CHECK(!board.inBounds(8, 0));
    CHECK(!board.inBounds(-1, 0));

    board.setCell(1, 2, true);
    CHECK(board.cell(1, 2));
    CHECK(board.population() == 1);
    CHECK(board.toggleCell(1, 2) == std::optional<bool>{false});
    CHECK(board.population() == 0);
    CHECK(board.toggleCell(1, 2) == std::optional<bool>{true});
    CHECK(board.population() == 1);

    CHECK(!board.toggleCell(100, 100).has_value());
    board.setCell(100, 100, true);
    CHECK(board.population() == 1);

    // Same-size resize is a no-op and keeps cells; a real resize clears.
    board.resize(10, 8);
    CHECK(board.population() == 1);
    board.resize(12, 8);
    CHECK(board.population() == 0);
    board.resize(12, 8);
    CHECK(board.population() == 0);

    board.clear();
    CHECK(board.population() == 0);
}

void testStillLifeAndOscillators()
{
    using lifegame::domain::Boundary;
    using lifegame::domain::Grid;
    using lifegame::domain::Rule;

    Grid block(6, 6);
    block.setCell(2, 2, true);
    block.setCell(2, 3, true);
    block.setCell(3, 2, true);
    block.setCell(3, 3, true);
    const Cells before = liveCells(block);
    for (int i = 0; i < 10; ++i)
    {
        block.advance(Rule(), Boundary::Dead);
    }
    CHECK(block.population() == 4);
    CHECK(liveCells(block) == before);

    Grid blinker(7, 7);
    blinker.setCell(3, 2, true);
    blinker.setCell(3, 3, true);
    blinker.setCell(3, 4, true);
    blinker.advance(Rule(), Boundary::Dead);
    CHECK(blinker.cell(2, 3));
    CHECK(blinker.cell(3, 3));
    CHECK(blinker.cell(4, 3));
    CHECK(blinker.population() == 3);
    blinker.advance(Rule(), Boundary::Dead);
    CHECK(blinker.cell(3, 2));
    CHECK(blinker.cell(3, 3));
    CHECK(blinker.cell(3, 4));
}

void testGliderAndWrap()
{
    using lifegame::domain::Boundary;
    using lifegame::domain::Grid;
    using lifegame::domain::Rule;

    const auto* glider = lifegame::domain::findBuiltinPattern("glider");
    CHECK(glider != nullptr);
    if (glider == nullptr)
    {
        return;
    }

    Grid board(12, 12);
    placePattern(&board, *glider, 2, 2);
    CHECK(board.population() == 5);
    for (int i = 0; i < 4; ++i)
    {
        board.advance(Rule(), Boundary::Dead);
    }

    Grid expected(12, 12);
    placePattern(&expected, *glider, 3, 3);
    CHECK(board.population() == 5);
    CHECK(liveCells(board) == liveCells(expected));

    Grid wrap(5, 5);
    wrap.setCell(4, 4, true);
    CHECK(wrap.neighbourCount(0, 0, Boundary::Dead) == 0);
    CHECK(wrap.neighbourCount(0, 0, Boundary::Wrap) == 1);

    Grid edge(5, 5);
    edge.setCell(0, 0, true);
    edge.setCell(0, 1, true);
    edge.setCell(0, 2, true);
    edge.advance(Rule(), Boundary::Wrap);
    CHECK(edge.cell(0, 1));
    CHECK(edge.cell(4, 1));
    CHECK(edge.cell(1, 1));
}

void testRandomize()
{
    using lifegame::domain::Grid;

    Grid board(20, 15);
    board.randomize(0, 123);
    CHECK(board.population() == 0);
    board.randomize(100, 123);
    CHECK(board.population() == 20 * 15);

    board.randomize(30, 123);
    const int first = board.population();
    CHECK(first > 0);
    CHECK(first < 20 * 15);

    Grid sameSeed(20, 15);
    sameSeed.randomize(30, 123);
    CHECK(board.equals(sameSeed));

    Grid otherSeed(20, 15);
    otherSeed.randomize(30, 124);
    CHECK(!board.equals(otherSeed));
}

void testRowSlicing()
{
    using lifegame::domain::Boundary;
    using lifegame::domain::Grid;
    using lifegame::domain::Rule;

    Grid source(320, 240);
    source.randomize(35, 424242);

    for (const Boundary boundary : {Boundary::Dead, Boundary::Wrap})
    {
        Grid serial = source;
        for (int generation = 0; generation < 3; ++generation)
        {
            serial.advance(Rule(), boundary);
        }

        Grid sliced = source;
        for (int generation = 0; generation < 3; ++generation)
        {
            Grid next;
            next.resize(320, 240);
            for (int begin = 0; begin < 240; begin += 37)
            {
                Grid::computeRows(sliced, &next, Rule(), boundary, begin, std::min(240, begin + 37));
            }
            sliced = std::move(next);
        }
        CHECK(sliced.equals(serial));
    }
}

void testPatternLibrary()
{
    const auto& patterns = lifegame::domain::builtinPatterns();
    CHECK(patterns.size() == 13);

    std::set<std::string> ids;
    for (const auto& pattern : patterns)
    {
        CHECK(!pattern.id().empty());
        CHECK(pattern.width() > 0);
        CHECK(pattern.height() > 0);
        CHECK(pattern.cells().size() ==
              static_cast<std::size_t>(pattern.width()) * static_cast<std::size_t>(pattern.height()));
        CHECK(pattern.population() > 0);
        ids.insert(pattern.id());
    }
    CHECK(ids.size() == patterns.size());

    CHECK(lifegame::domain::findBuiltinPattern("glider") != nullptr);
    CHECK(lifegame::domain::findBuiltinPattern("gosper_gun") != nullptr);
    CHECK(lifegame::domain::findBuiltinPattern("missing") == nullptr);

    if (const auto* gun = lifegame::domain::findBuiltinPattern("gosper_gun"))
    {
        CHECK(gun->width() == 36);
        CHECK(gun->height() == 9);
        CHECK(gun->population() == 36);
    }
    if (const auto* pulsar = lifegame::domain::findBuiltinPattern("pulsar"))
    {
        CHECK(pulsar->population() == 48);
    }

    const auto padded = lifegame::domain::Pattern::fromAscii("padded", {"O", "OOO"});
    CHECK(padded.width() == 3);
    CHECK(padded.height() == 2);

    CHECK(!lifegame::domain::Pattern::fromCells("bad", 2, 2, {1}).has_value());
    CHECK(lifegame::domain::Pattern::fromCells("ok", 2, 2, {1, 0, 0, 1}).has_value());
    CHECK(padded.alive(0, 0));
    CHECK(!padded.alive(0, 1));
    CHECK(padded.population() == 4);
}

lifegame::domain::RleDecodeResult decode(const std::string& text)
{
    return lifegame::domain::decodeRle(text);
}

void testPatternInvariant()
{
    using lifegame::domain::Pattern;

    const auto broken =
        Pattern::fromCells("broken", 10, 10, std::vector<lifegame::domain::Cell>(1, 1));
    CHECK(!broken.has_value());

    const auto consistent =
        Pattern::fromCells("consistent", 10, 10, std::vector<lifegame::domain::Cell>(100, 1));
    CHECK(consistent.has_value());
    if (consistent)
    {
        CHECK(consistent->isConsistent());
        CHECK(consistent->alive(9, 9));
    }
}

void testRleDecode()
{
    using lifegame::domain::RleError;

    const auto glider = decode("#N Glider\n#C test\nx = 3, y = 3, rule = B3/S23\nbob$2bo$3o!\n");
    CHECK(glider.error.code == RleError::None);
    CHECK(glider.document.has_value());
    if (glider.document)
    {
        CHECK(glider.document->title == "Glider");
        CHECK(glider.document->description == "test");
        CHECK(glider.document->rule == "B3/S23");
        CHECK(glider.document->pattern.width() == 3);
        CHECK(glider.document->pattern.height() == 3);
        CHECK(glider.document->pattern.population() == 5);
        CHECK(glider.document->pattern.alive(0, 1));
        CHECK(glider.document->pattern.alive(2, 0));
    }

    const auto noHeader = decode("bob$2bo$3o!");
    CHECK(noHeader.error.code == RleError::None);
    CHECK(noHeader.document.has_value());
    if (noHeader.document)
    {
        CHECK(noHeader.document->pattern.width() == 3);
        CHECK(noHeader.document->pattern.height() == 3);
        CHECK(noHeader.document->pattern.population() == 5);
    }

    CHECK(decode("").error.code == RleError::EmptyBody);
    CHECK(decode("#N only comments\n").error.code == RleError::EmptyBody);
    const auto invalidCharacter = decode("x = 3, y = 3\nbo?$!");
    CHECK(invalidCharacter.error.code == RleError::InvalidCharacter);
    CHECK(invalidCharacter.error.line == 2);
    CHECK(invalidCharacter.error.column == 3);
    CHECK(decode("x = 3, y = 3\nbo$!").error.code == RleError::None);
    CHECK(decode("x = 3, y = 3\nbo$").error.code == RleError::MissingTerminator);
    CHECK(decode("x = 3, y = 3\nbo").error.code == RleError::MissingTerminator);

    const auto commentOffset = decode("#C one\n#C two\nx = 3, y = 3\nbo?\n!");
    CHECK(commentOffset.error.code == RleError::InvalidCharacter);
    CHECK(commentOffset.error.line == 4);
    CHECK(commentOffset.error.column == 3);
    CHECK(decode("x = 3, y = 3, rule = nonsense\nbo$!").error.code == RleError::InvalidRule);
    CHECK(decode("x = 999999999, y = 3\nbo$!").error.code == RleError::InvalidHeader);
    CHECK(decode("x = 1501, y = 3\nbo$!").error.code == RleError::TooLarge);

    // Regression: a crafted column accumulator used to overflow and write
    // outside the row vector. It must be rejected, never crash.
    std::string hostile = "x = 1, y = 1\n";
    for (int i = 0; i < 3000; ++i)
    {
        hostile += "2000000b";
    }
    hostile += "o!";
    const auto hostileResult = decode(hostile);
    CHECK(hostileResult.error.code == RleError::RunTooLong);
    CHECK(!hostileResult.document.has_value());

    CHECK(decode("x = 1, y = 1\n1500001b!").error.code == RleError::RunTooLong);
    CHECK(decode("x = 1, y = 1\n10000001b!").error.code == RleError::RunTooLong);
    CHECK(decode("x = 1, y = 1\n1501b!").error.code == RleError::TooLarge);

    const auto empty = decode("x = 3, y = 2\n!");
    CHECK(empty.error.code == RleError::None);
    if (empty.document)
    {
        CHECK(empty.document->pattern.width() == 3);
        CHECK(empty.document->pattern.height() == 2);
        CHECK(empty.document->pattern.population() == 0);
    }
}

void testRleRoundTrip()
{
    using lifegame::domain::Grid;
    using lifegame::domain::Rule;

    Grid board(13, 9);
    board.randomize(35, 987654);
    const std::string encoded = lifegame::domain::encodeRle(board, Rule(), "roundtrip");
    CHECK(!encoded.empty());

    const auto decoded = lifegame::domain::decodeRle(encoded);
    CHECK(decoded.error.code == lifegame::domain::RleError::None);
    CHECK(decoded.document.has_value());
    if (decoded.document)
    {
        CHECK(decoded.document->title == "roundtrip");
        CHECK(decoded.document->rule == "B3/S23");
        CHECK(decoded.document->pattern.width() == 13);
        CHECK(decoded.document->pattern.height() == 9);
        CHECK(decoded.document->pattern.population() == board.population());
        for (int row = 0; row < 9; ++row)
        {
            for (int col = 0; col < 13; ++col)
            {
                CHECK(decoded.document->pattern.alive(row, col) == board.cell(row, col));
            }
        }
    }

    Grid empty(7, 4);
    const std::string emptyEncoded = lifegame::domain::encodeRle(empty, Rule(), "");
    const auto emptyDecoded = lifegame::domain::decodeRle(emptyEncoded);
    CHECK(emptyDecoded.error.code == lifegame::domain::RleError::None);
    if (emptyDecoded.document)
    {
        CHECK(emptyDecoded.document->pattern.width() == 7);
        CHECK(emptyDecoded.document->pattern.height() == 4);
        CHECK(emptyDecoded.document->pattern.population() == 0);
    }
}

void testPackedGrid()
{
    using lifegame::domain::Grid;
    using lifegame::domain::PackedGrid;

    Grid board(100, 80);
    board.setCell(0, 0, true);
    board.setCell(50, 40, true);
    board.setCell(79, 99, true);

    const PackedGrid packed(board);
    CHECK(packed.width() == 100);
    CHECK(packed.height() == 80);
    CHECK(packed.byteSize() <= (100u * 80u + 7u) / 8u + 8u);
    CHECK(!packed.empty());

    const Grid restored = packed.unpack();
    CHECK(restored.width() == 100);
    CHECK(restored.height() == 80);
    CHECK(restored.equals(board));
    CHECK(restored.cell(0, 0));
    CHECK(restored.cell(50, 40));
    CHECK(restored.cell(79, 99));
}

void testHistory()
{
    using lifegame::domain::Grid;
    using lifegame::domain::History;

    auto board = std::make_shared<const Grid>(10, 10);
    History history(100);
    history.reset(0, board);
    CHECK(history.oldestGeneration() == 0);
    CHECK(history.size() == 1);

    history.record(50, board);
    CHECK(history.size() == 1);
    history.record(100, board);
    history.record(200, board);
    CHECK(history.size() == 3);
    CHECK(history.oldestGeneration() == 0);

    const auto atOrBefore = history.snapshotAtOrBefore(150);
    CHECK(atOrBefore.has_value());
    if (atOrBefore)
    {
        CHECK(atOrBefore->generation == 100);
        CHECK(atOrBefore->packedBoard != nullptr);
        if (atOrBefore->packedBoard)
        {
            CHECK(atOrBefore->packedBoard->byteSize() <= (10u * 10u + 7u) / 8u + 8u);
            CHECK(atOrBefore->packedBoard->unpack().equals(*board));
        }
    }
    CHECK(!history.snapshotAtOrBefore(-1).has_value());

    history.invalidateFrom(150);
    CHECK(history.size() == 2);
    CHECK(history.generationAt(history.size() - 1) == 100);

    History longHistory(10);
    longHistory.reset(0, board);
    for (int generation = 10; generation <= 1000; generation += 10)
    {
        longHistory.record(generation, board);
    }
    CHECK(longHistory.size() == static_cast<std::size_t>(History::kMaxSnapshots) + 1u);
    CHECK(longHistory.generationAt(0) == 0);
    CHECK(longHistory.generationAt(longHistory.size() - 1) == 1000);

    longHistory.setInterval(50);
    CHECK(longHistory.interval() == 50);
    longHistory.invalidateFrom(0);
    CHECK(longHistory.empty());
}

} // namespace

int main()
{
    testRules();
    testGridBasics();
    testStillLifeAndOscillators();
    testGliderAndWrap();
    testRandomize();
    testRowSlicing();
    testPatternLibrary();
    testPatternInvariant();
    testRleDecode();
    testRleRoundTrip();
    testHistory();
    testPackedGrid();

    if (g_failures == 0)
    {
        std::cout << "Domain tests passed (" << g_checks << " checks)." << std::endl;
        return 0;
    }

    std::cerr << g_failures << " domain test(s) failed (" << g_checks << " checks)." << std::endl;
    return 1;
}
