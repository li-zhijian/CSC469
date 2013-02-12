% CSC469 Assignment 1 (Part A)
% Abhinav Gupta (998585963), Zeeshan Qureshi (997108954)
% 11 Feb 2013

<!-- vim:set spell tw=72: -->

Tracking Process Activity
=========================

Setup
-----

The experiments were run on a laptop with an [Intel Core 2 Duo T8300][]
processor running Linux. Since the CPU supported adaptive frequency stepping
we disabled it via 'cpufreq-set' and set it to a fixed 2.4 GHz. We also set
the processor affinity for our tasks to 'CPU 0' while starting them to get
consistent cache performance. The command used to do this was 'taskset'.

[Intel Core 2 Duo T8300]: http://ark.intel.com/products/33099/Intel-Core2-Duo-Processor-T8300-3M-Cache-2_40-GHz-800-MHz-FSB

Determining CPU Frequency
-------------------------

Our initial attempt at determining CPU frequency was to store the value of
the TSC, sleep for a couple of milliseconds and then look at the value of the
TSC to determine how many CPU cycles had passed. This approach was not very
accurate and we had about a 50% margin of error in our results and the actual
CPU frequency listed. After consulting the [Intel How to on Benchmarking][] we
figured that with the newer 'Constant TSC' counter we might get better results
but my CPU did not support it so we had to work with the old approach.

We then refined our approach by adding a bit of busy waiting between successive
counter readings by executing a look with 1E6 instructions and we also used
the newer POSIX 'nanosleep' to get a more accurate sleep time. This helped
get the process switched in and out since it was consuming CPU resources. We
further rounded it off by collecting 5 samples of 1ms sleep times and averaging
out the results.

Determining the Threshold
-------------------------

After consulting Jeff Deans talk at Stanford on [Building Software Systems at
Google][] and testing we chose that a threshold of '1000' nsec would be a good
one to disregard normal operations such as cache misses and RAM lookups but
catch other things where our process waits for other devices or is scheduled
out.

 Activity                             Latency
 ---------------------------  ---------------
 L1 cache reference                    0.5 ns
 Branch mispredict                       5 ns
 L2 cache reference                      7 ns
 Mutex lock/unlock                      25 ns
 Main memory reference                 100 ns
 Compress 1K w/cheap zippy            1000 ns
 Disk seek                      10,000,000 ns
 ---------------------------  ---------------
         
 : Numbers Everyone Should Know

[Intel How to on Benchmarking]: http://download.intel.com/embedded/software/IA/324264.pdf
[Building Software Systems at Google]: http://static.googleusercontent.com/external_content/untrusted_dlcp/research.google.com/en//people/jeff/Stanford-DL-Nov-2010.pdf
