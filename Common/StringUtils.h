#pragma once
#include <iostream>
#include <locale>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <Windows.h>
#include <cctype>
#include <cwctype>

#pragma warning(error:4834) //You can't discrad result from nodiscard function... we want it as error
#pragma warning(disable:4505) 

[[nodiscard]] static inline std::string string_replace(std::string src, std::string const& target, std::string const& repl)
{
	// handle error situations/trivial cases

	if (target.length() == 0) {
		// searching for a match to the empty string will result in 
		//  an infinite loop
		//  it might make sense to throw an exception for this case
		return src;
	}

	if (src.length() == 0) {
		return src;  // nothing to match against
	}

	size_t idx = 0;

	for (;;) {
		idx = src.find(target, idx);
		if (idx == std::string::npos)  break;

		src.replace(idx, target.length(), repl);
		idx += repl.length();
	}

	return src;
}



static inline void fixNumericLocale(char* begin, char* end) {
	while (begin < end) {
		if (*begin == ',') {
			*begin = '.';
		}
		++begin;
	}
}

[[nodiscard]] static inline std::string doubleToStringA(double value, unsigned int precision = 6) {
	// Allocate a buffer that is more than large enough to store the 16 digits of
	// precision requested below.
	char buffer[36];
	int len = -1;

	char formatString[6];
	sprintf_s(formatString, "%%.%ug", precision);

	// Print into the buffer. We need not request the alternative representation
	// that always has a decimal point because JSON doesn't distingish the
	// concepts of reals and integers.
	if (isfinite(value)) {
		len = snprintf(buffer, sizeof(buffer), formatString, value);

		// try to ensure we preserve the fact that this was given to us as a double on input
		if (!strstr(buffer, ".") && !strstr(buffer, "e")) {
			strcat_s(buffer, sizeof(buffer), ".0");
		}

	}
	else {
		// IEEE standard states that NaN values will not compare to themselves
		if (value != value) {
			len = snprintf(buffer, sizeof(buffer), "NaN");
		}
		else if (value < 0) {
			len = snprintf(buffer, sizeof(buffer), "-Infinity");
		}
		else {
			len = snprintf(buffer, sizeof(buffer), "Infinity");
		}
		// For those, we do not need to call fixNumLoc, but it is fast.
	}
	
	char * begin = &buffer[0];
	char * end = &buffer[0] + len;
	while (begin < end) {
		if (*begin == ',') {
			*begin = '.';
		}
		++begin;
	}

	return buffer;
}

[[nodiscard]] static inline std::wstring doubleToStringW(double value, unsigned int precision = 6) {
	// Allocate a buffer that is more than large enough to store the 16 digits of
	// precision requested below.
	wchar_t buffer[36];
	int len = -1;

	wchar_t formatString[6];
	_snwprintf_s(formatString,sizeof(formatString), L"%%.%ug", precision);

	// Print into the buffer. We need not request the alternative representation
	// that always has a decimal point because JSON doesn't distingish the
	// concepts of reals and integers.
	if (isfinite(value)) {
		len = _snwprintf_s(buffer, sizeof(buffer), formatString, value);
		
		// try to ensure we preserve the fact that this was given to us as a double on input
		if (!wcsstr(buffer, L".") && !wcsstr(buffer, L"e")) {
			wcscat_s(buffer, sizeof(buffer) / 2, L".0");
		}

	}
	else {
		// IEEE standard states that NaN values will not compare to themselves
		if (value != value) {
			len = _snwprintf_s(buffer, sizeof(buffer), L"NaN");
		}
		else if (value < 0) {
			len = _snwprintf_s(buffer, sizeof(buffer), L"-Infinity");
		}
		else {
			len = _snwprintf_s(buffer, sizeof(buffer), L"Infinity");
		}
		// For those, we do not need to call fixNumLoc, but it is fast.
	}

	

	wchar_t * begin = &buffer[0];
	wchar_t * end = &buffer[0] + len;
	while (begin < end) {
		if (*begin == L',') {
			*begin = L'.';
		}
		++begin;
	}
	return buffer;
}

[[nodiscard]] static uint32_t GetAnsiCodePageForLocale(LCID lcid)
{
	UINT acp;
	int sizeInChars = sizeof(acp) / sizeof(TCHAR);
	if (GetLocaleInfo(lcid,
		LOCALE_IDEFAULTANSICODEPAGE |
		LOCALE_RETURN_NUMBER,
		reinterpret_cast<LPTSTR>(&acp),
		sizeInChars) != sizeInChars) {
		// Oops - something went wrong
	}
	return acp;
}



