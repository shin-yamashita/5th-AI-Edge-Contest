# 5th AI Edge Contest

このリポジトリでは、[5th AI Edge Contest](https://signate.jp/competitions/537) に向けて実装したシステムのソースコードを公開する。 

## 課題

- 車両前方カメラの撮影動画から物体の写る矩形領域を検出し、追跡するアルゴリズムを作成する  
- RISC-Vをターゲットのプラットフォーム(Ultra96-V2)に実装し、RISC-Vコアを物体追跡の処理の中で使用する  

## TFlite delegate と rv32emc を用いた実装

- TFlite の delegate 機構を用いて FPGA にアクセラレータを実装した。([4th AI Edge Contest](https://github.com/shin-yamashita/4th-AI-Edge-Contest)で開発したものを修正して用いた)
- RISC-V は scratch から開発した rv32emc に対応する CPU core を実装した。
- アクセラレータ 及び rv32emc core は SystemVerilog で記述した。
- 推論アプリは PYNQ Linux (Ubuntu20.04) 上で実行する python で実装し、トラッキングアルゴリズムを rv32emc 上の C で実装した。

詳細は [doc/レポート](doc/report.pdf) 参照

### ./infer/ [→推論実行アプリケーション](infer/)  

- python で記述した推論アプリケーション。  
- Object Detection 推論ネットワークは [TF2 の SSD mobilenetv2](http://download.tensorflow.org/models/object_detection/tf2/20200711/ssd_mobilenet_v2_320x320_coco17_tpu-8.tar.gz) をベースに今回の課題に合わせて転移学習し、int8 量子化した。  

### ./tensorflow_src/tflite_delegate/  [→TFlite delegate interface](tensorflow_src/)  
- 推論アプリから delegate API を介して C++ reference model または FPGA アクセラレータに実行委譲するインターフェース関数のソース。  
- Conv2d, depthwiseConv2d の２種の演算を delegate する。
- C++ reference model は tflite の [チャネルごとの int8 量子化](https://www.tensorflow.org/lite/performance/quantization_spec) で実装した。
- FPGA アクセラレータはチャネルごとの int8 量子化のみに対応する。  

### ./tfacc_i8/  [→FPGA sources](tfacc_i8/)  
- アクセラレータ 及び rv32emc の RTL ソース。  
- rv32emc のファームウェア  
- 論理シミュレーション環境、論理合成環境。  

## files
```
├─ infer                Inference application
│ └─ infer_video.py
├─ tensorflow_src
│ └─ tflite_delegate    Delegate interface sources (C++)
└─ tfacc_i8             FPGA design sources
  ├─ bd                 ZYNQ block design
  ├─ firm               Firmware for rv32emc (C)
  │ └─ rvmon
  ├─ hdl                HDL sources
  │ ├─ acc              Accelerator sources (SystemVerilog)
  │ ├─ rv32_core        rv32emc sources (SystemVerilog)
  │ ├─ tfacc_cpu_v1_0.v Controller top design
  │ └─ tfacc_memif.sv   Data pass top design
  ├─ ip                 Xilinx ip
  ├─ sim                Logic simulation environment
  └─ syn                Logic synthesis environment
```
## References
- [第5回AIエッジコンテスト（実装コンテスト③)](https://signate.jp/competitions/537)
- [Avnet / Ultra96-PYNQ](https://github.com/Avnet/Ultra96-PYNQ/releases)
- [tensorflow r2.7 sources](https://github.com/tensorflow/tensorflow/tree/r2.7) 
- [TensorFlow Lite デリゲート](https://www.tensorflow.org/lite/performance/delegates)
- [TensorFlow Lite カスタムデリゲートの実装](https://www.tensorflow.org/lite/performance/implementing_delegate#when_should_i_create_a_custom_delegate)
- [TensorFlow Lite 8ビット量子化仕様](https://www.tensorflow.org/lite/performance/quantization_spec) 

## License
- [Apache License 2.0](LICENSE)
