/*  Copyright (C) 2012-2020 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "libresult/Result.h"

#include <list>
#include <map>
#include <string>

namespace sys {

    class Context {
    public:
        Context();

        virtual ~Context() noexcept = default;

    public:
        Context(const Context&) = delete;
        Context(Context&&) noexcept = delete;

        Context& operator=(const Context&) = delete;
        Context& operator=(Context&&) noexcept = delete;

    public:
        static constexpr char OS_SEPARATOR = '/';
        static constexpr char OS_PATH_SEPARATOR = ':';

        static std::list<std::string> split_path(const std::string& input);
        static std::string join_path(const std::list<std::string>& input);

    public:
        // Query methods about the process.
        [[nodiscard]] virtual std::map<std::string, std::string> get_environment() const;

        [[nodiscard]] virtual pid_t get_pid() const;
        [[nodiscard]] virtual pid_t get_ppid() const;

        // Query methods about the system.
        [[nodiscard]] virtual rust::Result<std::string> get_confstr(int key) const;
        [[nodiscard]] virtual rust::Result<std::map<std::string, std::string>> get_uname() const;

        // Return PATH from environment and fall back to confstr default one.
        [[nodiscard]] virtual rust::Result<std::list<std::string>> get_path() const;

        // Filesystem related operations
        [[nodiscard]] virtual rust::Result<std::string> get_cwd() const;

    private:
        const pid_t current_;
        const pid_t parent_;
        const char** const envp_;
    };
}