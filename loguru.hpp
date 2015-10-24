/*
Loguru logging library for C++, by Emil Ernerfeldt.
www.github.com/emilk/loguru
If you find Loguru useful, please let me know on twitter or in a mail!
Twitter: @ernerfeldt
Mail:    emil.ernerfeldt@gmail.com
Website: www.ilikebigbits.com

# License
	This software is in the public domain. Where that dedication is not
	recognized, you are granted a perpetual, irrevocable license to copy
	and modify this file as you see fit.

# Inspiration
	Much of Loguru was inspired by GLOG, https://code.google.com/p/google-glog/.
	The whole "single header" and public domain is fully due Sean T. Barrett
	and his wonderful stb libraries at https://github.com/nothings/stb

# Version history
	* Version 0.1 - 2015-03-22 - Works great on Mac.
	* Version 0.2 - 2015-09-17 - Removed the only dependency.
	* Version 0.3 - 2015-10-02 - Drop-in replacement for most of GLOG
	* Version 0.4 - 2015-10-07 - Single-file!
	* Version 0.5 - 2015-10-17 - Improved file logging
	* Version 0.6 - 2015-10-24 - Add stack traces

# Compiling
	Just include <loguru/loguru.hpp> where you want to use Loguru.
	Then, in one .cpp file:
		#define LOGURU_IMPLEMENTATION
		#include <loguru/loguru.hpp>
	Make sure you compile with -std=c++11.

# Usage
	#include <loguru/loguru.hpp>

	// Optional, but useful to timestamp the start of the log.
	// Will also detect verbosity level on comamnd line as -v.
	loguru::init(argc, argv);

	// Put every log message in "everything.log":
	loguru::add_file("everything.log", loguru::Append);

	// Only log INFO, WARNING, ERROR and FATAL to "latest_readable.log":
	loguru::add_file("latest_readable.log", loguru::Truncate, loguru::INFO);

	// Or just go with what Loguru suggests:
	char log_path[1024];
	loguru::suggest_log_path("~/loguru/", log_path, sizeof(log_path));
	loguru::add_file(log_path, loguru::FileMode::Truncate);

	LOG_SCOPE_F(INFO, "Will indent all log messages withing this scope.");
	LOG_F(INFO, "I'm hungry for some %.3f!", 3.14159);
	VLOG_F(2, "Will only show if verbosity is 2 or higher");
	LOG_IF_F(ERROR, badness, "Will only show if badness happens");
	auto fp = fopen(filename, "r");
	CHECK_F(fp != nullptr, "Failed to open file '%s'", filename);
	CHECK_GT_F(length, 0); // Will print the value of `length` on failure.
	CHECK_EQ_F(a, b, "You can also supply a custom message, like to print something: %d", a + b);
	LOG_SCOPE_F(INFO, "Will indent all log messages withing this scope.");

	// Each function also comes with a version prefixed with D for Debug:
	DCHECK_F(expensive_check(x)); // Only checked #if !NDEBUG
	DLOG_F("Only written in debug-builds");

	If you prefer logging with streams:

	#define LOGURU_WITH_STREAMS 1
	#include <loguru/loguru.hpp>
	...
	LOG_S(INFO) << "Look at my custom object: " << a.cross(b);
	CHECK_EQ_S(pi, 3.14) << "Maybe it is closer to " << M_PI;

	Before including <loguru/loguru.hpp> you may optionally want to define the following to 1:

	LOGURU_REDEFINE_ASSERT:
		Redefine "assert" call loguru version (!NDEBUG only).

	LOGURU_WITH_STREAMS:
		Add support for _S versions for all LOG and CHECK functions:
			LOG_S(INFO) << "My vec3: " << x.cross(y);
			CHECK_EQ_S(a, b) << "I expected a and b to be the same!";
		This is off by default to keep down compilation times.

	LOGURU_REPLACE_GLOG:
		Make Loguru mimic GLOG as close as possible,
		including #defining LOG, CHECK, FLAGS_v etc.
		LOGURU_REPLACE_GLOG imlies LOGURU_WITH_STREAMS.
*/

#ifndef LOGURU_HEADER_HPP
#define LOGURU_HEADER_HPP

#if defined(__clang__) || defined(__GNUC__)
	// Helper macro for declaring functions as having similar signature to printf.
	// This allows the compiler to catch format errors at compile-time.
	#define LOGURU_PRINTF_LIKE(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
	#define LOGURU_FORMAT_STRING_TYPE const char*
#elif defined(_MSC_VER)
	#define LOGURU_PRINTF_LIKE(fmtarg, firstvararg)
	#define LOGURU_FORMAT_STRING_TYPE _In_z_ _Printf_format_string_ const char*
#else
	#define LOGURU_PRINTF_LIKE(fmtarg, firstvararg)
	#define LOGURU_FORMAT_STRING_TYPE const char*
#endif

// Used to mark log_and_abort for the benefit of the static analyzer and optimizer.
#define LOGURU_NORETURN __attribute__((noreturn))

#define LOGURU_PREDICT_FALSE(x) (__builtin_expect(x,     0))
#define LOGURU_PREDICT_TRUE(x)  (__builtin_expect(!!(x), 1))

namespace loguru
{
	// free after use
	char* strprintf(LOGURU_FORMAT_STRING_TYPE format, ...) LOGURU_PRINTF_LIKE(1, 2);

	// Overloaded for variadic template matching.
	char* strprintf();

	using Verbosity = int;

#undef FATAL
#undef ERROR
#undef WARNING
#undef INFO
#undef MAX

	enum NamedVerbosity : Verbosity
	{
		// Value is the verbosity level one must pass.
		// Negative numbers go to stderr and cannot be skipped.
		FATAL   = -3, // Prefer to use ABORT_F over LOG_F(FATAL)
		ERROR   = -2,
		WARNING = -1,
		INFO    =  0, // Normal messages.
		MAX     = +9
	};

	struct Message
	{
		// You would generally print a Message by just concating the buffers without spacing.
		// Optionally, ignore preamble and indentation.
		Verbosity   verbosity;   // Already part of preamble
		const char* filename;    // Already part of preamble
		unsigned    line;        // Already part of preamble
		const char* preamble;    // Date, time, uptime, thread, file:line, verbosity.
		const char* indentation; // Just a bunch of spacing.
		const char* prefix;      // Assertion failure info goes here (or "").
		const char* message;     // User message goes here.
	};

