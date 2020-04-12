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

#include "Reporter.h"
#include "SystemCalls.h"
#include "libresult/Result.h"

#include <chrono>
#include <iostream>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "supervise.grpc.pb.h"

namespace {

    std::string to_json_string(const std::string& value)
    {
        std::string result;

        char const* wsrc_it = value.c_str();
        char const* const wsrc_end = wsrc_it + value.length();

        for (; wsrc_it != wsrc_end; ++wsrc_it) {
            // Insert an escape character before control characters.
            switch (*wsrc_it) {
            case L'\b':
                result += "\\b";
                break;
            case L'\f':
                result += "\\f";
                break;
            case L'\n':
                result += "\\n";
                break;
            case L'\r':
                result += "\\r";
                break;
            case L'\t':
                result += "\\t";
                break;
            case L'"':
                result += "\\\"";
                break;
            case L'\\':
                result += "\\\\";
                break;
            default:
                result += char(*wsrc_it);
                break;
            }
        }
        return result;
    }

    void json_string(std::ostream& os, const char* value)
    {
        os << '"' << to_json_string(value) << '"';
    }

    void json_attribute(std::ostream& os, const char* key, const char* value)
    {
        os << '"' << key << '"' << ':';
        json_string(os, value);
    }

    void json_attribute(std::ostream& os, const char* key, const char** value)
    {
        os << '"' << key << '"' << ':';
        os << '[';
        for (const char** it = value; *it != nullptr; ++it) {
            if (it != value)
                os << ',';
            json_string(os, *it);
        }
        os << ']';
    }

    void json_attribute(std::ostream& os, const char* key, const int value)
    {
        os << '"' << key << '"' << ':' << value;
    }

    class TimedEvent : public er::Event {
    private:
        std::chrono::system_clock::time_point const when_;

    public:
        TimedEvent() noexcept
                : when_(std::chrono::system_clock::now())
        {
        }

        std::chrono::system_clock::time_point const& when() const noexcept
        {
            return when_;
        }
    };

    struct ProcessStartEvent : public TimedEvent {
        pid_t child_;
        pid_t supervisor_;
        pid_t parent_;
        std::string cwd_;
        const char** cmd_;

        ProcessStartEvent(pid_t child, pid_t supervisor, pid_t parent, std::string cwd, const char** cmd) noexcept
                : TimedEvent()
                , child_(child)
                , supervisor_(supervisor)
                , parent_(parent)
                , cwd_(std::move(cwd))
                , cmd_(cmd)
        {
        }

        const char* name() const override
        {
            return "process_start";
        }

        void to_json(std::ostream& os) const override
        {
            os << '{';
            json_attribute(os, "pid", child_);
            os << ',';
            json_attribute(os, "ppid", supervisor_);
            os << ',';
            json_attribute(os, "pppid", parent_);
            os << ',';
            json_attribute(os, "cwd", cwd_.c_str());
            os << ',';
            json_attribute(os, "cmd", cmd_);
            os << '}';
        }
    };

    struct ProcessStopEvent : public TimedEvent {
        pid_t child_;
        pid_t supervisor_;
        int exit_;

        ProcessStopEvent(pid_t child,
            pid_t supervisor,
            int exit) noexcept
                : TimedEvent()
                , child_(child)
                , supervisor_(supervisor)
                , exit_(exit)
        {
        }

        const char* name() const override
        {
            return "process_stop";
        }

        void to_json(std::ostream& os) const override
        {
            os << '{';
            json_attribute(os, "pid", child_);
            os << ',';
            json_attribute(os, "ppid", supervisor_);
            os << ',';
            json_attribute(os, "exit", exit_);
            os << '}';
        }
    };

    class ReporterImpl : public er::Reporter {
    public:
        ReporterImpl(const char* target, const sys::Context& context) noexcept;

        rust::Result<int> send(er::Event::SharedPtr event) noexcept override;

        rust::Result<er::Event::SharedPtr> start(pid_t pid, const char** cmd) override;
        rust::Result<er::Event::SharedPtr> stop(pid_t pid, int exit) override;

    private:
        rust::Result<std::shared_ptr<std::ostream>> create_stream(const std::string&) const;

        std::string const target_;
        const sys::Context& context_;
    };

    ReporterImpl::ReporterImpl(const char* target, const sys::Context& context) noexcept
            : er::Reporter()
            , target_(target)
            , context_(context)
    {
    }

    rust::Result<int> ReporterImpl::send(er::Event::SharedPtr event) noexcept
    {
        return create_stream(event->name())
            .map<int>([&event](auto stream) {
                event->to_json(*stream);
                return 0;
            });
    }

    rust::Result<er::Event::SharedPtr> ReporterImpl::start(pid_t pid, const char** cmd)
    {
        return context_.get_cwd()
            .map<er::Event::SharedPtr>([&pid, &cmd, this](auto cwd) {
                const auto current = context_.get_pid();
                const auto parent = context_.get_ppid();

                return er::Event::SharedPtr(new ProcessStartEvent(pid, current, parent, cwd, cmd));
            });
    }

    rust::Result<er::Event::SharedPtr> ReporterImpl::stop(pid_t pid, int exit)
    {
        const auto current = context_.get_pid();
        return rust::Ok(er::Event::SharedPtr(new ProcessStopEvent(pid, current, exit)));
    }

    rust::Result<std::shared_ptr<std::ostream>> ReporterImpl::create_stream(const std::string& prefix) const
    {
        return er::SystemCalls::temp_file(target_.c_str(), ("." + prefix + ".json").c_str());
    }
}

namespace er {

    rust::Result<Reporter::SharedPtr> Reporter::from(char const* path, const sys::Context& context) noexcept
    {
        SharedPtr result = std::make_unique<ReporterImpl>(path, context);
        return rust::Ok(std::move(result));
    }
}
