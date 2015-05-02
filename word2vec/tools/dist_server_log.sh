set -x
source common.sh

LOG=./server.cluster.log

rm -f $LOG

for node in $NODES; do
    echo $node >> $LOG
    echo >> $LOG
    echo "============================================" >> $LOG
    echo "** server:" >> $LOG
    ssh chunwei@$node "tail -n20 /home/chunwei/cluster_demo/log.server.1" >> $LOG
done
