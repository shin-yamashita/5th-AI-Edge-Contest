
# TFlite delegate interface C++ sources

TFlite の [custom_delegate](https://www.tensorflow.org/lite/performance/implementing_delegate) に従って実装した C++ ソース。  
1. Linux PC (Ubuntu20.04) 上で Conv2d/dwConv2d の Reference model を作り、delegate の動作を確認。  
また、論理検証用の test vector file を生成。  
2. ultra96v2(PYNQ Ubuntu20.04) 上で FPGA に delegate する。  

Linux PC と ultra96v2 の共通ソース。  

## files
```
tflite_delegate/
├── BUILD            bazel
├── Conv2D.cc        Reference model
├── Conv2D.h
├── MyDelegate.cc    delegate interface source   
├── dummy_delegate.cc
├── dummy_delegate.h
├── external_delegate_adaptor.cc
├── tfacc_u8.cc      FPGA interface
└── tfacc_u8.h
```
## delegate interface source

Linux 用（Reference model） と、 ultra96v2-pynq 用(FPGA とのインターフェース) でソースは共通。   

test vector file を出力するには、MyDelegate.cc の以下の２つの変数を設定する。  
static int dumpfrom = 0;  
static int dumpto = 65;    
推論アプリを実行し、Reference model (C++) に delegate すると、"tdump-%d-i8.in" というファイル名で Conv パラメータ + input/filter/bias/output の Tensor データを出力する。論理シミュレーション環境で一致検証用に用いる。


## Build TensorFlow Lite Python Wheel Package

1. **環境設定**  

   https://www.tensorflow.org/lite/guide/build_cmake_pip  を参照  
   python3 version 3.8 で検証  
   ```$ pip3 install wheel```  

   **tensorflow r2.7 ソースをダウンロード、 ../tensorflow_src に配置する。**  
   ```$ git clone -b r2.7 https://github.com/tensorflow/tensorflow.git  ../tensorflow_src```

2. **build / install**  
   ```
   $ pip install pybind11  
   $ cd ../tensorflow_src  
   ($ export BUILD_NUM_JOBS=1  Ultra96-V2 では cpu リソース不足のため job を制限)  
   $ tensorflow/lite/tools/pip_package/build_pip_package_with_cmake.sh native  
   ==> tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/dist/tflite_runtime-2.8.0-cp38-cp38-linux_x86_64.whl  

   $ pip install tensorflow/lite/tools/pip_package/gen/tflite_pip/python3/dist/tflite_runtime-2.8.0-cp38-cp38-linux_x86_64.whl  
   ```

## **build delegate interface library**

tensorflow r2.7 のソースツリーに tflite_delegate/* を配置する。  
../tensorflow_src/tflite_delegate/*  

build には bazel を用いており、Linux PC と Ultra96-V2 で build 手順は同じ。  

1. **bazel の導入**  
   https://docs.bazel.build/versions/4.2.2/install-ubuntu.html を参照  
   - Linux PC :  "Using Bazel's apt repository" を参照  
      apt によりインストール    
   - Ultra96-v2 : "Using the binary installer" を参照  
      Download https://github.com/bazelbuild/bazel/releases/download/4.2.2/bazel-4.2.2-linux-arm64  
      $ cp bazel-4.2.2-linux-arm64  ~/bin/bazel  

2. **delegate interface の build**  
   tensorflow_src の下で以下のコマンドを実行する  
   ``` bash
   $ cd ../tensorflow_src  
   $ bazel build -c opt tflite_delegate/dummy_external_delegate.so  --define `uname -m`=1
   ```
   --define \`uname -m\`=1 によって LinuxPC(x86_64) と Ultra96-V2(aarch64) を切り替えでいる。C++ ソースでは、 `#ifdef ULTRA96` で切り替える。    

   Ultra96-V2 では cpu リソース不足のため、--local_ram_resources=HOST_RAM*.5 などのオプションをつけたほうが良い。  

   bazel で build すると、自動的に bazel-bin などの symbolic link ができており、  

   bazel-bin/tflite_delegate/dummy_external_delegate.so が python の delegate interface になる  

   生成物: **dummy_external_delegate.so**   
   tflite python API から呼び出す。 tflite_runtime.Interpreter に link して用いる。  

## python API

Google は、FlatBuffer に変換した推論グラフを高速に実行する軽量の tflite_runtime[tflite_runtime](https://www.tensorflow.org/lite/guide/python) を提供している。ultra96v2 にも pip で導入できる。  
新たに作成した delegate インターフェースは、ダイナミックリンクライブラリ(dummy_external_delegate\.so)として tflite_runtime とリンクすることで実行の delegate が行われる。  
tflite_runtime の python インターフェースでは interpreter のインスタンス時に dummy_external_delegate\.so を指定してリンクするだけである。

[^6]:https://www.tensorflow.org/lite/guide/python

```python
import tflite_runtime.interpreter as tflite
# Instantiate interpreter
interpreter = tflite.Interpreter(model_path=model, 
      experimental_delegates=[tflite.load_delegate(
        　　　　　　　'{path-to}/dummy_external_delegate.so.1')] )      
interpreter.allocate_tensors()
# Get input and output tensors.
input_details  = interpreter.get_input_details()
output_details = interpreter.get_output_details()
# set image
interpreter.set_tensor(input_details[0]['index'], np.uint8(image)) 
# Invoke
interpreter.invoke()
# Output inference result
score = interpreter.get_tensor(output_details[0]['index'])[0]
boxes = interpreter.get_tensor(output_details[1]['index'])[0]
num   = interpreter.get_tensor(output_details[2]['index'])[0]
classes = interpreter.get_tensor(output_details[3]['index'])[0]
```






