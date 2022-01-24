#!/bin/bash
set -e

echo "Cleaning local files"
make clean
cd ..
echo "Removing files from remote host"
ssh students "rm -r ~/remote_compile/acc-mouse-control/"
echo "Copying files to remote host"
scp -r acc-mouse-control/ students:~/remote_compile/
echo "Compiling on remote host"
ssh students "cd ~/remote_compile/acc-mouse-control/ && make"
echo "Copying compiled binary from host to local"
scp students:~/remote_compile/acc-mouse-control/main.bin acc-mouse-control/