% CSC469 Part B
% Abhinav Gupta (998585963), Zeeshan Qureshi (997108954)
% 11 Feb 2013

<!-- vim:set spell tw=72: -->

Experiment
==========

All experiments were executed on CDF machines. We originally attempted
the experiment on an Ubuntu virtual machine but it turned out that
`perf` could not access the low level information it needed inside a VM.

We first ran both versions of all 3 test programs on the sample data
with an empty environment to get a baseline. The total execution time
for each test program version on all the input was:

  Test Program   Total Time (O2)   Total Time (O3)   Improvement (O2/O3)
  -------------- ----------------- ----------------- ---------------------
  `bzip2`        66.70             69.09             0.965
  `lbm`          204.50            204.08            1.002
  `perlbench`    29.81             29.68             1.004

  : Baseline with empty environments

As shown in the table, `bzip2` actually had a performance degradation
going from O2 to O3.

With this baseline to compare against, we began running each experiment
with varying environment sizes.

A major automation issue encountered here was that if `perf` was running
inside a "piped environment", that is, if the `perf` command was a level
or two deep inside a bash pipe, it failed with the error, `Failed
opening logfd: Invalid argument`. 
