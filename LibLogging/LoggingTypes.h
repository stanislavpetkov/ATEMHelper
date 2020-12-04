#pragma once

enum class LOGLEVEL { Debug = 0, Info, Warn, Error, Fatal, Performance, MaxLevels };
constexpr const char * _log_levels_[(unsigned long long)LOGLEVEL::MaxLevels] = { "DEBUG", "INFO", "WARN", "ERROR",  "FATAL", "PERFORMANCE" };


enum class LOGTYPE { System = 0, Audit, Stats, MaxTypes };
constexpr const char* _log_types_[(unsigned long long)LOGTYPE::MaxTypes] = { "SYSTEM", "AUDIT", "STATS" };

typedef void (*LogEventCallback)(LOGLEVEL level, const char* submodule, const char* message);

enum class LoggingTarget { ODS, IPC, CONSOLE, FILE };

constexpr long long MAX_LOG_FILE_SIZE = 8 * 1024 * 1024;

#define MacroStr(x)   #x
#define MacroStr2(x)  MacroStr(x)
#define Message(desc) __pragma(message(__FILE__ "(" MacroStr2(__LINE__) ") :" #desc))

#define Stringize( L )     #L 
#define MakeString( M, L ) M(L)
#define $Line MakeString( Stringize, __LINE__ )

#define Reminder __FILE__ "(" $Line ") : Reminder: "
#define TODO __FILE__ "(" $Line ") : TODO: "

