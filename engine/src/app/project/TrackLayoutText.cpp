#include "TrackLayoutText.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace ihub::app {

namespace {

struct TrackBlock {
  std::string id;
  std::size_t entryLine = 0;
  std::size_t endLine = 0;
  std::size_t keyIndent = 0;
};

std::size_t leadingSpaces(const std::string& line) noexcept {
  std::size_t count = 0;

  while (count < line.size() && line[count] == ' ') ++count;

  return count;
}

std::string trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r");

  if (first == std::string::npos) return {};

  const auto last = text.find_last_not_of(" \t\r");
  return text.substr(first, last - first + 1);
}

bool isIgnored(const std::string& line) {
  const auto text = trim(line);
  return text.empty() || text.front() == '#';
}

std::vector<std::string> splitLines(const std::string& text,
                                    bool& trailingNewline) {
  std::vector<std::string> lines;
  trailingNewline = !text.empty() && text.back() == '\n';

  std::size_t start = 0;

  while (start < text.size()) {
    const auto end = text.find('\n', start);

    if (end == std::string::npos) {
      lines.push_back(text.substr(start));
      break;
    }

    auto line = text.substr(start, end - start);

    if (!line.empty() && line.back() == '\r') line.pop_back();

    lines.push_back(std::move(line));
    start = end + 1;
  }

  return lines;
}

std::string joinLines(const std::vector<std::string>& lines,
                      bool trailingNewline, const std::string& newline) {
  std::string result;

  for (std::size_t index = 0; index < lines.size(); ++index) {
    if (index > 0) result += newline;

    result += lines[index];
  }

  if (trailingNewline) result += newline;

  return result;
}

std::string scalarText(const std::string& value) {
  const bool safe = !value.empty() && value.front() != '-' &&
                    value.front() != '?' && value.front() != ':' &&
                    value.front() != '#' &&
                    value.find_first_of(":#\n\r\t'\"") == std::string::npos;

  if (safe) return value;

  std::string quoted = "'";

  for (const char character : value)
    quoted += character == '\'' ? std::string("''") : std::string(1, character);

  quoted.push_back('\'');
  return quoted;
}

std::string readId(const std::string& line) {
  auto text = trim(line);

  if (text.rfind("-", 0) == 0) text = trim(text.substr(1));

  if (text.rfind("id:", 0) != 0) return {};

  text = trim(text.substr(3));

  if (text.empty() || text.front() == '#') return {};

  const char quote = text.front();

  if (quote == '"' || quote == '\'') {
    std::string value;

    for (std::size_t index = 1; index < text.size(); ++index) {
      if (text[index] != quote) {
        value.push_back(text[index]);
        continue;
      }

      if (quote == '\'' && index + 1 < text.size() && text[index + 1] == '\'') {
        value.push_back('\'');
        ++index;
        continue;
      }

      return value;
    }

    return {};
  }

  const auto end = text.find_first_of(" \t#");
  return end == std::string::npos ? text : text.substr(0, end);
}

std::vector<TrackBlock> findTrackBlocks(const std::vector<std::string>& lines,
                                        std::size_t tracksLine,
                                        std::size_t sectionEnd) {
  std::vector<TrackBlock> blocks;

  for (std::size_t index = tracksLine + 1; index < sectionEnd; ++index) {
    if (trim(lines[index]).rfind("-", 0) != 0) continue;

    const auto id = readId(lines[index]);

    if (id.empty()) continue;

    TrackBlock block;
    block.id = id;
    block.entryLine = index;

    std::size_t keyPosition = lines[index].find('-') + 1;

    while (keyPosition < lines[index].size() &&
           lines[index][keyPosition] == ' ')
      ++keyPosition;

    block.keyIndent = keyPosition;
    blocks.push_back(std::move(block));
  }

  for (std::size_t index = 0; index < blocks.size(); ++index)
    blocks[index].endLine =
        index + 1 < blocks.size() ? blocks[index + 1].entryLine : sectionEnd;

  return blocks;
}