	extern Verbosity g_verbosity;        // Anything greater than this is ignored.
	extern bool      g_alsologtostderr;  // Ignored right now. Only used for LOGURU_REPLACE_GLOG.
	extern bool      g_colorlogtostderr; // Ignored right now. Only used for LOGURU_REPLACE_GLOG.

	// May not throw!
	typedef void (*log_handler_t)(void* user_data, const Message& message);
	typedef void (*close_handler_t)(void* user_data);
	typedef void (*fatal_handler_t)();

	/*  Should be called from the main thread.
		You don't need to call this, but it's nice if you do.
		This will look for arguments meant for loguru and remove them.
		Arguments meant for loguru are:
			-v n   Set verbosity level */
	void init(int& argc, char* argv[]);

	/* Given a prefix of e.g. "~/loguru/" this might return
	   "/home/your_username/loguru/app_name/20151017_161503.123.log"

	   where "app_name" is a sanatized version of argv[0].
	*/
	void suggest_log_path(const char* prefix, char* buff, unsigned buff_size);

	enum FileMode { Truncate, Append };

	/*  Will log to a file at the given path.
		`verbosity` is the cutoff, but this is applied *after* g_verbosity.
		add_file will also create all directories in 'path' if needed.
	*/
	bool add_file(const char* path, FileMode mode, Verbosity verbosity = NamedVerbosity::MAX);

	/*  Will be called right before abort().
		This can be used to print a callstack.
		Feel free to call LOG:ing function from this, but not FATAL ones! */
	void set_fatal_handler(fatal_handler_t handler);

	/*  Will be called on each log messages that passes verbocity tests etc.
		Useful for displaying messages on-screen in a game, for eample.
		`verbosity` is the cutoff, but this is applied *after* g_verbosity.
	*/
	void add_callback(const char* id, log_handler_t callback, void* user_data,
					  Verbosity verbosity = NamedVerbosity::MAX,
					  close_handler_t on_close = nullptr);
	void remove_callback(const char* id);

	// Actual logging function. Use the LOG macro instead of calling this directly.
	void log(Verbosity verbosity, const char* file, unsigned line, LOGURU_FORMAT_STRING_TYPE format, ...) LOGURU_PRINTF_LIKE(4, 5);

	// Log without any preamble or indentation.
	void raw_log(Verbosity verbosity, const char* file, unsigned line, LOGURU_FORMAT_STRING_TYPE format, ...) LOGURU_PRINTF_LIKE(4, 5);

	// Helper class for LOG_SCOPE_F
	class LogScopeRAII
	{
	public:
		LogScopeRAII(Verbosity verbosity, const char* file, unsigned line, LOGURU_FORMAT_STRING_TYPE format, ...) LOGURU_PRINTF_LIKE(5, 6);
		~LogScopeRAII();

	private:
		LogScopeRAII(LogScopeRAII&);
		LogScopeRAII(LogScopeRAII&&);
		LogScopeRAII& operator=(LogScopeRAII&);
		LogScopeRAII& operator=(LogScopeRAII&&);

		Verbosity   _verbosity;
		const char* _file; // Set to null if we are disabled due to verbosity
		unsigned    _line;
		long long   _start_time_ns;
		char        _name[128]; // Long enough to get most things, short enough not to clutter the stack.
	};

	// Marked as 'noreturn' for the benefit of the static analyzer and optimizer.
	// stack_strace_skip is the number of extrace stack frames to skip above log_and_abort.
	void log_and_abort(int stack_trace_skip, const char* expr, const char* file, unsigned line, LOGURU_FORMAT_STRING_TYPE format, ...) LOGURU_PRINTF_LIKE(5, 6) LOGURU_NORETURN;
	void log_and_abort(int stack_trace_skip, const char* expr, const char* file, unsigned line) LOGURU_NORETURN;

	// Free after use!
	template<class T> inline char* format_value(const T&)                    { return strprintf("N/A");     }
	template<>        inline char* format_value(const char& v)               { return strprintf("%c",   v); }
	template<>        inline char* format_value(const int& v)                { return strprintf("%d",   v); }
	template<>        inline char* format_value(const unsigned int& v)       { return strprintf("%u",   v); }
	template<>        inline char* format_value(const long& v)               { return strprintf("%lu",  v); }
	template<>        inline char* format_value(const unsigned long& v)      { return strprintf("%ld",  v); }
	template<>        inline char* format_value(const long long& v)          { return strprintf("%llu", v); }
	template<>        inline char* format_value(const unsigned long long& v) { return strprintf("%lld", v); }
	template<>        inline char* format_value(const float& v)              { return strprintf("%f",   v); }
	template<>        inline char* format_value(const double& v)             { return strprintf("%f",   v); }

	// Convenience:
	void set_thread_name(const char* name);
	const char* home_dir();

	/* Generates a readable stacktrace as a string. You must free the returned string!
	   'skip' specifies how many stack frames to skip.
	   For instance, the default skip (1) means:
	   don't include the call to loguru::stacktrace in the stack trace. */
	char* stacktrace(int skip = 1);
} // namespace loguru

// --------------------------------------------------------------------
// Utitlity macros

// Used for giving a unique name to a RAII-object
#define LOGURU_GIVE_UNIQUE_NAME(arg1, arg2) LOGURU_STRING_JOIN(arg1, arg2)
#define LOGURU_STRING_JOIN(arg1, arg2) arg1 ## arg2

// --------------------------------------------------------------------
// Logging macros

// LOG_F(2, "Only logged if verbosity is 2 or higher: %d", some_number);
#define VLOG_F(verbosity, ...)                                                                     \
	(verbosity > loguru::g_verbosity) ? (void)0                                                    \
									  : loguru::log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

// LOG_F(INFO, "Foo: %d", some_number);
#define LOG_F(verbosity_name, ...) VLOG_F(loguru::NamedVerbosity::verbosity_name, __VA_ARGS__)

