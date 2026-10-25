#!/bin/bash
# Self-healing supervisor: (re)starts training or testing if the sandbox restarted.
cd /home/tenki/ft; . /home/tenki/v/bin/activate
grep -q AFTER_DONE test2.log 2>/dev/null && exit 0
if ! grep -q TRAIN_DONE train2.log; then
  pgrep -f "^python train2\.py" >/dev/null || { echo "restart_train $(date -u +%H:%M)" >> restarts.log; setsid nohup nice -n 10 python train2.py 718 >> train2.log 2>&1 < /dev/null & }
else
  pgrep -f "^python test\.py" >/dev/null || { echo "start_test $(date -u +%H:%M)" >> restarts.log; setsid nohup bash -c 'nice -n 10 python test.py adapter_v2 test_results_v2.json nobase > test2.log 2>&1; tar czf llama31-gpt35style-lora-v2.tar.gz adapter_v2 HOW_TO_LOAD.txt train.py train2.py prep.py test.py && ./vault_upload.sh llama31-gpt35style-lora-v2.tar.gz "Round 2 adapter (400 + 718 clean GPT-3.5 examples, no Confidence lines), 2026-10-25" > vault_v2.log 2>&1; echo AFTER_DONE >> test2.log' < /dev/null > /dev/null 2>&1 & }
fi
