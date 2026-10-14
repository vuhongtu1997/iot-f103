#ifndef __XLOG_H__
#define __XLOG_H__

#include <stdarg.h>
#include <stdint.h>
#include <time.h>


#ifndef bswap_16
#define bswap_16(x) __builtin_bswap16(x)
#endif
#ifndef bswap_32
#define bswap_32(x) __builtin_bswap32(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  XLOG_NONE,  /*!< No log output */
  XLOG_ERROR, /*!< Critical errors, software module can not recover on its own
               */
  XLOG_WARN,  /*!< Error conditions from which recovery measures have been taken
               */
  XLOG_INFO,  /*!< Information messages which describe normal flow of events */
  XLOG_DEBUG, /*!< Extra information which is not necessary for normal use
                 (values, pointers, sizes, etc). */
  XLOG_VERBOSE /*!< Bigger chunks of debugging information, or frequent messages
                  which can potentially flood the output. */
} log_level_t;

extern log_level_t log_level;

typedef int (*vprintf_like_t)(const char *, va_list);

char *timestr(void);

void log_set_level(log_level_t log_level_); // {log_level = log_level_;}

void log_set_vprintf(vprintf_like_t func);

void log_write(const char *format, ...) __attribute__((format(printf, 1, 2)));

char *log_cut_str(char *full_path, uint8_t len);

#define CONFIG_XLOG_COLORS 1

#if CONFIG_XLOG_COLORS
#define XLOG_COLOR_BLACK "30"
#define XLOG_COLOR_RED "31"
#define XLOG_COLOR_GREEN "32"
#define XLOG_COLOR_BROWN "33"
#define XLOG_COLOR_BLUE "34"
#define XLOG_COLOR_PURPLE "35"
#define XLOG_COLOR_CYAN "36"
#define XLOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define XLOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define XLOG_RESET_COLOR "\033[0m"
#define XLOG_COLOR_E XLOG_COLOR(XLOG_COLOR_RED)
#define XLOG_COLOR_W XLOG_COLOR(XLOG_COLOR_BROWN)
#define XLOG_COLOR_I XLOG_COLOR(XLOG_COLOR_GREEN)
#define XLOG_COLOR_D XLOG_COLOR(XLOG_COLOR_BLUE)
#define XLOG_COLOR_V
#else // CONFIG_XLOG_COLORS
#define XLOG_COLOR_E
#define XLOG_COLOR_W
#define XLOG_COLOR_I
#define XLOG_COLOR_D
#define XLOG_COLOR_V
#define XLOG_RESET_COLOR
#endif // CONFIG_XLOG_COLORS

#define FUNC_NAME_LEN 10
#define FILE_NAME_LEN 20

#define XLOG_FORMAT(letter, format)                                            \
  XLOG_COLOR_##letter #letter                                                  \
      " (%s)(%10s)(..%12s)(%4d): " format XLOG_RESET_COLOR "\n"

#define XLOG_DEFAULT_LEVEL XLOG_INFO
#ifndef XLOG_LOCAL_LEVEL
#define XLOG_LOCAL_LEVEL ((log_level_t)XLOG_DEFAULT_LEVEL)
#endif

#define LOGE(format, ...)                                                      \
  if (log_level >= XLOG_ERROR) {                                               \
    log_write(XLOG_FORMAT(E, format), timestr(),                               \
              log_cut_str((char *)__FUNCTION__, FUNC_NAME_LEN),                \
              log_cut_str((char *)__FILE__, FILE_NAME_LEN), __LINE__,          \
              ##__VA_ARGS__);                                                  \
  }
#define LOGW(format, ...)                                                      \
  if (log_level >= XLOG_WARN) {                                                \
    log_write(XLOG_FORMAT(W, format), timestr(),                               \
              log_cut_str((char *)__FUNCTION__, FUNC_NAME_LEN),                \
              log_cut_str((char *)__FILE__, FILE_NAME_LEN), __LINE__,          \
              ##__VA_ARGS__);                                                  \
  }
#define LOGI(format, ...)                                                      \
  if (log_level >= XLOG_INFO) {                                                \
    log_write(XLOG_FORMAT(I, format), timestr(),                               \
              log_cut_str((char *)__FUNCTION__, FUNC_NAME_LEN),                \
              log_cut_str((char *)__FILE__, FILE_NAME_LEN), __LINE__,          \
              ##__VA_ARGS__);                                                  \
  }
#define LOGD(format, ...)                                                      \
  if (log_level >= XLOG_DEBUG) {                                               \
    log_write(XLOG_FORMAT(D, format), timestr(),                               \
              log_cut_str((char *)__FUNCTION__, FUNC_NAME_LEN),                \
              log_cut_str((char *)__FILE__, FILE_NAME_LEN), __LINE__,          \
              ##__VA_ARGS__);                                                  \
  }
#define LOGV(format, ...)                                                      \
  if (log_level >= XLOG_VERBOSE) {                                             \
    log_write(XLOG_FORMAT(V, format), timestr(),                               \
              log_cut_str((char *)__FUNCTION__, FUNC_NAME_LEN),                \
              log_cut_str((char *)__FILE__, FILE_NAME_LEN), __LINE__,          \
              ##__VA_ARGS__);                                                  \
  }

#ifdef __cplusplus
}
#endif

#endif /* XLOG_H__ */
