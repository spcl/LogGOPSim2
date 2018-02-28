for i in 2 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 \
  131072 262144 524288 1048576; do 
  if [ $i -ge 1000 ]; then 
    runs=1000;
  else
    runs=$i;
  fi;
  for root in $(seq 0 $runs); do
    ./schedgen -s $i -p ${1} --root=$root -o ${1}_${i}_r_${root}.out; 
    echo "$i root $root";
  done;  
done;
