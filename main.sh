#!/usr/bin/env bash
src="networkit--networkit"
out="$HOME/Logs/$src.log"
ulimit -s unlimited
printf "" > "$out"

# Download source code
if [[ "$DOWNLOAD" != "0" ]]; then
  rm -rf $src
  git clone --depth=1 --recursive https://github.com/wolfram77/$src
  rm -rf $src/extlibs
  rm -rf $src/extrafiles
  rm -rf $src/include
  rm -rf $src/networkit
  rm -rf $src/MANIFEST.in
  rm -rf $src/networkit.pc
  rm -rf $src/pyproject.toml
  rm -rf $src/requirements.txt
  rm -rf $src/setup.py
  rm -rf $src/version.py
fi
cd $src

# Build
# make -j32

# Convert graph to binary format, run Networkit PLM, and clean up
runNetworkit() {
  stdbuf --output=L printf "Converting $1 to $1.edgelist ...\n" | tee -a "$out"
  lines="$(node process.js header-lines "$1")"
  tail -n +$((lines+1)) "$1" > "$1.edgelist"
  stdbuf --output=L python3 main.py "$1.edgelist"          2>&1 | tee -a "$out"
  stdbuf --output=L printf "\n\n"                               | tee -a "$out"
  rm -rf "$1.edgelist"
}

# Run Networkit PLM on all graphs
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

# Run NetworKit PLM 5 times for each graph
for i in {1..5}; do
  runAll
done

# Signal completion
curl -X POST "https://maker.ifttt.com/trigger/puzzlef/with/key/${IFTTT_KEY}?value1=$src$1"