[[nodiscard]] static std::wstring ansiTowstringCP(const std::string& ansi_str, uint32_t codePage)
{

	std::wstring ws;
	ws.resize((ansi_str.length() + 1) * 2);
	int res = MultiByteToWideChar(codePage, 0, ansi_str.c_str(), (int)ansi_str.length(), (LPWSTR)ws.c_str(), (int)ws.length());
	ws.resize(res);
	if (codePage == 20269) //workarounds bug in accented chars
	{
		const  wchar_t * subst = L"\u0300\u0301\u0302\u0303\u0304\u0306\u0307\u0308?\u030a\u0337?\u030b\u0328\u030c";
		if (ansi_str.length() == ws.length())
		{
			for (size_t i = 0; i < ansi_str.length(); i++)
			{
				if (((uint8_t)ansi_str[i] >= 0xC1) && ((uint8_t)ansi_str[i] <= 0xCF))
				{
					//acennt
					ws[i] = ws[i + 1];
					ws[i + 1] = subst[(uint8_t)ansi_str[i] - 0xC1];
				}
			}
		}
	}
	return ws;
}

[[nodiscard]] static std::wstring ansiTowstringLCID(const std::string& ansi_str, uint32_t LCID)
{

	UINT codePage;
	int sizeInChars = sizeof(codePage) / sizeof(wchar_t);
	if (GetLocaleInfo(LCID,
		LOCALE_IDEFAULTANSICODEPAGE |
		LOCALE_RETURN_NUMBER,
		reinterpret_cast<LPTSTR>(&codePage),
		sizeInChars) != sizeInChars) {
		// Oops - something went wrong
	}
	return ansiTowstringCP(ansi_str, codePage);
}





[[nodiscard]] static inline std::wstring utf8_to_wstring(const std::string& utf8_str)
{
	std::wstring ws;
	ws.resize((utf8_str.length() + 1) * 2);
	int res = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), (int)utf8_str.length(), (LPWSTR)ws.c_str(), (int)ws.length());
	ws.resize(res);
	return ws;
}

// convert wstring to UTF-8 string
[[nodiscard]] static inline std::string wstring_to_utf8(const std::wstring& str)
{

	std::string res = {};
	res.resize(str.length() * sizeof(wchar_t) * 2);
	int lenres = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (LPSTR)res.c_str(), (int)res.length(), nullptr, nullptr);
	if (lenres < 1) return "";
	res.resize(lenres);

	return res;
}



[[nodiscard]] inline  bool isSpace(char s) {
	return ((s == 0x20) || (s == 0x09) || (s == 0x0a) || (s == 0x0b) || (s == 0x0c) || (s == 0x0d));
}

[[nodiscard]] inline  bool isSpace(wchar_t s) {
	return ((s == 0x20) || (s == 0x09) || (s == 0x0a) || (s == 0x0b) || (s == 0x0c) || (s == 0x0d));
}


template<class charT>
[[nodiscard]] inline std::basic_string<charT>  ltrim(const std::basic_string<charT> & s) {
	
	if (s.empty()) return {};
	if (!isSpace(s.front())) return s;
	std::basic_string<charT> result{};
	result.reserve(s.size());
	
	for (auto it = s.begin(); it != s.end(); it++)
	{
		
		if (*it == 0x20 ) continue;
		std::copy(it, s.end(), std::back_inserter(result));
		break;
	}
	return result;
}



template<class charT>
[[nodiscard]] inline  std::basic_string<charT> rtrim(const std::basic_string<charT> & s) {

	if (s.empty()) return {};
	if (!isSpace(s.back())) return s;

	std::basic_string<charT> result;	
	result.reserve(s.size());
	for (auto it = s.rbegin(); it !=s.rend() ; it++)
	{
		if (*it != 0x20)
		{
			std::reverse_copy(it, s.rend(), std::back_inserter(result));
			break;
		}
	}
	
	return result;
}



template<class charT>
[[nodiscard]] inline  std::basic_string<charT> trim(const std::basic_string<charT> & s)
{
	return ltrim(rtrim(s));	
}


[[nodiscard]] inline std::wstring toUpper(const std::wstring & instr)
{
	std::wstring  dstString; dstString.resize(instr.size());
	auto ret = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, (LPCWSTR)instr.data(), (int)instr.size(), (LPWSTR)dstString.data(), (int)dstString.size(), nullptr, nullptr, 0);
	if (ret == 0) return instr;

	dstString.resize(ret);
	return dstString;
}



[[nodiscard]] inline std::string toUpper(const std::string& instr)
{
	return  wstring_to_utf8(toUpper(utf8_to_wstring(instr))); //that is going to be slow....
}


[[nodiscard]] inline std::wstring toLower(const std::wstring& instr)
{
	std::wstring  dstString; dstString.resize(instr.size());
	auto ret = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, (LPCWSTR)instr.data(), (int)instr.size(), (LPWSTR)dstString.data(), (int)dstString.size(), nullptr, nullptr, 0);
	if (ret == 0) return instr;

	dstString.resize(ret);
	return dstString;
}



[[nodiscard]] inline std::string toLower(const std::string& instr)
{	
	return  wstring_to_utf8(toLower(utf8_to_wstring(instr))); //that is going to be slow....
}
#pragma warning(default:4505) //You can't discrad result from nodiscard function... we want it as error