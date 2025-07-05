#!/usr/bin/env bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=64
#SBATCH --exclusive
#SBATCH --job-name slurm
#SBATCH --output=slurm.out
# module load openmpi/4.1.5
# module load hpcx-2.7.0/hpcx-ompi
# source scl_source enable gcc-toolset-11
# module load cuda/12.3
source /opt/rh/gcc-toolset-13/enable
src="networkit--networkit"
out="$HOME/Logs/$src$1.log"
ulimit -s unlimited
printf "" > "$out"

# Download source code
if [[ "$DOWNLOAD" != "0" ]]; then
  rm -rf $src
  git clone --depth=1 --recursive https://github.com/wolfram77/$src -b lowmem-communities--leiden
  cd $src
  git checkout lowmem-communities--leiden
fi

# Build
if [[ "$BUILD" != "0" ]]; then
  # Clean up
  pkgdir="$HOME/.local/lib/python3.6/site-packages"
  # rm -rf "$pkgdir/networkit-10.1.1-py3.6.egg-info"
  # rm -rf "$pkgdir/networkit-11.0-py3.6-linux-x86_64.egg"
  # rm -rf "$pkgdir/networkit"
  # Build
  pip3 uninstall -y networkit
  python3 setup.py build_ext -j32
  pip3 install -e . --prefix "$HOME/.local"
  pip3 install --user psutil
fi

# Convert graph to edgelist, run Networkit ParallelLeiden, and clean up
runNetworkit() {
  stdbuf --output=L printf "Converting $1 to $1.elist ...\n"   | tee -a "$out"
  lines="$(node process.js header-lines "$1")"
  tail -n +$((lines+1)) "$1" > "$1.elist"
  stdbuf --output=L printf "Running Networkit ParallelLeiden on $1 ...\n" | tee -a "$out"
  stdbuf --output=L python3 main.py "$1.elist" "$1.clstr" 2>&1 | tee -a "$out"
  stdbuf --output=L printf "\n\n"                              | tee -a "$out"
  rm -rf "$1.elist"
}

# Run Networkit ParallelLeiden on all graphs
runAll() {
  # runNetworkit "$HOME/Data/web-Stanford.mtx"
  runNetworkit "$HOME/Data/indochina-2004.mtx"
  runNetworkit "$HOME/Data/uk-2002.mtx"
  runNetworkit "$HOME/Data/arabic-2005.mtx"
  runNetworkit "$HOME/Data/uk-2005.mtx"
  runNetworkit "$HOME/Data/webbase-2001.mtx"
  runNetworkit "$HOME/Data/it-2004.mtx"
  runNetworkit "$HOME/Data/sk-2005.mtx"
  runNetworkit "$HOME/Data/com-LiveJournal.mtx"
  runNetworkit "$HOME/Data/com-Orkut.mtx"
  runNetworkit "$HOME/Data/asia_osm.mtx"
  runNetworkit "$HOME/Data/europe_osm.mtx"
  runNetworkit "$HOME/Data/kmer_A2a.mtx"
  runNetworkit "$HOME/Data/kmer_V1r.mtx"
}

# Run NetworKit ParallelLeiden 5 times for each graph
for i in {1..1}; do
  runAll
done

# Signal completion
curl -X POST "https://maker.ifttt.com/trigger/puzzlef/with/key/${IFTTT_KEY}?value1=$src$1"
