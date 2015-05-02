set -x 
set -e

source common.sh

NUM_WORKERS=1
NUM_SERVERS=1

cd $ROOT

rm -f param_back/*

export GLOG_minloglevel=2

for((i=1;i<=$NUM_WORKERS;i++)); do
    echo "start worker $i"
    nohup sh run_worker.sh >log.worker.$i 2>&1 &
done

for((i=1;i<=$NUM_SERVERS;i++)); do
    echo "start server $i"
    nohup sh run_server.sh >log.server.$i 2>&1 &
done
