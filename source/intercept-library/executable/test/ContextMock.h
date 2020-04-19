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

#include "libsys/Context.h"

#include "gmock/gmock.h"

class ContextMock : public sys::Context {
public:
    MOCK_METHOD(
        (std::map<std::string, std::string>),
        get_environment,
        (),
        (const, override));
    MOCK_METHOD(
        pid_t,
        get_pid,
        (),
        (const, override));
    MOCK_METHOD(
        pid_t,
        get_ppid,
        (),
        (const, override));
    MOCK_METHOD(
        rust::Result<std::string>,
        get_confstr,
        (int key),
        (const, override));
    MOCK_METHOD(
        (rust::Result<std::map<std::string, std::string>>),
        get_uname,
        (),
        (const, override));
    MOCK_METHOD(
        rust::Result<std::list<std::string>>,
        get_path,
        (),
        (const, override));
    MOCK_METHOD(
        rust::Result<std::string>,
        get_cwd,
        (),
        (const, override));
};