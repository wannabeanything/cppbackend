#pragma once
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <pqxx/pqxx>
#include <string>

#include "tagged.h"

namespace util {

namespace detail {

using UUIDType = boost::uuids::uuid;

UUIDType NewUUID();
constexpr UUIDType ZeroUUID{{0}};

std::string UUIDToString(const UUIDType& uuid);
UUIDType UUIDFromString(std::string_view str);

}  // namespace detail

template <typename Tag>
class TaggedUUID : public Tagged<detail::UUIDType, Tag> {
public:
    using Base = Tagged<detail::UUIDType, Tag>;
    using Tagged<detail::UUIDType, Tag>::Tagged;

    TaggedUUID()
        : Base{detail::ZeroUUID} {
    }

    static TaggedUUID New() {
        return TaggedUUID{detail::NewUUID()};
    }

    static TaggedUUID FromString(const std::string& uuid_as_text) {
        return TaggedUUID{detail::UUIDFromString(uuid_as_text)};
    }


    std::string ToString() const {
        return detail::UUIDToString(**this);
    }
};

}  // namespace util


namespace pqxx {

template <typename Tag>
struct string_traits<util::TaggedUUID<Tag>> {
    static constexpr const char* name() noexcept { return "TaggedUUID"; }
    static constexpr bool has_null() noexcept { return false; }

    static util::TaggedUUID<Tag> from_string(std::string_view text) {
        return util::TaggedUUID<Tag>::FromString(std::string{text});
    }

    static std::string to_string(const util::TaggedUUID<Tag>& uuid) {
        return uuid.ToString();
    }
};
template <typename Tag>
struct nullness<util::TaggedUUID<Tag>> {
    static constexpr bool has_null = false;
};
}  // namespace pqxx
