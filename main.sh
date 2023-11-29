#!/usr/bin/env bash
src="networkit--networkit"
out="$HOME/Logs/$src$1.log"
ulimit -s unlimited
printf "" > "$out"

# Download source code
if [[ "$DOWNLOAD" != "0" ]]; then
  rm -rf $src
  git clone --recursive https://github.com/wolfram77/$src
  cd $src
  git checkout for-leiden-communities-openmp
  rm -rf extlibs
  rm -rf extrafiles
  rm -rf include
  rm -rf networkit
  rm -rf MANIFEST.in
  rm -rf networkit.pc
  rm -rf pyproject.toml
  rm -rf requirements.txt
  rm -rf setup.py
  rm -rf version.py
fi

# Convert graph to edgelist, run Networkit ParallelLeiden, and clean up
runNetworkit() {
  stdbuf --output=L printf "Converting $1 to $1.elist ...\n"   | tee -a "$out"
  lines="$(node process.js header-lines "$1")"
  tail -n +$((lines+1)) "$1" > "$1.elist"
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
for i in {1..5}; do
  runAll
done

# Signal completion
curl -X POST "https://maker.ifttt.com/trigger/puzzlef/with/key/${IFTTT_KEY}?value1=$src$1"
