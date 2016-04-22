#!/bin/bash
#PBS -N rs401
#PBS -l nodes=1:ppn=24
#PBS -q tgp
#PBS -V
#PBS -m n
#PBS -k oe
#PBS -e /data/dunham/kallison/eqcycle/data/testInstability/m2D_rs401_theta.err
#PBS -o /data/dunham/kallison/eqcycle/data/testInstability/m2D_rs401_theta.out
#

EXEC_DIR=/data/dunham/kallison/eqcycle
INIT_DIR=$EXEC_DIR
cd $PBS_O_WORKDIR

mpirun $EXEC_DIR/mainMaxwell $INIT_DIR/linEl2D.in
#~mpirun $EXEC_DIR/mainMaxwell $INIT_DIR/powerLaw2D.in
