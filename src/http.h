#pragma once

#include <vector>
#include <string>

bool httpGetString(const std::string& url, std::string& out);
bool httpPost(
	const std::string& url,
	const std::string& postData,
	std::string& out,
	const std::vector<std::string>* pHeaderLines = 0);