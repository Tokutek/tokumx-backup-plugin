// @file backup_plugin.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include <string>

#include <backup.h>

#include "mongo/db/commands.h"
#include "mongo/db/jsobj.h"
#include "mongo/plugins/command_loader.h"
#include "mongo/util/debug_util.h"
#include "mongo/util/log.h"

namespace mongo {

    // ugh
    extern string dbpath;

    namespace backup {

        class BackupCommand : public Command {
          public:
            BackupCommand(const char *name) : Command(name) {}
            virtual LockType locktype() const { return WRITE; }
            virtual bool requiresSync() const { return false; }
            virtual bool lockGlobally() const { return true; }
            virtual bool needsTxn() const { return false; }
            virtual bool canRunInMultiStmtTxn() const { return true; }
            virtual int txnFlags() const { return noTxnFlags(); }
            virtual bool adminOnly() const { return true; }
            virtual bool slaveOk() const { return true; }
        };

        class BackupManager : boost::noncopyable {
          public:
            static int c_poll_fun(float progress, const char *progress_string, void *poll_extra) {
                BackupManager *t = static_cast<BackupManager *>(poll_extra);
                return t->poll(progress, progress_string);
            }
            static void c_error_fun(int error_number, const char *error_string, void *error_extra) {
                BackupManager *t = static_cast<BackupManager *>(error_extra);
                t->error(error_number, error_string);
            }

            int poll(float progress, const char *progress_string) {
                int pct = progress * 100.0;
                LOG(0) << "backup is at " << pct << "%: " << progress_string << endl;
                return 0;
            }

            void error(int error_number, const char *error_string) {
                LOG(0) << "backup error " << error_number << ": " << error_string << endl;
            }

            bool start(const string &dest, string &errmsg, BSONObjBuilder &result) {
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
            bool throttle(long long bps, string &errmsg, BSONObjBuilder &result) {
                if (bps < 0) {
                    errmsg = "backupThrottle argument cannot be negative";
                    return false;
                }
                DEV LOG(0) << "Throttling backup to " << bps << endl;
                tokubackup_throttle_backup(bps);
                return true;
            }
        } manager;

        class BackupStartCommand : public BackupCommand {
          public:
            BackupStartCommand() : BackupCommand("backupStart") {}
            virtual void help(stringstream &h) const {
                h << "Starts a hot backup." << endl
                  << "{ backupStart: <destination directory> }";
            }
            virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
                BSONElement e = cmdObj.firstElement();
                string dest = e.str();
                if (dest.empty()) {
                    errmsg = "invalid destination directory: '" + dest + "'";
                    return false;
                }
                return manager.start(dest, errmsg, result);
            }
        };

        class BackupThrottleCommand : public BackupCommand {
          public:
            BackupThrottleCommand() : BackupCommand("backupThrottle") {}
            virtual void help(stringstream &h) const {
                h << "Throttles hot backup to consume only N bytes/sec of I/O." << endl
                  << "{ backupThrottle: <N> }";
            }
            virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
                BSONElement e = cmdObj.firstElement();
                if (!e.isNumber()) {
                    errmsg = "backupThrottle argument must be a number";
                    return false;
                }
                long long bps = e.safeNumberLong();
                return manager.throttle(bps, errmsg, result);
            }
        };

        class BackupInterface : public plugins::CommandLoader {
          protected:
            bool preLoad(string &errmsg, BSONObjBuilder &result) {
                StringData backupVersion(tokubackup_version_string);
                if (backupVersion.find("disabled") != string::npos) {
                    errmsg = "cannot load backup_plugin: enterprise backup library support not found";
                    return false;
                }
                return true;
            }

            CommandVector commands() const {
                CommandVector cmds;
                cmds.push_back(boost::make_shared<BackupStartCommand>());
                cmds.push_back(boost::make_shared<BackupThrottleCommand>());
                return cmds;
            }

          public:
            const string &name() const {
                static const string n = "backup_plugin";
                return n;
            }

            const string &version() const {
                static const string v = string(tokubackup_version_string);
                return v;
            }
        } backupInterface;

    } // namespace backup

} // namespace mongo

extern "C" {

    __attribute__((visibility("default")))
    mongo::plugins::PluginInterface *getInterface(void) {
        return &mongo::backup::backupInterface;
    }

}
