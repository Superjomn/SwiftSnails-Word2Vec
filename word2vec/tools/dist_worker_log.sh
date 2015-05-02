set -x
source common.sh

LOG=./worker.cluster.log

rm -f $LOG

for node in $NODES; do
    echo $node >> $LOG
    echo >> $LOG
    echo "============================================" >> $LOG
    echo "** worker:" >> $LOG
    ssh chunwei@$node "tail -n20 /home/chunwei/cluster_demo/log.worker.1" >> $LOG
done
