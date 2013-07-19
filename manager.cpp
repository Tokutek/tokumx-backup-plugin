// @file manager.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include "manager.h"

#include <iomanip>
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
            double percentDone = progress * 100.0;
            stringstream ss;
            ss << std::setw(6) << std::fixed << std::setprecision(2) << percentDone << "%";
            LOG(1) << "Backup progress " << ss.str() << endl;

            if (strncmp(progress_string, "Preparing backup", sizeof("Preparing backup")) == 0) {
                return 0;
            }
            LOG(1) << progress_string << endl;

            size_t bytesDone;
            int filesDone;
            int consumed;
            const char *p = progress_string;
            int r = sscanf(p, "Backup progress %zu bytes, %d files. %n", &bytesDone, &filesDone, &consumed);
            if (r != 2) {
                DEV LOG(0) << "1 Unexpected backup poll message: " << progress_string << endl;
                return 0;
            }
            p += consumed;

            StringData progressString(p);
            if (progressString.find("more files known of") != string::npos) {
                // Example:
                // Backup progress 475607 bytes, 13 files.  4 more files known of. Copying file /__tokumx_loc
                int filesRemaining;
                r = sscanf(p, "%d more files known of. Copying file %n", &filesRemaining, &consumed);
                if (r != 1) {
                    DEV LOG(0) << "2 Unexpected backup poll message: " << progress_string << endl;
                    return 0;
                }
                p += consumed;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                StringData currentFile(p);
                if (currentFile == ".") {
                    // Just noting that we're copying the directory, don't need to save this progress.
                    return 0;
                }

                _progress.filesTotal = filesDone + filesRemaining;
                _progress.currentSource = currentFile.toString();
                _progress.currentDest = "";
                _progress.currentDone = 0;
                _progress.currentTotal = 0;
            }
            else {
                // Example:
                // Backup progress 442839 bytes, 10 files.  Copying file: 0/32768 bytes done of /data/db/tokumx.rollback to /data/backup/tokumx.rollback.
                size_t currentDone;
                size_t currentTotal;
                r = sscanf(p, "Copying file: %zu/%zu bytes done of %n", &currentDone, &currentTotal, &consumed);
                if (r != 2) {
                    DEV LOG(0) << "3 Unexpected backup poll message: " << progress_string << endl;
                    return 0;
                }
                p += consumed;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                StringData rest(p);
                size_t toPos = rest.find(" to ");
                StringData currentSource = rest.substr(0, toPos);
                StringData currentDest = rest.substr(toPos + 4, rest.size() - 5 - toPos);

                _progress.currentDone = currentDone;
                _progress.currentTotal = currentTotal;
                _progress.currentSource = currentSource.toString();
                _progress.currentDest = currentDest.toString();
            }
            _progress.progress = progress;
            _progress.bytesDone = bytesDone;
            _progress.filesDone = filesDone - 1;  // number reported is the current file number, it's not done yet.
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

        void Manager::Progress::get(BSONObjBuilder &b) const {
            b.append("percent", progress * 100.0);
            b.append("bytesDone", bytesDone);
            {
                BSONObjBuilder fb(b.subobjStart("files"));
                fb.append("done", filesDone);
                fb.append("total", filesTotal);
                fb.doneFast();
            }
            if (!currentSource.empty()) {
                BSONObjBuilder cb(b.subobjStart("current"));
                cb.append("source", currentSource);
                if (!currentDest.empty()) {
                    cb.append("dest", currentDest);
                    BSONObjBuilder bb(cb.subobjStart("bytes"));
                    bb.append("done", currentDone);
                    bb.append("total", currentTotal);
                    bb.doneFast();
                }
                cb.doneFast();
            }
        }

        void Manager::status(BSONObjBuilder &result) const {
            _progress.get(result);
        }

    } // namespace backup

} // namespace mongo
