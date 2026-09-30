#pragma once

#include <filesystem>

auto log_access(const std::filesystem::path& path) -> void;

auto log_stat(const std::filesystem::path& path) -> void;

auto log_lstat(const std::filesystem::path& path) -> void;

auto log_fstat(const std::filesystem::path& path) -> void;

auto log_user_readable(const std::filesystem::path& path) -> void;

auto log_head(const std::filesystem::path& path) -> void;

auto log_dirs(const std::filesystem::path& path) -> void;