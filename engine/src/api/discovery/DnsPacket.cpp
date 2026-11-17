#include "api/discovery/DnsPacket.h"

#include "api/Discovery.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace Api {

namespace {

constexpr std::uint16_t dnsTypeA = 1;
constexpr std::uint16_t dnsTypePTR = 12;
constexpr std::uint16_t dnsTypeTXT = 16;
constexpr std::uint16_t dnsTypeAAAA = 28;
constexpr std::uint16_t dnsTypeSRV = 33;
constexpr std::uint16_t dnsTypeNSEC = 47;
constexpr std::uint16_t dnsTypeANY = 255;

constexpr std::uint16_t dnsClassIN = 1;
constexpr std::uint16_t dnsCacheFlush = 0x8000;

constexpr std::uint32_t ttlService = 4500;
constexpr std::uint32_t ttlHost = 120;

using Labels = std::vector<std::string>;

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
}

std::uint16_t readU16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

void appendName(std::vector<std::uint8_t>& out, const Labels& labels) {
    for (const auto& label : labels) {
        const auto length = std::min<std::size_t>(label.size(), 63);
        out.push_back(static_cast<std::uint8_t>(length));
        out.insert(out.end(), label.begin(), label.begin() + static_cast<std::ptrdiff_t>(length));
    }

    out.push_back(0);
}

bool readName(const std::uint8_t* data, std::size_t size, std::size_t& offset, Labels& labels) {
    std::size_t position = offset;
    std::size_t resumeAt = 0;
    bool jumped = false;
    int jumps = 0;

    for (;;) {
        if (position >= size) return false;

        const auto length = data[position];

        if (length == 0) {
            ++position;
            break;
        }

        if ((length & 0xc0) == 0xc0) {
            if (position + 1 >= size) return false;

            const auto target =
                static_cast<std::size_t>(((length & 0x3f) << 8) | data[position + 1]);

            if (!jumped) {
                resumeAt = position + 2;
                jumped = true;
            }

            if (++jumps > 16 || target >= size) return false;

            position = target;
            continue;
        }

        if ((length & 0xc0) != 0 || position + 1 + length > size) return false;

        labels.emplace_back(reinterpret_cast<const char*>(data + position + 1),
                            static_cast<std::size_t>(length));
        position += 1 + static_cast<std::size_t>(length);

        if (labels.size() > 128) return false;
    }

    offset = jumped ? resumeAt : position;
    return true;
}

bool equalLabels(const Labels& left, const Labels& right) {
    if (left.size() != right.size()) return false;

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].size() != right[index].size()) return false;

        for (std::size_t character = 0; character < left[index].size(); ++character) {
            const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };

            if (lower(static_cast<unsigned char>(left[index][character])) !=
                lower(static_cast<unsigned char>(right[index][character]))) {
                return false;
            }
        }
    }

    return true;
}

Labels serviceLabels() { return {"_instrumenthub", "_tcp", "local"}; }

Labels enumerationLabels() { return {"_services", "_dns-sd", "_udp", "local"}; }

Labels instanceLabels(const DiscoveryService& service) {
    return {service.instanceName, "_instrumenthub", "_tcp", "local"};
}

Labels hostLabels(const DiscoveryService& service) { return {service.hostLabel, "local"}; }

std::vector<std::uint8_t> encodeTxt(const DiscoveryService& service) {
    std::vector<std::uint8_t> result;

    for (const auto& entry : service.txt) {
        auto text = entry.first.empty() ? entry.second : entry.first + "=" + entry.second;

        if (text.size() > 255) text.resize(255);

        result.push_back(static_cast<std::uint8_t>(text.size()));
        result.insert(result.end(), text.begin(), text.end());
    }

    if (result.empty()) result.push_back(0);

    return result;
}

bool parseIpv4(const std::string& text, std::array<std::uint8_t, 4>& address) {
    std::size_t part = 0;
    std::size_t value = 0;
    bool haveDigit = false;

    for (std::size_t index = 0; index <= text.size(); ++index) {
        const bool end = index == text.size();
        const auto character = end ? '.' : text[index];

        if (character == '.') {
            if (!haveDigit || part >= 4 || value > 255) return false;

            address[part++] = static_cast<std::uint8_t>(value);
            value = 0;
            haveDigit = false;

            if (end && part != 4) return false;
        } else if (character >= '0' && character <= '9') {
            value = value * 10 + static_cast<std::size_t>(character - '0');
            haveDigit = true;
        } else {
            return false;
        }
    }

    return part == 4;
}

struct ResourceRecord {
    Labels name;
    std::uint16_t type = 0;
    std::uint16_t klass = dnsClassIN;
    std::uint32_t ttl = 0;
    std::vector<std::uint8_t> rdata;
};

struct RecordSet {
    ResourceRecord servicePtr;
    ResourceRecord enumerationPtr;
    ResourceRecord srv;
    ResourceRecord txt;
    ResourceRecord address;
    ResourceRecord nsec;
    bool hasAddress = false;
};

