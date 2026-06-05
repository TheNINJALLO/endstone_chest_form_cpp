#pragma once

#ifdef _WIN32
    #ifdef CHEST_FORM_API_EXPORTS
        #define CHEST_FORM_API __declspec(dllexport)
    #else
        #define CHEST_FORM_API __declspec(dllimport)
    #endif
#else
    #define CHEST_FORM_API __attribute__((visibility("default")))
#endif
