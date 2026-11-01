#pragma once

#include <string>

namespace MappingTest
{

void expectInvalid (const std::string& label,
                    const std::string& yaml,
                    const std::string& expectedFragment);

void runStructureChecks();
void runValueChecks();

}
