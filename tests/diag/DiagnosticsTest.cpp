#include "TestSupport.h"
#include "Concurrency.h"
#include "CrashFormat.h"
#include "Names.h"
#include "Plugins.h"
#include "Records.h"
#include "Report.h"
#include "SignalHandler.h"

#include <iostream>

int main()
{
    std::cout << "ihub-diagnostics-test" << std::endl;

    DiagTest::runStructuredRecords();
    DiagTest::runLevelAndSourceNames();
    DiagTest::runConcurrentWriters();
    DiagTest::runCrashDumpFormat();
    DiagTest::runSignalHandler();
    DiagTest::runPluginMerge();
    DiagTest::runFormattedReport();

    std::cout << "\n" << (DiagTest::failures == 0 ? "PASS" : "FAIL")
              << ": " << DiagTest::failures << " failure(s)" << std::endl;

    return DiagTest::failures == 0 ? 0 : 1;
}
