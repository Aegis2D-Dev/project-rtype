/// @file
/// @brief The addresses banned by the administrator, kept in a JSON file across restarts.
#pragma once
#include <aegis2d/net/endpoint.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rtype {

/// @brief The IP addresses that cannot log in, kept in a JSON file.
/// @details The file is `{"bans": ["203.0.113.7", "::1"]}`. It is read once by the
/// constructor: a missing file is an empty list; a file that cannot be read or is not a
/// list of addresses is reported by load_error() and logged, and the list starts empty,
/// which never stops the server. Each ban is written at once, through a temporary file that
/// then replaces the old one (`core::Config::save()`); a write that fails is logged and the
/// ban holds until the server stops. Not thread-safe.
class BanList {
public:
    /// @brief Reads the bans of a file.
    /// @param file Where the bans are kept; created at the first ban.
    explicit BanList(std::filesystem::path file);

    /// @brief Bans an address, its port ignored.
    /// @param address Address of a client.
    /// @return `false` if it was banned already: nothing changed.
    bool ban(const aegis2d::net::Endpoint& address);

    /// @brief Tells whether an address is banned, whatever its port.
    /// @param address Address of a client.
    /// @return `true` if it may not log in.
    [[nodiscard]] bool banned(const aegis2d::net::Endpoint& address) const;

    /// @brief The banned addresses, port 0.
    /// @return In the order they were banned.
    [[nodiscard]] const std::vector<aegis2d::net::Endpoint>& addresses() const noexcept;

    /// @brief Why the file could not be read at start.
    /// @return The reason, or `std::nullopt` if it was read or missing.
    [[nodiscard]] const std::optional<std::string>& load_error() const noexcept;

private:
    void load();
    void save() const;

    std::filesystem::path file_;
    std::vector<aegis2d::net::Endpoint> addresses_;
    std::optional<std::string> load_error_;
};

} // namespace rtype
