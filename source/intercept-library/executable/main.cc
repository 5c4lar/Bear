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

#include "Command.h"
#include "Flags.h"
#include "SystemCalls.h"
#include "er.h"

#include <iostream>
#include <string_view>

namespace {

    constexpr char VERSION[] = _ER_VERSION;

    std::ostream& error_stream()
    {
        std::cerr << "er: [pid: "
                  << er::SystemCalls::get_pid().unwrap_or(0)
                  << ", ppid: "
                  << er::SystemCalls::get_ppid().unwrap_or(0)
                  << "] ";
        return std::cerr;
    }

    std::ostream& operator<<(std::ostream& os, char* const* values)
    {
        os << '[';
        for (char* const* it = values; *it != nullptr; ++it) {
            if (it != values) {
                os << ", ";
            }
            os << '"' << *it << '"';
        }
        os << ']';

        return os;
    }
}

int main(int argc, char* argv[], char* envp[])
{
    const flags::Parser parser("er", VERSION,
        { { ::er::flags::VERBOSE, { 0, false, "make the interception run verbose", std::nullopt, std::nullopt } },
            { ::er::flags::DESTINATION, { 1, true, "path to report directory", std::nullopt, std::nullopt } },
            { ::er::flags::LIBRARY, { 1, true, "path to the intercept library", std::nullopt, std::nullopt } },
            { ::er::flags::EXECUTE, { 1, true, "the path parameter for the command", std::nullopt, std::nullopt } },
            { ::er::flags::COMMAND, { -1, true, "the executed command", std::nullopt, std::nullopt } } });
    return parser.parse_or_exit(argc, const_cast<const char**>(argv))
        // log the original command line as it was received.
        .map<flags::Arguments>([&argv](const auto& args) {
            if (args.as_bool(::er::flags::VERBOSE).unwrap_or(false)) {
                error_stream() << argv << std::endl;
            }
            return args;
        })
        // if parsing success, we create the main command and execute it.
        .and_then<er::Session>([](auto args) {
            return er::create(args);
        })
        .and_then<int>([&envp](auto command) {
            return er::run(std::move(command), envp);
        })
        // set the return code from error and print message
        .unwrap_or_else([](auto error) {
            return EXIT_FAILURE;
        });
}
