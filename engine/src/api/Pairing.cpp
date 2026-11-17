#include "Pairing.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <random>

namespace Api {

namespace {
constexpr const char* pairingPath = "/remote";

std::string trim(const std::string& text) {
    const auto first = std::find_if_not(text.begin(), text.end(),
                                        [](unsigned char c) { return std::isspace(c) != 0; });

    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
                          return std::isspace(c) != 0;
                      }).base();

    return first < last ? std::string(first, last) : std::string();
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;

    unsigned char difference = 0;

    for (std::size_t index = 0; index < left.size(); ++index)
        difference |= static_cast<unsigned char>(left[index] ^ right[index]);

    return difference == 0;
}

std::mt19937_64& randomEngine() {
    static thread_local std::mt19937_64 engine = [] {
        std::random_device device;
        std::seed_seq seed{device(), device(), device(), device()};
        return std::mt19937_64(seed);
    }();

    return engine;
}

std::string joinHostAndPort(const std::string& address, int port) {
    const bool ipv6 =
        address.find(':') != std::string::npos && address.find(']') == std::string::npos;

    if (!ipv6) return address + ":" + std::to_string(port);

    return "[" + address + "]:" + std::to_string(port);
}
}  // namespace

PairingManager::PairingManager(std::string advertisedAddressToUse, int portToUse)
    : advertisedAddress(advertisedAddressToUse.empty() ? discoverLanAddress()
                                                       : std::move(advertisedAddressToUse)),
      port(portToUse > 0 ? portToUse : 8080) {}

void PairingManager::setEndpoint(std::string advertisedAddressToUse, int portToUse) {
    const std::lock_guard<std::mutex> lock(mutex);

    if (!advertisedAddressToUse.empty())
        advertisedAddress = std::move(advertisedAddressToUse);
    else if (advertisedAddress.empty())
        advertisedAddress = discoverLanAddress();

    if (portToUse > 0) port = portToUse;
}

std::string PairingManager::getAdvertisedAddress() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return advertisedAddress;
}

void PairingManager::setAccountChangeHandler(AccountChangeHandler handler) {
    const std::lock_guard<std::mutex> lock(mutex);
    accountChangeHandler = std::move(handler);
}

void PairingManager::restoreToken(std::string token) {
    if (token.empty()) return;

    const std::lock_guard<std::mutex> lock(mutex);
    pairingToken = token;
    tokens[std::move(token)] = "restored";
}

bool PairingManager::setLanEnabled(bool enabled) {
    AccountChangeHandler handler;
    bool lan = false;
    std::string token;

    {
        const std::lock_guard<std::mutex> lock(mutex);

        if (lanEnabled == enabled) return false;

        lanEnabled = enabled;

        if (enabled) {
            issueCodeLocked();
        } else {
            code.clear();
            codeExpiry = {};
            tokens.clear();
            pairingToken.clear();
        }

        lan = lanEnabled;
        token = pairingToken;
        handler = accountChangeHandler;
    }

    if (handler != nullptr) handler(lan, token);

    return true;
}

bool PairingManager::isLanEnabled() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return lanEnabled;
}

PairingSnapshot PairingManager::snapshot() {
    const std::lock_guard<std::mutex> lock(mutex);

    PairingSnapshot result;
    result.lanEnabled = lanEnabled;
    result.address = advertisedAddress;
    result.port = port;
    result.pairedClients = static_cast<int>(tokens.size());

    if (!lanEnabled) return result;

    const auto now = std::chrono::steady_clock::now();

    if (code.empty() || now >= codeExpiry) issueCodeLocked();

    result.code = code;
    result.codeExpiresInMs =
        std::max(0.0, std::chrono::duration<double, std::milli>(codeExpiry - now).count());
    result.payload = makePayloadLocked();

    return result;
}

bool PairingManager::pair(const std::string& suppliedCode, std::string& token, std::string& error,
                          std::string clientName) {
    const auto normalized = trim(suppliedCode);
    AccountChangeHandler handler;
    std::string issued;

    {
        const std::lock_guard<std::mutex> lock(mutex);

        if (!lanEnabled) {
            error = "LAN mode is off";
            return false;
        }

        const auto now = std::chrono::steady_clock::now();

        if (code.empty() || now >= codeExpiry) {
            issueCodeLocked();
            error = "pairing code has expired; scan the new code";
            return false;
        }

        if (!isValidCode(normalized)) {
            error = "pairing code must be 6 digits";
            return false;
        }

        if (!constantTimeEquals(normalized, code)) {
            error = "invalid pairing code";
            return false;
        }

        issued = randomToken();
        tokens[issued] = std::move(clientName);
        pairingToken = issued;

        issueCodeLocked();

        token = issued;
        handler = accountChangeHandler;
    }

    if (handler != nullptr) handler(true, issued);

    return true;
}

bool PairingManager::validateToken(const std::string& token) const {
    if (token.empty()) return false;

    const std::lock_guard<std::mutex> lock(mutex);
    return tokens.find(token) != tokens.end();
}

std::size_t PairingManager::revokeAll() {
    AccountChangeHandler handler;
    std::size_t count = 0;
    bool lan = false;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        count = tokens.size();
        tokens.clear();
        pairingToken.clear();
        lan = lanEnabled;
        handler = accountChangeHandler;
    }

    if (handler != nullptr) handler(lan, {});

    return count;
}

std::size_t PairingManager::pairedClientCount() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return tokens.size();
}

std::string PairingManager::discoverLanAddress() {
    auto local = juce::IPAddress::getLocalAddress(false);

    if (local.toString().isNotEmpty() && local.toString() != "127.0.0.1")
        return local.toString().toStdString();

    for (const auto& address : juce::IPAddress::getAllAddresses(false)) {
        const auto text = address.toString();

        if (text.isNotEmpty() && text != "127.0.0.1") return text.toStdString();
    }

    return "127.0.0.1";
}

bool PairingManager::isValidCode(const std::string& codeToCheck) {
    if (codeToCheck.size() != static_cast<std::size_t>(codeDigits)) return false;

    return std::all_of(codeToCheck.begin(), codeToCheck.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

void PairingManager::issueCodeLocked() {
    code = randomDigits(codeDigits);
    codeExpiry = std::chrono::steady_clock::now() + codeLifetime;
}

std::string PairingManager::makePayloadLocked() const {
    if (!lanEnabled || code.empty()) return {};

    return "http://" + joinHostAndPort(advertisedAddress, port) + pairingPath + "?code=" + code;
}

std::string PairingManager::randomDigits(int count) {
    std::uniform_int_distribution<int> digit(0, 9);
    std::string result;

    result.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index)
        result.push_back(static_cast<char>('0' + digit(randomEngine())));

    return result;
}

std::string PairingManager::randomToken() {
    static constexpr const char* hex = "0123456789abcdef";
    std::uniform_int_distribution<unsigned int> byte(0, 255);

    std::string result;
    result.reserve(64);

    for (int index = 0; index < 32; ++index) {
        const auto value = byte(randomEngine());
        result.push_back(hex[(value >> 4) & 0x0f]);
        result.push_back(hex[value & 0x0f]);
    }

    return result;
}

}  // namespace Api
