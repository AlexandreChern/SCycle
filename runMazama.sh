#!/bin/bash
#PBS -N bruteForce_al_a7_Dc1_sNGrad
#PBS -l nodes=1:ppn=24
#PBS -q tgp
#PBS -V
#PBS -m n
#PBS -k oe
#PBS -e /data/dunham/kallison/eqcycle/outFiles/bruteForce_al_a7_Dc1_sNGrad.err
#PBS -o /data/dunham/kallison/eqcycle/outFiles/bruteForce_al_a7_Dc1_sNGrad.out


EXEC_DIR=/data/dunham/kallison/eqcycle
INIT_DIR=/data/dunham/kallison/eqcycle/in
cd $PBS_O_WORKDIR

#~ mpirun $EXEC_DIR/main $INIT_DIR/max_cremeBrulee_cycle.in
#~ mpirun $EXEC_DIR/main $INIT_DIR/max_cremeBrulee_spinUp.in


#~ mpirun $EXEC_DIR/mainLinearElastic $INIT_DIR/he.in
#~ mpirun $EXEC_DIR/main $INIT_DIR/he.in

#~ mpirun $EXEC_DIR/main $INIT_DIR/sNgrad.in
mpirun $EXEC_DIR/main $INIT_DIR/spinUpTest.in
#~ mpirun $EXEC_DIR/main $INIT_DIR/flashHeating.in

#~ mpirun $EXEC_DIR/main $INIT_DIR/basin.in

#~ mpirun $EXEC_DIR/main $INIT_DIR/he2D.in
#~ mpirun $EXEC_DIR/main $INIT_DIR/he2D_sNgrad.in

