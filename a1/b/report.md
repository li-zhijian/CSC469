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
  `bzip2`        77.59             78.86             0.983
  `lbm`          120.12            121.38            0.989
  `perlbench`    36.23             35.95             1.007

  : Baseline with empty environments

As shown in the table, with an empty environment, only the `perlbench`
test showed performance improvement on going from O2 to O3.

With this baseline to compare against, we ran each experiment with
increasing environment sizes.
