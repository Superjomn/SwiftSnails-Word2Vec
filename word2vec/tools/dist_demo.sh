set -x
set -e
source common.sh

ROOT=./

cd ../

rm -f ./cluster_demo.tar*

tar cvf cluster_demo.tar ./cluster_demo
#gzip cluster_demo.tar

for node in $NODES; do
    scp cluster_demo.tar chunwei@${node}:~/
    ssh chunwei@$node "tar xvfm cluster_demo.tar"
done
