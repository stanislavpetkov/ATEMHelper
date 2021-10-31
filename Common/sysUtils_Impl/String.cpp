#include "String.h"
#include <Windows.h>
#include "fs.h"
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace pbn
{
	[[nodiscard]] uint32_t GetAnsiCodePageForLocale(LCID lcid)
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


	// convert wstring to UTF-8 string
	[[nodiscard]] inline std::string wstring_to_utf8(const std::wstring& str)
	{

		std::string res = {};
		res.resize(str.length() * sizeof(wchar_t) * 2);
		int lenres = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (LPSTR)res.c_str(), 0, nullptr, nullptr);
		if (lenres < 1) return "";

		res.resize(lenres);

		int result = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (LPSTR)res.c_str(), (int)res.length(), nullptr, nullptr);



		return res;
	}

#ifdef __cpp_char8_t
	// convert wstring to UTF-8 string
	[[nodiscard]] inline std::u8string wstring_to_utf8string(const std::wstring& str)
	{

		std::u8string res = {};
		res.resize(str.length() * sizeof(wchar_t) * 2);
		int lenres = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (LPSTR)res.c_str(), 0, nullptr, nullptr);
		if (lenres < 1) return u8"";

		res.resize(lenres);

		int result = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), (LPSTR)res.c_str(), (int)res.length(), nullptr, nullptr);



		return res;
	}
#endif

	template<typename T>
	[[nodiscard]] inline std::wstring codePage_to_wstring(const std::basic_string<T>& code_str, uint32_t codePage)
	{
		std::wstring ws;

		int wcharsNeeded = MultiByteToWideChar(codePage, 0, (LPCCH)code_str.c_str(), (int)code_str.length(), (LPWSTR)ws.c_str(), 0);
		if (wcharsNeeded == 0) return {};
		ws.resize(wcharsNeeded);

		int res = MultiByteToWideChar(codePage, 0, (LPCCH)code_str.c_str(), (int)code_str.length(), (LPWSTR)ws.c_str(), (int)ws.length());
		if (res == 0) return {};
		if (codePage == 20269) //workarounds bug in accented chars
		{
			const  wchar_t* subst = L"\u0300\u0301\u0302\u0303\u0304\u0306\u0307\u0308?\u030a\u0337?\u030b\u0328\u030c";
			if (code_str.length() == ws.length())
			{
				for (size_t i = 0; i < code_str.length(); i++)
				{
					if (((uint8_t)code_str[i] >= 0xC1) && ((uint8_t)code_str[i] <= 0xCF))
					{
						//acennt
						ws[i] = ws[i + 1];
						ws[i + 1] = subst[(uint8_t)code_str[i] - 0xC1];
					}
				}
			}
		}
		return ws;
	}

	std::wstring to_wstring(const std::basic_string<char>& txt, uint32_t codePage)
	{
		return codePage_to_wstring(txt, codePage);
	}
#ifdef __cpp_char8_t
	std::wstring to_wstring(const std::basic_string<char8_t>& txt)
	{
		return codePage_to_wstring(txt, CP_UTF8);
	}
#endif
	std::wstring to_wstring(const std::basic_string<wchar_t>& txt)
	{
		return txt;
	}

#ifdef __cpp_char8_t
	std::u8string to_u8string(const std::wstring& txt)
	{
		return wstring_to_utf8string(txt);
	}

	String::operator std::u8string() const
	{
		return to_u8string(data);
	}

	bool String::operator==(const std::u8string& other) const
	{
		return data == to_wstring(other);
	}

	bool String::operator==(const char8_t* other) const
	{
		String s(other);
		return s == *this;
	}
	pbn::String& String::operator+=(const std::u8string& other)
	{
		data.append(to_wstring(other));
		return *this;
	}
#endif

	std::string to_utf8(const std::wstring& txt)
	{
		return wstring_to_utf8(txt);
	}
	bool compare_logical(const std::wstring& lhs, const std::wstring& rhs)
	{
		return StrCmpLogicalW(lhs.c_str(), rhs.c_str()) < 1;
	}
	bool compare_logical(const std::string& lhs, const std::string& rhs)
	{
		return StrCmpLogicalW(to_wstring(lhs).c_str(), to_wstring(rhs).c_str()) < 1;
	}
	bool compare_logical(const pbn::String& lhs, const pbn::String& rhs)
	{
		return StrCmpLogicalW(lhs.c_str(), rhs.c_str()) < 1;
	}
	const wchar_t* String::c_str() const noexcept
	{
		return data_.c_str();
	}
	const wchar_t* String::data() const noexcept
	{
		return data_.data();
	}
	bool String::empty() const noexcept
	{
		return data_.empty();
	}
	String::operator std::string() const
	{
		return to_utf8(data_);
	}
	String::operator std::wstring() const
	{
		return data_;
	}


	bool String::operator==(const pbn::String& other) const
	{
		return data_ == other.data_;
	}

	bool String::operator==(const std::wstring& other) const
	{
		return data_ == other;
	}

	bool String::operator==(const std::string& other) const
	{
		return data_ == to_wstring(other);
	}



	bool String::operator==(const wchar_t* other) const
	{
		String s(other);
		return s == *this;
	}

	bool String::operator==(const char* other) const
	{
		String s(other);
		return s == *this;
	}



	pbn::String& String::operator=(const pbn::String& other)
	{
		data_ = other.data_;
		return *this;
	}

	pbn::String& String::operator+=(const pbn::String& other)
	{

		data_.append(other);
		return *this;
	}

	pbn::String& String::operator+=(const std::wstring& other)
	{
		data_.append(other);
		return *this;
	}

	pbn::String& String::operator+=(const std::string& other)
	{
		data_.append(to_wstring(other));
		return *this;
	}


	std::vector<std::string> split(const std::string& s, char seperator)
	{
		std::vector<std::string> output;

		std::string::size_type prev_pos = 0, pos = 0;

		while ((pos = s.find(seperator, pos)) != std::string::npos)
		{
			std::string substring(s.substr(prev_pos, pos - prev_pos));

			output.push_back(substring);

			prev_pos = ++pos;
		}

		output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

		return output;
	}

}