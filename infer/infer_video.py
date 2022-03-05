#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#

import argparse
import os
import os.path as osp
import sys
import json
import select
import tty
import termios
import cv2
import numpy as np
import tflite_runtime.interpreter as tflite
import time
from ctypes import *

def iskbhit():
    return select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], [])


class pre_data(Structure):
    _fields_ = [
            ("n", c_int),
            ("uid", c_int),
            ("cls", c_int),
            ("score", c_float),
            ("x", c_float),
            ("y", c_float),
            ("area", c_float),
            ("Y", c_ubyte),
            ("Cb", c_byte),
            ("Cr", c_byte),]

font = cv2.FONT_HERSHEY_SIMPLEX
boxcolor = {'Pedestrian':(64, 64, 255),'Car':(64, 255, 0)}

parser = argparse.ArgumentParser(description='object trcking')
parser.add_argument('-m', '--model', default='./tflite_model_mb2/model_q.tflite', help="tflite model path")
parser.add_argument('-i', '--input', default='../data/test_videos', help="Input file dir")
parser.add_argument('-t', '--score', type=float, default=0.75, help="minscore")
parser.add_argument('-d', '--delegate', action='store_false', default=True, help="delegate disable")
parser.add_argument('-x', action='store_true', default=False, help="X image display")

args = parser.parse_args()

model = args.model
category_index = ['Pedestrian' ,'Car' ,'Motorbike' ,'Bicycle' ,'Truck' ,'Bus' ,'Svehicle' ,'Train' ,'Signs' ,'Signal']

pre = CDLL(osp.join(os.getcwd(), "libtrack_if.so.1.0"))
delegate = osp.join(os.getcwd(), "../tensorflow_src/bazel-bin/tflite_delegate/dummy_external_delegate.so")

if args.delegate:
  interpreter = tflite.Interpreter(model_path=model, 
    experimental_delegates=[tflite.load_delegate(delegate)])
else:
  interpreter = tflite.Interpreter(model_path=model)
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

#print(input_details)
#print(output_details)
# input, output parameters
inq_param = (1.0/127.0, 127.0)
if input_details[0]['quantization'][0]>0.0:
  inq_param = input_details[0]['quantization']
in_dtype = input_details[0]['dtype']

print("in:  ", input_details[0]['name'], "id:", input_details[0]['index'], 
  "Q:", inq_param, in_dtype, input_details[0]['shape'])

q_param = []
for i, outs in enumerate(output_details):
  if outs['quantization'][0]>0.0:
    q_param.append(outs['quantization'])
  else:
    q_param.append((1.0, 0.0))
  print("out: ", outs['name'], "id:", outs['index'], "Q:", q_param[i], outs['shape'])


def deQuantize(x, q_param):
  (scale, zero) = q_param
  #print("dQ:",x,zero)
  return (x - float(zero)) * scale

def run_inference_for_single_image(image):
  # Run inference
  if(in_dtype == np.uint8):
    image = np.uint8(image)
  else:
    image = deQuantize(np.float32(image), inq_param)

  interpreter.set_tensor(input_details[0]['index'], image)
  interpreter.invoke()

  scores  = deQuantize(interpreter.get_tensor(output_details[0]['index'])[0], q_param[0])
  boxes   = np.clip(deQuantize(interpreter.get_tensor(output_details[1]['index'])[0], q_param[1]), 0.0, 1.0)
  num     = round(deQuantize(interpreter.get_tensor(output_details[2]['index'])[0], q_param[2]))
  classes = np.round(deQuantize(interpreter.get_tensor(output_details[3]['index'])[0], q_param[3])).astype(np.int32)

  return num,classes,boxes,scores

def ycc_value(img_ycc, box, class_id):
  h, w, c = img_ycc.shape
  cbox = (box * np.array([h, w,  h, w])).astype(np.int32)
  y1, x1, y2, x2 = cbox
  cimg = img_ycc[y1:y2,x1:x2]
  ycc = cv2.mean(cimg)
  return [int(ycc[0]),int(ycc[1]),int(ycc[2])]

