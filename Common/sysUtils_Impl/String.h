#pragma once
#include <string>
#include <type_traits>
#include <vector>

namespace pbn
{
#ifdef __cpp_char8_t
	extern  std::wstring to_wstring(const std::basic_string<char8_t>&);
	extern std::u8string to_u8string(const std::wstring&);
#endif


	extern  std::wstring to_wstring(const std::basic_string<char>&, uint32_t codePage = 65001);//CP_UTF8=65001	
	extern  std::wstring to_wstring(const std::basic_string<wchar_t>&);

	extern std::string to_utf8(const std::wstring&);


	
	



	class String
	{
	private:

		std::wstring data_;
	public:
		String() = default;
		String(String&&) = default;
		String(const String&) = default;
		//String(String&) = default;

		[[nodiscard]] const wchar_t* c_str() const noexcept;
		[[nodiscard]] const wchar_t* data() const noexcept;
		bool empty() const noexcept;

		template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
			|| std::is_same_v<ST, char8_t>
#endif
				>>
				String(const std::basic_string<ST>& str)
			{
				data_ = to_wstring(str);
			};


			template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
				|| std::is_same_v<ST, char8_t>
#endif
					>>
					String(const ST* str)
				{
					data_ = to_wstring(std::basic_string<ST>(str));
				};

				template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
					|| std::is_same_v<ST, char8_t>
#endif
						>>
						String(const ST* str, size_t length)
					{
						data_ = to_wstring(std::basic_string<ST>(str, length));
					};


					operator std::string() const;
					operator std::wstring() const;
#ifdef __cpp_char8_t
					operator std::u8string() const;
#endif



					template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
						|| std::is_same_v<ST, char8_t>
#endif
							>>
							String & append(const std::basic_string<ST>& str)
						{
							auto tmp = to_wstring(str);
							data_.append(tmp);
							return *this;
						};



						template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
							|| std::is_same_v<ST, char8_t>
#endif
								>>
								String & append(const ST* str)
							{
								auto tmp = to_wstring(std::basic_string<ST>(str));
								data_.append(tmp);
								return *this;
							};

							template<typename ST, typename = std::enable_if_t<std::is_same_v<ST, char> || std::is_same_v<ST, wchar_t>
#ifdef __cpp_char8_t
								|| std::is_same_v<ST, char8_t>
#endif
									>>
									String & append(const ST* str, size_t count)
								{
									auto tmp = to_wstring(std::basic_string<ST>(str, count));
									data_.append(tmp);
									return *this;
								};


								bool operator ==(const pbn::String& other) const;
								bool operator ==(const std::wstring& other) const;
								bool operator ==(const std::string& other) const;
#ifdef __cpp_char8_t
								bool operator ==(const std::u8string& other) const;
#endif


								bool operator ==(const wchar_t* other) const;
								bool operator ==(const char* other) const;
#ifdef __cpp_char8_t
								bool operator ==(const char8_t* other) const;
#endif


								pbn::String& operator =(const pbn::String& other);

								pbn::String& operator +=(const pbn::String& other);
								pbn::String& operator +=(const std::wstring& other);
								pbn::String& operator +=(const std::string& other);
#ifdef __cpp_char8_t
								pbn::String& operator +=(const std::u8string& other);
#endif

								template <typename Other>
								bool operator !=(const Other& other) const
								{
									return !(*this == other);
								}
	};

	template <typename Lhs>
	bool operator ==(const Lhs& lhs, const pbn::String& rhs)
	{
		return (rhs == lhs);
	}

	template <typename Lhs>
	bool operator !=(const Lhs& lhs, const pbn::String& rhs)
	{
		return !(rhs == lhs);
	}


	extern bool compare_logical(const std::wstring& lhs, const std::wstring& rhs);
	extern bool compare_logical(const std::string& lhs, const std::string& rhs);
	extern bool compare_logical(const pbn::String& lhs, const pbn::String& rhs);
}