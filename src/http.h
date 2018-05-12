#pragma once

bool httpGetString(const std::string& url, std::string& out);
bool httpPostUrlEncoded(const std::string& url, const std::string& postFields, std::string& out);