def tointlist(box): # normalized box -> original image sized box
  y1, x1, y2, x2 = box * np.array([1216, 1936, 1216, 1936])
  return [int(x1),int(y1), int(x2), int(y2)]  # convert to submit format

def mvec(rec_d, rec):
  vec = {}
  for cat in rec.keys():
    if cat in rec_d:
      vec[cat] = []
      for bxs in rec[cat]:
        rid = bxs['id']
        x1,y1,x2,y2 = bxs['box2d']
        for bxsd in rec_d[cat]:
          if bxsd['id'] == rid: # match id, 
            x1d,y1d,x2d,y2d = bxsd['box2d']
            a = (x2-x1)*(y2-y1)
            ad = (x2d-x1d)*(y2d-y1d)
            vec[cat].append({'id':rid,'vec':[(x1+x2)/2, (y1+y2)/2, (x1d+x2d)/2, (y1d+y2d)/2, a/ad]})
  return vec

if __name__ == '__main__':

  images = args.input
  if images.endswith('.mp4'):
    dirname = osp.dirname(images)
    imlist = [osp.basename(images)]
  else:
    dirname = images
    imlist = [img for img in os.listdir(images) if osp.splitext(img)[1].lower() == '.mp4']
    imlist.sort()

  minscore = args.score
  _,H,W,_ = input_details[0]['shape']
