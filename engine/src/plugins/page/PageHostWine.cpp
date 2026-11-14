#include "PageHostWine.h"

#include "PageHostProcessTree.h"

namespace ihub::plugins::page_detail
{

    juce::String winePrefixForBundle (const juce::File& bundle)
    {
        const auto windows = bundle.getChildFile ("Contents").getChildFile ("x86_64-win");

        juce::Array<juce::File> entries;
        windows.findChildFiles (entries, juce::File::findFilesAndDirectories, false);

        for (const auto& entry : entries)
        {
            const auto target = entry.getLinkedTarget ().getFullPathName ();
            const auto index = target.indexOf ("/drive_c/");

            if (index > 0)
                return target.substring (0, index);
        }

        return {};
    }

    void startWinePrefix (const juce::String& prefix)
    {
        try
        {
            juce::ChildProcess process;
            juce::StringArray arguments;
            arguments.add ("/usr/bin/env");
            arguments.add ("WINEPREFIX=" + prefix);
            arguments.add ("wineserver");
            arguments.add ("-p");

            if (process.start (arguments, 0))
                process.waitForProcessToFinish (2000);
        }
        catch (...)
        {
        }
    }

    void stopWinePrefixes (const juce::String& group, const juce::String& knownPrefix)
    {
        try
        {
            juce::StringArray prefixes;

            if (knownPrefix.isNotEmpty ())
                prefixes.add (knownPrefix);
            else
                for (const auto& prefix : winePrefixesMatching (group.toRawUTF8 ()))
                    prefixes.addIfNotAlreadyThere (juce::String (prefix.c_str ()));

            for (const auto& prefix : prefixes)
            {
                juce::ChildProcess process;
                juce::StringArray arguments;
                arguments.add ("/usr/bin/env");
                arguments.add ("WINEPREFIX=" + prefix);
                arguments.add ("wineserver");
                arguments.add ("-k");

                if (process.start (arguments, 0))
                    process.waitForProcessToFinish (5000);
            }
        }
        catch (...)
        {
        }
    }

    bool warmWinePrefix (const juce::String& prefix, int timeoutMs)
    {
        try
        {
            juce::ChildProcess process;
            juce::StringArray arguments;
            arguments.add ("/usr/bin/env");
            arguments.add ("WINEPREFIX=" + prefix);
            arguments.add ("wine");
            arguments.add ("cmd");
            arguments.add ("/c");
            arguments.add ("exit");

            if (!process.start (arguments, 0))
                return false;

            const bool finished = process.waitForProcessToFinish (timeoutMs);

            if (!finished)
                process.kill ();

            return finished && process.getExitCode () == 0;
        }
        catch (...)
        {
            return false;
        }
    }

}
