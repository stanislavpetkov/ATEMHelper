#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "fs.h"

#include <vector>
#include <Windows.h>

#include <algorithm>
#include <iterator>     // std::back_inserter
#include <Shlwapi.h>
#include "String.h"


inline  std::wstring GetNextSegment(const std::wstring::const_iterator& beg, const std::wstring::const_iterator& end, std::wstring::const_iterator& nextIter)
{
	std::wstring::const_iterator begin = beg;
	if (begin == end)
	{
		nextIter = end;
		return L"";
	}
	const auto c = *begin;
	auto b_search = (c == L'\\') ||
		(c == L'/');

	std::wstring result;
	while (true)
	{
		if (begin == end)
		{
			nextIter = end;
			return result;
		}
		const auto ch = *begin;

		if (b_search)
		{
			const auto b_skip = (ch == L'\\') || (ch == L'/');
			if (b_skip)
			{
				++begin;
				continue;
			}
			b_search = false;
		}
		const bool b_end = (ch == L'\\') || (ch == L'/');

		if (b_end)
		{
			++begin;
			nextIter = begin;
			return result;
		}

		result += ch;

		++begin;
	}
}

inline std::vector<std::wstring> GetPathChunks(const std::wstring& relativePath)
{


	std::vector<std::wstring> relative_chunks;
	if (relativePath.empty()) return relative_chunks;

	auto begin = relativePath.begin();
	const auto end = relativePath.end();
	while (true)
	{
		auto folder = GetNextSegment(begin, end, begin);

		if (folder == L".") continue;

		if (folder == L"..")
		{
			if (begin == end)
			{
				if (!relative_chunks.empty())
				{
					relative_chunks.pop_back();
				}
				break;
			}
			continue;
		}

		if (!folder.empty())
		{
			relative_chunks.push_back(folder);
		}

		if (begin == end)
		{

			break;
		}

	}

	return relative_chunks;
}


namespace pbn::fs
{

		struct Path::impl
		{
			bool veryLongPath = false;
			PathType pathType = PathType::relative;



			std::wstring root;
			std::wstring params;
			std::vector<std::wstring> pathChunks;


			[[nodiscard]] std::wstring get_root() const
			{
				if (!is_absolute_path()) return {};

				std::wstring result{};

				if (pathType == PathType::windows_local)
				{				
					if (veryLongPath)
					{
						result = L"\\\\?\\";
					}

					result.append(root);
					return result;
				}

				if (pathType == PathType::windows_remote)
				{
					if (veryLongPath)
					{
						result = L"\\\\?\\UNC\\" + root;
					}
					else
					{
						result = L"\\\\" + root;
					}
					return result;
				}
				
				if (pathType == PathType::url)
				{
					return root;
				}
				return {};
			}


			[[nodiscard]] std::wstring file_name() const
			{
				if (pathType == PathType::url)  return {};
				return  pathChunks.empty() ? L"" : pathChunks.back();
			}

			[[nodiscard]] bool is_absolute_path() const;

			[[nodiscard]] std::wstring file_name_wo_extension() const
			{
				auto fn = file_name();

				if (fn.empty()) return {};


				for (auto it = fn.rbegin(); it != fn.rend(); ++it)
				{
					if (*it == L'.')
					{
						const size_t pos = fn.rend() - it;
						fn.erase(pos - 1);
						return fn;
					}
				}

				return {};
			}


			bool set_extension(const std::wstring& ext)
			{
				if (pathChunks.empty()) return false;

				if ((pathType == PathType::windows_remote) && (pathChunks.size() < 2)) return false;

				std::wstring extInt = ext;

				while ((!extInt.empty()) && (extInt.front() == L'.')) extInt.erase(0, 1);
				std::replace(extInt.begin(), extInt.end(), L'/', L'_');
				std::replace(extInt.begin(), extInt.end(), L'\\', L'_');




				auto last = file_name();
				if (last.empty()) return false;


				for (auto it = last.rbegin(); it != last.rend(); it++)
				{
					if (*it == L'.')
					{
						const size_t pos = last.rend() - it;
						last.erase(pos);
						last += extInt;
						pathChunks.back() = last;
						return true;
					}
				}

				last += L"." + extInt;
				pathChunks.back() = last;
				return true;
			}


			[[nodiscard]] std::wstring extension() const
			{
				auto last = file_name();
				if (last.empty()) return {};


				for (auto it = last.rbegin(); it != last.rend(); ++it)
				{
					if (*it == L'.')
					{
						const size_t pos = last.rend() - it;
						return last.substr(pos);
					}
				}

				return {};
			}
			void set_long_path(const bool enable)
			{
				veryLongPath = enable;
			}


