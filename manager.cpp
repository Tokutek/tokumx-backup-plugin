// @file manager.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include "manager.h"

#include <string>

#include <backup.h>

#include "mongo/db/jsobj.h"
#include "mongo/util/debug_util.h"
#include "mongo/util/log.h"

namespace mongo {

    // ugh
    extern string dbpath;

    namespace backup {

        Manager manager;

        static int c_poll_fun(float progress, const char *progress_string, void *poll_extra) {
            Manager *t = static_cast<Manager *>(poll_extra);
            return t->poll(progress, progress_string);
        }
        static void c_error_fun(int error_number, const char *error_string, void *error_extra) {
            Manager *t = static_cast<Manager *>(error_extra);
            t->error(error_number, error_string);
        }

        int Manager::poll(float progress, const char *progress_string) {
            int pct = progress * 100.0;
            LOG(0) << "backup is at " << pct << "%: " << progress_string << endl;
            return 0;
        }

        void Manager::error(int error_number, const char *error_string) {
            LOG(0) << "backup error " << error_number << ": " << error_string << endl;
        }

        bool Manager::start(const string &dest, string &errmsg, BSONObjBuilder &result) {
            const char *source_dirs[1];
            const char *dest_dirs[1];
            const int dir_count = 1;
            source_dirs[0] = dbpath.c_str();
            dest_dirs[0] = dest.c_str();
            DEV LOG(0) << "Starting backup on " << dest << endl;
            int r = tokubackup_create_backup(source_dirs, dest_dirs, dir_count,
                                             c_poll_fun, this,
                                             c_error_fun, this);
            return r == 0;
        }

        bool Manager::throttle(long long bps, string &errmsg, BSONObjBuilder &result) {
            if (bps < 0) {
                errmsg = "backupThrottle argument cannot be negative";
                return false;
            }
            DEV LOG(0) << "Throttling backup to " << bps << endl;
            tokubackup_throttle_backup(bps);
            return true;
        }

    } // namespace backup

} // namespace mongo
