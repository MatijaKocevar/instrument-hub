#include "api/PluginBridge.h"
#include "api/Server.h"
#include "plugins/PluginInventory.h"

#include <httplib.h>
#include <juce_core/juce_core.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef IHUB_TESTPLUGIN_VST3
#define IHUB_TESTPLUGIN_VST3 ""
#endif

#ifndef IHUB_SCANNER_HELPER
#define IHUB_SCANNER_HELPER ""
#endif

namespace
{
int failures = 0;

void check (bool condition, const std::string& what)
{
    std::cout << (condition ? "PASS " : "FAIL ") << what << std::endl;

    if (! condition)
        ++failures;
}

juce::var parseJson (const std::string& text)
{
    return juce::JSON::parse (juce::String (text));
}

juce::var property (const juce::var& object, const char* name)
{
    if (! object.isObject() || object.getDynamicObject() == nullptr)
        return {};

    return object.getDynamicObject()->getProperty (name);
}

std::string textProperty (const juce::var& object, const char* name)
{
    return property (object, name).toString().toStdString();
}

int intProperty (const juce::var& object, const char* name)
{
    return static_cast<int> (property (object, name));
}

const juce::Array<juce::var>* pluginArray (const juce::var& body)
{
    return property (body, "plugins").getArray();
}

juce::var findPlugin (const juce::var& body, const juce::String& namePart)
{
    if (const auto* plugins = pluginArray (body))
    {
        for (const auto& plugin : *plugins)
            if (textProperty (plugin, "name").find (namePart.toStdString()) != std::string::npos)
                return plugin;
    }

    return {};
}

juce::var getJson (httplib::Client& client, const std::string& path)
{
    const auto response = client.Get (path.c_str());

    if (! response || response->status != 200)
        return {};

    return parseJson (response->body);
}

bool rescan (httplib::Client& client, ihub::plugins::PluginInventory& inventory)
{
    const auto response = client.Post ("/plugins/rescan");

    if (! response || (response->status != 200 && response->status != 202))
        return false;

    const auto body = parseJson (response->body);
    check (property (body, "started").isBool(), "POST /plugins/rescan reports started");

    if (! inventory.waitForScan (60000))
        return false;

    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File sourceBundle { juce::String (IHUB_TESTPLUGIN_VST3) };
    const juce::File helper { juce::String (IHUB_SCANNER_HELPER) };

    check (sourceBundle.isDirectory(), "TestPlugin.vst3 bundle exists");
    check (helper.existsAsFile(), "ihub-scanner helper exists");

    if (failures != 0)
        return 1;

    const auto workspace = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ihub-plugin-inventory-"
                                              + juce::Uuid().toString());

    workspace.deleteRecursively();
    workspace.createDirectory();

    const auto scanDirectory = workspace.getChildFile ("vst3");
    const auto dataDirectory = workspace.getChildFile ("user-data");
    scanDirectory.createDirectory();
    dataDirectory.createDirectory();

    const auto installedBundle = scanDirectory.getChildFile ("TestPlugin.vst3");

    check (sourceBundle.copyDirectoryTo (installedBundle),
           "copied TestPlugin.vst3 into the isolated VST3 directory");

    ihub::plugins::PluginInventory::Options inventoryOptions;
    inventoryOptions.dataDirectory = dataDirectory;
    inventoryOptions.helperExecutable = helper;
    inventoryOptions.searchDirectories.add (scanDirectory.getFullPathName());
    inventoryOptions.timeoutMs = 20000;
    inventoryOptions.logSink = [] (const juce::String& line)
    {
        std::cout << "scan: " << line << std::endl;
    };

    ihub::plugins::PluginInventory inventory (inventoryOptions);

    Api::StateStore state (64);

    Api::Server::Options serverOptions;
    serverOptions.address = "127.0.0.1";
    serverOptions.port = 0;
    serverOptions.meterHz = 10;

    Api::Server server (state, serverOptions);
    Api::attachPluginInventory (server, inventory);

