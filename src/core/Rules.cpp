#include "core/internal.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

namespace {

std::string trim_ascii(const std::string &value) {
	size_t start = 0;
	while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
		start++;
	}
	size_t end = value.size();
	while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
		end--;
	}
	return value.substr(start, end - start);
}

std::string to_lower_ascii(const std::string &value) {
	std::string out = value;
	for (char &ch : out) {
		if (ch >= 'A' && ch <= 'Z') {
			ch = static_cast<char>(ch - 'A' + 'a');
		}
	}
	return out;
}

bool parse_bool(const std::string &value, bool *out) {
	const std::string lower = to_lower_ascii(value);
	if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") {
		*out = true;
		return true;
	}
	if (lower == "0" || lower == "false" || lower == "no" || lower == "off") {
		*out = false;
		return true;
	}
	return false;
}

bool contains_case_insensitive(const std::string &text, const std::string &needle) {
	if (needle.empty()) {
		return true;
	}
	const std::string text_lower = to_lower_ascii(text);
	const std::string needle_lower = to_lower_ascii(needle);
	return text_lower.find(needle_lower) != std::string::npos;
}

} // namespace

void server_apply_window_rules(KristalView *view, const char *title, const char *app_id) {
	if (view == nullptr) {
		return;
	}
	const char *env = getenv("KRISTAL_WINDOW_RULES");
	if (env == nullptr || env[0] == '\0') {
		return;
	}

	const std::string title_str = title ? title : "";
	const std::string app_id_str = app_id ? app_id : "";

	std::stringstream rules_stream(env);
	std::string rule_entry;
	while (std::getline(rules_stream, rule_entry, ';')) {
		rule_entry = trim_ascii(rule_entry);
		if (rule_entry.empty()) {
			continue;
		}

		std::string rule_app_id;
		std::string rule_title;
		int rule_workspace = 0;
		bool rule_floating = false;
		bool floating_set = false;

		std::stringstream rule_stream(rule_entry);
		std::string token;
		while (std::getline(rule_stream, token, ',')) {
			const auto eq = token.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			std::string key = to_lower_ascii(trim_ascii(token.substr(0, eq)));
			std::string value = trim_ascii(token.substr(eq + 1));
			if (key == "app_id") {
				rule_app_id = value;
			} else if (key == "title") {
				rule_title = value;
			} else if (key == "workspace") {
				rule_workspace = std::atoi(value.c_str());
			} else if (key == "floating") {
				bool parsed = false;
				bool bool_value = false;
				parsed = parse_bool(value, &bool_value);
				if (parsed) {
					floating_set = true;
					rule_floating = bool_value;
				}
			}
		}

		if (rule_app_id.empty() && rule_title.empty()) {
			continue;
		}
		if (!rule_app_id.empty() && rule_app_id != app_id_str) {
			continue;
		}
		if (!rule_title.empty() && !contains_case_insensitive(title_str, rule_title)) {
			continue;
		}

		if (rule_workspace >= 1 && rule_workspace <= view->server->workspace_count) {
			view->workspace = rule_workspace;
		}
		if (floating_set) {
			view->force_floating = rule_floating;
		}
		return;
	}
}
