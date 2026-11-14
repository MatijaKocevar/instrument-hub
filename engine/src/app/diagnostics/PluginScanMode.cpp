#include "app/DiagnosticModes.h"

#include "plugins/Scanner.h"

#include <iostream>

namespace ihub::app
{

    int runPluginScan (const juce::String& commandLine)
    {
        const auto tokens = juce::StringArray::fromTokens (commandLine, true);
        juce::StringArray directories;
        ihub::plugins::Scanner::Options options;
        bool sawFlag = false;

        for (int i = 0; i < tokens.size (); ++i)
        {
            const auto& token = tokens[i];

            if (token == "--plugin-data-dir")
            {
                if (i + 1 < tokens.size ())
                    options.dataDirectory = juce::File (tokens[++i].unquoted ());

                continue;
            }

            if (token == "--scan-plugins")
            {
                sawFlag = true;
                continue;
            }

            if (sawFlag && !token.startsWith ("--"))
                directories.add (token.unquoted ());
        }

        options.logSink = [] (const juce::String& line) { std::cout << line << std::endl; };

        ihub::plugins::Scanner scanner (options);
        const auto report = scanner.scan (directories);

        std::cout << "scan-plugins: " << report.filesDiscovered << " discovered, "
                  << report.succeeded << " ok, " << report.failed << " failed"
                  << " (app survived), report=" << report.reportFile.getFullPathName ()
                  << (report.wroteReport ? "" : " (NOT written)") << std::endl;

        return report.helperAvailable && report.wroteReport ? 0 : 1;
    }

}
