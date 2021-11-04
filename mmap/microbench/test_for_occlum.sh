#! /bin/bash

if [ ! -d occlum_instance ]; then
	occlum-gcc -o mmap main.c
	occlum new occlum_instance
	cp mmap occlum_instance/image/bin
	cp Occlum.json occlum_instance
	cd occlum_instance && occlum build
else
	occlum-gcc -o mmap main.c
	cp mmap occlum_instance/image/bin
	cd occlum_instance && occlum build -f
fi

occlum start

count=$1

for i in $(seq 1 $count)
do
	occlum exec /bin/mmap &
done
for job in `jobs -p`
do
echo $job
    wait $job || let "FAIL+=1"
done
occlum stop

if [ "$FAIL" == "0" ];
then
echo "Done!"
else
echo "FAIL! ($FAIL)"
fi
