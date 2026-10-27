#! /bin/bash
export GTI_RECORD_GEN_IMPL="/scratch/c703429/software/must-1.9.1/modules/libgtiRecordGenImpl.so"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/scratch/c703429/software/must-1.9.1/modules::/usr/site/hpc/spack/v0.19-lcc3-20230919/opt/spack/linux-rocky8-westmere/gcc-12.2.0/openmpi-3.1.6-d2gmn55g7hoinwfuk2lc3ibz6odzujak/lib/"
export DYLD_LIBRARY_PATH="/scratch/c703429/software/must-1.9.1/modules::/usr/site/hpc/spack/v0.19-lcc3-20230919/opt/spack/linux-rocky8-westmere/gcc-12.2.0/openmpi-3.1.6-d2gmn55g7hoinwfuk2lc3ibz6odzujak/lib/:$DYLD_LIBRARY_PATH"

/scratch/c703429/software/must-1.9.1/bin/wrappgen weaver-wrapp-gen-input-0.xml
/scratch/c703429/software/must-1.9.1/bin/wrappgen weaver-wrapp-gen-input-1.xml
/scratch/c703429/software/must-1.9.1/bin/recvgen weaver-receival-gen-input-0.xml
/scratch/c703429/software/must-1.9.1/bin/recvgen weaver-receival-gen-input-1.xml
