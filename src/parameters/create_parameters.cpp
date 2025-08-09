#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// helper structure describing a parameter row
struct Parameter {
  std::string param;
  std::string min;
  std::string max;
  std::string grain;
  std::string exp;
  std::string defaultVal;
  std::string automatable;
  std::string name;
  std::string suffix;
  std::string tooltip;
  std::vector<std::string> toStringArr;
};

// remove leading and trailing whitespace
static inline void trim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

// ensure float literals have suffix
std::string processFloatLiteral(const std::string &s) {
  return (s.find('.') == std::string::npos) ? s + ".0f" : s + "f";
}

// read csv and produce header file
int main() {
  std::ifstream csv("parameters.csv");
  if (!csv) {
    std::cerr << "Error: Could not open parameters.csv\n";
    return 1;
  }

  std::string header;
  std::getline(csv, header);

  std::vector<Parameter> params;
  std::string line;
  while (std::getline(csv, line)) {
    if (line.empty())
      continue;
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
      trim(token);
      tokens.push_back(token);
    }
    if (tokens.size() < 11)
      tokens.resize(11, "");

    Parameter p;
    p.param = tokens[0];
    p.min = processFloatLiteral(tokens[1]);
    p.max = processFloatLiteral(tokens[2]);
    p.grain = processFloatLiteral(tokens[3]);
    p.exp = processFloatLiteral(tokens[4]);
    p.defaultVal = processFloatLiteral(tokens[5]);
    p.automatable = tokens[6];
    p.name = tokens[7];
    p.suffix = tokens[8];
    p.tooltip = tokens[9];

    std::istringstream arrStream(tokens[10]);
    std::string arrToken;
    while (arrStream >> arrToken) {
      if (arrToken.size() >= 2 && arrToken.front() == '"' &&
          arrToken.back() == '"')
        arrToken = arrToken.substr(1, arrToken.size() - 2);
      p.toStringArr.push_back(arrToken);
    }
    params.push_back(p);
  }

  std::ofstream headerFile("ParameterDefines.h");
  if (!headerFile) {
    std::cerr << "Error: Could not open ParameterDefines.h for writing\n";
    return 1;
  }

  headerFile << "#pragma once\n#include <juce_core/juce_core.h>\n";

  headerFile << "enum PARAM {\n";
  for (const auto &p : params)
    headerFile << "\t" << p.param << ",\n";
  headerFile << "\tTOTAL_NUMBER_PARAMETERS\n};\n";

  headerFile << "static const std::array<juce::Identifier, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_IDS{\n";
  for (const auto &p : params)
    headerFile << "\t\"" << p.param << "\",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<juce::String, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> PARAMETER_NAMES{\n";
  for (const auto &p : params)
    headerFile << "\t\"" << p.param << "\",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<juce::NormalisableRange<float>, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> PARAMETER_RANGES {\n";
  for (const auto &p : params)
    headerFile << "\tjuce::NormalisableRange<float>(" << p.min << ", " << p.max
               << ", " << p.grain << ", " << p.exp << "),\n";
  headerFile << "};\n";

  headerFile << "static const std::array<float, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> PARAMETER_DEFAULTS {\n";
  for (const auto &p : params)
    headerFile << "\t" << p.defaultVal << ",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<bool, PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_AUTOMATABLE {\n";
  for (const auto &p : params)
    headerFile << "\t" << (p.automatable == "1" ? "true" : "false") << ",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<juce::String, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_NICKNAMES{\n";
  for (const auto &p : params)
    headerFile << "\t\"" << p.name << "\",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<juce::String, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_SUFFIXES {\n";
  for (const auto &p : params)
    headerFile << "\t\"" << p.suffix << "\",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<juce::String, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_TOOLTIPS {\n";
  for (const auto &p : params)
    headerFile << "\t\"" << p.tooltip << "\",\n";
  headerFile << "};\n";

  headerFile << "static const std::array<std::vector<juce::String>, "
                "PARAM::TOTAL_NUMBER_PARAMETERS> "
                "PARAMETER_TO_STRING_ARRS {\n";
  for (const auto &p : params) {
    headerFile << "\tstd::vector<juce::String>{";
    for (const auto &s : p.toStringArr)
      headerFile << "\"" << s << "\", ";
    headerFile << "},\n";
  }
  headerFile << "};\n";

  return 0;
}
