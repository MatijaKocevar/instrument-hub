#include "WebView.h"

#if ! JUCE_DEBUG && JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    #include "BinaryData.h"
#endif

#include <cstddef>
#include <iostream>
#include <optional>

namespace
{
#if ! JUCE_DEBUG && JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
juce::WebBrowserComponent::Resource makeResource (const char* data, int size, const char* mimeType)
{
    juce::WebBrowserComponent::Resource resource;
    const auto* begin = reinterpret_cast<const std::byte*> (data);
    resource.data.assign (begin, begin + size);
    resource.mimeType = mimeType;
    return resource;
}

std::optional<juce::WebBrowserComponent::Resource> provideResource (const juce::String& requestedPath)
{
    auto path = requestedPath;

    if (path.isEmpty())
        path = "/";

    if (! path.startsWithChar ('/'))
        path = "/" + path;

    if (path == "/" || path == "/index.html")
        return makeResource (BinaryData::index_html,
                             BinaryData::index_htmlSize,
                             "text/html");

    if (path == "/assets/app.js")
        return makeResource (BinaryData::app_js,
                             BinaryData::app_jsSize,
                             "text/javascript");

    if (path == "/assets/index.css")
        return makeResource (BinaryData::index_css,
                             BinaryData::index_cssSize,
                             "text/css");

    // Never serve the HTML shell for a missing script or stylesheet: that
    // would surface as a confusing MIME error instead of a missing asset.
    if (path.startsWith ("/assets/"))
        return std::nullopt;

    // Client-side routes all render the same index.html.
    return makeResource (BinaryData::index_html,
                         BinaryData::index_htmlSize,
                         "text/html");
}
#endif

juce::WebBrowserComponent::Options makeOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend (juce::WebBrowserComponent::Options::Backend::defaultBackend)
                       .withKeepPageLoadedWhenBrowserIsHidden()
                       // On Linux the resource-provider URI scheme is installed as part
                       // of the native integration. We only ever load our own UI.
                       .withNativeIntegrationEnabled();

#if ! JUCE_DEBUG && JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
    options = options.withResourceProvider ([] (const juce::String& path)
    {
        return provideResource (path);
    });
#endif

    return options;
}
} // namespace

WebView::WebView (PageLoadedCallback onPageLoaded)
    : juce::WebBrowserComponent (makeOptions()),
      pageLoadedCallback (std::move (onPageLoaded))
{
    goToURL (initialUrl());
}

juce::String WebView::initialUrl()
{
#if JUCE_DEBUG
    return "http://localhost:5173";
#else
    #if JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE
        return juce::WebBrowserComponent::getResourceProviderRoot();
    #else
        return "about:blank";
    #endif
#endif
}

bool WebView::isDevelopmentBuild()
{
#if JUCE_DEBUG
    return true;
#else
    return false;
#endif
}

void WebView::pageFinishedLoading (const juce::String& url)
{
    if (pageLoadedCallback != nullptr)
        pageLoadedCallback (url);
}

bool WebView::pageLoadHadNetworkError (const juce::String& errorInfo)
{
    std::cerr << "webview: failed to load " << initialUrl() << ": " << errorInfo << std::endl;

    // Let the platform show its built-in error page rather than a blank view.
    return true;
}
