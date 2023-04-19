#pragma once

#include <vector>
#include <set>
#include <string>

// По смыслу он ближе к модулю search_server, чем к модулю document. Куда его лучше перенести?

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

//std::vector<std::string> SplitIntoWords(const std::string& text);
std::vector<std::string_view> SplitIntoWords(std::string_view str);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	std::set<std::string, std::less<>> non_empty_strings;
	for (std::string_view str : strings) {
		if (!str.empty()) {
			non_empty_strings.emplace(str);
		}
	}
	return non_empty_strings;
}
