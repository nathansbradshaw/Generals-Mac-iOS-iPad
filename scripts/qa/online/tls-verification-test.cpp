// GeneralsX @bugfix Codex 09/10/2026 Exercise the production TLS policy with repeated real libcurl connections.
#include "GameNetwork/GeneralsOnline/HTTP/TLSVerification.h"
#include <cstdio>
#include <cstring>

static size_t DiscardBody(char*, size_t size, size_t count, void*)
{
    return size * count;
}

int main(int argc, char** argv)
{
    if (argc != 3 || curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return 2;
    const bool websocket = std::strcmp(argv[2], "ws") == 0;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        CURL* handle = curl_easy_init();
        if (handle == nullptr)
            return 3;
        ConfigureCurlTLSVerification(handle);
        char error[CURL_ERROR_SIZE] = {};
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, error);
        curl_easy_setopt(handle, CURLOPT_URL, argv[1]);
        curl_easy_setopt(handle, CURLOPT_PROXY, "");
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 5000L);
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, DiscardBody);
        if (websocket)
            curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);
        const CURLcode result = curl_easy_perform(handle);
        std::printf("%d\n", static_cast<int>(result));
        if (result != CURLE_OK)
            std::fprintf(stderr, "%s\n", error);
        curl_easy_cleanup(handle);
    }
    curl_global_cleanup();
    return 0;
}
