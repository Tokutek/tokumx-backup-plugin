/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
// @file manager.cpp
/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include "manager.h"

#include <iomanip>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/filesystem.hpp>

#include <backup.h>

#include "mongo/db/client.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/kill_current_op.h"
#include "mongo/util/concurrency/mutex.h"
#include "mongo/util/debug_util.h"
#include "mongo/util/log.h"

namespace mongo {

    // ugh
    extern string dbpath;

    namespace backup {

        SimpleMutex Manager::_currentMutex("backup manager");
        Manager *Manager::_currentManager = NULL;

        static int c_poll_fun(float progress, const char *progress_string, void *poll_extra) {
            Manager *t = static_cast<Manager *>(poll_extra);
            return t->poll(progress, progress_string);
        }
        static void c_error_fun(int error_number, const char *error_string, void *error_extra) {
            Manager *t = static_cast<Manager *>(error_extra);
            t->error(error_number, error_string);
        }

        Manager::~Manager() {
            SimpleMutex::scoped_lock lk(_currentMutex);
            if (_currentManager == this) {
                _currentManager = NULL;
            }
        }

        int Manager::poll(float progress, const char *progress_string) {
            _killedString = killCurrentOp.checkForInterruptNoAssert(_c);
            if (!_killedString.empty()) {
                return -1;
            }

            if (strncmp(progress_string, "Preparing backup", sizeof("Preparing backup")) == 0) {
                // We won the race (if any), we're the current backup.
                SimpleMutex::scoped_lock lk(_currentMutex);
                if (_currentManager != NULL) {
                    // There's a small possible race condition here.  It's possible that the last
                    // backup has ended and released its internal lock, but has not yet reached the
                    // manager's destructor so it is still the value of _currentManager when we
                    // reach this point.  If that's the case, we don't want to crash so we won't
                    // assert here.
                    //verify(_currentManager == NULL);

                    LOG(1) << "A different manager already exists, and we are being polled.  This "
                           << "should only happen if backups are being done in quick succession." << endl;
                }
                _currentManager = this;
                return 0;
            }

            double percentDone = progress * 100.0;
            stringstream ss;
            ss << std::setw(6) << std::fixed << std::setprecision(2) << percentDone << "%";
            LOG(1) << "Backup progress " << ss.str() << endl;
            LOG(1) << progress_string << endl;

            _progress.parse(progress, progress_string);
            return 0;
        }

        void Manager::Progress::parse(float progress, const char *progress_string) {
            size_t bytesDone;
            int filesDone;
            int consumed;
            const char *p = progress_string;
            int r = sscanf(p, "Backup progress %zu bytes, %d files. %n", &bytesDone, &filesDone, &consumed);
            if (r != 2) {
                DEV LOG(0) << "Unexpected backup poll message: " << progress_string << endl;
                return;
            }
            p += consumed;

            StringData progressString(p);
            if (progressString.find("more files known of") != string::npos) {
                // Example:
                // Backup progress 475607 bytes, 13 files.  4 more files known of. Copying file /__tokumx_loc
                int filesRemaining;
                r = sscanf(p, "%d more files known of. Copying file %n", &filesRemaining, &consumed);
                if (r != 1) {
                    DEV LOG(0) << "Unexpected backup poll message: " << progress_string << endl;
                    return;
                }
                p += consumed;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                StringData currentFile(p);
                if (currentFile == ".") {
                    // Just noting that we're copying the directory, don't need to save this progress.
                    return;
                }

                {
                    SimpleMutex::scoped_lock lk(_mutex);
                    _progress = progress;
                    _bytesDone = bytesDone;
                    _filesDone = filesDone - 1;  // number reported is the current file number, it's not done yet.
                    _filesTotal = filesDone + filesRemaining;
                    _currentSource = currentFile.toString();
                    _currentDest = "";
                    _currentDone = 0;
                    _currentTotal = 0;
                }
            }
            else if (progressString.find("Throttled: copied") != string::npos) {
                // Example:
                // Backup progress %ld bytes, %ld files.  Throttled: copied %ld/%ld bytes of %s to %s. Sleeping %.2fs for throttling.
                size_t currentDone;
                size_t currentTotal;
                r = sscanf(p, "Throttled: copied %zu/%zu bytes of %n", &currentDone, &currentTotal, &consumed);
                if (r != 2) {
                    DEV LOG(0) << "Unexpected backup poll message: " << progress_string << endl;
                    return;
                }
                p += consumed;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                StringData rest(p);
                size_t toPos = rest.find(" to ");
                StringData currentSource = rest.substr(0, toPos);
                StringData currentDest = rest.substr(toPos + 4, rest.size() - 5 - toPos);

                size_t sleeping = rest.find(". Sleeping ");
                p += sleeping + 11;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                float sleepTime;
                r = sscanf(p, "%fs for throttling.", &sleepTime);
                if (r != 1) {
                    DEV LOG(0) << "Unexpected backup poll message: " << progress_string << endl;
                    return;
                }

                // TODO: maybe report this somewhere?
                (void) sleepTime;

                {
                    SimpleMutex::scoped_lock lk(_mutex);
                    _progress = progress;
                    _bytesDone = bytesDone;
                    _filesDone = filesDone - 1;  // number reported is the current file number, it's not done yet.
                    _currentDone = currentDone;
                    _currentTotal = currentTotal;
                    _currentSource = currentSource.toString();
                    _currentDest = currentDest.toString();
                }
            }
            else {
                // Example:
                // Backup progress 442839 bytes, 10 files.  Copying file: 0/32768 bytes done of /data/db/tokumx.rollback to /data/backup/tokumx.rollback.
                size_t currentDone;
                size_t currentTotal;
                r = sscanf(p, "Copying file: %zu/%zu bytes done of %n", &currentDone, &currentTotal, &consumed);
                if (r != 2) {
                    DEV LOG(0) << "Unexpected backup poll message: " << progress_string << endl;
                    return;
                }
                p += consumed;
                while (*p == ' ' || *p == '\t') {
                    ++p;
                }

                StringData rest(p);
                size_t toPos = rest.find(" to ");
                StringData currentSource = rest.substr(0, toPos);
                StringData currentDest = rest.substr(toPos + 4, rest.size() - 5 - toPos);

                {
                    SimpleMutex::scoped_lock lk(_mutex);
                    _progress = progress;
                    _bytesDone = bytesDone;
                    _filesDone = filesDone - 1;  // number reported is the current file number, it's not done yet.
                    _currentDone = currentDone;
                    _currentTotal = currentTotal;
                    _currentSource = currentSource.toString();
                    _currentDest = currentDest.toString();
                }
            }
        }

