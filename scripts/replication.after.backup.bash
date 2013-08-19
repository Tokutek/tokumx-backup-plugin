#!/bin/bash

# NOTE: Edit this to be the directory of the mongoDB binaries...
# Or make it an argument...
dir=./TEMP/tokumx-2013-08-06-linux-x86_64/bin/

# Copy our javascript files to the data dir.
cp add_data.js $dir/
cp make_backup.js $dir/
cp initate_replication.js $dir/

# Switch to the directory where the mongoDB binaries and our
# javascript files reside.
pushd $dir

# Clean up any directories, if they exist.
rm -rf rs1
rm -rf rs2
mkdir rs1
mkdir rs2

# Start single mongo server.
./mongod --port 11223 --dbpath ./rs1 &

# Add some data.
./mongo --port 11223 add_data.js

# Make a backup.
./mongo --port 11223 make_backup.js

# Shutdown server.
pkill -9 mongod

echo 'Sleeping..'
sleep 5
echo 'Resuming'

# Restart Server with replication turned on.
./mongod --port 11223 --dbpath ./rs1 --replSet myreplsetid --smallfiles --oplogSize 128 &

# Start up second server, using backup directory as data directory.
./mongod --port 11224 --dbpath ./rs2 --replSet myreplsetid --smallfiles --oplogSize 128 &

echo 'Sleeping..'
sleep 5
echo 'Resuming'

# Start Replication.
./mongo --port 11223 initate_replication.js

# Verify backup...
# TODO:

echo 'Sleeping..'
sleep 5
echo 'Resuming'

# Kill the mongo servers.
pkill -9 mongod

# Return to our original directory.
popd

echo 'End of Script...'
