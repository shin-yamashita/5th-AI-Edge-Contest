#!/bin/sh

## https://www.tensorflow.org/lite/guide/build_cmake
#$ ln -s ../../tflite_build/tools/

#GRAPH=tflite_graphs/detect_f.tflite
#GRAPH=/media/shin/Linux-Ext/TFseg/models/research/deeplab/tflite_graphs/seg_graph_f.tflite
#GRAPH=/media/shin/Linux-Ext/TFseg/models/research/deeplab/tflite_graphs/seg_graph_q.tflite
#INPUT=ImageTensor
#INPUTSHAPE=1,513,513,3

#GRAPH=tflite_model_mb2/model.tflite
#INPUT=serving_default_input:0
#INPUTSHAPE=1,300,300,3

#GRAPH=tflite_model_mb2/model.tflite
GRAPH=tflite_model_mb2/model_q.tflite
INPUT=serving_default_input:0
INPUTSHAPE=1,300,300,3

/home/shin/app5th/TF/tflite_build/tools/benchmark/benchmark_model \
  --graph=$GRAPH \
  --benchmark_name=test\
  --output_prefix=ttt\
  --enable_op_profiling=true\
  --verbose=true \
  --use_xnnpack=false \
  --input_layer=$INPUT \
  --input_layer_shape=$INPUTSHAPE \
