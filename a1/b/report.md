% CSC469 Part B
% Abhinav Gupta (998585963), Zeeshan Qureshi (997108954)
% 11 Feb 2013

<!-- vim:set spell tw=72: -->

Environment
===========

We used [Vagrant][] to set up an isolated Ubuntu 10.04 virtual machine
with only 512 MB of RAM and a single CPU. Address Space Layout
Randomization was disabled on the virtual machine. The host machine for
the VM was running Mac OS X 10.7 with a Core 2 Duo CPU. All experiments
were executed under this set up.

  [Vagrant]: http://www.vagrantup.com/

Experiments
===========

We first ran both versions of all 3 test programs on the sample data
with an empty environment to get a baseline. The total execution time
for each test program version on all the input was:

  Test Program   Total User Time (O2)   Total User Time (O3)
  -------------- ---------------------- ----------------------
  `bzip2`        77.59                  78.86
  `lbm`          120.12                 121.38
  `perlbench`    36.23                  35.95

  : Total running times on the train data


