#!/bin/bash
cd /home/tenki/ft
until grep -q TRAIN_DONE train.log; do
  pgrep -f "python train" >/dev/null || { echo "TRAIN_DIED" > after.log; exit 1; }
  sleep 30
done
. /home/tenki/v/bin/activate
nice -n 10 python test.py adapter_final > test.log 2>&1
echo AFTER_DONE >> test.log
