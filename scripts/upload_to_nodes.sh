#!/bin/bash

echo "Building firmware..."
pio run

echo "Closing serial on all nodes..."
curl -s -o /dev/null -H 'Content-Type: application/json' -d '{ "op": "serial_close" }' https://mesh.holt.dj/nodes/admin

for agent in rr dj cw hcc temp d bno gv; do
  echo "Uploading to $agent"
  pio remote -a $agent run -t upload >> logs/upload_$agent.log &
  #pio remote -a $agent device list
done

echo "Waiting for all uploads to finish..."
wait

echo "Opening serial on all nodes..."
curl -s -o /dev/null -H 'Content-Type: application/json' -d '{ "op": "serial_open" }' https://mesh.holt.dj/nodes/admin
