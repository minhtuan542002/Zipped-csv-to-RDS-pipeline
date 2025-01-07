// ProcessCsvToDatabase.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <cstdlib> 
#include <vector> 
#include <aws/core/Aws.h>
#include "aspell.h"

std::string getEnvironmentValue(std::string const& key) {
	char* value = getenv(key.c_str());
	std::string envValue;
	if (value != NULL) {
		envValue = value;
	}
	return envValue;
}

std::string checkSpelling(const std::string& word, AspellSpeller* speller) {
	if (aspell_speller_check(speller, word.c_str(), word.size()) == 0) {
		const AspellWordList* suggestions = aspell_speller_suggest(
			speller, word.c_str(), word.size());
		AspellStringEnumeration* elements = aspell_word_list_elements(suggestions);
		const char* suggestion;
		suggestion = aspell_string_enumeration_next(elements);
		delete_aspell_string_enumeration(elements);
		if (suggestion != NULL) {
			return suggestion;
		}
	}
	return "";
}

bool isEnglish(Aws::String languageCode) {
	std::vector<std::string> englishCodes = {
			"eng", "en", "english", "en-uk", "en-gb", "en-us"};
	std::string lowerCode(languageCode.c_str(), languageCode.size());
	for (char& c : lowerCode) { 
		c = tolower(c); 
	}
	for (auto const& code : englishCodes) {
		if (lowerCode == code) {
			return true;
		}
	}
	return false;
}