// ReadBookCsv.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <cstdlib> 

std::string getEnvironmentValue(std::string const& key) {
	char* value = getenv(key.c_str());
	std::string envValue;
	if (value != NULL) {
		envValue = value;
	}
	return envValue;
}