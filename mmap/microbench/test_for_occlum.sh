#! /bin/bash

if [ ! -d occlum_instance ]; then
	occlum-gcc -o mmap mmap.c -lpthread
	occlum new occlum_instance
	cp mmap occlum_instance/image/bin
	cp Occlum.json occlum_instance
	cd occlum_instance && occlum build
else
	occlum-gcc -o mmap mmap.c -lpthread
	cp mmap occlum_instance/image/bin
	cp Occlum.json occlum_instance
	cd occlum_instance && occlum build -f
fi

occlum start --cpus 4

if [ -z "$PNUM" ]; then
	count=1
else
	count=$PNUM
fi

for i in $(seq 1 $count)
do
	occlum exec /bin/mmap $@ &
done
for job in `jobs -p`
do
    wait $job || let "FAIL+=1"
done
occlum stop

if [ -z "$FAIL" ];
then
echo "Done!"
else
echo "FAIL! ($FAIL)"
fi