			[[nodiscard]] std::wstring as_string() const
			{
				

				std::wstring divider;
				if (pathType == pbn::fs::PathType::url)
				{
					divider = L"/";
				}
				else
				{
					divider = L"\\";
				}

				
				auto rootStr = get_root();

				size_t counter = 0;
				for (const auto& elm : pathChunks)
				{
					counter++;
					rootStr.append(elm);
					if (counter != pathChunks.size())
					{
						rootStr.append(divider);
					}
				}

				if (PathType::url == pathType)
				{
					if (!params.empty()) rootStr.append(params);
				}

				return rootStr;

			}

			void parse_url(std::wstring& url)
			{
				const auto access_method_divider = url.find(L"://");
				const auto next_part = url.find(L'/', access_method_divider+3);

				root = url.substr(0, next_part);

				if (next_part == std::wstring::npos) {
					url.clear();
					return;
				}
				else
				{
					url.erase(url.begin(), url.begin() + next_part + 1);
					if (url.empty()) return;
				}

				const auto qm = std::min(url.find(L'?'), url.find(L'#'));
				if (qm != std::wstring::npos)
				{
					params = url.substr(qm);
					url.erase(qm);
				}
				
				
				//parse the rest

				pathChunks = GetPathChunks(url);

			}

			explicit impl(const std::wstring& path)
			{
				std::wstring tmp_path(path);


				const auto access_method_divider = tmp_path.find(L"://");
				if (access_method_divider != std::wstring::npos)
				{
					pathType = PathType::url;
					parse_url(tmp_path);
					return;
				}


				std::replace(tmp_path.begin(), tmp_path.end(), L'/', L'\\');


				if (tmp_path._Starts_with(L"\\\\?\\"))
				{
					veryLongPath = true;
					tmp_path.erase(static_cast<size_t>(0), static_cast<size_t>(4));

					if (tmp_path._Starts_with(L"UNC"))
					{
						tmp_path.erase(static_cast<size_t>(0), static_cast<size_t>(3));
						tmp_path = L"\\" + tmp_path;
					}
				}

				if (tmp_path.size() > 1)
				{
					if (tmp_path[1] == L':')
					{
						const auto first = static_cast<wchar_t>(tmp_path[0] | 0x20);
						if ((first < L'a') || (first > L'z')) throw std::exception("Invalid path");
						//Should be drive
						pathType = PathType::windows_local;
						tmp_path = tmp_path.erase(static_cast<size_t>(0), static_cast<size_t>(2));
						while (!tmp_path.empty())
						{
							if (tmp_path.front() == L'\\') tmp_path = tmp_path.erase(static_cast<size_t>(0), static_cast<size_t>(1));
							break;
						}
						
						root = static_cast<wchar_t>(first ^ 0x20); //To Upper
						root.append(L":\\");


						pathChunks = GetPathChunks(tmp_path);

						return;
					}
				}


				if (tmp_path._Starts_with(L"\\\\")) //Storage - \\host\path
				{
					pathType = PathType::windows_remote;
					tmp_path.erase(static_cast<size_t>(0), static_cast<size_t>(2));

					const auto pos = tmp_path.find_first_of(L'\\');
					if (pos == std::wstring::npos)
					{
						root = tmp_path + L"\\";
						tmp_path.clear();
					}
					else
					{
						root = tmp_path.substr(0, pos) + L"\\";
						tmp_path.erase(0, pos);
					}
				}

				pathChunks = GetPathChunks(tmp_path);
				if ((pathType == PathType::windows_remote) && (!pathChunks.empty()))
				{
					root.append(pathChunks.front() + L"\\");
					pathChunks.erase(pathChunks.begin());
				}

				
			}



			void append(const std::wstring& path)
			{
				
				auto chunks = GetPathChunks(path);
				std::copy(chunks.begin(), chunks.end(), std::back_inserter(pathChunks));
			}
		};

		bool Path::impl::is_absolute_path() const
		{
			return (pathType != PathType::relative);
		}


		Path::Path(Path& other) :
			impl_(std::make_shared<impl>(other))
		{

		}

		Path::Path(const Path& other):
			impl_(std::make_shared<impl>(other))
		{
				
		}

		Path::Path(Path&& other) noexcept
		{
			impl_.swap(other.impl_);
		}

		Path::Path(const pbn::String& path) :
			impl_(std::make_shared<impl>(path))
		{

		}

		Path& Path::operator=(const Path& other)
		{
			impl_ = std::make_shared<impl>(other.wstring());
			return *this;
		}

		PathType Path::type() const
		{
			return impl_->pathType;
		}

		Path& Path::append(const pbn::String& path)
		{
			const Path tmp(path);
			if (tmp.is_absolute_path()) throw std::exception( "Append absolute path is not possible");

			impl_->append(path);
			return *this;
		}
		void Path::set_long_path(bool enable) const
		{
			return impl_->set_long_path(enable);
		}
		std::string Path::u8string() const
		{
			return to_utf8(wstring());
		}
		std::wstring Path::wstring() const
		{
			return impl_->as_string();
		}

