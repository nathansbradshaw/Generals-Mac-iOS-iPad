// GeneralsX stub: sentry-native is not imported (telemetry excluded at
// friends-scale — see docs/port/go-online/NGMP_FILE_INVENTORY.md category c).
// Variadic no-op templates absorb every call the NGMP code makes.
#pragma once

struct sentry_value_t {};
typedef struct sentry_options_s sentry_options_t;
typedef enum { SENTRY_LEVEL_DEBUG = -1, SENTRY_LEVEL_INFO = 0, SENTRY_LEVEL_WARNING = 1,
               SENTRY_LEVEL_ERROR = 2, SENTRY_LEVEL_FATAL = 3 } sentry_level_t;
#define SENTRY_BACKEND_NONE nullptr

inline sentry_options_t* sentry_options_new() { return nullptr; }
template <typename... A> inline void sentry_options_set_backend(A&&...) {}
template <typename... A> inline void sentry_options_set_database_path(A&&...) {}
template <typename... A> inline void sentry_options_set_debug(A&&...) {}
template <typename... A> inline void sentry_options_set_dsn(A&&...) {}
template <typename... A> inline void sentry_options_set_environment(A&&...) {}
template <typename... A> inline void sentry_options_set_logger(A&&...) {}
template <typename... A> inline void sentry_options_set_logger_level(A&&...) {}
template <typename... A> inline void sentry_options_set_release(A&&...) {}
template <typename... A> inline int  sentry_init(A&&...) { return 0; }
template <typename... A> inline int  sentry_close(A&&...) { return 0; }
template <typename... A> inline sentry_value_t sentry_value_new_object(A&&...) { return {}; }
template <typename... A> inline sentry_value_t sentry_value_new_string(A&&...) { return {}; }
template <typename... A> inline sentry_value_t sentry_value_new_message_event(A&&...) { return {}; }
template <typename... A> inline sentry_value_t sentry_value_new_int32(A&&...) { return {}; }
template <typename... A> inline void sentry_value_set_by_key(A&&...) {}
template <typename... A> inline void sentry_set_context(A&&...) {}
template <typename... A> inline void sentry_set_extra(A&&...) {}
template <typename... A> inline void sentry_set_tag(A&&...) {}
template <typename... A> inline sentry_value_t sentry_capture_event(A&&...) { return {}; }
