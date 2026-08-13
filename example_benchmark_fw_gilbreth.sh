#!/usr/bin/env bash

module load cuda/12.6.0
module load cudnn/9.2.0.82-12

# export LD_LIBRARY_PATH="/depot/cms/users/colberte/SONIC/nereid/onnx_lib/onnxruntime/build/Linux/Release:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="/depot/cms/users/colberte/SONIC/nereid/torch_lib/libtorch/lib:$LD_LIBRARY_PATH"

# ./nereid-server
./framework/build/nereid-bench --launch-server

