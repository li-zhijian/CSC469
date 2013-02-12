import re
import shlex
import argparse
import operator
import subprocess
from os import path
from itertools import groupby

TRACE = path.abspath("trace_context")
TITLE = "Active and Inactive Periods"
BAR_HEIGHT = 1
BAR_BOTTOM = 0.1
FIRST = operator.itemgetter(0)
THIRD = operator.itemgetter(2)

ARG = argparse.ArgumentParser()
ARG.add_argument('-n', metavar='NUM', type=int, default=25, dest='n')
ARG.add_argument('-t', metavar='TITLE', type=str, default=TITLE, dest='title')
ARG.add_argument('output', metavar='OUTPUT', type=str)


def runcmd(args, stdin=None):
    """
    Run the command specified by the given argument. `args` can be a string or
    a list specifying the command to execute and its arguments.
    """
    if type(args) is str:
        args = shlex.split(args)
    args = map(str, args)
    proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                            stdin=stdin)
    if stdin is None:
        return proc.stdout.read()
    else:
        return proc


def flipflop(it, state=True):
    """An generator wrapper that flips between True and False."""
    for i in it:
        yield (state, i)
        state = not state


def trace_context(n):
    """
    A generator that runs trace_context with `n` and yields tuples in the form
    `(pid, start, duration)`.
    """
    # Drop the first four lines: the clock speed, the threshold, and the first
    # active/inactive pair.
    for line in runcmd([TRACE, n]).splitlines()[2:]:
        if len(line.strip()) is 0:
            continue
        m = re.search(r'^PID: (\d+).*start at (\d+).*\(([\d\.]+) ms\)', line)
        if not m:
            print line
        yield (int(m.group(1)), int(m.group(2)), float(m.group(3)))


def ireduce(f, it):
    """
    If `it` is a,b,c,..., this yields, a, f(a, b), f(f(a, b), c), ...
    """
    acc = it.next()
    yield acc
    for x in it:
        acc = f(acc, x)
        yield acc


def main():
    args = ARG.parse_args()
    n = args.n

    for (pid, it) in groupby(sorted(trace_context(n), key=THIRD), FIRST):
        print pid, list(it)


if __name__ == '__main__':
    main()
