import re
import shlex
import argparse
import subprocess
from os import path
from operator import add
from textwrap import dedent

TRACE = path.abspath("trace")
TITLE = "Active and Inactive Periods"
BAR_HEIGHT = 1
BAR_BOTTOM = 0.1

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


def trace(n):
    """
    A generator that runs trace with the given argument and yields floats that
    represent period lengths. The first represents active, the next inactive,
    and so on.
    """
    # Drop the first four lines: the clock speed, the threshold, and the first
    # active/inactive pair.
    for line in runcmd([TRACE, n]).splitlines()[4:]:
        if len(line.strip()) is 0:
            continue
        val = re.search(r'\(([\d\.]+) ms\) *$', line).group(1)
        yield float(val)


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
    gp = runcmd('gnuplot', stdin=subprocess.PIPE)

    scripthead = """\
            set title "%(title)s"
            set xlabel "Time (ms)"
            set nokey
            set noytics
            set term postscript eps 10
            set size 0.45, 0.35
            set output "%(output)s"
            """ % {'title': args.title, 'output': args.output}
    gp.stdin.write(dedent(scripthead))

    obj = 1
    start = 0
    maxend = 0
    for (is_active, end) in flipflop(ireduce(add, trace(n))):
        props = dict(obj=obj, x1=start, y1=BAR_BOTTOM,
                     x2=end, y2=BAR_BOTTOM + BAR_HEIGHT,
                     color='blue' if is_active else 'red')
        draw = ('set object %(obj)d rect from %(x1)f, %(y1)f to '
                '%(x2)f, %(y2)f, 2 fc rgb "%(color)s" fs solid '
                '\n') % props
        gp.stdin.write(draw)

        obj += 1
        start = end
        maxend = max(end, maxend)

    props = dict(maxend=maxend, height=BAR_BOTTOM + BAR_HEIGHT)
    scriptfoot = ('plot [0:%(maxend)d] [0:%(height)d] '
                  '0 title "Active", '
                  '0 title "Inactive"'
                  '\n') % props
    gp.stdin.write(dedent(scriptfoot))
    gp.stdin.close()

if __name__ == '__main__':
    main()
