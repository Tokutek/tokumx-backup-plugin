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
            struct Progress {
                float progress;
                long long bytesDone;
                int filesDone;
                int filesTotal;
                long long currentDone;
                long long currentTotal;
                string currentSource;
                string currentDest;
                Progress() :
                        progress(0.0),
                        bytesDone(0),
                        filesDone(0),
                        filesTotal(0),
                        currentDone(0),
                        currentTotal(0),
                        currentSource(),
                        currentDest()
                {}
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

