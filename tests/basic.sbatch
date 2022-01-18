#!/bin/sh
#SBATCH -A CSC332_crusher
#SBATCH -J quintain
#SBATCH -o %x-%j.out
#SBATCH -t 00:10:00
#SBATCH -N 4

set -euo pipefail
WORKDIR=$(pwd)
OUTPUTDIR=/gpfs/alpine/scratch/${USER}/csc332


srun -N 1 -n 1 bedrock -c ${WORKDIR}/mochi-quintain-provider.json ofi+cxi:// &

srun -N 3 -n 30 --ntasks-per-node=10 quintain-benchmark  -g quintain.ssg -j ${WORKDIR}/quintain-benchmark-example.json -o test-output

kill %1
sleep 10
wait
