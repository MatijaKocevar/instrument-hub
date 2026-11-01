#pragma once

#include <httplib.h>

#include <functional>
#include <string>

namespace ApiTest
{

httplib::Result requestWithDispatch (std::function<httplib::Result()> send);

httplib::Result postWithDispatch (httplib::Client& client,
                                  const std::string& path,
                                  const std::string& body);

httplib::Result patchWithDispatch (httplib::Client& client,
                                   const std::string& path,
                                   const std::string& body);

httplib::Result putWithDispatch (httplib::Client& client,
                                 const std::string& path,
                                 const std::string& body);

httplib::Result deleteWithDispatch (httplib::Client& client,
                                    const std::string& path);

}
