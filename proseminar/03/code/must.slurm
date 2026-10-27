#!/bin/bash

#SBATCH --output=must.log

/scratch/c703429/software/must-1.9.1/bin/mustrun -np 2 ./example_1 --must:output stdout
