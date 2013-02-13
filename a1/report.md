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
we disabled it via `cpufreq-set` and set it to a fixed 2.4 GHz. We also set
the processor affinity for our tasks to `CPU 0` while starting them to get
consistent cache performance. The command used to do this was `taskset`.

To test the program under both light and heavy loads we ran it under
`runlevel 1` with all services disabled, and a standard Gnome shell with
a browser playing video in flash.

[Intel Core 2 Duo T8300]: http://ark.intel.com/products/33099/Intel-Core2-Duo-Processor-T8300-3M-Cache-2_40-GHz-800-MHz-FSB

Determining CPU Frequency
-------------------------

Our initial attempt at determining CPU frequency was to store the value of
the TSC, sleep for a couple of milliseconds and then look at the value of the
TSC to determine how many CPU cycles had passed. This approach was not very
accurate and we had about a 50% margin of error in our results and the actual
CPU frequency listed. After consulting the [Intel How to on Benchmarking][] we
figured that with the newer `Constant TSC` counter we might get better results
but my CPU did not support it so we had to work with the old approach.

We then refined our approach by adding a bit of busy waiting between successive
counter readings by executing a look with 1E6 instructions and we also used
the newer POSIX `nanosleep` to get a more accurate sleep time. This helped
get the process switched in and out since it was consuming CPU resources. We
further rounded it off by collecting 5 samples of 1ms sleep times and averaging
out the results.

After these changes we got the CPU frequency to about a 10-15% margin of
error.

Determining the Threshold
-------------------------

After consulting Jeff Deans talk at Stanford on [Building Software Systems at
Google][] and testing we chose that a threshold of `1000` nsec would be a good
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

Test Run
---------

We ran the tracing tool multiple times with inactivity counts ranging
from 5 to 100.

Note: Test data output for `trace` is in file `trace.out.txt`.

Conclusions
-----------

Based on the data observed, we got an average timer interval of 3ms.

The average time to handle a timer interrupt was about 0.005 msec.

While testing in `runlevel 1` the program didn't seem to be interrupted
switched out much since there weren't many background processes running
but while running in usrlevel with a gui and video playing in the
background, it was frequently being switched out by the scheduler.

Since we didn't output anything to the console while collecting data,
there are no kernel level calls being made. The only interrupts in the
program are thus the timer interrupts by the scheduler and they
constitute about 0.0016% of the total process time. Had we been doing
other IO and disk based activity this time would've certainly been
higher.

Context Switches
================

Setup
-----

The setup was the same as the last experiment except that for this one
we only ran the processes in `Runlevel 3` to get as little measuremet
bias as possible.

Experiment
-----------------

For measuring the context switch time between processes we first had the idea
of modifying our existing trace utility to output the global TSC value rather
than just the difference since the start of the process. This way we could
run multiple processes on a system and trace their activities and merge it
all together into a single ordering of events from which we can tell when
a process was switched out with another.

To do this, we initially had our trace process run, determine the CPU
frequency and threshold then fork into 50 children with each of them
vying for CPU time. For measuring context switches, we lowered the
threshold from 1000ns to 10ns as that would be closer to how long it
takes for a context switch (as referenced in Jeff Dean's talk). The problem
with this approach was that by the time the 50th child started up, the first
one would've finished its trace and thus there was very little contention 
between the child processes.

To increase contention between the children, we instead used a POSIX message
queue to have all children start up and wait for the parent to give a go 
signal so that they can start their trace. This helped increase context
switching activity on the system and we got some meaningful data.


Test Run
--------

We ran the test with 20 child processes and 25 inactive period 
measurements per process.

NOTE: The trace_context output is in the file tc.data.txt

Conclusion
----------

A time slice is roughly about 5 msec.

Yes, time slice goes down as number of processes goes up but not by a lot.

Definitely! Didn't realize that the CPU was so fast, a 100 ns to switch
between processes.

As far as we were taught in 369, time slices were mostly allocated in round
robin manner but over here scheduling policies and system load determine
which thread gets a slice and which one doesn't, it isn't a fair game as
made out to be in 369.