#  print(H,W)

  submit = {}

  # C -> rv32 interface
  _map = [0]*60
  map = (c_int * 60)(*_map)
  pre.pre_map_get.restype = c_int
  pre.pre_map_get.argtypes = (POINTER(c_int),c_int)
  pre.tracking.argtypes = (c_int,)

  elapsed = [0] * 10
  avel = [0] * 10
  nave = 0

  pause = True
  old_settings = termios.tcgetattr(sys.stdin)
  try:
    tty.setcbreak(sys.stdin.fileno())

    for nfile,imgfile in enumerate(imlist): # input image files
      imgpath = osp.join(dirname, imgfile)
      cap = cv2.VideoCapture(imgpath)
      rec_d = {}
      print(imgfile)
      submit[imgfile] = []
      frame = 0
      uid = 0   # reset object unique id

      while True:
        if(iskbhit()):
          c = sys.stdin.read(1) # non blocking keyboard input
          key = 0
          if(c == '\x1b'): break  # ESC : next file
          if(c == 'q'):           # 'q' : quit
            key = ord('q')
            break

        ret, img = cap.read()
        if not ret: break

        submit[imgfile].append({'Car':[], 'Pedestrian':[]})

        start = time.time()
        img_bgr = cv2.resize(img, (W, H))
        # convert bgr to ycc
        img_ycc = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2YCrCb) - np.array([0,128,128])
        # convert bgr to rgb
        img_rgb = img_bgr[:,:,::-1]
        image_np_expanded = np.expand_dims(img_rgb, axis=0)

        elapsed[0] = time.time() - start
        num,classes,boxes,scores = run_inference_for_single_image(image_np_expanded)

        dnum = 0
        scorelist = {'Car':[], 'Pedestrian':[]}
        uidlist = []
        for i in range(num):
          class_id = int(classes[i])
          if class_id > 1: continue  # Car, Pede only
          label = category_index[class_id]

          score = scores[i]
          box = boxes[i]
          area = (box[2]-box[0])*(box[3]-box[1])*1936*1216/1024 
          if score > minscore and area > 1.0: # area > 1024 px^2
            xc = (box[1] + box[3])/2.0
            yc = (box[0] + box[2])/2.0
            ycc = ycc_value(img_ycc, box, class_id)
            # 
            pd = pre_data(dnum, uid, class_id, score, xc, yc, area, ycc[0], ycc[1], ycc[2])
            pre.pre_data_set(pd, dnum)  # object det data to C -> rv32

            submit[imgfile][frame][label].append({'id': uid, 'box2d': tointlist(box)})
            scorelist[label].append(score)
            uidlist.append(uid)

            #print("%d %4.1f:%-10s %5.3f,%5.3f %5.2f %d,%d,%d"%(dnum, score * 100.0, label, xc, yc, area, ycc[0],ycc[1],ycc[2]))
            dnum += 1
            uid += 1

        print("%s:%3d %2d/%d(%3.2f) "%(imgfile, frame, dnum,num,minscore), end='')
        elapsed[1] = time.time() - start
        pre.tracking(frame) # kick rv32 tracking routine and wait for complete
        elapsed[2] = time.time() - start

        if(frame > 0 and dnum > 0): 
          for cls in range(2):
            label = category_index[cls]
            nm = pre.pre_map_get(cast(pointer(map),POINTER(c_int)), cls)  # get rv32 tracking result
            #print(cls, list(map)[0:nm])
            for id, idd in enumerate(list(map)[0:nm]):
              if idd < 0: continue
              #print(id,idd,submit[imgfile][frame][label])
              submit[imgfile][frame][label][id]['id'] = idd  # tracked id

          #print('Ped maped uid:', [id['id'] for id in submit[imgfile][frame]['Pedestrian']])
          #print('Car maped uid:', [id['id'] for id in submit[imgfile][frame]['Car']])
        elapsed[3] = time.time() - start

        key = 0
        if(args.x): # display image
          h, w, c = img.shape
          if(h > 720):
            img = cv2.resize(img, (w//2, h//2))
          h, w, c = img.shape

          rec = submit[imgfile][frame]
          vec = mvec(rec_d, rec)
          rec_d = rec
          for cls in rec.keys():
            objs = rec[cls]
            color = boxcolor[cls]
            uids = []
            for j, obj in enumerate(objs):
              xmin, ymin, xmax, ymax = np.array(obj['box2d']) // 2
              cv2.rectangle(img, (xmin, ymin), (xmax, ymax), color, 1)
              score = scorelist[cls][j]
              info1 = '%d' % obj['id']
              info2 = '%d' % int(score * 100.0)
              uids.append(obj['id'])
              cv2.putText(img, info1, (xmin, ymin + 12), font, 0.4, color, 1, cv2.LINE_AA)
              cv2.putText(img, info2, (xmin, ymin + 24), font, 0.4, color, 1, cv2.LINE_AA)
            if cls in vec:
              for dic in vec[cls]:
                x,y,xd,yd,a = dic['vec']
                cv2.arrowedLine(img, (int(xd/2), int(yd/2)), (int(x/2), int(y/2)), color, 4)
            if(len(set(uids)) != len(uids)): 
              print("   ========== not unique error ============", uids)
              pause = True
          cv2.imshow('detection result', img)

          sleep = 0 if pause else 10
          key = cv2.waitKey(sleep)
          if key == 27: # when ESC key is pressed break
            break
          if key == ord('p'):
            pause = not pause
          if key == ord('q'):
            break

        elapsed[4] = time.time() - start
        print(" pre:%5.1fms infer:%5.1f track:%5.1f xdisp:%5.1f"%(elapsed[0]*1e3,elapsed[1]*1e3,elapsed[3]*1e3,elapsed[4]*1e3))
        for i in range(5):
          avel[i] += elapsed[i]
        nave += 1

        frame += 1
        #pre.next_frame()

      #if nfile > 1: break
      if key == ord('q'): break

    kav = 1e3 / nave
    print("elapsed time average(%d) : pre:%5.1fms infer:%5.1f track:%5.1f xdisp:%5.1f"%(nave,avel[0]*kav,avel[2]*kav,avel[3]*kav,avel[4]*kav))
 
  #  print(submit)
    with open('sample_submit.json', 'w') as f:
      json.dump(submit, f)

    if(args.x):
      cv2.destroyAllWindows()

  finally:
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
     
