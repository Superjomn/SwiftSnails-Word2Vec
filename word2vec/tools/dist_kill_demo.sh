set -x

source common.sh

for node in $NODES; do
    echo "$nodeid $node"
    let nodeid="nodeid + 1"
    ssh chunwei@$node "cd $ROOT; sh kill_demo.sh"
done

