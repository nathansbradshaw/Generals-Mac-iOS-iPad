#pragma once

#include "GameNetwork/GeneralsOnline/Vendor/libcurl/curl.h"
#include <fstream>

// GeneralsX @bugfix Codex 09/10/2026 Keep every HTTP and WebSocket connection verified, including after certificate failures.
inline void ConfigureCurlTLSVerification(CURL* handle)
{
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);

    // Fresh handles retain libcurl's default trust configuration if the packaged bundle is absent.
    std::ifstream certFile("cacert.pem");
    if (certFile.good())
    {
        curl_easy_setopt(handle, CURLOPT_CAINFO, "cacert.pem");
    }
}
