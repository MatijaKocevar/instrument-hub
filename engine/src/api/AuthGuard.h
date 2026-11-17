#pragma once

#include <mutex>
#include <string>

namespace httplib {
struct Request;
}

namespace Api {

class PairingManager;

class AuthGuard {
 public:
  explicit AuthGuard(PairingManager& pairing, std::string token = {});

  AuthGuard(const AuthGuard&) = delete;
  AuthGuard& operator=(const AuthGuard&) = delete;

  void setToken(std::string token);

  bool hasToken() const;

  bool authorized(const httplib::Request& request) const;

 private:
  PairingManager& pairing;

  mutable std::mutex mutex;
  std::string token;
};

}  // namespace Api
