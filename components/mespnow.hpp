#pragma once

#include "esp_now.h"

class MespNow
{
public:
    static MespNow getInstance()
    {
        static MespNow instance;
    }

    MespNow(const MespNow&) = delete;
    MespNow(MespNow&&) = delete;
    MespNow& operator=(const MespNow&) =delete;
    MespNow& operator=(MespNow&&) =delete;

    esp_err_t init();
    esp_err_t deInit();
    static esp_err_t getVersion(uint32_t *version)
    {
        return esp_now_get_version(version);
    }
private:
    MespNow() = default;
    ~MespNow() = default;
};