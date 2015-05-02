set -x 

source common.sh

PARAM_DIR=~/params

mkdir -p $PARAM_DIR

rm -f $PARAM_DIR/*

for node in $NODES; do 
    ssh chunwei@$node "scp ~/parameter.shard.txt chunwei@info01:${PARAM_DIR}/shard.${node}"
done