std::vector<std::string> hotFxLines(const std::vector<std::string>& hotFx,
                                    std::size_t keyIndent) {
  std::size_t lastAssigned = 0;
  bool anyAssigned = false;

  for (std::size_t index = 0; index < hotFx.size(); ++index) {
    if (hotFx[index].empty()) continue;

    lastAssigned = index + 1;
    anyAssigned = true;
  }

  if (!anyAssigned) return {};

  const std::string indent(keyIndent, ' ');
  std::vector<std::string> lines{indent + "hotFx:"};

  for (std::size_t index = 0; index < lastAssigned; ++index)
    lines.push_back(indent + "  - " + scalarText(hotFx[index]));

  return lines;
}

void replaceName(std::vector<std::string>& lines, TrackBlock& block,
                 const std::string& name) {
  if (name.empty()) return;

  const std::string indent(block.keyIndent, ' ');
  const std::string replacement = indent + "name: " + scalarText(name);

  for (std::size_t index = block.entryLine; index < block.endLine; ++index) {
    if (leadingSpaces(lines[index]) != block.keyIndent) continue;

    if (trim(lines[index]).rfind("name:", 0) != 0) continue;

    lines[index] = replacement;
    return;
  }

  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(block.entryLine + 1),
               replacement);
  ++block.endLine;
}

void replaceHotFx(std::vector<std::string>& lines, const TrackBlock& block,
                  const std::vector<std::string>& replacement) {
  std::size_t hotStart = block.endLine;
  std::size_t hotEnd = block.endLine;

  for (std::size_t index = block.entryLine + 1; index < block.endLine;
       ++index) {
    if (leadingSpaces(lines[index]) != block.keyIndent) continue;

    if (trim(lines[index]).rfind("hotFx:", 0) != 0) continue;

    hotStart = index;
    hotEnd = index + 1;

    while (hotEnd < block.endLine && !trim(lines[hotEnd]).empty() &&
           leadingSpaces(lines[hotEnd]) > block.keyIndent)
      ++hotEnd;

    break;
  }

  if (hotStart != block.endLine) {
    lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(hotStart),
                lines.begin() + static_cast<std::ptrdiff_t>(hotEnd));
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(hotStart),
                 replacement.begin(), replacement.end());
    return;
  }

  if (replacement.empty()) return;

  std::size_t insertAt = block.entryLine + 1;

  for (std::size_t index = block.entryLine + 1; index < block.endLine;
       ++index) {
    if (isIgnored(lines[index]) ||
        leadingSpaces(lines[index]) < block.keyIndent)
      continue;

    insertAt = index + 1;
  }

  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAt),
               replacement.begin(), replacement.end());
}

}  // namespace

bool rewriteTrackLayout(const std::string& mappingYaml,
                        const std::vector<TrackLayoutUpdate>& updates,
                        std::string& destination) {
  destination = mappingYaml;

  if (updates.empty()) return true;

  bool trailingNewline = false;
  auto lines = splitLines(mappingYaml, trailingNewline);

  std::size_t tracksLine = lines.size();

  for (std::size_t index = 0; index < lines.size(); ++index)
    if (leadingSpaces(lines[index]) == 0 && trim(lines[index]) == "tracks:") {
      tracksLine = index;
      break;
    }

  if (tracksLine == lines.size()) return false;

  std::size_t sectionEnd = lines.size();

  for (std::size_t index = tracksLine + 1; index < lines.size(); ++index) {
    if (isIgnored(lines[index])) continue;

    if (leadingSpaces(lines[index]) == 0) {
      sectionEnd = index;
      break;
    }
  }

  auto blocks = findTrackBlocks(lines, tracksLine, sectionEnd);

  for (auto block = blocks.rbegin(); block != blocks.rend(); ++block) {
    const auto update =
        std::find_if(updates.begin(), updates.end(),
                     [&block](const TrackLayoutUpdate& candidate) {
                       return candidate.trackId == block->id;
                     });

    if (update == updates.end()) continue;

    replaceName(lines, *block, update->name);
    replaceHotFx(lines, *block, hotFxLines(update->hotFx, block->keyIndent));
  }

  destination =
      joinLines(lines, trailingNewline,
                mappingYaml.find("\r\n") != std::string::npos ? "\r\n" : "\n");
  return true;
}

}  // namespace ihub::app
