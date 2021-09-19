#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "String.h"


namespace pbn
{
	namespace fs
	{
		enum class PathType
		{
			relative,
			// C:\ xx
			windows_local,
			//Samba share
			windows_remote,
			//URL
			url

		};

	

		//this class does not touch the filesystem
		class Path
		{
			struct impl;
		private:
			std::shared_ptr<impl> impl_;
		public:

			Path() = delete;
			explicit Path(const Path&);
			explicit Path(Path&);
			explicit Path(Path&&) noexcept;


			Path(const pbn::String& path);

			Path& operator = (const Path&);

			[[nodiscard]] PathType type() const;

			Path& append(const pbn::String& path);

			void set_long_path(bool enable) const;

			[[nodiscard]] std::string u8string() const;
			[[nodiscard]] std::wstring wstring() const;

			operator std::string() const;
			operator std::wstring() const;
			operator pbn::String() const;


			//extension without dot
			[[nodiscard]] pbn::String extension() const;
			bool set_extension(const pbn::String& ext) const;

			[[nodiscard]] pbn::String file_name() const;
			[[nodiscard]] pbn::String file_name_wo_ext() const;

			[[nodiscard]] pbn::String get_root() const;

			[[nodiscard]] bool is_absolute_path() const;
			[[nodiscard]] bool is_relative_path() const;

			[[nodiscard]] bool is_local_path() const;
			[[nodiscard]] bool is_remote_path() const;

			[[nodiscard]] std::vector<pbn::String> get_sub_paths() const;

			void chunk_checker(const std::function<void(std::wstring& chunk)> & checker) const;

			Path parent_path() const;

			[[nodiscard]] bool get_relative_path(const Path& RootPath, Path& result);
			[[nodiscard]] bool get_relative_path(const Path& RootPath, std::wstring& result);
			[[nodiscard]] bool get_relative_path(const Path& RootPath, std::string& result);
			[[nodiscard]] bool get_relative_path(const Path& RootPath, pbn::String& result);
		};





		pbn::String get_file_name(const pbn::String & path_);
		pbn::String get_file_name_without_extension(const pbn::String& path_);
		

		bool dir_exist(const pbn::String& absolute_path);
		bool file_exist(const pbn::String& absolute_path);
		bool create_dirs(const pbn::String& absolute_path);
		bool delete_folder_recursive(const pbn::String& absolute_path);
		bool remove(const pbn::String& absolute_path);

		
		bool isFolderEmpty(const pbn::String& absolute_path);
		std::vector< pbn::String> get_directories(const pbn::String& absolute_path);
		std::vector< pbn::String> get_files(const pbn::String& absolute_path);

		bool file_copy(const Path& src, const Path& dst, bool fail_if_exit);
		std::optional<uint64_t> file_size(const pbn::String& absolute_path);
		bool file_can_open_for_write(const pbn::String& absolute_path);


	}

}