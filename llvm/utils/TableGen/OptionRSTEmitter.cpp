//===- OptionRSTEmitter.cpp - Table Driven Command Line Option Parsing ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Common/OptEmitter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;

/// This tablegen backend takes an input .td file describing a list of options
/// and emits a RST man page.
static void emitOptionRst(const RecordKeeper &Records, raw_ostream &OS) {
  llvm::StringMap<std::vector<const Record *>> OptionsByGroup;

  // Get the options.
  std::vector<const Record *> Opts = Records.getAllDerivedDefinitions("Option");
  llvm::sort(Opts, IsOptionRecordsLess);

  // Get the option groups.
  for (const Record *R : Records.getAllDerivedDefinitions("OptionGroup"))
    OptionsByGroup.try_emplace(R->getValueAsString("Name"));

  // Map options to their group.
  for (const Record *R : Opts) {
    if (const DefInit *DI = dyn_cast<DefInit>(R->getValueInit("Group")))
      OptionsByGroup[DI->getDef()->getValueAsString("Name")].push_back(R);
    else
      OptionsByGroup["options"].push_back(R);
  }

  // Print options under their group.
  for (const auto &KV : OptionsByGroup) {
    std::string GroupName = KV.getKey().upper();
    OS << GroupName << '\n';
    OS << std::string(GroupName.size(), '-') << '\n';
    OS << '\n';

    for (const Record *R : KV.getValue()) {
      OS << ".. option:: ";

      // Print the prefix.
      std::vector<StringRef> Prefixes = R->getValueAsListOfStrings("Prefixes");
      if (!Prefixes.empty())
        OS << Prefixes[0];

      // Print the option name.
      OS << R->getValueAsString("Name");

      StringRef MetaVarName;
      // Print the meta-variable.
      if (!isa<UnsetInit>(R->getValueInit("MetaVarName"))) {
        MetaVarName = R->getValueAsString("MetaVarName");
      } else if (!isa<UnsetInit>(R->getValueInit("Values")))
        MetaVarName = "<value>";

      if (!MetaVarName.empty()) {
        OS << '=';
        OS.write_escaped(MetaVarName);
      }

      OS << "\n\n";

      std::string HelpText;
      // The option help text.
      if (!isa<UnsetInit>(R->getValueInit("HelpText"))) {
        HelpText = R->getValueAsString("HelpText").trim().str();
        if (!HelpText.empty() && HelpText.back() != '.')
          HelpText.push_back('.');
      }

      if (!isa<UnsetInit>(R->getValueInit("Values"))) {
        SmallVector<StringRef> Values;
        SplitString(R->getValueAsString("Values"), Values, ",");
        HelpText += (" " + MetaVarName + " must be '").str();

        if (Values.size() > 1) {
          HelpText += join(Values.begin(), Values.end() - 1, "', '");
          HelpText += "' or '";
        }
        HelpText += (Values.back() + "'.").str();
      }

      if (!HelpText.empty()) {
        OS << ' ';
        OS.write_escaped(HelpText);
        OS << "\n\n";
      }
    }
  }
}

static TableGen::Emitter::Opt X("gen-opt-rst", emitOptionRst,
                                "Generate option RST");
