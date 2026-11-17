#pragma once

#include <string>
#include <vector>

namespace Api::Tls {

struct Certificate {
    std::string certificatePath;
    std::string privateKeyPath;
};

bool ensureCertificate(const std::string& directory, const std::vector<std::string>& addresses,
                       Certificate& certificate, std::string& error);

}  // namespace Api::Tls
