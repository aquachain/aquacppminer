#include <curl/curl.h>
#include <assert.h>
#include <string>
#include <vector>
#include <cstring>

#include "http.h"

using std::string;

// inspired from https://curl.haxx.se/libcurl/c/getinmemory.html
struct MemoryStruct
{
	char* memory;
	size_t size;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct*)userp;

	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		printf("FATAL ERROR: httpGetString, not enough memory (realloc returned NULL), aborting now\n");
		exit(1);
		return 0;
	}

	std::memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

http_connection_handle_t newHttpConnectionHandle() {
	CURL* curlHandle = curl_easy_init();
	return (http_connection_handle_t)curlHandle;
}

void destroyHttpConnectionHandle(http_connection_handle_t h) {
	CURL* curlHandle = (CURL*)h;
	if (curlHandle) {
		curl_easy_cleanup(curlHandle);
	}
}



// inspired from: https://raw.githubusercontent.com/curl/curl/master/docs/examples/postinmemory.c
// ex: httpPostUrlEncodedRaw("http://www.example.org/", "Field=1&Field=2&Field=3")
// warning this function will have undefined behavior if same handle is used simultaneously by multiple threads
// (so user is responsible for thread safety)
bool httpPost(
	http_connection_handle_t handle,
	const std::string& url,
	const std::string& postData,
	std::string& out,
	const std::vector<std::string>* pHeaderLines)
{
	CURLcode res = CURLE_FAILED_INIT;
	out.clear();

	if (!handle) {
		return false;
	}

	struct MemoryStruct chunk;
	chunk.memory = (char*)malloc(1);  /* will be grown as needed by realloc above */
	chunk.size = 0;                   /* no data at this point */

	CURL *curlHandle = (CURL*)handle;
	if (curlHandle) {
		// A parameter set to 1 tells libcurl to do a regular HTTP post. This will also make the library use a "Content-Type: application/x-www-form-urlencoded" header. 
		curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);

		if (pHeaderLines) {
			struct curl_slist *headers = NULL;
			for (auto it : *pHeaderLines) {
				headers = curl_slist_append(headers, it.c_str());
			}
			curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
		}

		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, postData.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, postData.size());
		curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void *)&chunk);

		res = curl_easy_perform(curlHandle);
		if (res == CURLE_OK) {
			std::vector<char> bytes;
			bytes.resize(chunk.size, 0);
			std::memcpy(bytes.data(), chunk.memory, chunk.size);

			out = std::string(bytes.data(), bytes.data() + bytes.size());
		}
	}

	free(chunk.memory);

	// todo: show error message on failure via: curl_easy_strerror(res));
	return (res == CURLE_OK);
}