        void Manager::Progress::get(BSONObjBuilder &b) const {
            SimpleMutex::scoped_lock lk(_mutex);
            b.append("percent", _progress * 100.0);
            b.append("bytesDone", _bytesDone);
            {
                BSONObjBuilder fb(b.subobjStart("files"));
                fb.append("done", _filesDone);
                fb.append("total", _filesTotal);
                fb.doneFast();
            }
            if (!_currentSource.empty()) {
                BSONObjBuilder cb(b.subobjStart("current"));
                cb.append("source", _currentSource);
                if (!_currentDest.empty()) {
                    cb.append("dest", _currentDest);
                    BSONObjBuilder bb(cb.subobjStart("bytes"));
                    bb.append("done", _currentDone);
                    bb.append("total", _currentTotal);
                    bb.doneFast();
                }
                cb.doneFast();
            }
        }

        void Manager::error(int error_number, const char *error_string) {
            LOG(0) << "backup error " << error_number << ": " << error_string << endl;
            _error.parse(error_number, error_string);
        }

        void Manager::Error::parse(int error_number, const char *error_string) {
            eno = error_number;
            errstring = error_string;
#if 0  // I don't want to try to parse these right now
            StringData errorString(error_string);
            if (errorString == "User aborted backup") {
                cause = error_string;
            }
            else if (errorString.startsWith("This backup")) {
                cause = error_string;
            }
            else if (errorString.find(", errno=") != string::npos) {
                size_t comma = errorString.find(", errno=");
                cause = errorString.substr(0, comma).toString();
            }
            else if (errorString.startsWith("clock_gettime returned an error")) {
                cause = "clock_gettime returned an error";
            }
            else if (errorString.startsWith("mutex_trylock")) {
                cause = "mutex_trylock";
            }
            else {
                cause = "";
            }
#endif
        }

        void Manager::Error::get(BSONObjBuilder &b) const {
            b.append("message", errstring);
            b.append("errno", eno);
            b.append("strerror", strerror(eno));
        }

        // This method determines whether there is a separate log dir.
        // If there is, we want to back that up separately from the
        // data dir.  If there isn't we just want to backup the data
        // dir.  NOTE: If the log dir is a subdirectory (i.e. child)
        // of the data dir, then we do NOT want to back it up
        // separately.
        bool Manager::_multipleDirsNeeded() {
            if (cmdLine.logDir == "" || cmdLine.logDir == dbpath) {
                return false;
            }

            const int subpath = !(cmdLine.logDir.compare(0, dbpath.size(), dbpath));
            if (subpath) {
                return false;
            }

            return true;
        }

        bool Manager::start(const string &dest, string &errmsg, BSONObjBuilder &result) {
            boost::filesystem::path data_dest = dest;
            boost::filesystem::path log_dest = dest;
            const char *source_dirs[2];
            const char *dest_dirs[2];
            int dir_count = 1;
            source_dirs[0] = dbpath.c_str();

	    // If the user has set a separate log directory, we should
	    // back that up as well.
	    if (_multipleDirsNeeded()) {
                source_dirs[1] = cmdLine.logDir.c_str();

                // Create two directories underneath the given
                // destination directory, one for the data directory,
                // the other for the log directory.
                data_dest = data_dest / "data";
                log_dest = log_dest / "log";
                try {
                    boost::filesystem::create_directory(data_dest);
                    boost::filesystem::create_directory(log_dest);
                } catch (const boost::filesystem::filesystem_error &e) {
                    DEV LOG(0) << "ERROR: Hot Backup could not create backup subdirectories:"
                               << e.what()
                               << endl;
                    return false;
                }

                // We have to set BOTH destination directories to the
                // newly created directories.
                dest_dirs[0] = data_dest.generic_string().c_str();
                dest_dirs[1] = log_dest.generic_string().c_str();
                dir_count = 2;
	    } else {
                dest_dirs[0] = dest.c_str();
            }
            
            
            DEV LOG(0) << "Starting backup on " << dest << endl;
            int r = tokubackup_create_backup(source_dirs, dest_dirs, dir_count,
                                             c_poll_fun, this,
                                             c_error_fun, this);
            bool ok = r == 0;
            if (ok && !_error.empty()) {
                LOG(0) << "backup succeeded but reported an error" << endl;
            }
            else if (!ok && _error.empty()) {
                LOG(0) << "backup failed but didn't report an error" << endl;
            }

            if (!ok) {
                _error.get(result);
            }

            if (!_killedString.empty()) {
                result.append("reason", _killedString);
            }

            return ok;
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

        bool Manager::status(string &errmsg, BSONObjBuilder &result) {
            SimpleMutex::scoped_lock lk(_currentMutex);
            if (_currentManager == NULL) {
                errmsg = "no backup running";
                return false;
            }
            _currentManager->_progress.get(result);
            return true;
        }

    } // namespace backup

} // namespace mongo