		fs::Path::operator pbn::String() const
		{
			return impl_->as_string();
		}

		fs::Path::operator std::string() const
		{
			return to_utf8(wstring());
		}

		fs::Path::operator std::wstring() const
		{
			return impl_->as_string();
		}


		pbn::String Path::extension() const
		{
			return impl_->extension();
		}

		bool Path::set_extension(const pbn::String& ext) const
		{
			return impl_->set_extension(ext);
		}


		pbn::String Path::file_name() const
		{
			return impl_->file_name();
		}

		

		pbn::String Path::file_name_wo_ext() const
		{
			return impl_->file_name_wo_extension();
		}

		pbn::String Path::get_root() const
		{
			return impl_->get_root();
		}

		bool Path::is_absolute_path() const
		{
			return impl_->is_absolute_path();
		}

		bool Path::is_relative_path() const
		{
			return !is_absolute_path();
		}

		bool Path::is_local_path() const
		{
			return impl_->pathType == PathType::windows_local;
		}

		bool Path::is_remote_path() const
		{
			return impl_->pathType == PathType::windows_remote;
		}

		std::vector<pbn::String> Path::get_sub_paths() const
		{
			std::vector<pbn::String> return_data;
			
			if (impl_->pathType == PathType::url) {
				return_data.emplace_back(impl_->as_string());
				return return_data;
			}
			//windows or relative

			auto root_str = impl_->get_root();

			auto is_first = impl_->pathType != PathType::relative;
			for (const auto& elm : impl_->pathChunks)
			{
				if (!is_first)
					root_str.append(L"\\");
				is_first = false;
				root_str.append(elm);
				return_data.emplace_back(root_str);
			}

			return return_data;
		}

		void Path::chunk_checker(const std::function<void(std::wstring& chunk)> & checker) const
		{
			
			for (auto& elm : impl_->pathChunks)
			{
				checker(elm);
			}
		}

		Path Path::parent_path() const
		{
			auto sp = get_sub_paths();
			if (sp.size()<2)
			{
				
				Path p(get_root());
				return p;
			}
			
			Path p(sp[sp.size() - 2]);
			
			return p;
		}

		bool Path::get_relative_path(const Path& RootPath, Path & result)
		{
			if (!impl_->is_absolute_path()) return false;

			auto me = impl_->as_string();
			auto root = RootPath.wstring();
			auto res = me.find(root);
			if (res != 0) return false;

			
			result = Path(me.substr(root.length()));

			return true;
		}

		bool Path::get_relative_path(const Path& RootPath, std::wstring& result)
		{
			Path p("/");
			if (!get_relative_path(RootPath, p)) return false;
			result = p;
			return true;
		}

		bool Path::get_relative_path(const Path& RootPath, std::string& result)
		{
			Path p("/");
			if (!get_relative_path(RootPath, p)) return false;
			result = p;
			return true;
		}

		bool Path::get_relative_path(const Path& RootPath, pbn::String& result)
		{
			Path p("/");
			if (!get_relative_path(RootPath, p)) return false;
			result = p;
			return true;
		}



		pbn::String get_file_name(const pbn::String& path_)
		{
			const Path p(path_);

			return p.file_name();
		}

		pbn::String get_file_name_without_extension(const pbn::String& path_)
		{
			const Path p(path_);

			return p.file_name_wo_ext();
		}

