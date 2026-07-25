## Run in dev container for safety as such: 

docker run -it --privileged --cgroupns=private -v $(pwd):/src ubuntu:24.04 bash

It is ironic to run this project inside another container, but that gets at the point of containerization in general, because we want to ensure our pivot_roots and rewriting of cgroups are fully isolated and non-interferring with our host machine, in the event of bugs


## The following needs to be done to allow cgroup setup, by initializing child cgroup first



mkdir -p /sys/fs/cgroup/init

for pid in $(cat /sys/fs/cgroup/cgroup.procs); do
    echo $pid > /sys/fs/cgroup/init/cgroup.procs
done

echo "+cpu +memory +pids" > /sys/fs/cgroup/cgroup.subtree_control



cat /sys/fs/cgroup/cgroup.controllers
ls /sys/fs/cgroup/



## MUST COMPILE TESTS AS SUCH: gcc -static test.c -o test

Need -static flag
