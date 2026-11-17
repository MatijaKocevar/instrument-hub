#include "api/tls/TlsCertificate.h"

#include "api/Discovery.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>

namespace Api::Tls {
namespace {

using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using ExtPtr = std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;

std::string opensslError(const char* what) {
    const auto code = ERR_get_error();

    if (code == 0) return what;

    char buffer[256];
    ERR_error_string_n(code, buffer, sizeof(buffer));

    return std::string(what) + ": " + buffer;
}

bool readFile(const std::filesystem::path& path, std::string& contents) {
    std::ifstream stream(path, std::ios::binary);

    if (!stream) return false;

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    contents = buffer.str();

    return !contents.empty();
}

bool writeFile(const std::filesystem::path& path, const std::string& contents,
               bool privateFile) {
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);

        if (!stream) return false;

        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));

        if (!stream) return false;
    }

    if (privateFile) {
        std::error_code permissionsError;
        std::filesystem::permissions(temporary,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::replace, permissionsError);
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);

    if (renameError) {
        std::filesystem::remove(temporary);
        return false;
    }

    return true;
}

std::vector<std::string> buildSanEntries(const std::vector<std::string>& addresses) {
    std::set<std::string> unique;

    unique.insert("DNS:localhost");
    unique.insert("DNS:instrument-hub.local");
    unique.insert("DNS:" + Discovery::defaultHostLabel() + ".local");
    unique.insert("IP:127.0.0.1");

    for (const auto& address : addresses) {
        if (!address.empty()) unique.insert("IP:" + address);
    }

    return {unique.begin(), unique.end()};
}

std::string joinLines(const std::vector<std::string>& values) {
    std::string result;

    for (const auto& value : values) {
        if (!result.empty()) result.push_back('\n');

        result += value;
    }

    return result;
}

bool addExtension(X509& certificate, X509V3_CTX& context, int nid, const char* value,
                  std::string& error) {
    ExtPtr extension(X509V3_EXT_conf_nid(nullptr, &context, nid, value), X509_EXTENSION_free);

    if (extension == nullptr) {
        error = opensslError("could not build a certificate extension");
        return false;
    }

    if (X509_add_ext(&certificate, extension.get(), -1) != 1) {
        error = opensslError("could not add a certificate extension");
        return false;
    }

    return true;
}

bool certificateIsUsable(const std::filesystem::path& certificatePath,
                         const std::filesystem::path& keyPath) {
    BioPtr certificateBio(
        BIO_new_file(certificatePath.string().c_str(), "r"), BIO_free);
    BioPtr keyBio(BIO_new_file(keyPath.string().c_str(), "r"), BIO_free);

    if (certificateBio == nullptr || keyBio == nullptr) return false;

    X509Ptr certificate(PEM_read_bio_X509(certificateBio.get(), nullptr, nullptr, nullptr),
                        X509_free);
    PkeyPtr key(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr),
                EVP_PKEY_free);

    if (certificate == nullptr || key == nullptr) return false;

    return X509_check_private_key(certificate.get(), key.get()) == 1;
}

