// @file manager.h

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#pragma once

#include "mongo/pch.h"

namespace mongo {

    namespace backup {

        class Manager : boost::noncopyable {
          public:
            int poll(float progress, const char *progress_string);

            void error(int error_number, const char *error_string);

            bool start(const string &dest, string &errmsg, BSONObjBuilder &result);

            bool throttle(long long bps, string &errmsg, BSONObjBuilder &result);
        };

        extern Manager manager;

    } // namespace backup

} // namespace mongo

