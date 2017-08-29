import collections
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy
import os
import re
import sys

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

def plot_graph(c1, c2, title):
    if c1 and c2:
        plot(
            xs          = [[x.threads for x in s], [x.threads for x in c]],
            ys          = [[x.avg for x in s], [x.avg for x in c]],
            colors      = ['bs-', 'rs-'],
            x_label     = '#threads', 
            y_label     = 'nano seconds', 
            title       = title,
            func_labels = ['6 hp', '5 hp'],
            save        = '{}.png'.format(title),
        )

def main(subtitle, c_1_path, c_2_path):
    c_i_1, c_l_1, c_r_1, c_a_1 = [postprocess_dict(x) if x else [] for x in c_to_dicts(c_1_path)]
    c_i_2, c_l_2, c_r_2, c_a_2 = [postprocess_dict(x) if x else [] for x in c_to_dicts(c_2_path)]
    plot_graph(c_i_1, c_i_2, 'insert ({})'.format(subtitle))
    plot_graph(c_l_1, c_l_2, 'lookup ({})'.format(subtitle))
    plot_graph(c_r_1, c_r_2, 'remove ({})'.format(subtitle))
    plot_graph(c_a_1, c_a_2, 'action ({})'.format(subtitle))

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print('Usage: {} <subtitle> <c-6-hp-dir> <c-5-hp-dir>'.format(sys.argv[0]))
        sys.exit(1)
    main(*sys.argv[1:])
