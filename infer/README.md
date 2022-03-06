
# 推論実行アプリケーション

## 実行環境

同一のソースで以下の２つの環境で動作を確認した。  

1. ultra96v2 [PYNQ Linux V2.7 (Ubuntu20.04)](https://github.com/Avnet/Ultra96-PYNQ/releases)   
   FPGA に delegate 機構を介してアクセラレーション実行する。  

2. LinuxPC (Ubuntu20.06)  
   C++ reference model に delegate 実行する。  

## files

- **tfacc_load.py**  
  FPGA に fpga-data/design_1.bit ファイルから回路のコンフィグレーションを行う。  
  また、rv32emc コアのメモリーに、アクセラレータ制御プログラム(fpga-data/rvmon.mot) をロードする。  
- **infer_video.py**  
  tflite_runtime python ライブラリを用いて推論実行し、rv32rmc と通信してトラッキング処理を行う。  
  また、track_if.c は python - C インターフェースで、トラッキング用データを rv32emc コアとやり取りする。  
  tflite_model_mb2/ に今回の課題で作成した学習済み tflite graph を置いた。  
- **tensorflow_src/tflite_delegate/**  
  TFlite の delegate API で FPGA と接続するためのインターフェース関数のソースである。  
同時にハード実装のための C++ リファレンス実装でもある。  

  tflite_runtime と delegate interface の build は、[../tensorflow_src/README.md](../tensorflow_src/README.md) を参照。  

   ```
   infer/
   ├── infer_video.py       推論実行アプリ
   ├── tfacc_load.py        FPGA 初期化アプリ
   ├── fpga-data/ 
   │ ├── design_1.bit
   │ └── rvmon.mot         risc-v のプログラム(FPGA アクセラレータ制御、tracking 処理)
   ├── tflite_model_mb2
   │   └── model_q.tflite  TFlite graph int8
   ├── track_if.c
   └── libtrack_if.so.1.0   python と risc-v とのデータ授受インターフェース(source は track_if.c)

   tensorflow_src/tflite_delegate/  tflite delegate interface source
   ├── BUILD
   ├── Conv2D.cc             conv2d/dwconv2d のパラメータを risc-v に渡し、kick するインターフェース
   ├── MyDelegate.cc         delegate interface source
   └── tfacc_u8.cc           PL interface, cma allocate

   tensorflow_src/bazel-bin/tflite_delegate/dummy_external_delegate.so  : tflite delegate interface binary
   ```

## 推論実行手順

#### 準備 : PYNQ Linux V2.7 (Ubuntu 20.04) 及び LinuxPC (Ubuntu 20.04)  
  - python3-opencv Pillow のインストールが必要  
  - [../tensorflow_src/README.md](../tensorflow_src/README.md) に従って tflite-runtime の build/install  
    及び dummy_external_delegate.so を build しておく。  

#### ultra96v2 での推論実行

0. login  
   PYNQ Linux, based on Ubuntu 20.04 pynq  
   usb 接続 ether : 192.168.3.1  

   推論アプリ実行ディレクトリ: infer/

1. FPGA 初期化  
   ultra96v2 電源投入後、一度実行する  
   ```
   $ cd infer/  
   $ sudo ./tfacc_load.py  # FPGA に fpga-data/design_1.bit をロードし、fpga 内 risc-v cpu にプログラム rvmon.mot をロードする
   ** Load "design_1.bit" to Overlay
   PL clock : 125 (MHz)
   base:a0000000 range:10000
   *** Load "rvmon.mot" to processor memory
   ```

2. 推論実行

   a) 結果を画像で表示 (host PC から usb ether 経由で接続)    
      ssh -X xilinx@192.168.3.1  # usb 経由で X forwarding   
      ```
      $ cd infer/  
      $ sudo -E ./infer_video.py -x -i {video data dir} 
         {video data dir}/*.mp4 を読んで推論実行し、結果を cv2.imshow() で表示。  
         'q' キーで終了  
         ESC キーで次のファイルに移動  
         'p' キーで video を連続再生 / pause 切り替え、pause 中は他の任意のキーで次のframe  
         終了時、"sample_submit.json" に結果を出力  
      ```

   b) 推論時間計測  
      ```
      $ sudo ./infer_video.py  
      INFO: MyDelegate delegate: 72 nodes delegated out of 102 nodes with 11 partitions.  

      PL_if_config(): m_reg:0xffff72fc9000 accparam:0xffff72fc9100  
      PL_if_config(): tfacc_buf:0xffff6aa79000 cma:0x78300000 - 0x7e300000  
         :
      test_00.mp4:  0  4/60(0.75)  pre: 21.2ms infer:267.2 track:267.7 xdisp:267.7  
      test_00.mp4:  1  6/60(0.75)  pre: 21.5ms infer:264.1 track:265.2 xdisp:265.2  
      test_00.mp4:  2  6/60(0.75)  pre: 20.4ms infer:262.9 track:263.8 xdisp:263.8
         :
      test_00.mp4: 17  3/60(0.75)  pre: 18.8ms infer:267.3 track:268.2 xdisp:268.2
      elapsed time average(18) : pre: 18.5ms infer:261.8 track:262.1 xdisp:262.1
      PL_if_free():cma_munmap     # 表示時間は積算され、平均した値
      PL_if_free():cma_free

      # 処理時間計測の起点: cap.read()が完了した時点
      # pre:    画像の前処理が完了した時点 
      # infer:  起点から 前処理した画像を interpreter にセットして ObjectDetection 結果を得るまでの時間
      # track:  起点から tracking 処理が終了、結果を取得し、json に反映するまでの時間
      # xdisp:  起点から cv2.imshow() で表示終了するまでの時間

      #  'q' キーで終了
      終了時、"sample_submit.json" に結果を出力
      ```
3. 他のオプション  
   ```
    $ ./infer_video.py --help
    usage: infer_video.py [-h] [-m MODEL] [-i INPUT] [-t SCORE] [-d] [-x]
    object trcking
    optional arguments:
      -h, --help                show this help message and exit
      -m MODEL, --model MODEL   tflite model path
      -i INPUT, --input INPUT   Input video file dir/file (default: '../data/test_videos/')
      -t SCORE, --score SCORE   minscore
      -d, --delegate            delegate disable
      -x                        X image display
   ```
   
5. risc-v debug ターミナル  
   GPIO に rv32emc のデバッグ用 uart I/F を出して、モニタープログラムでデバッグを行った。   
   cp2103 など USB-uart で接続する。  
   ```
   F8.HD_GPIO_1 TXD_0  from USB-UART TX 115200 baud
   F7.HD_GPIO_2 RXD_0  to   USB-UART RX
   G7.HD_GPIO_3 LED    1 sec blink (risc-v の割り込み動作確認)
   ```
   ```
   rvmon loaded. 125.0 MHz    # tfacc_load.py 時のメッセージ
   Np = 42 
   rvmon$ h          # list command
   rvmon$ d {addr}   # dump memory
   rvmon$ e          # infer_video.py 実行後、Conv 演算の各node の実行時間を表示
   0:  2.47 % 5.18 ms
   1:  3.02 % 6.33 ms
   2:  1.91 % 4.01 ms
   3:  4.66 % 9.78 ms
      :
               209.69 ms  run:207.82 ms xrdy:47.19 ms
   ```

#### Linux PC での推論実行

- 結果を画像で表示    

   ```
   $ cd infer/  
   $ ./infer_video.py -x -i {video data dir} {-d}  
      {video data dir}/*.mp4 を読んで推論実行し、結果を cv2.imshow() で表示。  
      'q' キーで終了  
      ESC キーで次のファイルに移動  
      'p' キーで video を連続再生 / pause 切り替え、pause 中は他の任意のキーで次のframe  
      終了時、"sample_submit.json" に結果を出力  
   ```

