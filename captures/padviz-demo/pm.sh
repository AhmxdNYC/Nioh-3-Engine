#!/bin/bash
# usage: pm.sh LABEL [seconds]  - PresentMon capture of Nioh3.exe into captures/padviz-demo/cost/LABEL.csv
cd "C:/Users/Ahmad Hamza/nioh3-engine"
PID=$(tasklist | awk '/^Nioh3.exe/{print $2}')
tools/PresentMon.exe --process_id $PID --stop_existing_session --session_name PadvizCost --output_file "C:\Users\Ahmad Hamza\nioh3-engine\captures\padviz-demo\cost\$1.csv" --timed ${2:-30} --terminate_after_timed --no_console_stats --v1_metrics >/dev/null 2>&1