bool generateCertificate(const std::vector<std::string>& sanEntries, std::string& certificatePem,
                         std::string& privateKeyPem, std::string& error) {
    PkeyPtr key(EVP_EC_gen("P-256"), EVP_PKEY_free);

    if (key == nullptr) {
        error = opensslError("could not generate the TLS private key");
        return false;
    }

    X509Ptr certificate(X509_new(), X509_free);

    if (certificate == nullptr) {
        error = opensslError("could not create the TLS certificate");
        return false;
    }

    X509_set_version(certificate.get(), 2);

    unsigned char serialBytes[16];

    if (RAND_bytes(serialBytes, sizeof(serialBytes)) != 1) {
        error = opensslError("could not generate a certificate serial");
        return false;
    }

    serialBytes[0] &= 0x7f;
    BIGNUM* serial = BN_bin2bn(serialBytes, sizeof(serialBytes), nullptr);

    if (serial == nullptr || BN_to_ASN1_INTEGER(serial, X509_get_serialNumber(certificate.get())) ==
                                 nullptr) {
        BN_free(serial);
        error = opensslError("could not set the certificate serial");
        return false;
    }

    BN_free(serial);

    X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 60L * 60L * 24L * 3650L);
    X509_set_pubkey(certificate.get(), key.get());

    auto* subject = const_cast<X509_NAME*>(X509_get_subject_name(certificate.get()));
    const auto* commonName = reinterpret_cast<const unsigned char*>("Instrument Hub");

    if (X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, commonName, -1, -1, 0) != 1 ||
        X509_set_issuer_name(certificate.get(), subject) != 1) {
        error = opensslError("could not set the certificate subject");
        return false;
    }

    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate.get(), certificate.get(), nullptr, nullptr, 0);

    if (!addExtension(*certificate, context, NID_basic_constraints, "critical,CA:FALSE", error) ||
        !addExtension(*certificate, context, NID_key_usage,
                      "critical,digitalSignature,keyEncipherment", error) ||
        !addExtension(*certificate, context, NID_ext_key_usage, "serverAuth", error) ||
        !addExtension(*certificate, context, NID_subject_key_identifier, "hash", error)) {
        return false;
    }

    const auto sanString = [&sanEntries] {
        std::string value;

        for (const auto& entry : sanEntries) {
            if (!value.empty()) value.push_back(',');

            value += entry;
        }

        return value;
    }();

    if (!addExtension(*certificate, context, NID_subject_alt_name, sanString.c_str(), error))
        return false;

    if (X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
        error = opensslError("could not sign the TLS certificate");
        return false;
    }

    BioPtr certificateBio(BIO_new(BIO_s_mem()), BIO_free);
    BioPtr keyBio(BIO_new(BIO_s_mem()), BIO_free);

    if (certificateBio == nullptr || keyBio == nullptr ||
        PEM_write_bio_X509(certificateBio.get(), certificate.get()) != 1 ||
        PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr,
                                 nullptr) != 1) {
        error = opensslError("could not serialise the TLS certificate");
        return false;
    }

    char* data = nullptr;
    const auto certificateSize = BIO_get_mem_data(certificateBio.get(), &data);

    if (certificateSize <= 0 || data == nullptr) {
        error = "the TLS certificate is empty";
        return false;
    }

    certificatePem.assign(data, static_cast<std::size_t>(certificateSize));

    data = nullptr;
    const auto keySize = BIO_get_mem_data(keyBio.get(), &data);

    if (keySize <= 0 || data == nullptr) {
        error = "the TLS private key is empty";
        return false;
    }

    privateKeyPem.assign(data, static_cast<std::size_t>(keySize));

    return true;
}

}  // namespace

bool ensureCertificate(const std::string& directory, const std::vector<std::string>& addresses,
                       Certificate& certificate, std::string& error) {
    error.clear();

    if (directory.empty()) {
        error = "the TLS certificate directory is not configured";
        return false;
    }

    const auto root = std::filesystem::path(directory);
    std::error_code directoryError;
    std::filesystem::create_directories(root, directoryError);

    if (directoryError) {
        error = "could not create the TLS directory: " + directoryError.message();
        return false;
    }

    const auto certificatePath = root / "server.crt";
    const auto keyPath = root / "server.key";
    const auto sanPath = root / "server.sans";
    const auto sanEntries = buildSanEntries(addresses);
    const auto sanText = joinLines(sanEntries);

    std::string storedSan;
    const bool storedSanMatches =
        readFile(sanPath, storedSan) && storedSan == sanText;

    if (storedSanMatches && certificateIsUsable(certificatePath, keyPath)) {
        certificate.certificatePath = certificatePath.string();
        certificate.privateKeyPath = keyPath.string();
        return true;
    }

    std::string certificatePem;
    std::string privateKeyPem;

    if (!generateCertificate(sanEntries, certificatePem, privateKeyPem, error)) return false;

    if (!writeFile(keyPath, privateKeyPem, true)) {
        error = "could not write the TLS private key";
        return false;
    }

    if (!writeFile(certificatePath, certificatePem, false)) {
        error = "could not write the TLS certificate";
        return false;
    }

    if (!writeFile(sanPath, sanText, false)) {
        error = "could not write the TLS certificate metadata";
        return false;
    }

    certificate.certificatePath = certificatePath.string();
    certificate.privateKeyPath = keyPath.string();

    return true;
}

}  // namespace Api::Tls
