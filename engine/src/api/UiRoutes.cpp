#include "api/UiRoutes.h"

#include "BinaryData.h"

#include <httplib.h>

#include <string>

namespace Api::Routes {
namespace {

bool isAssetPath(const std::string& path) { return path.rfind("/assets/", 0) == 0; }

bool hasExtension(const std::string& path) {
    const auto slash = path.find_last_of('/');
    const auto dot = path.find_last_of('.');

    return dot != std::string::npos && (slash == std::string::npos || dot > slash);
}

void setAsset(httplib::Response& response, const char* data, int size, const char* mimeType) {
    response.set_header("Cache-Control", "no-cache");
    response.set_content(data, static_cast<std::size_t>(size), mimeType);
}

void setIndex(httplib::Response& response) {
    setAsset(response, BinaryData::index_html, BinaryData::index_htmlSize,
             "text/html; charset=utf-8");
}

}  // namespace

bool isUiRequestPath(const std::string& path) {
    return path == "/" || path == "/remote" || path == "/remote/" || isAssetPath(path) ||
           path == "/favicon.ico";
}

void registerUiRoutes(httplib::Server& server) {
    server.Get(R"(/.*)", [](const httplib::Request& request, httplib::Response& response) {
        const auto& path = request.path;

        if (path == "/assets/app.js") {
            setAsset(response, reinterpret_cast<const char*>(BinaryData::app_js),
                     BinaryData::app_jsSize, "text/javascript; charset=utf-8");
            return;
        }

        if (path == "/assets/index.css") {
            setAsset(response, reinterpret_cast<const char*>(BinaryData::index_css),
                     BinaryData::index_cssSize, "text/css; charset=utf-8");
            return;
        }

        if (isAssetPath(path) || hasExtension(path)) {
            response.status = 404;
            response.set_content("Not found", "text/plain; charset=utf-8");
            return;
        }

        if (path.size() > 1 && path.back() == '/') {
            auto location = path.substr(0, path.size() - 1);

            if (!request.target.empty()) {
                const auto query = request.target.find('?');

                if (query != std::string::npos) location += request.target.substr(query);
            }

            response.status = 302;
            response.set_header("Location", location);
            return;
        }

        setIndex(response);
    });
}

}  // namespace Api::Routes
