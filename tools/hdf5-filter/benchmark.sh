#! /bin/bash
julea_path="/julea"
storage_path="/tmp/julea-1000"
hdf5_in="mbetest.h5"
hdf5_out="mbetest-out.h5"

export JULEA_PREFIX="${HOME}/julea-install"
. "$julea_path"/scripts/environment.sh
export HDF5_PLUGIN_PATH=/usr/local/hdf5/lib/plugin:$HDF5_PLUGIN_PATH
mkdir -p logs
mkdir -p h5_out

for chunk_size in 8000 16000 32000 64000 128000 256000
do
	echo
	echo Benchmark running with chunk size "$chunk_size"
	echo
	"$julea_path"/scripts/setup.sh start
	export H5REPACK_CHUNK_SIZE="$chunk_size"
	(time h5repack -v -f UD=4123,0 "$hdf5_in" "$hdf5_out") > logs/h5repack_"$chunk_size".log 2>&1
	julea-statistics > logs/statistics_"$chunk_size".log
	du "$storage_path"/posix > logs/du_"$chunk_size".log
	du -sch "$storage_path"/posix > logs/du_h_"$chunk_size".log
	mdb_stat "$storage_path"/lmdb/ > logs/mdb_stat_"$chunk_size".log
	mv "$hdf5_out" h5_out/"$chunk_size".h5
	"$julea_path"/scripts/setup.sh stop
	rm -rf "$storage_path"
done