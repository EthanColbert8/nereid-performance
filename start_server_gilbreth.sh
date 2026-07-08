#!/usr/bin/env bash

module load cuda/12.6.0
export LD_LIBRARY_PATH="/depot/cms/users/colberte/SONIC/nereid/torch_lib/libtorch/lib:$LD_LIBRARY_PATH"

# ./nereid-server
./nereid-server-bigmsg
