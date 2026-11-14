#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Api
{

    struct PairingSnapshot
    {
        bool lanEnabled = false;

        std::string address;
        int port = 8080;

        std::string code;

        double codeExpiresInMs = 0.0;

        std::string payload;

        int pairedClients = 0;
    };

    class PairingManager
    {
    public:
        static constexpr int codeDigits = 6;
        static constexpr std::chrono::minutes codeLifetime{5};

        using AccountChangeHandler =
            std::function<void (bool lanEnabled, const std::string& pairingToken)>;

        explicit PairingManager (std::string advertisedAddress = {}, int port = 8080);

        void setEndpoint (std::string advertisedAddress, int port);

        void setAccountChangeHandler (AccountChangeHandler handler);

        void restoreToken (std::string token);

        std::string getAdvertisedAddress () const;

        bool setLanEnabled (bool enabled);

        bool isLanEnabled () const;

        PairingSnapshot snapshot ();

        bool pair (const std::string& code,
                   std::string& token,
                   std::string& error,
                   std::string clientName = {});

        bool validateToken (const std::string& token) const;

        std::size_t revokeAll ();

        std::size_t pairedClientCount () const;

        static std::string discoverLanAddress ();
        static bool isValidCode (const std::string& code);

    private:
        void issueCodeLocked ();
        std::string makePayloadLocked () const;

        static std::string randomDigits (int count);
        static std::string randomToken ();

        mutable std::mutex mutex;

        bool lanEnabled = false;
        std::string advertisedAddress;
        int port = 8080;

        std::string code;
        std::chrono::steady_clock::time_point codeExpiry;

        std::unordered_map<std::string, std::string> tokens;
        std::string pairingToken;

        AccountChangeHandler accountChangeHandler;
    };

}
