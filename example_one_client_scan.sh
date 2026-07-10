#!/bin/bash
#SBATCH -A cms
#SBATCH -p a30
#SBATCH -q standby
#SBATCH --time=00:30:00
#SBATCH --gres=gpu:1
#SBATCH --cpus-per-gpu=4
#SBATCH --mem-per-cpu=4000
#SBATCH --output=/depot/cms/users/colberte/SONIC/nereid/perf_studies/scans/scan_cuda_particlenet_2026-07-08/job_output.out
#SBATCH --error=/depot/cms/users/colberte/SONIC/nereid/perf_studies/scans/scan_cuda_particlenet_2026-07-08/job_output.out

JOB_DIR="/depot/cms/users/colberte/SONIC/nereid/perf_studies/scans/scan_cuda_particlenet_2026-07-08"
CLIENT_LOG="client_output.log"
SERVER_LOG="server_output.log"
MONITOR_FILE="$JOB_DIR/resource_usage.csv"

cd /depot/cms/users/colberte/SONIC/nereid/perf_studies

echo "Starting resource monitor"

echo "Timestamp, GPU Util (%), GPU Mem (%), CPU Util (%), RAM Util (%)" >> $MONITOR_FILE
while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S.%1N')

    GPU_UTIL=$(nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits)
    GPU_MEM=$(nvidia-smi --query-gpu=memory.used,memory.total --format=csv,noheader,nounits | awk -F', ' '{printf "%.2f", ($1/$2)*100}')
    CPU_UTIL=$(top -bn1 | grep "Cpu(s)" | sed "s/.*, *\([0-9.]*\)%* id.*/\1/" | awk '{print 100 - $1}')
    MAIN_MEM=$(free | awk '/Mem:/ {printf "%.2f", $3/$2 * 100}')

    echo "$TIMESTAMP, $GPU_UTIL, $GPU_MEM, $CPU_UTIL, $MAIN_MEM" >> $MONITOR_FILE

    sleep 0.25
done &
MONITOR_PID=$!

echo "Starting Nereid server"

./start_server_gilbreth.sh > "$JOB_DIR/$SERVER_LOG" 2>&1 &
SERVER_PID=$!

# wait for server to start up
sleep 30

module load conda
conda activate /depot/cms/private/users/colberte/conda_envs/NereidClientEnv

echo "Starting batch size scan"

python batch_scan_gilbreth_particlenet.py $JOB_DIR > "$JOB_DIR/$CLIENT_LOG" 2>&1
python plot_scan.py "$JOB_DIR/scan_timings.json" --title "ParticleNet-AK4, NVIDIA A30"

kill $SERVER_PID
kill $MONITOR_PID

echo "Finished"
