#!/bin/bash
set -e

echo "Cleaning local files"
make clean
cd ..
echo "Removing files from remote host"
ssh students "rm -f -r ~/remote_compile/csrc/"
echo "Copying files to remote host"
scp -r csrc/ students:~/remote_compile/
echo "Compiling on remote host"
ssh students "cd ~/remote_compile/csrc/ && make"
echo "Copying compiled binary from host to local"
scp students:~/remote_compile/csrc/main.bin csrc/