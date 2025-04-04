#pragma once

sf::Packet& operator<<(sf::Packet& out, const Responses& rep) {
    return out << static_cast<uint8_t>(rep);
}

sf::Packet& operator>>(sf::Packet& in, Responses& rep) {
    uint8_t ans;
    in >> ans;
    rep = static_cast<Responses>(ans);
    return in;
}

sf::Packet& operator<<(sf::Packet& out, const Queries& rep) {
    return out << static_cast<uint8_t>(rep);
}

sf::Packet& operator>>(sf::Packet& in, Queries& rep) {
    uint8_t ans;
    in >> ans;
    rep = static_cast<Queries>(ans);
    return in;
}

sf::Packet& operator<<(sf::Packet& packet, uint64_t value) {
    return packet << static_cast<uint32_t>(value >> 32) << static_cast<uint32_t>(value);
}

sf::Packet& operator>>(sf::Packet& packet, uint64_t& value) {
    uint32_t high, low;
    packet >> high >> low;
    value = (static_cast<uint64_t>(high) << 32) | low;
    return packet;
}
