// make_backup.js -
// Makes a backup of a mongo database

db.adminCommand({loadPlugin: 'backup_plugin'});
db.adminCommand({ backupStart : "rs2"} );
