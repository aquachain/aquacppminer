#pragma once

#include <vector>
#include <string>

typedef void* http_connection_handle_t;

http_connection_handle_t newHttpConnectionHandle();
void destroyHttpConnectionHandle(http_connection_handle_t h);

void setGlobalProxy(std::string s);

bool httpPost(
	http_connection_handle_t handle,
	const std::string& url,
	const std::string& postData,
	std::string& out,
	const std::vector<std::string>* pHeaderLines = 0);