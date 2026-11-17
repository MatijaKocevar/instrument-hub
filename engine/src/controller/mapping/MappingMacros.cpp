#include "controller/mapping/MappingDecode.h"

#include "controller/mapping/MappingDiagnostics.h"

#include <string>
#include <utility>

namespace Controller::MappingDetail {

bool decodeMacros(const YamlValue& root, SetMapping& out, MappingError& error) {
    const auto* macros = root.find("macros");

    if (macros == nullptr || macros->kind == YamlKind::nullValue) return true;

    if (macros->kind != YamlKind::mapping)
        return fail(error, *macros, "macros must be a mapping of macro ids");

    for (const auto& [macroId, macroValue] : macros->mapping) {
        if (macroId.empty()) return fail(error, macroValue, "a macro id must not be empty");

        if (macroValue.kind != YamlKind::mapping)
            return fail(error, macroValue, "macro '" + macroId + "' must be a mapping");

        if (!checkKeys(macroValue, {"name", "parameter", "min", "max", "default"}, error))
            return false;

        MacroSpec macro;
        macro.id = macroId;

        const auto* name = macroValue.find("name");
        const auto* parameter = macroValue.find("parameter");

        if (name == nullptr || parameter == nullptr)
            return fail(error, macroValue, "macro '" + macroId + "' needs name and parameter");

        if (!readString(*name, macro.name, error, "macro name") ||
            !readString(*parameter, macro.parameter, error, "macro parameter"))
            return false;

        if (const auto* minimum = macroValue.find("min"))
            if (!readFloat(*minimum, macro.minimum, error, "macro min")) return false;

        if (const auto* maximum = macroValue.find("max"))
            if (!readFloat(*maximum, macro.maximum, error, "macro max")) return false;

        if (!(macro.minimum < macro.maximum))
            return fail(error, macroValue, "macro '" + macroId + "' needs min < max");

        macro.defaultValue = macro.minimum;

        if (const auto* defaultValue = macroValue.find("default")) {
            if (!readFloat(*defaultValue, macro.defaultValue, error, "macro default")) return false;

            if (macro.defaultValue < macro.minimum || macro.defaultValue > macro.maximum)
                return fail(error, *defaultValue,
                            "macro '" + macroId + "' default is outside its range");
        }

        out.macros.emplace(macro.id, std::move(macro));
    }

    return true;
}

bool decodeBanks(const YamlValue& root, SetMapping& out, MappingError& error) {
    if (const auto* banks = root.find("banks")) {
        if (banks->kind != YamlKind::sequence)
            return fail(error, *banks, "banks must be a sequence");

        for (const auto& bankValue : banks->sequence) {
            if (bankValue.kind != YamlKind::mapping)
                return fail(error, bankValue, "a bank entry must be a mapping");

            if (!checkKeys(bankValue, {"bank", "name", "macros"}, error)) return false;

            const auto* bankNumber = bankValue.find("bank");
            const auto* name = bankValue.find("name");
            const auto* macroList = bankValue.find("macros");

            if (bankNumber == nullptr || name == nullptr || macroList == nullptr)
                return fail(error, bankValue, "a bank entry needs bank, name and macros");

            MacroBankSpec bank;

            if (!readInt(*bankNumber, bank.bank, error, "bank") ||
                !readString(*name, bank.name, error, "bank name"))
                return false;

            if (bank.bank < 0 || bank.bank >= SetMapping::maximumBanks)
                return fail(
                    error, *bankNumber,
                    "bank must be within 0.." + std::to_string(SetMapping::maximumBanks - 1));

            if (out.bankBinding(bank.bank) != nullptr)
                return fail(error, *bankNumber,
                            "duplicate bank binding for bank " + std::to_string(bank.bank));

            if (macroList->kind != YamlKind::sequence || macroList->sequence.empty())
                return fail(error, *macroList,
                            "bank " + std::to_string(bank.bank) + " needs a non-empty macros list");

            for (const auto& macroValue : macroList->sequence) {
                std::string macroId;

                if (!readString(macroValue, macroId, error, "bank macro")) return false;

                if (out.macros.find(macroId) == out.macros.end())
                    return fail(error, macroValue,
                                "bank " + std::to_string(bank.bank) + " names unknown macro '" +
                                    macroId + "'");

                bank.macros.push_back(std::move(macroId));
            }

            out.banks[static_cast<std::size_t>(bank.bank)] = std::move(bank);
        }
    }

    return true;
}

}  // namespace Controller::MappingDetail
