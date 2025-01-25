#!/bin/sh

# Install cortex.llamacpp engine

cortex engines install llama-cpp -s /opt/cortex.llamacpp
cortex -v

# Start the cortex server

sed -i 's/apiServerHost: 127.0.0.1/apiServerHost: 0.0.0.0/' /root/.cortexrc

cortex start
# Keep the container running by tailing the log files
tail -f /root/cortexcpp/logs/cortex.log &
tail -f /root/cortexcpp/logs/cortex-cli.log &
wait