#!/bin/bash
/home/tenki/ft/ensure.sh
cd /home/tenki/ft
echo "time_utc $(date -u +%H:%M) | uptime $(uptime -p) | load $(cut -d' ' -f1 /proc/loadavg)"
grep -E '^examples|^RESUMED|^step|Traceback|Error|TRAIN_DONE' train2.log 2>/dev/null | tail -2
tail -3 restarts.log 2>/dev/null
grep -E 'done|Traceback|AFTER_DONE' test2.log 2>/dev/null | tail -2
cat vault_v2.log 2>/dev/null