    if (! server.start())
    {
        std::cerr << "plugin-inventory-api-test: FAILED to start the server" << std::endl;
        return 1;
    }

    std::cout << "plugin-inventory-api-test: listening on 127.0.0.1:"
              << server.getPort() << std::endl;

    httplib::Client client ("127.0.0.1", server.getPort());
    client.set_connection_timeout (2, 0);
    client.set_read_timeout (40, 0);

    auto body = getJson (client, "/plugins");

    check (body.isObject(), "GET /plugins returns a JSON object before a scan");
    check (intProperty (body, "count") == 0, "the initial inventory is empty");
    check (! static_cast<bool> (property (body, "scanning")),
           "the initial inventory is not scanning");

    check (rescan (client, inventory), "POST /plugins/rescan completes");

    body = getJson (client, "/plugins");

    check (body.isObject(), "GET /plugins returns a JSON object after a scan");
    check (static_cast<bool> (property (body, "scanned")),
           "the inventory reports that it has been scanned");
    check (! static_cast<bool> (property (body, "scanning")),
           "the inventory is idle after the scan");
    check (intProperty (body, "total") >= 1,
           "the scan found at least one VST3 type");
    check (intProperty (body, "count") == intProperty (body, "total"),
           "an unfiltered list reports every installed type");

    const auto testPlugin = findPlugin (body, "TestPlugin");

    check (testPlugin.isObject(), "GET /plugins lists TestPlugin");
    check (textProperty (testPlugin, "vendor") == "Instrument Hub",
           "TestPlugin carries its vendor");
    check (! textProperty (testPlugin, "version").empty(),
           "TestPlugin carries a version");
    check (! textProperty (testPlugin, "category").empty(),
           "TestPlugin carries a category");
    check (! static_cast<bool> (property (testPlugin, "isInstrument")),
           "TestPlugin is classified as an effect, not an instrument");
    check (static_cast<bool> (property (testPlugin, "isEffect")),
           "TestPlugin's isEffect flag is set");
    check (textProperty (testPlugin, "format").find ("VST3") != std::string::npos,
           "TestPlugin reports the VST3 format");
    check (textProperty (testPlugin, "path").find ("TestPlugin.vst3") != std::string::npos,
           "TestPlugin carries its bundle path");
    check (! textProperty (testPlugin, "id").empty(),
           "TestPlugin carries a stable id");

    auto filtered = getJson (client, "/plugins?search=testplugin");

    check (intProperty (filtered, "count") == 1,
           "search is case-insensitive on the plugin name");
    check (intProperty (filtered, "total") >= 1,
           "a filtered response still reports the unfiltered total");

    filtered = getJson (client, "/plugins?search=Instrument%20Hub");

    check (intProperty (filtered, "count") == 1,
           "search matches the vendor");

    filtered = getJson (client, "/plugins?search=no-such-plugin");

    check (intProperty (filtered, "count") == 0
               && intProperty (filtered, "total") >= 1,
           "search can return an empty filtered list");

    check (installedBundle.deleteRecursively(), "removed TestPlugin.vst3 from the scan directory");
    check (rescan (client, inventory), "rescan after removing the bundle completes");

    body = getJson (client, "/plugins");
    check (intProperty (body, "count") == 0,
           "a rescan removes a bundle that is no longer installed");

    check (sourceBundle.copyDirectoryTo (installedBundle),
           "copied TestPlugin.vst3 back into the scan directory");
    check (rescan (client, inventory), "rescan after reinstalling the bundle completes");

    body = getJson (client, "/plugins");
    check (intProperty (body, "count") >= 1 && findPlugin (body, "TestPlugin").isObject(),
           "a rescan picks up a newly installed bundle");

    server.stop();

    workspace.deleteRecursively();

    std::cout << (failures == 0 ? "plugin-inventory-api-test: PASS"
                                : "plugin-inventory-api-test: FAIL")
              << " (" << failures << " failure(s))" << std::endl;

    return failures == 0 ? 0 : 1;
}
