#include "Fixtures.h"

#include <iostream>

namespace ApiTest::Pairing
{

bool isSixDigits (const juce::var& value)
{
    if (! value.isString())
        return false;

    const auto text = value.toString();

    if (text.length() != 6)
        return false;

    for (int index = 0; index < text.length(); ++index)
        if (! juce::CharacterFunctions::isDigit (text[index]))
            return false;

    return true;
}

Response retry (httplib::Client& client,
                const std::string& path,
                const httplib::Headers& headers,
                const std::function<bool (const Response&)>& predicate,
                int timeoutMs)
{
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds (timeoutMs);

    while (std::chrono::steady_clock::now() < deadline)
    {
        auto response = client.Get (path, headers);

        if (predicate (response))
            return response;

        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }

    return client.Get (path, headers);
}

bool Fixture::start()
{
    if (! server.start())
    {
        std::cerr << "pairing-test: FAILED to start the server" << std::endl;
        return false;
    }

    port = server.getPort();
    std::cout << "pairing-test: listening on 127.0.0.1:" << port << std::endl;

    client = std::make_unique<httplib::Client> ("127.0.0.1", port);
    client->set_connection_timeout (2, 0);
    client->set_read_timeout (2, 0);
    return true;
}

}
