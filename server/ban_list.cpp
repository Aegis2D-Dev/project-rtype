#include <aegis2d/core/config.hpp>
#include <aegis2d/core/log.hpp>
#include <aegis2d/net/endpoint.hpp>

#include <rtype/ban_list.hpp>
#include <rtype/rate_limit.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace rtype {

namespace core = aegis2d::core;
namespace net = aegis2d::net;

BanList::BanList(std::filesystem::path file) : file_{std::move(file)} {
    load();
}

// A missing file is an empty list; anything else that is not a list of addresses is reported.
void BanList::load() {
    std::error_code missing;
    if (!std::filesystem::exists(file_, missing)) {
        return;
    }
    try {
        const core::Config config = core::Config::from_file(file_);
        const std::optional<core::ConfigValue> bans = config.value("bans");
        const auto* const list =
            bans.has_value() ? std::get_if<core::ConfigValue::Array>(&bans->data) : nullptr;
        if (list == nullptr) {
            throw core::ConfigError{std::format("{}: no \"bans\" array", file_.string())};
        }
        std::vector<net::Endpoint> read;
        for (std::size_t index = 0; index < list->size(); ++index) {
            const std::optional<std::string> text = config.string(std::format("bans.{}", index));
            const std::optional<net::Endpoint> address =
                text ? net::Endpoint::parse(*text, 0) : std::nullopt;
            if (!address) {
                throw core::ConfigError{
                    std::format("{}: ban {} is not an IP address", file_.string(), index)};
            }
            read.push_back(*address);
        }
        addresses_ = std::move(read);
    } catch (const core::ConfigError& error) {
        load_error_ = error.what();
        core::log_warn("bans: {} (starting with none, the file is replaced at the next ban)",
                       error.what());
    }
}

bool BanList::ban(const net::Endpoint& address) {
    if (banned(address)) {
        return false;
    }
    addresses_.push_back(address_of(address));
    save();
    return true;
}

bool BanList::banned(const net::Endpoint& address) const {
    return std::ranges::find(addresses_, address_of(address)) != addresses_.end();
}

void BanList::save() const {
    core::ConfigValue::Array bans;
    bans.reserve(addresses_.size());
    for (const net::Endpoint& address : addresses_) {
        bans.push_back(core::ConfigValue{address.address()});
    }
    core::Config config;
    config.set("bans", core::ConfigValue{std::move(bans)});
    try {
        config.save(file_);
    } catch (const core::ConfigError& error) {
        core::log_error("bans: {} (the bans hold until the server stops)", error.what());
    }
}

const std::vector<net::Endpoint>& BanList::addresses() const noexcept {
    return addresses_;
}

const std::optional<std::string>& BanList::load_error() const noexcept {
    return load_error_;
}

} // namespace rtype