RecordSet makeRecordSet(const DiscoveryService& service, bool goodbye) {
    const auto serviceTtl = goodbye ? 0u : ttlService;
    const auto hostTtl = goodbye ? 0u : ttlHost;
    const auto instance = instanceLabels(service);
    const auto host = hostLabels(service);

    RecordSet set;

    set.servicePtr.name = serviceLabels();
    set.servicePtr.type = dnsTypePTR;
    set.servicePtr.ttl = serviceTtl;
    appendName(set.servicePtr.rdata, instance);

    set.enumerationPtr.name = enumerationLabels();
    set.enumerationPtr.type = dnsTypePTR;
    set.enumerationPtr.ttl = serviceTtl;
    appendName(set.enumerationPtr.rdata, serviceLabels());

    set.srv.name = instance;
    set.srv.type = dnsTypeSRV;
    set.srv.klass = static_cast<std::uint16_t>(dnsClassIN | dnsCacheFlush);
    set.srv.ttl = hostTtl;
    appendU16(set.srv.rdata, 0);
    appendU16(set.srv.rdata, 0);
    appendU16(set.srv.rdata, static_cast<std::uint16_t>(service.port));
    appendName(set.srv.rdata, host);

    set.txt.name = instance;
    set.txt.type = dnsTypeTXT;
    set.txt.klass = static_cast<std::uint16_t>(dnsClassIN | dnsCacheFlush);
    set.txt.ttl = serviceTtl;
    set.txt.rdata = encodeTxt(service);

    std::array<std::uint8_t, 4> ip{};

    if (parseIpv4(service.address, ip) && ip[0] != 127) {
        set.address.name = host;
        set.address.type = dnsTypeA;
        set.address.klass = static_cast<std::uint16_t>(dnsClassIN | dnsCacheFlush);
        set.address.ttl = hostTtl;
        set.address.rdata.assign(ip.begin(), ip.end());
        set.hasAddress = true;

        set.nsec.name = host;
        set.nsec.type = dnsTypeNSEC;
        set.nsec.klass = static_cast<std::uint16_t>(dnsClassIN | dnsCacheFlush);
        set.nsec.ttl = hostTtl;
        appendName(set.nsec.rdata, host);
        set.nsec.rdata.insert(set.nsec.rdata.end(), {0x00, 0x04, 0x40, 0x00, 0x00, 0x00});
    }

    return set;
}

void appendRecord(std::vector<std::uint8_t>& packet, const ResourceRecord& record) {
    appendName(packet, record.name);
    appendU16(packet, record.type);
    appendU16(packet, record.klass);
    appendU32(packet, record.ttl);
    appendU16(packet, static_cast<std::uint16_t>(record.rdata.size()));
    packet.insert(packet.end(), record.rdata.begin(), record.rdata.end());
}
bool isRecordOfInterest(const Labels& name, std::uint16_t type, const DiscoveryService& service) {
    const auto serviceName = serviceLabels();
    const auto instance = instanceLabels(service);
    const auto host = hostLabels(service);
    const auto enumeration = enumerationLabels();

    if (type == dnsTypeANY) {
        return equalLabels(name, serviceName) || equalLabels(name, instance) ||
               equalLabels(name, host) || equalLabels(name, enumeration);
    }

    if (equalLabels(name, serviceName)) return type == dnsTypePTR;

    if (equalLabels(name, instance)) return type == dnsTypeSRV || type == dnsTypeTXT;

    if (equalLabels(name, host)) return type == dnsTypeA || type == dnsTypeAAAA;

    if (equalLabels(name, enumeration)) return type == dnsTypePTR;

    return false;
}

}  // namespace

std::vector<std::uint8_t> buildResponse(const DiscoveryService& service, bool goodbye) {
    const auto records = makeRecordSet(service, goodbye);

    std::vector<std::uint8_t> packet;
    packet.reserve(512);

    appendU16(packet, 0);
    appendU16(packet, 0x8400);
    appendU16(packet, 0);
    appendU16(packet, 2);
    appendU16(packet, 0);
    appendU16(packet, records.hasAddress ? 4 : 2);

    appendRecord(packet, records.servicePtr);
    appendRecord(packet, records.enumerationPtr);
    appendRecord(packet, records.srv);
    appendRecord(packet, records.txt);

    if (records.hasAddress) {
        appendRecord(packet, records.address);
        appendRecord(packet, records.nsec);
    }

    return packet;
}

bool wantsResponse(const std::uint8_t* data, std::size_t size, const DiscoveryService& service) {
    if (size < 12) return false;

    const auto flags = readU16(data + 2);

    if ((flags & 0x8000) != 0) return false;

    const auto questions = readU16(data + 4);
    std::size_t offset = 12;

    for (std::uint16_t index = 0; index < questions; ++index) {
        Labels name;

        if (!readName(data, size, offset, name) || offset + 4 > size) return false;

        const auto type = readU16(data + offset);
        offset += 4;

        if (isRecordOfInterest(name, type, service)) return true;
    }

    return false;
}

}  // namespace Api
