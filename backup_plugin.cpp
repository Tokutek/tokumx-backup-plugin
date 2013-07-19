// @file backup_plugin.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include <string>

#include <backup.h>

#include "manager.h"

#include "mongo/db/commands.h"
#include "mongo/db/jsobj.h"
#include "mongo/plugins/command_loader.h"

namespace mongo {

    namespace backup {

        class BackupCommand : public Command {
          public:
            BackupCommand(const char *name) : Command(name) {}
            virtual LockType locktype() const { return NONE; }
            virtual bool requiresSync() const { return false; }
            virtual bool lockGlobally() const { return false; }
            virtual bool needsTxn() const { return false; }
            virtual bool canRunInMultiStmtTxn() const { return true; }
            virtual int txnFlags() const { return noTxnFlags(); }
            virtual bool adminOnly() const { return true; }
            virtual bool slaveOk() const { return true; }
        };

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
