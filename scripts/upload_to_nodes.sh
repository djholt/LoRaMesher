#!/bin/bash

mkdir -p logs

echo "Building firmware..."
pio run

if [[ $? -ne 0 ]]; then
  echo "Aborting: build failed!"
  exit 1
fi

echo "Closing serial on all nodes..."
curl -s -o /dev/null -H 'Content-Type: application/json' -d '{ "op": "serial_close" }' https://mesh.holt.dj/nodes/admin

for agent in snow rr dj cw hcc temp d bno gv; do
  echo "Uploading to $agent"
  pio remote -a $agent run -t upload >> logs/upload_$agent.log &
  #pio remote -a $agent device list
done

echo "Waiting for all uploads to finish..."
wait

echo "Opening serial on all nodes..."
curl -s -o /dev/null -H 'Content-Type: application/json' -d '{ "op": "serial_open" }' https://mesh.holt.dj/nodes/admin

echo "UPLOAD RESULTS:"
tail -n 1 logs/upload_*.log
