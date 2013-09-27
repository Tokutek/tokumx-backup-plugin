// @file backup_plugin.cpp

/**
*    Copyright (C) 2013 Tokutek Inc.
*/

#include "mongo/pch.h"

#include <string>

#include <backup.h>

#include "manager.h"

#include "mongo/base/status.h"
#include "mongo/base/units.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/client.h"
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
            virtual void addRequiredPrivileges(const std::string& dbname,
                                               const BSONObj& cmdObj,
                                               std::vector<Privilege>* out) {
                ActionSet actions;
                actions.addAction(ActionType::backupStart);
                out->push_back(Privilege(AuthorizationManager::SERVER_RESOURCE_NAME, actions));
            }
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
                Manager manager(cc());
                return manager.start(dest, errmsg, result);
            }
        };

        class BackupThrottleCommand : public BackupCommand {
          public:
            BackupThrottleCommand() : BackupCommand("backupThrottle") {}
            virtual void addRequiredPrivileges(const std::string& dbname,
                                               const BSONObj& cmdObj,
                                               std::vector<Privilege>* out) {
                ActionSet actions;
                actions.addAction(ActionType::backupThrottle);
                out->push_back(Privilege(AuthorizationManager::SERVER_RESOURCE_NAME, actions));
            }
            virtual void help(stringstream &h) const {
                h << "Throttles hot backup to consume only N bytes/sec of I/O." << endl
                  << "{ backupThrottle: <N> }" << endl
                  << "N can be an integer or a string with a \"k/m/g\" suffix";
            }
            virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
                BSONElement e = cmdObj.firstElement();
                long long bps;
                if (e.type() == String) {
                    Status status = BytesQuantity<long long>::fromString(e.Stringdata(), bps);
                    if (!status.isOK()) {
                        stringstream ss;
                        ss << "error parsing number " << e.Stringdata() << ": " << status.codeString() << " " << status.reason();
                        errmsg = ss.str();
                        return false;
                    }
                }
                else {
                    if (!e.isNumber()) {
                        errmsg = "backupThrottle argument must be a number";
                        return false;
                    }
                    bps = e.safeNumberLong();
                }
                return Manager::throttle(bps, errmsg, result);
            }
        };

        class BackupStatusCommand : public BackupCommand {
          public:
            BackupStatusCommand() : BackupCommand("backupStatus") {}
            virtual void addRequiredPrivileges(const std::string& dbname,
                                               const BSONObj& cmdObj,
                                               std::vector<Privilege>* out) {
                ActionSet actions;
                actions.addAction(ActionType::backupStatus);
                out->push_back(Privilege(AuthorizationManager::SERVER_RESOURCE_NAME, actions));
            }
            virtual void help(stringstream &h) const {
                h << "Report the current status of hot backup." << endl
                  << "{ backupStatus: <N> }";
            }
            virtual bool run(const string &db, BSONObj &cmdObj, int options, string &errmsg, BSONObjBuilder &result, bool fromRepl) {
                return Manager::status(errmsg, result);
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
                cmds.push_back(boost::make_shared<BackupStatusCommand>());
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
    mongo::plugins::PluginInterface *TokuMX_Plugin__getInterface(void) {
        return &mongo::backup::backupInterface;
    }

}