		bool dir_exist(const pbn::String& absolute_path)
		{
			const auto attributes = ::GetFileAttributesW(absolute_path.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) return false;

			const auto is_folder_exist = ((attributes & FILE_ATTRIBUTE_DEVICE) == 0) &&
				((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) && (attributes & FILE_ATTRIBUTE_DIRECTORY) && ((attributes & FILE_ATTRIBUTE_OFFLINE) == 0);

			return is_folder_exist;

		};

		bool file_exist(const pbn::String& absolute_path)
		{
			const auto attributes = ::GetFileAttributesW(absolute_path.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES) return false;

			const auto is_file_exist = ((attributes & FILE_ATTRIBUTE_DEVICE) == 0) &&
				((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) &&
				((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
				((attributes & FILE_ATTRIBUTE_OFFLINE) == 0);

			return is_file_exist;


		};

		bool create_dirs(const pbn::String& absolute_path)
		{

			if (dir_exist(absolute_path)) return true;

			const Path p(absolute_path);
			if (p.is_relative_path()) return false;

			const auto sub_path = p.get_sub_paths();

			  // NOLINT(readability-use-anyofallof)
			for (const auto& path : sub_path)
			{
				if (dir_exist(path)) continue;
				if (TRUE != CreateDirectoryW(path.c_str(), nullptr)) return false;
			}			
			return true;
		}

		bool delete_folder_recursive(const pbn::String& absolute_path)
		{
			const Path p(absolute_path);
			if (!p.is_absolute_path()) return false;

			auto files = get_files(absolute_path);
			for (const auto& file : files)
			{
				Path fi(absolute_path);
				fi.append(file);
				std::wstring w = fi;
				if (TRUE != DeleteFileW(w.c_str())) return false;
			}

			auto dirs = get_directories(absolute_path);
			for (const auto& dir : dirs)
			{
				if (dir == L"..") continue;
				Path fi(absolute_path);
				fi.append(dir);
				std::wstring w = fi;
				if (!delete_folder_recursive(w)) return false;
			}

			return RemoveDirectoryW(p.wstring().c_str()) == TRUE;
		}

		bool remove(const pbn::String& absolute_path)
		{
			if (dir_exist(absolute_path))
			{
				return delete_folder_recursive(absolute_path);
			}

			const auto res = DeleteFileW(absolute_path.c_str());
			if (!res)
			{
				const auto dw = GetLastError();
				if (dw == ERROR_FILE_NOT_FOUND) return true;
				return false;
			}
			return true;
		}





		bool isFolderEmpty(const pbn::String& absolute_path)
		{
			
			return PathIsDirectoryEmptyW(absolute_path.c_str()) == TRUE;
		}

		std::vector< pbn::String> get_directories(const pbn::String& absolute_path)
		{
			std::vector< pbn::String> result;

			const pbn::fs::Path main_path(absolute_path);
			if (!main_path.is_absolute_path()) return result;


			if (!dir_exist(main_path)) return result;

			//should be an Absolute path
			//We can use windows functions here
			std::wstring dir = main_path;


			dir.append(L"\\*");



			WIN32_FIND_DATAW ffd{};
			HANDLE h_find = FindFirstFileExW(dir.c_str(), FindExInfoStandard, &ffd,
			                                       FindExSearchLimitToDirectories, nullptr, 0);

			if (INVALID_HANDLE_VALUE == h_find)
			{
				return result;
			}




			do
			{


				if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;


				pbn::String s(&ffd.cFileName[0]);
				if (s == L".") continue;
				if (s == L"..") continue;


				result.push_back(s);
			} while (FindNextFileW(h_find, &ffd));

			FindClose(h_find);


			std::sort(result.begin(), result.end(), [](const pbn::String& a, const pbn::String& b) {
				if (a == L"..") return true;
				if (b == L"..") return false;

				return StrCmpLogicalW(a.c_str(), b.c_str()) == -1;
				});


			return result;
		}



		std::vector< pbn::String> get_files(const pbn::String& absolute_path)
		{
			std::vector< pbn::String> result;

			const pbn::fs::Path main_path(absolute_path);
			if (!main_path.is_absolute_path()) return result;


			if (!dir_exist(main_path)) return result;

			//should be an Absolute path
			//We can use windows functions here
			std::wstring dir = main_path;


			dir.append(L"\\*");



			WIN32_FIND_DATAW ffd{};
			HANDLE hFind = FindFirstFileExW(dir.c_str(), FindExInfoStandard, &ffd, FindExSearchNameMatch, nullptr, 0);

			if (INVALID_HANDLE_VALUE == hFind)
			{
				return result;
			}




			do
			{
				const auto is_file = ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) &&
					((ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) == 0) &&
					((ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);

				if (!is_file) continue;


				pbn::String s(&ffd.cFileName[0]);

				result.push_back(s);
			} while (FindNextFileW(hFind, &ffd));

			FindClose(hFind);


			std::sort(result.begin(), result.end(), [](const pbn::String& a, const pbn::String& b) {
				return StrCmpLogicalW(a.c_str(), b.c_str()) == -1;
				});


			return result;
		}

		bool file_copy(const Path& src, const Path& dst, const bool fail_if_exit)
		{
			const std::wstring s = src;
			const std::wstring d = dst;

			return TRUE == CopyFileW(s.c_str(), d.c_str(), fail_if_exit);			
		}


		std::optional<uint64_t> file_size(const pbn::String& absolute_path)
		{
			const Path dir(absolute_path);
			const std::wstring w(dir);

			HANDLE hndl = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
			if (hndl == INVALID_HANDLE_VALUE) return {};

			LARGE_INTEGER li;
			
			if (TRUE != GetFileSizeEx(hndl, &li))
			{
				CloseHandle(hndl);
				return {};
			}
			CloseHandle(hndl);
			return li.QuadPart;			
		}

		bool file_can_open_for_write(const pbn::String& absolute_path)
		{
			const Path dir(absolute_path);
			const std::wstring w(dir);

			HANDLE hndl = CreateFileW(w.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
			if (hndl == INVALID_HANDLE_VALUE) return false;			
			CloseHandle(hndl);
			return true;
		}
	
} //pbn
