// @file manager.h

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#pragma once

#include "mongo/pch.h"

#include "mongo/db/jsobj.h"

namespace mongo {

    namespace backup {

        class Manager : boost::noncopyable {
            struct Progress {
                float progress;
                long long bytesDone;
                int filesDone;
                int filesRemaining;
                long long currentDone;
                long long currentRemaining;
                string currentSource;
                string currentDest;
                Progress() :
                        progress(0.0),
                        bytesDone(0),
                        filesDone(0),
                        filesRemaining(0),
                        currentDone(0),
                        currentRemaining(0),
                        currentSource(),
                        currentDest()
                {}
                void get(BSONObjBuilder &b) const;
            } _progress;

          public:
            int poll(float progress, const char *progress_string);

            void error(int error_number, const char *error_string);

            bool start(const string &dest, string &errmsg, BSONObjBuilder &result);

            bool throttle(long long bps, string &errmsg, BSONObjBuilder &result);

            void status(BSONObjBuilder &b) const;
        };

        extern Manager manager;

    } // namespace backup

} // namespace mongo

