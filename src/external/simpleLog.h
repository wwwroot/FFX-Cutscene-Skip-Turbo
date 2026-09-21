#pragma once
#include <mutex>
#pragma warning(disable: 4244)
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#ifdef LOGLIB
#define LOGLIB_EXPORT extern "C" __declspec(dllexport) 
#else
#define LOGLIB_EXPORT extern "C" __declspec(dllimport) 
#endif

enum LogLevel {
	INFO = 0,
	WARN = 1,
	ERR = 2,
	NO_TIMESTAMP = 0x8000
};

LOGLIB_EXPORT void _log(int l, const char* s);

struct LOG {
protected:
	std::stringstream s_;
	LogLevel l_;
	LOG(const LOG & o) = delete;
	LOG& operator=(const LOG & o) = delete;
public:
	LOG(LogLevel l) :l_(l) {}
	~LOG()
	{
		s_ << std::endl;
		s_.flush();
		_log((int)l_, s_.str().c_str());
	}
	template<class T>
	LOG &operator<<(const T &x) {
		s_ << x;
		return *this;
	}
	LOG &operator<<(const std::wstring &s) {
		return *this << std::string(s.begin(), s.end());
	}
	LOG &operator<<(const wchar_t *s) {
		return *this << std::wstring(s);
	}
};

template<typename T>
LOG &operator<<(LOG &o, const std::vector<T> &v) {
	o << "[";
	for (size_t i = 0; i < v.size(); i++)
		o << v[i] << (i + 1 < v.size() ? ", " : "]");
	return o;
}

#define ASSERT(x) {if (!(x)) LOG(ERR) << __FILE__ << ":" << __LINE__ << "    Assertion failed! " << #x; }
