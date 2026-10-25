#!/bin/bash
cd /home/tenki/ft
until grep -q TRAIN_DONE train2.log; do
  pgrep -f "python train2" >/dev/null || { echo "TRAIN2_DIED $(date -u +%H:%M)" > after2.log; exit 1; }
  sleep 30
done
. /home/tenki/v/bin/activate
nice -n 10 python test.py adapter_v2 test_results_v2.json nobase > test2.log 2>&1
echo AFTER_DONE >> test2.log
