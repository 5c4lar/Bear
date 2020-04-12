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

#include "config.h"

#include "Application.h"
#include "Session.h"
#include "er/Flags.h"
#include "libexec/Environment.h"
#include "libsys/Environment.h"

#include <functional>
#include <numeric>
#include <unistd.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace {

    rust::Result<ic::Session::HostInfo> create_host_info(const sys::Context& context)
    {
        return context.get_uname()
#ifdef HAVE_CS_PATH
            .map<ic::Session::HostInfo>([&context](auto result) {
                context.get_confstr(_CS_PATH)
                    .map<int>([&result](auto value) {
                        result.insert({ "_CS_PATH", value });
                        return 0;
                    });
                return result;
            })
#endif
#ifdef HAVE_CS_GNU_LIBC_VERSION
            .map<ic::Session::HostInfo>([&context](auto result) {
                context.get_confstr(_CS_GNU_LIBC_VERSION)
                    .map<int>([&result](auto value) {
                        result.insert({ "_CS_GNU_LIBC_VERSION", value });
                        return 0;
                    });
                return result;
            })
#endif
#ifdef HAVE_CS_GNU_LIBPTHREAD_VERSION
            .map<ic::Session::HostInfo>([&context](auto result) {
                context.get_confstr(_CS_GNU_LIBPTHREAD_VERSION)
                    .map<int>([&result](auto value) {
                        result.insert({ "_CS_GNU_LIBPTHREAD_VERSION", value });
                        return 0;
                    });
                return result;
            })
#endif
            .map_err<std::runtime_error>([](auto error) {
                return std::runtime_error("failed to get host info.");
            });
    }
}

namespace env {

    constexpr char GLIBC_PRELOAD_KEY[] = "LD_PRELOAD";

    using env_t = std::map<std::string, std::string>;
    using mapper_t = std::function<std::string(const std::string&, const std::string&)>;

    std::string
    merge_into_paths(const std::string& current, const std::string& value) noexcept
    {
        auto paths = sys::Context::split_path(current);
        if (std::find(paths.begin(), paths.end(), value) == paths.end()) {
            paths.emplace_front(value);
            return sys::Context::join_path(paths);
        } else {
            return current;
        }
    }

    void insert_or_assign(env_t& target, const char* key, const std::string& value) noexcept
    {
        if (auto it = target.find(key); it != target.end()) {
            it->second = value;
        } else {
            target.emplace(key, value);
        }
    }

    void insert_or_merge(
        env_t& target,
        const char* key,
        const std::string& value,
        const mapper_t& merger) noexcept
    {
        if (auto it = target.find(key); it != target.end()) {
            it->second = merger(it->second, value);
        } else {
            target.emplace(key, value);
        }
    }
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

namespace {

    class LibraryPreloadSession : public ic::Session {
    public:
        LibraryPreloadSession(HostInfo&& host_info, const std::string_view& library, const std::string_view& executor, const sys::Context& context);

    public:
        [[nodiscard]] rust::Result<std::string_view> resolve(const std::string& name) const override;
        [[nodiscard]] rust::Result<std::map<std::string, std::string>> update(const std::map<std::string, std::string>& env) const override;
        [[nodiscard]] rust::Result<int> supervise(const std::vector<std::string_view>& command) const override;

        void set_server_address(const std::string&) override;

        [[nodiscard]] const HostInfo& get_host_info() const override;
        [[nodiscard]] std::string get_session_type() const override;

    private:
        HostInfo host_info_;
        std::string server_address_;
        std::string library_;
        std::string executor_;
        const sys::Context& context_;
    };

    LibraryPreloadSession::LibraryPreloadSession(ic::Session::HostInfo&& host_info, const std::string_view& library, const std::string_view& executor, const sys::Context& context)
            : host_info_(host_info)
            , server_address_()
            , library_(library)
            , executor_(executor)
            , context_(context)
    {
        spdlog::debug("Created library preload session. [library={0}, executor={1}]", library_, executor_);
    }

    rust::Result<std::string_view> LibraryPreloadSession::resolve(const std::string& name) const
    {
        // The method has to be MT safe!!!
        return rust::Err(std::runtime_error("The session does not support resolve."));
    }

    rust::Result<std::map<std::string, std::string>> LibraryPreloadSession::update(const std::map<std::string, std::string>& env) const
    {
        // The method has to be MT safe!!!
        std::map<std::string, std::string> copy(env);
        env::insert_or_assign(copy, el::env::KEY_REPORTER, executor_);
        env::insert_or_assign(copy, el::env::KEY_DESTINATION, server_address_);
        env::insert_or_merge(copy, env::GLIBC_PRELOAD_KEY, library_, env::merge_into_paths);

        return rust::Ok(copy);
    }

    rust::Result<int> LibraryPreloadSession::supervise(const std::vector<std::string_view>& command) const
    {
        auto environment = update(context_.get_environment());
        auto program = context_.resolve_executable(std::string(command.front()));

        return rust::merge(program, environment)
            .and_then<pid_t>([&command, this](auto pair) {
                const auto& [program, environment] = pair;
                // create the argument list
                std::vector<const char*> args = {
                    executor_.c_str(),
                    er::flags::DESTINATION,
                    server_address_.c_str(),
                    er::flags::EXECUTE,
                    program.c_str(),
                    er::flags::COMMAND
                };
                std::transform(command.begin(), command.end(), std::back_insert_iterator(args),
                    [](const auto& it) { return it.data(); });
                spdlog::debug("command execution requested: {}", args);
                args.push_back(nullptr);
                // create environment pointer
                sys::env::Guard guard(environment);
                return context_.spawn(executor_.c_str(), args.data(), guard.data());
            })
            .and_then<int>([this](auto pid) {
                return context_.wait_pid(pid);
            })
            .map_err<std::runtime_error>([](auto error) {
                spdlog::warn("command execution failed: {}", error.what());
                return error;
            });
    }

    void LibraryPreloadSession::set_server_address(const std::string& value)
    {
        server_address_ = value;
    }

    const ic::Session::HostInfo& LibraryPreloadSession::get_host_info() const
    {
        return host_info_;
    }

    std::string LibraryPreloadSession::get_session_type() const
    {
        return std::string("library preload");
    }
}

namespace ic {

    rust::Result<Session::SharedPtr> Session::from(const flags::Arguments& args, const sys::Context& ctx)
    {
        auto host_info = create_host_info(ctx)
                             .unwrap_or_else([](auto error) {
                                 spdlog::info(error.what());
                                 return std::map<std::string, std::string>();
                             });

        auto library = args.as_string(ic::Application::LIBRARY);
        auto executor = args.as_string(ic::Application::EXECUTOR);

        return merge(library, executor)
            .map<Session::SharedPtr>([&host_info, &ctx](auto pair) {
                const auto& [library, executor] = pair;
                auto result = new LibraryPreloadSession(std::move(host_info), library, executor, ctx);
                return std::shared_ptr<Session>(result);
            });
    }
}
