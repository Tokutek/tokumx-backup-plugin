/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
// @file manager.h

/*======
This file is part of Percona Server for MongoDB.
Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.
    Percona Server for MongoDB is free software: you can redistribute
    it and/or modify it under the terms of the GNU Affero General
    Public License, version 3, as published by the Free Software
    Foundation.
    Percona Server for MongoDB is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU Affero General Public License for more details.
    You should have received a copy of the GNU Affero General Public
    License along with Percona Server for MongoDB.  If not, see
    <http://www.gnu.org/licenses/>.  
======= */

#pragma once

#include "mongo/pch.h"

#include <boost/filesystem.hpp>

#include "mongo/db/client.h"
#include "mongo/db/jsobj.h"
#include "mongo/util/concurrency/mutex.h"

namespace mongo {

    namespace backup {

        class Manager : boost::noncopyable {
            Client &_c;
            string _killedString;

            class Progress {
                mutable SimpleMutex _mutex;
                float _progress;
                long long _bytesDone;
                int _filesDone;
                int _filesTotal;
                long long _currentDone;
                long long _currentTotal;
                string _currentSource;
                string _currentDest;
              public:
                Progress() :
                        _mutex("backup progress"),
                        _progress(0.0),
                        _bytesDone(0),
                        _filesDone(0),
                        _filesTotal(0),
                        _currentDone(0),
                        _currentTotal(0),
                        _currentSource(),
                        _currentDest()
                {}
                void parse(float progress, const char *progress_string);
                void get(BSONObjBuilder &b) const;
            } _progress;

            struct Error {
                // errno, but avoid shadowing
                int eno;
                string errstring;
                Error() : eno(0), errstring() {}
                void parse(int error_number, const char *error_string);
                bool empty() const { return errstring.empty(); }
                void get(BSONObjBuilder &b) const;
            } _error;

            static SimpleMutex _currentMutex;
            static Manager *_currentManager;

            static std::vector<string> _getSourceDirs(const boost::filesystem::path &data_src,
                                                      const boost::filesystem::path &log_src);

          public:
            explicit Manager(Client &c) : _c(c), _killedString(), _progress(), _error() {}
            ~Manager();

            int poll(float progress, const char *progress_string);

            void error(int error_number, const char *error_string);

            bool start(const string &dest, string &errmsg, BSONObjBuilder &result);

            static bool throttle(long long bps, string &errmsg, BSONObjBuilder &result);

            static bool status(string &errmsg, BSONObjBuilder &result);
        };

    } // namespace backup

} // namespace mongo

