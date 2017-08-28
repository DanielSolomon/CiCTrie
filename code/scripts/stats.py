import collections
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy
import os
import re
import sys

SCALA_THREAD_REGEX = re.compile('(\d+) threads')
SCALA_INSERT_REGEX = re.compile('insert in (\d+)')
SCALA_LOOKUP_REGEX = re.compile('lookup in (\d+)')
SCALA_REMOVE_REGEX = re.compile('remove in (\d+)')
SCALA_ACTION_REGEX = re.compile('action in (\d+)')

C_INSERT_REGEX = re.compile('Insert took (\d+)')
C_LOOKUP_REGEX = re.compile('Lookup took (\d+)')
C_REMOVE_REGEX = re.compile('Remove took (\d+)')
C_ACTION_REGEX = re.compile('Action took (\d+)')

Result = collections.namedtuple('Result', 'threads avg var min max')

def plot(xs, ys, colors, x_label, y_label, title, func_labels=None, x_scale=None, legend_location=None, save=None):
    f, ax = matplotlib.pyplot.subplots(1)
    plots = []
    for i, params in enumerate(zip(xs, ys, colors)):
        x, y, color = params
        if func_labels is not None:
            plots.append(ax.plot(x, y, color, label=func_labels[i]))
        else:
            ax.plot(x, y, color)
    #ax.set_ylim(ymin=max(0, min([x for y in ys for x in y]) * 0.9), ymax=min(1, max([x for y in ys for x in y]) * 1.1))
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    if x_scale is not None:
        ax.set_xscale(x_scale)
    if plots:
        legend_location = legend_location if legend_location is not None else 'best'
        ax.legend([plot for plot, in plots], [plot.get_label() for plot, in plots], loc=legend_location)
    f.suptitle(title, fontsize=14, fontweight='bold')
    if save is not None:
        matplotlib.pyplot.savefig(save)
    else:
        matplotlib.pyplot.show()
    matplotlib.pyplot.close(f)

def c_to_dicts(path):
    insert_d = collections.defaultdict(list)
    lookup_d = collections.defaultdict(list)
    remove_d = collections.defaultdict(list)
    action_d = collections.defaultdict(list)

    for root, dirs, files in os.walk(path):
        if dirs:
            continue
        thread_num = int(os.path.basename(root))
        for f in files:
            with open(os.path.join(root, f)) as reader:
                data = reader.read()
            m = C_INSERT_REGEX.search(data)
            if m:
                insert_d[thread_num].append(int(m.group(1)))
            m = C_LOOKUP_REGEX.search(data)
            if m:
                lookup_d[thread_num].append(int(m.group(1)))
            m = C_REMOVE_REGEX.search(data)
            if m:
                remove_d[thread_num].append(int(m.group(1)))
            m = C_ACTION_REGEX.search(data)
            if m:
                action_d[thread_num].append(int(m.group(1)))

    return insert_d, lookup_d, remove_d, action_d

def scala_to_dicts(path):
    insert_d = collections.defaultdict(list)
    lookup_d = collections.defaultdict(list)
    remove_d = collections.defaultdict(list)
    action_d = collections.defaultdict(list)

    with open(path) as reader:
        lines = reader.read().splitlines()

    thread_num = 0
    for line in lines:
        m = SCALA_THREAD_REGEX.search(line)
        if m:
            thread_num = int(m.group(1))
            continue
        m = SCALA_INSERT_REGEX.search(line)
        if m:
            insert_d[thread_num].append(int(m.group(1)))
            continue
        m = SCALA_LOOKUP_REGEX.search(line)
        if m:
            lookup_d[thread_num].append(int(m.group(1)))
            continue
        m = SCALA_REMOVE_REGEX.search(line)
        if m:
            remove_d[thread_num].append(int(m.group(1)))
            continue
        m = SCALA_ACTION_REGEX.search(line)
        if m:
            action_d[thread_num].append(int(m.group(1)))
            continue

    return insert_d, lookup_d, remove_d, action_d

def postprocess_dict(d):
    l = []
    for threads, results in sorted(d.iteritems(), cmp=lambda x, y: x[0] - y[0]):
        avg     = numpy.average(results)
        var     = numpy.var(results)
        min_    = min(results)
        max_    = max(results)
        l.append(Result(threads, avg, var, min_, max_))
        print('#threads: {} var: {} min: {} max: {}'.format(threads, var, min_, max_))
    return l

def plot_graph(s, c, title):
    if s and c:
        plot(
            xs          = [[x.threads for x in s], [x.threads for x in c]],
            ys          = [[x.avg for x in s], [x.avg for x in c]],
            colors      = ['bs-', 'rs-'],
            x_label     = '#threads', 
            y_label     = 'nano seconds', 
            title       = title,
            func_labels = ['scala', 'c'],
            save        = '{}.png'.format(title),
        )

def main(scala_path, c_path):
    print('scala')
    scala_i, scala_l, scala_r, scala_a  = [postprocess_dict(x) if x else [] for x in scala_to_dicts(scala_path)]
    print('c')
    c_i, c_l, c_r, c_a                  = [postprocess_dict(x) if x else [] for x in c_to_dicts(c_path)]
    plot_graph(scala_i, c_i, 'insert')
    plot_graph(scala_l, c_l, 'lookup')
    plot_graph(scala_r, c_r, 'remove')
    plot_graph(scala_a, c_a, 'action')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: {} <scala-file> <c-dir>'.format(sys.argv[0]))
        sys.exit(1)
    main(*sys.argv[1:])
