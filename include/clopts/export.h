#ifndef CLOPTS_EXPORT_H
#define CLOPTS_EXPORT_H

#if defined(CLOPTS_STATIC)
#define CLOPTS_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(CLOPTS_BUILDING)
#define CLOPTS_API __declspec(dllexport)
#else
#define CLOPTS_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define CLOPTS_API __attribute__((visibility("default")))
#else
#define CLOPTS_API
#endif

#endif /* CLOPTS_EXPORT_H */
