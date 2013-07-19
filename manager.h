// @file manager.h

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#pragma once

#include "mongo/pch.h"

#include "mongo/db/jsobj.h"
#include "mongo/util/concurrency/mutex.h"

namespace mongo {

    namespace backup {

        class Manager : boost::noncopyable {
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

            static SimpleMutex _currentMutex;
            static Manager *_currentManager;

          public:
            ~Manager();

            int poll(float progress, const char *progress_string);

            void error(int error_number, const char *error_string);

            bool start(const string &dest, string &errmsg, BSONObjBuilder &result);

            static bool throttle(long long bps, string &errmsg, BSONObjBuilder &result);

            static bool status(string &errmsg, BSONObjBuilder &result);
        };

    } // namespace backup

} // namespace mongo

