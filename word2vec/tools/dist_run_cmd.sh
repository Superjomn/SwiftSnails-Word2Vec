set -x
source common.sh

CMD="tail cluster_demo/log.server.1"

for node in $NODES; do
    ssh chunwei@$node "$CMD"
done
