#! /bin/bash

occlum-gcc -o mmap mmap.c -lpthread
if [ -z "$PNUM" ]; then
	count=1
else
	count=$PNUM
fi

for i in $(seq 1 $count)
do
	 ./mmap $@ &
done

for job in `jobs -p`
do
    wait $job || let "FAIL+=1"
done



if [ -z "$FAIL" ];
then
echo "Done!"
else
echo "FAIL! ($FAIL)"
fi
