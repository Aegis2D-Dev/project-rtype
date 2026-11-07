#include <aegis2d/net/binary.hpp>

#include <rtype/login.hpp>
#include <rtype/scores.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rtype {

namespace net = aegis2d::net;

bool read_list_scores(std::span<const std::byte> message) noexcept {
    return message.size() == 1 && std::to_integer<std::uint8_t>(message.front()) ==
                                      static_cast<std::uint8_t>(TcpKind::ListScores);
}

std::optional<std::vector<ScoreRecord>> read_score_list(std::span<const std::byte> message) {
    net::BinaryReader reader{message};
    const std::optional<std::uint8_t> kind = reader.read_u8();
    const std::optional<std::uint8_t> count = reader.read_u8();
    if (kind != static_cast<std::uint8_t>(TcpKind::ScoreList) || !count ||
        *count > max_listed_scores) {
        return std::nullopt;
    }
    std::vector<ScoreRecord> scores;
    scores.reserve(*count);
    for (std::uint8_t index = 0; index < *count; ++index) {
        std::optional<std::string> name = reader.read_string();
        const std::optional<std::uint32_t> score = reader.read_u32();
        if (!name || !score) {
            return std::nullopt;
        }
        scores.push_back(ScoreRecord{.name = std::move(*name), .score = *score});
    }
    if (!reader.at_end()) {
        return std::nullopt;
    }
    return scores;
}

std::vector<std::byte> write_list_scores() {
    return {std::byte{static_cast<std::uint8_t>(TcpKind::ListScores)}};
}

std::vector<std::byte> write_score_list(std::span<const ScoreRecord> scores) {
    const std::span<const ScoreRecord> listed =
        scores.first(std::min(scores.size(), max_listed_scores));
    net::BinaryWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(TcpKind::ScoreList));
    writer.write_u8(static_cast<std::uint8_t>(listed.size()));
    for (const ScoreRecord& record : listed) {
        writer.write_string(record.name);
        writer.write_u32(record.score);
    }
    return {writer.data().begin(), writer.data().end()};
}

} // namespace rtype