#define VLOG_IF_F(verbosity, cond, ...)                                                            \
	(verbosity > loguru::g_verbosity || (cond) == false)                                           \
		? (void)0                                                                                  \
		: loguru::log(verbosity, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_IF_F(verbosity_name, cond, ...)                                                        \
	VLOG_IF_F(loguru::NamedVerbosity::verbosity_name > loguru::g_verbosity, cond, __VA_ARGS__)

#define VLOG_SCOPE_F(verbosity, ...)                                                               \
	loguru::LogScopeRAII LOGURU_GIVE_UNIQUE_NAME(error_context_RAII_, __LINE__)                    \
	{                                                                                              \
		verbosity, __FILE__, __LINE__, __VA_ARGS__                                                 \
	}

// Raw logging - no preamble, no indentation. Slightly faster than full logging.
#define RAW_VLOG_F(verbosity, ...)                                                                  \
	(verbosity > loguru::g_verbosity) ? (void)0                                                     \
									  : loguru::raw_log(verbosity, __FILE__, __LINE__, __VA_ARGS__)
#define RAW_LOG_F(verbosity_name, ...) RAW_VLOG_F(loguru::NamedVerbosity::verbosity_name, __VA_ARGS__)

// Use to book-end a scope. Affects logging on all threads.
#define LOG_SCOPE_F(verbosity_name, ...)                                                           \
	VLOG_SCOPE_F(loguru::NamedVerbosity::verbosity_name, __VA_ARGS__)
#define LOG_SCOPE_FUNCTION(verbosity_name) LOG_SCOPE_F(verbosity_name, __PRETTY_FUNCTION__)

// --------------------------------------------------------------------
// Check/Abort macros

// Message is optional
#define ABORT_F(...) loguru::log_and_abort(0, "ABORT: ", __FILE__, __LINE__, __VA_ARGS__)

#define CHECK_WITH_INFO_F(test, info, ...)                                                         \
	LOGURU_PREDICT_TRUE((test) == true) ? (void)0 : loguru::log_and_abort(0, "CHECK FAILED:  " info "  ", __FILE__,      \
													   __LINE__, ##__VA_ARGS__)

/* Checked at runtime too. Will print error, then call abort_handler (if any), then 'abort'.
   Note that the test must be boolean.
   CHECK_F(ptr); will not compile, but CHECK_F(ptr != nullptr); will. */
#define CHECK_F(test, ...) CHECK_WITH_INFO_F(test, #test, ##__VA_ARGS__)

#define CHECK_NOTNULL_F(x, ...) CHECK_WITH_INFO_F((x) != nullptr, #x " != nullptr", ##__VA_ARGS__)

#define CHECK_OP_F(expr_left, expr_right, op, ...)                                                 \
	do                                                                                             \
	{                                                                                              \
		auto val_left = expr_left;                                                                 \
		auto val_right = expr_right;                                                               \
		if (! LOGURU_PREDICT_TRUE(val_left op val_right))                                          \
		{                                                                                          \
			char* str_left = loguru::format_value(val_left);                                       \
			char* str_right = loguru::format_value(val_right);                                     \
			char* fail_info = loguru::strprintf("CHECK FAILED:  %s %s %s  (%s %s %s)  ",           \
				#expr_left, #op, #expr_right, str_left, #op, str_right);                           \
			char* user_msg = loguru::strprintf(__VA_ARGS__);                                       \
			loguru::log_and_abort(0, fail_info, __FILE__, __LINE__, "%s", user_msg);                  \
			/* free(user_msg);  // no need - we never get here anyway! */                          \
			/* free(fail_info); // no need - we never get here anyway! */                          \
			/* free(str_right); // no need - we never get here anyway! */                          \
			/* free(str_left);  // no need - we never get here anyway! */                          \
		}                                                                                          \
	} while (false)

#define CHECK_EQ_F(a, b, ...) CHECK_OP_F(a, b, ==, ##__VA_ARGS__)
#define CHECK_NE_F(a, b, ...) CHECK_OP_F(a, b, !=, ##__VA_ARGS__)
#define CHECK_LT_F(a, b, ...) CHECK_OP_F(a, b, < , ##__VA_ARGS__)
#define CHECK_GT_F(a, b, ...) CHECK_OP_F(a, b, > , ##__VA_ARGS__)
#define CHECK_LE_F(a, b, ...) CHECK_OP_F(a, b, <=, ##__VA_ARGS__)
#define CHECK_GE_F(a, b, ...) CHECK_OP_F(a, b, >=, ##__VA_ARGS__)

#ifndef NDEBUG
	#define DLOG_F(verbosity_name, ...)     LOG_F(verbosity_name, __VA_ARGS__)
	#define DVLOG_F(verbosity, ...)         VLOG_F(verbosity, __VA_ARGS__)
	#define DLOG_IF_F(verbosity_name, ...)  LOG_IF_F(verbosity_name, __VA_ARGS__)
	#define DVLOG_IF_F(verbosity, ...)      VLOG_IF_F(verbosity, __VA_ARGS__)
	#define DRAW_LOG_F(verbosity_name, ...) RAW_LOG_F(verbosity_name, __VA_ARGS__)
	#define DRAW_VLOG_F(verbosity, ...)     RAW_VLOG_F(verbosity, __VA_ARGS__)
	#define DCHECK_F(test, ...)             CHECK_F(test, ##__VA_ARGS__)
	#define DCHECK_NOTNULL_F(x, ...)        CHECK_NOTNULL_F(x, ##__VA_ARGS__)
	#define DCHECK_EQ_F(a, b, ...)          CHECK_EQ_F(a, b, ##__VA_ARGS__)
	#define DCHECK_NE_F(a, b, ...)          CHECK_NE_F(a, b, ##__VA_ARGS__)
	#define DCHECK_LT_F(a, b, ...)          CHECK_LT_F(a, b, ##__VA_ARGS__)
	#define DCHECK_LE_F(a, b, ...)          CHECK_LE_F(a, b, ##__VA_ARGS__)
	#define DCHECK_GT_F(a, b, ...)          CHECK_GT_F(a, b, ##__VA_ARGS__)
	#define DCHECK_GE_F(a, b, ...)          CHECK_GE_F(a, b, ##__VA_ARGS__)
#else // NDEBUG
	#define DLOG_F(verbosity_name, ...)
	#define DVLOG_F(verbosity, ...)
	#define DLOG_IF_F(verbosity_name, ...)
	#define DVLOG_IF_F(verbosity, ...)
	#define DRAW_LOG_F(verbosity_name, ...)
	#define DRAW_VLOG_F(verbosity, ...)
	#define DCHECK_F(test, ...)
	#define DCHECK_NOTNULL_F(x, ...)
	#define DCHECK_EQ_F(a, b, ...)
	#define DCHECK_NE_F(a, b, ...)
	#define DCHECK_LT_F(a, b, ...)
	#define DCHECK_LE_F(a, b, ...)
	#define DCHECK_GT_F(a, b, ...)
	#define DCHECK_GE_F(a, b, ...)
#endif // NDEBUG

#ifdef LOGURU_REDEFINE_ASSERT
	#undef assert
	#ifndef NDEBUG
		#define assert(test) CHECK_WITH_INFO_F(!!(test), #test) // HACK
	#else
		#define assert(test)
	#endif
#endif // LOGURU_REDEFINE_ASSERT

// ----------------------------------------------------------------------------
// .dP"Y8 888888 88""Yb 888888    db    8b    d8 .dP"Y8
// `Ybo."   88   88__dP 88__     dPYb   88b  d88 `Ybo."
// o.`Y8b   88   88"Yb  88""    dP__Yb  88YbdP88 o.`Y8b
// 8bodP'   88   88  Yb 888888 dP""""Yb 88 YY 88 8bodP'

#if LOGURU_WITH_STREAMS || LOGURU_REPLACE_GLOG

/* This file extends loguru to enable std::stream-style logging, a la Glog.
   It's an optional feature beind the LOGURU_WITH_STREAMS settings
   because including it everywhere will slow down compilation times.
*/

#include <sstream> // Adds about 38 kLoC on clang.

namespace loguru
{
	class StreamLogger : public std::ostringstream
	{
	public:
		StreamLogger(Verbosity verbosity, const char* file, unsigned line) : _verbosity(verbosity), _file(file), _line(line) {}
		~StreamLogger()
		{
			auto message = this->str();
			log(_verbosity, _file, _line, "%s", message.c_str());
		}

	private:
		Verbosity   _verbosity;
		const char* _file;
		unsigned    _line;
	};

	class AbortLogger : public std::ostringstream
	{
	public:
		AbortLogger(const char* expr, const char* file, unsigned line) : _expr(expr), _file(file), _line(line) {}
		~AbortLogger() LOGURU_NORETURN;

	private:
		const char* _expr;
		const char* _file;
		unsigned    _line;
	};

	class Voidify
	{
	public:
		Voidify() {}
		// This has to be an operator with a precedence lower than << but higher than ?:
		void operator&(const std::ostream&) {}
	};

	/*  Helper functions for CHECK_OP_S macro.
		GLOG trick: The (int, int) specialization works around the issue that the compiler
		will not instantiate the template version of the function on values of unnamed enum type. */
	#define DEFINE_CHECK_OP_IMPL(name, op)                                                             \
		template <typename T1, typename T2>                                                            \
		inline std::string* name(const char* expr, const T1& v1, const char* op_str, const T2& v2)     \
		{                                                                                              \
			if (LOGURU_PREDICT_TRUE(v1 op v2)) { return NULL; }                                        \
			std::ostringstream ss;                                                                     \
			ss << "CHECK FAILED:  " << expr << "  (" << v1 << " " << op_str << " " << v2 << ")  ";     \
			return new std::string(ss.str());                                                          \
		}                                                                                              \
		inline std::string* name(const char* expr, int v1, const char* op_str, int v2)                 \
		{                                                                                              \
			return name<int, int>(expr, v1, op_str, v2);                                               \
		}

	DEFINE_CHECK_OP_IMPL(check_EQ_impl, ==)
	DEFINE_CHECK_OP_IMPL(check_NE_impl, !=)
	DEFINE_CHECK_OP_IMPL(check_LE_impl, <=)
	DEFINE_CHECK_OP_IMPL(check_LT_impl, < )
	DEFINE_CHECK_OP_IMPL(check_GE_impl, >=)
	DEFINE_CHECK_OP_IMPL(check_GT_impl, > )
	#undef DEFINE_CHECK_OP_IMPL

	/*  GLOG trick: Function is overloaded for integral types to allow static const integrals
		declared in classes and not defined to be used as arguments to CHECK* macros. */
	template <class T>
	inline const T&           referenceable_value(const T&           t) { return t; }
	inline char               referenceable_value(char               t) { return t; }
	inline unsigned char      referenceable_value(unsigned char      t) { return t; }
	inline signed char        referenceable_value(signed char        t) { return t; }
	inline short              referenceable_value(short              t) { return t; }
	inline unsigned short     referenceable_value(unsigned short     t) { return t; }
	inline int                referenceable_value(int                t) { return t; }
	inline unsigned int       referenceable_value(unsigned int       t) { return t; }
	inline long               referenceable_value(long               t) { return t; }
	inline unsigned long      referenceable_value(unsigned long      t) { return t; }
	inline long long          referenceable_value(long long          t) { return t; }
	inline unsigned long long referenceable_value(unsigned long long t) { return t; }
} // namespace loguru

// -----------------------------------------------
// Logging macros:

// usage:  LOG_STREAM(INFO) << "Foo " << std::setprecision(10) << some_value;
#define VLOG_IF_S(verbosity, cond)                                                                 \
	(verbosity > loguru::g_verbosity || (cond) == false)                                           \
		? (void)0                                                                                  \
		: loguru::Voidify() & loguru::StreamLogger(verbosity, __FILE__, __LINE__)
#define LOG_IF_S(verbosity_name, cond) VLOG_IF_S(loguru::NamedVerbosity::verbosity_name, cond)
#define VLOG_S(verbosity)              VLOG_IF_S(verbosity, true)
#define LOG_S(verbosity_name)          VLOG_S(loguru::NamedVerbosity::verbosity_name)

// -----------------------------------------------
// CHECKS:

#define CHECK_WITH_INFO_S(cond, info)                                                              \
	LOGURU_PREDICT_TRUE((cond) == true)                                                            \
		? (void)0                                                                                  \
		: loguru::Voidify() & loguru::AbortLogger("CHECK FAILED:  " info "  ", __FILE__, __LINE__)

#define CHECK_S(cond) CHECK_WITH_INFO_S(cond, #cond)
#define CHECK_NOTNULL_S(x) CHECK_WITH_INFO_S((x) != nullptr, #x " != nullptr")

#define CHECK_OP_S(function_name, expr1, op, expr2)                                                \
	while (auto error_string = loguru::function_name(#expr1 " " #op " " #expr2,                    \
													 loguru::referenceable_value(expr1), #op,      \
													 loguru::referenceable_value(expr2)))          \
		loguru::AbortLogger(error_string->c_str(), __FILE__, __LINE__)

#define CHECK_EQ_S(expr1, expr2) CHECK_OP_S(check_EQ_impl, expr1, ==, expr2)
#define CHECK_NE_S(expr1, expr2) CHECK_OP_S(check_NE_impl, expr1, !=, expr2)
#define CHECK_LE_S(expr1, expr2) CHECK_OP_S(check_LE_impl, expr1, <=, expr2)
#define CHECK_LT_S(expr1, expr2) CHECK_OP_S(check_LT_impl, expr1, < , expr2)
#define CHECK_GE_S(expr1, expr2) CHECK_OP_S(check_GE_impl, expr1, >=, expr2)
#define CHECK_GT_S(expr1, expr2) CHECK_OP_S(check_GT_impl, expr1, > , expr2)

#ifndef NDEBUG
	#define DVLOG_IF_S(verbosity, cond)     VLOG_IF_S(verbosity, cond)
	#define DLOG_IF_S(verbosity_name, cond) LOG_IF_S(verbosity_name, cond)
	#define DVLOG_S(verbosity)              VLOG_S(verbosity)
	#define DLOG_S(verbosity_name)          LOG_S(verbosity_name)
	#define DCHECK_S(cond)                  CHECK_S(cond)
	#define DCHECK_NOTNULL_S(x)             CHECK_NOTNULL_S(x)
	#define DCHECK_EQ_S(a, b)               CHECK_EQ_S(a, b)
	#define DCHECK_NE_S(a, b)               CHECK_NE_S(a, b)
	#define DCHECK_LT_S(a, b)               CHECK_LT_S(a, b)
	#define DCHECK_LE_S(a, b)               CHECK_LE_S(a, b)
	#define DCHECK_GT_S(a, b)               CHECK_GT_S(a, b)
	#define DCHECK_GE_S(a, b)               CHECK_GE_S(a, b)
#else // NDEBUG
	#define DVLOG_IF_S(verbosity, cond)                                                     \
		(true || verbosity > loguru::g_verbosity || (cond) == false)                        \
			? (void)0                                                                       \
			: loguru::Voidify() & loguru::StreamLogger(verbosity, __FILE__, __LINE__)

	#define DLOG_IF_S(verbosity_name, cond) DVLOG_IF_S(loguru::NamedVerbosity::verbosity_name, cond)
	#define DVLOG_S(verbosity)              DVLOG_IF_S(verbosity, true)
	#define DLOG_S(verbosity_name)          DVLOG_S(loguru::NamedVerbosity::verbosity_name)
	#define DCHECK_S(cond)                  while (false) CHECK(cond)
	#define DCHECK_NOTNULL_S(x)             while (false) CHECK((x) != nullptr)
	#define DCHECK_EQ_S(a, b)               while (false) CHECK_EQ_S(a, b)
	#define DCHECK_NE_S(a, b)               while (false) CHECK_NE_S(a, b)
	#define DCHECK_LT_S(a, b)               while (false) CHECK_LT_S(a, b)
	#define DCHECK_LE_S(a, b)               while (false) CHECK_LE_S(a, b)
	#define DCHECK_GT_S(a, b)               while (false) CHECK_GT_S(a, b)
	#define DCHECK_GE_S(a, b)               while (false) CHECK_GE_S(a, b)
#endif // NDEBUG

#if LOGURU_REPLACE_GLOG
	#define LOG            LOG_S
	#define VLOG           VLOG_S
	#define LOG_IF         LOG_IF_S
	#define VLOG_IF        VLOG_IF_S
	#define CHECK(cond)    CHECK_S(!!(cond))
	#define CHECK_NOTNULL  CHECK_NOTNULL_S
	#define CHECK_EQ       CHECK_EQ_S
	#define CHECK_NE       CHECK_NE_S
	#define CHECK_LT       CHECK_LT_S
	#define CHECK_LE       CHECK_LE_S
	#define CHECK_GT       CHECK_GT_S
	#define CHECK_GE       CHECK_GE_S
	#define DLOG           DLOG_S
	#define DVLOG          DVLOG_S
	#define DLOG_IF        DLOG_IF_S
	#define DVLOG_IF       DVLOG_IF_S
	#define DCHECK         DCHECK_S
	#define DCHECK_NOTNULL DCHECK_NOTNULL_S
	#define DCHECK_EQ      DCHECK_EQ_S
	#define DCHECK_NE      DCHECK_NE_S
	#define DCHECK_LT      DCHECK_LT_S
	#define DCHECK_LE      DCHECK_LE_S
	#define DCHECK_GT      DCHECK_GT_S
	#define DCHECK_GE      DCHECK_GE_S

	#define FLAGS_v                loguru::g_verbosity
	#define FLAGS_alsologtostderr  loguru::g_alsologtostderr
	#define FLAGS_colorlogtostderr loguru::g_colorlogtostderr

	#define VLOG_IS_ON(verbosity) ((verbosity) <= loguru::g_verbosity)
#endif // LOGURU_REPLACE_GLOG

#endif // LOGURU_WITH_STREAMS || LOGURU_REPLACE_GLOG

#endif // LOGURU_HEADER_HPP

// ----------------------------------------------------------------------------
// 88 8b    d8 88""Yb 88     888888 8b    d8 888888 88b 88 888888    db    888888 88  dP"Yb  88b 88
// 88 88b  d88 88__dP 88     88__   88b  d88 88__   88Yb88   88     dPYb     88   88 dP   Yb 88Yb88
// 88 88YbdP88 88"""  88  .o 88""   88YbdP88 88""   88 Y88   88    dP__Yb    88   88 Yb   dP 88 Y88
// 88 88 YY 88 88     88ood8 888888 88 YY 88 888888 88  Y8   88   dP""""Yb   88   88  YbodP  88  Y8


/* In one of your .cpp files you need to do the following:
#define LOGURU_IMPLEMENTATION
#include <loguru/loguru.hpp>

This will define all the Loguru functions so that the linker may find them.
*/

#if defined(LOGURU_IMPLEMENTATION) && !defined(LOGURU_HAS_BEEN_IMPLEMENTED)
#define LOGURU_HAS_BEEN_IMPLEMENTED

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

#ifdef _MSC_VER
	#include <direct.h>
#else
	#include <sys/stat.h> // mkdir
	#define LOGURU_PTHREADS 1
#endif

#ifdef __GNUG__
	#include <cxxabi.h>   // for __cxa_demangle
	#include <dlfcn.h>    // for dladdr
	#include <execinfo.h> // for backtrace
#endif // __GNUG__

#if LOGURU_PTHREADS
	#include <pthread.h>
#endif

using namespace std::chrono;

namespace loguru
{
	struct Callback
	{
		std::string     id;
		log_handler_t   callback;
		void*           user_data;
		Verbosity       verbosity;
		close_handler_t close;
	};

	using CallbackVec = std::vector<Callback>;

	const auto SCOPE_TIME_PRECISION = 3; // 3=ms, 6≈us, 9=ns

	const auto s_start_time = system_clock::now();

	int  g_verbosity        = 0;
	bool g_alsologtostderr  = false;
	bool g_colorlogtostderr = false;

	std::recursive_mutex s_mutex;
	std::string          s_argv0_filename;
	std::string          s_file_arguments;
	CallbackVec          s_callbacks;
	FILE*                s_err             = stderr;
	FILE*                s_out             = stdout;
	fatal_handler_t      s_fatal_handler   = ::abort;
	bool                 s_strip_file_path = true;
	std::atomic<int>     s_indentation     { 0 };

	const int THREAD_NAME_WIDTH = 16;
	const char* PREAMBLE_EXPLAIN = "date       time         ( uptime  ) [ thread name/id ]                   file:line     v| ";

	// ------------------------------------------------------------------------------

	void file_log(void* user_data, const Message& message)
	{
		FILE* file = reinterpret_cast<FILE*>(user_data);
		fprintf(file, "%s%s%s%s\n",
			message.preamble, message.indentation, message.prefix, message.message);
		fflush(file);
	}

	void file_close(void* user_data)
	{
		FILE* file = reinterpret_cast<FILE*>(user_data);
		fclose(file);
	}

	// ------------------------------------------------------------------------------

	// Helpers:

	// free after use
	static char* strprintfv(const char* format, va_list vlist)
	{
#ifdef _MSC_VER
		int bytes_needed = vsnprintf(nullptr, 0, format, vlist);
		CHECK_F(bytes_needed >= 0, "Bad string format: '%s'", format);
		char* buff = (char*)malloc(bytes_needed + 1);
		vsnprintf(str.data(), bytes_needed, format, vlist);
		return buff;
#else
		char* buff = nullptr;
		int result = vasprintf(&buff, format, vlist);
		CHECK_F(result >= 0, "Bad string format: '%s'", format);
		return buff;
#endif
	}

	// free after use
	char* strprintf(const char* format, ...)
	{
		va_list vlist;
		va_start(vlist, format);
		char* result = strprintfv(format, vlist);
		va_end(vlist);
		return result;
	}

	// Overloaded for variadic template matching.
	char* strprintf()
	{
		return (char*)calloc(1, 1);
	}

	const char* indentation(unsigned depth)
	{
		static const char* buff =
		".   .   .   .   .   .   .   .   .   .   " ".   .   .   .   .   .   .   .   .   .   "
		".   .   .   .   .   .   .   .   .   .   " ".   .   .   .   .   .   .   .   .   .   "
		".   .   .   .   .   .   .   .   .   .   " ".   .   .   .   .   .   .   .   .   .   "
		".   .   .   .   .   .   .   .   .   .   " ".   .   .   .   .   .   .   .   .   .   "
		".   .   .   .   .   .   .   .   .   .   " ".   .   .   .   .   .   .   .   .   .   ";
		depth = std::min<unsigned>(depth, 100);
		return buff + 4 * (100 - depth);
	}

	static void parse_args(int& argc, char* argv[])
	{
		CHECK_GT_F(argc,       0,       "Expected proper argc/argv");
		CHECK_EQ_F(argv[argc], nullptr, "Expected proper argc/argv");

		int arg_dest = 1;
		int out_argc = argc;

		for (int arg_it = 1; arg_it < argc; ++arg_it) {
			auto cmd = argv[arg_it];
			if (strncmp(cmd, "-v", 2) == 0 && !std::isalpha(cmd[2])) {
				out_argc -= 1;
				auto value_str = cmd + 2;
				if (value_str[0] == '\0') {
					// Value in separate argument
					arg_it += 1;
					CHECK_LT_F(arg_it, argc, "Missing verbosiy level after -v");
					value_str = argv[arg_it];
					out_argc -= 1;
				}
				if (*value_str == '=') { value_str += 1; }
				g_verbosity = atoi(value_str);
			} else {
				argv[arg_dest++] = argv[arg_it];
			}
		}

		argc = out_argc;
		argv[argc] = nullptr;
	}

	static long long now_ns()
	{
		return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
	}

	inline const char* filename(const char* path)
	{
		for (auto ptr = path; *ptr; ++ptr) {
			if (*ptr == '/' || *ptr == '\\') {
				path = ptr + 1;
			}
		}
		return path;
	}

	// ------------------------------------------------------------------------------

	static void on_atexit()
	{
		LOG_F(INFO, "atexit");
	}

	void init(int& argc, char* argv[])
	{
		s_argv0_filename = filename(argv[0]);

		s_file_arguments = "";
		for (int i = 0; i < argc; ++i) {
			s_file_arguments += argv[i];
			if (i + 1 < argc) {
				s_file_arguments += " ";
			}
		}

		parse_args(argc, argv);

		#if LOGURU_PTHREADS
			// Set thread name, unless it is already set:
			char old_thread_name[128] = {0};
			auto this_thread = pthread_self();
			pthread_getname_np(this_thread, old_thread_name, sizeof(old_thread_name));
			if (old_thread_name[0] == 0) {
				#ifdef __APPLE__
					pthread_setname_np("main thread");
				#else
					pthread_setname_np(this_thread, "main thread");
				#endif
			}
		#endif // LOGURU_PTHREADS

		fprintf(stdout, "%s\n", PREAMBLE_EXPLAIN); fflush(stdout);
		LOG_F(INFO, "arguments:       %s", s_file_arguments.c_str());
		LOG_F(INFO, "Verbosity level: %d", g_verbosity);
		LOG_F(INFO, "-----------------------------------");

		atexit(on_atexit);
	}

	static void write_date_time(char* buff, unsigned buff_size)
	{
		auto now = system_clock::now();
		time_t ms_since_epoch = duration_cast<milliseconds>(now.time_since_epoch()).count();
		time_t sec_since_epoch = ms_since_epoch / 1000;
		tm time_info;
		localtime_r(&sec_since_epoch, &time_info);
		snprintf(buff, buff_size, "%04d%02d%02d_%02d%02d%02d.%03ld",
			1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday,
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec, ms_since_epoch % 1000);
	}

	const char* home_dir()
	{
		#if _WIN32
			auto user_profile = getenv("USERPROFILE");
			CHECK_F(user_profile != nullptr, "Missing USERPROFILE");
			return user_profile;
		#else // _WIN32
			auto home = getenv("HOME");
			CHECK_F(home != nullptr, "Missing HOME");
			return home;
		#endif // _WIN32
	}

	void suggest_log_path(const char* prefix, char* buff, unsigned buff_size)
	{
		if (prefix[0] == '~') {
			snprintf(buff, buff_size - 1, "%s%s", home_dir(), prefix + 1);
		} else {
			snprintf(buff, buff_size - 1, "%s", prefix);
		}

		// Check for terminating /
		auto n = strlen(buff);
		if (n != 0) {
			if (buff[n - 1] != '/') {
				CHECK_F(n + 2 < buff_size, "Filename buffer too small");
				buff[n] = '/';
				buff[n + 1] = '\0';
				n += 1;
			}
		}

		strncat(buff, s_argv0_filename.c_str(), buff_size - strlen(buff) - 1);
		strncat(buff, "/",                      buff_size - strlen(buff) - 1);
		write_date_time(buff + strlen(buff),    buff_size - strlen(buff));
		strncat(buff, ".log",                   buff_size - strlen(buff) - 1);
	}

	bool mkpath(const char* file_path_const)
	{
		CHECK_F(file_path_const && *file_path_const);
		char* file_path = strdup(file_path_const);
		for (char* p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
			*p = '\0';

	#ifdef _MSC_VER
			if (_mkdir(file_path) == -1) {
	#else
			if (mkdir(file_path, 0755) == -1) {
	#endif
				if (errno != EEXIST) {
					LOG_F(ERROR, "Failed to create directory '%s'", file_path);
					LOG_IF_F(ERROR, errno == EACCES,       "EACCES");
					LOG_IF_F(ERROR, errno == ENAMETOOLONG, "ENAMETOOLONG");
					LOG_IF_F(ERROR, errno == ENOENT,       "ENOENT");
					LOG_IF_F(ERROR, errno == ENOTDIR,      "ENOTDIR");
					LOG_IF_F(ERROR, errno == ELOOP,        "ELOOP");

					*p = '/';
					free(file_path);
					return false;
				}
			}
			*p = '/';
		}
		free(file_path);
		return true;
	}

	bool add_file(const char* path, FileMode mode, Verbosity verbosity)
	{
		if (!mkpath(path)) {
			LOG_F(ERROR, "Failed to create directories to '%s'", path);
		}

		const char* mode_str = (mode == FileMode::Truncate ? "w" : "a");
		auto file = fopen(path, mode_str);
		if (!file) {
			LOG_F(ERROR, "Failed to open '%s'", path);
			return false;
		}
		add_callback(path, file_log, file, verbosity, file_close);

		fprintf(file, "arguments:       %s\n", s_file_arguments.c_str());
		fprintf(file, "Verbosity level: %d\n", std::max(g_verbosity, verbosity));
		fprintf(file, "%s\n", PREAMBLE_EXPLAIN);
		fflush(file);

		LOG_F(INFO, "Logging to '%s', mode: '%s', verbosity: %d", path, mode_str, verbosity);
		return true;
	}

	// Will be called right before abort().
	void set_fatal_handler(fatal_handler_t handler)
	{
		s_fatal_handler = handler;
	}

	void add_callback(const char* id, log_handler_t callback, void* user_data,
					  Verbosity verbosity, close_handler_t on_close)
	{
		std::lock_guard<std::recursive_mutex> lock(s_mutex);
		s_callbacks.push_back(Callback{id, callback, user_data, verbosity, on_close});
	}

	void remove_callback(const char* id)
	{
		std::lock_guard<std::recursive_mutex> lock(s_mutex);
		auto it = std::find_if(begin(s_callbacks), end(s_callbacks), [&](const Callback& c) { return c.id == id; });
		if (it != s_callbacks.end()) {
			if (it->close) { it->close(it->user_data); }
			s_callbacks.erase(it);
		} else {
			LOG_F(ERROR, "Failed to locate callback with id '%s'", id);
		}
	}

	void set_thread_name(const char* name)
	{
		#if LOGURU_PTHREADS
			#ifdef __APPLE__
				pthread_setname_np(name);
			#else
				pthread_setname_np(pthread_self(), name);
			#endif
		#else // LOGURU_PTHREADS
			(void)name; // TODO: Windows
		#endif // LOGURU_PTHREADS
	}

	// ------------------------------------------------------------------------
	// Stack traces

#ifdef __GNUG__
	std::string demangle(const char* name)
	{
		int status = -1;
		char* demangled = abi::__cxa_demangle(name, 0, 0, &status);
		std::string result = (status == 0 ? demangled : name);
		free(demangled);
		return result;
	}

	template <class T>
	std::string type_name() {
		return demangle(typeid(T).name());
	}

	using StringPair     = std::pair<std::string, std::string>;
	using StringPairList = std::vector<StringPair>;
	static const StringPairList REPLACE_LIST = {
		{ type_name<std::string>(),    "std::string"    },
		{ type_name<std::wstring>(),   "std::wstring"   },
		{ type_name<std::u16string>(), "std::u16string" },
		{ type_name<std::u32string>(), "std::u32string" },
		{ "std::__1::",                "std::"          },
		{ "__thiscall ",               ""               },
		{ "__cdecl ",                  ""               },
	};

	std::string prettify_stacktrace(const std::string& input)
	{
		std::string output = input;

		for (auto&& p : REPLACE_LIST) {
			size_t it;
			while ((it=output.find(p.first)) != std::string::npos) {
				output.replace(it, p.first.size(), p.second);
			}
		}

		std::regex std_allocator_re(R"(,\s*std::allocator<[^<>]+>)");
		output = std::regex_replace(output, std_allocator_re, "");

		std::regex template_spaces_re(R"(<\s*([^<> ]+)\s*>)");
		output = std::regex_replace(output, template_spaces_re, "<$1>");

		return output;
	}

	std::string stacktrace_as_stdstring(int skip)
	{
		// From https://gist.github.com/fmela/591333
		void* callstack[128];
		const auto max_frames = sizeof(callstack) / sizeof(callstack[0]);
		char buf[1024];
		int num_frames = backtrace(callstack, max_frames);
		char** symbols = backtrace_symbols(callstack, num_frames);

		std::string result;
		// Print stack traces so the most relevant ones are written last
		// Rationale: http://yellerapp.com/posts/2015-01-22-upside-down-stacktraces.html
		for (int i = num_frames - 1; i >= skip; --i) {
			Dl_info info;
			if (dladdr(callstack[i], &info) && info.dli_sname) {
				char* demangled = NULL;
				int status = -1;
				if (info.dli_sname[0] == '_') {
					demangled = abi::__cxa_demangle(info.dli_sname, 0, 0, &status);
				}
				snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n",
						 i - skip, int(2 + sizeof(void*) * 2), callstack[i],
						 status == 0 ? demangled :
						 info.dli_sname == 0 ? symbols[i] : info.dli_sname,
						 (char *)callstack[i] - (char *)info.dli_saddr);
				free(demangled);
			} else {
				snprintf(buf, sizeof(buf), "%-3d %*p %s\n",
						 i - skip, int(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
			}
			result += buf;
		}
		free(symbols);
		if (num_frames == max_frames) {
			result = "[truncated]\n" + result;
		}

		if (!result.empty() && result[result.size() - 1] == '\n') {
			result.resize(result.size() - 1);
		}

		return prettify_stacktrace(result);
	}

#else // __GNUG__
	std::string stacktrace_as_stdstring(int)
	{
		#warning "Loguru: No stacktraces available on this platform"
		return "";
	}

#endif // __GNUG__

	char* stacktrace(int skip)
	{
		auto str = stacktrace_as_stdstring(skip + 1);
		return strdup(str.c_str());
	}

	// ------------------------------------------------------------------------

	static void print_preamble(char* out_buff, size_t out_buff_size, Verbosity verbosity, const char* file, unsigned line)
	{
		auto now = system_clock::now();
		time_t ms_since_epoch = duration_cast<milliseconds>(now.time_since_epoch()).count();
		time_t sec_since_epoch = ms_since_epoch / 1000;
		tm time_info;
		localtime_r(&sec_since_epoch, &time_info);

		auto uptime_ms = duration_cast<milliseconds>(now - s_start_time).count();
		auto uptime_sec = uptime_ms / 1000.0;

		#if LOGURU_PTHREADS
			auto thread = pthread_self();
			char thread_name[THREAD_NAME_WIDTH + 1] = {0};
			pthread_getname_np(thread, thread_name, sizeof(thread_name));

			if (thread_name[0] == 0) {
				#ifdef __APPLE__
					uint64_t thread_id;
					pthread_threadid_np(thread, &thread_id);
				#else
					uint64_t thread_id = thread;
				#endif
				snprintf(thread_name, sizeof(thread_name), "%16X", (unsigned)thread_id);
			}
		#else // LOGURU_PTHREADS
			const char* thread_name = ""; // TODO: Windows
		#endif // LOGURU_PTHREADS

		if (s_strip_file_path) {
			file = filename(file);
		}

		char level_buff[6];
		if (verbosity <= NamedVerbosity::FATAL) {
			strcpy(level_buff, "FATL");
		} else if (verbosity == NamedVerbosity::ERROR) {
			strcpy(level_buff, "ERR");
		} else if (verbosity == NamedVerbosity::WARNING) {
			strcpy(level_buff, "WARN");
		} else {
			snprintf(level_buff, sizeof(level_buff) - 1, "% 4d", verbosity);
		}

		snprintf(out_buff, out_buff_size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld (%8.3fs) [%-*s]%23s:%-5u %4s| ",
			1900 + time_info.tm_year, 1 + time_info.tm_mon, time_info.tm_mday,
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec, ms_since_epoch % 1000,
			uptime_sec,
			THREAD_NAME_WIDTH, thread_name,
			file, line, level_buff);
	}

	static void log_message(const Message& message)
	{
		const auto verbosity = message.verbosity;
		std::lock_guard<std::recursive_mutex> lock(s_mutex);

		FILE* out = (verbosity <= static_cast<Verbosity>(NamedVerbosity::WARNING) ? s_err : s_out);
		fprintf(out, "%s%s%s%s\n",
			message.preamble, message.indentation, message.prefix, message.message);
		fflush(out);

		for (auto& p : s_callbacks) {
			if (verbosity <= p.verbosity) {
				p.callback(p.user_data, message);
			}
		}
	}

	void log_to_everywhere(Verbosity verbosity, const char* file, unsigned line, const char* prefix,
						   const char* buff)
	{
		char preamble_buff[128];
		print_preamble(preamble_buff, sizeof(preamble_buff), verbosity, file, line);

		auto message =
			Message{verbosity, file, line, preamble_buff, indentation(s_indentation), prefix, buff};

		log_message(message);
	}

	void log_to_everywhere_v(Verbosity verbosity, const char* file, unsigned line, const char* prefix, const char* format, va_list vlist)
	{
		auto buff = strprintfv(format, vlist);
		log_to_everywhere(verbosity, file, line, prefix, buff);
		free(buff);
	}

	void log(Verbosity verbosity, const char* file, unsigned line, const char* format, ...)
	{
		va_list vlist;
		va_start(vlist, format);
		log_to_everywhere_v(verbosity, file, line, "", format, vlist);
	}

	void raw_log(Verbosity verbosity, const char* file, unsigned line, const char* format, ...)
	{
		va_list vlist;
		va_start(vlist, format);
		auto buff = strprintfv(format, vlist);
		auto message = Message{verbosity, file, line, "", "", "", buff};
		log_message(message);
		free(buff);
		va_end(vlist);
	}

	LogScopeRAII::LogScopeRAII(Verbosity verbosity, const char* file, unsigned line, const char* format, ...)
		: _verbosity(verbosity), _file(file), _line(line), _start_time_ns(now_ns())
	{
		if ((int)verbosity <= g_verbosity) {
			va_list vlist;
			va_start(vlist, format);
			vsnprintf(_name, sizeof(_name), format, vlist);
			log_to_everywhere(_verbosity, file, line, "{ ", _name);
			va_end(vlist);
			++s_indentation;
		} else {
			_file = nullptr;
		}
	}

	LogScopeRAII::~LogScopeRAII()
	{
		if (_file) {
			--s_indentation;
			auto duration_sec = (now_ns() - _start_time_ns) / 1e9;
			log(_verbosity, _file, _line, "} %.*f s: %s", SCOPE_TIME_PRECISION, duration_sec, _name);
		}
	}

	void log_and_abort(int stack_strace_skip, const char* expr, const char* file, unsigned line, const char* format, ...)
	{
		va_list vlist;
		va_start(vlist, format);

		char* st = loguru::stacktrace(stack_strace_skip + 2);
		if (st && *st)
		{
			LOG_F(ERROR, "Stack trace:\n%s", st);
		}
		free(st);

		log_to_everywhere_v(NamedVerbosity::FATAL, file, line, expr, format, vlist);

		va_end(vlist);

		if (s_fatal_handler) {
			s_fatal_handler();
		}
		abort();
	}

	void log_and_abort(int stack_strace_skip, const char* expr, const char* file, unsigned line)
	{
		log_and_abort(stack_strace_skip + 1, expr, file, line, " ");
	}

	AbortLogger::~AbortLogger()
	{
		auto message = this->str();
		loguru::log_and_abort(1, _expr, _file, _line, "%s", message.c_str());
	}
} // namespace loguru

#endif // LOGURU_IMPLEMENTATION
