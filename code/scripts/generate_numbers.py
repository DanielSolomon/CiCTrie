import numpy as np
import random
import struct
import sys

biggest_fmt = '<III'
max_size    = struct.calcsize(biggest_fmt)

class ActionType(object):
    INSERT  = 0
    LOOKUP  = 1
    REMOVE  = 2

class Insert(object):
    
    fmt     = '<III'
    size    = struct.calcsize(fmt)
    
    def __init__(self, key, value):
        self.key    = key
        self.value  = value

    def pack(self, pad):
        return struct.pack(self.fmt + 'x' * (pad - self.size), ActionType.INSERT, self.key, self.value)

class Lookup(object):
    
    fmt     = '<II'
    size    = struct.calcsize(fmt)
    
    def __init__(self, key):
        self.key    = key

    def pack(self, pad):
        return struct.pack(self.fmt + 'x' * (pad - self.size), ActionType.LOOKUP, self.key)

class Remove(object):
    
    fmt     = '<II'
    size    = struct.calcsize(fmt)
    
    def __init__(self, key):
        self.key    = key

    def pack(self, pad):
        return struct.pack(self.fmt + 'x' * (pad - self.size), ActionType.REMOVE, self.key)

def generate_inserts(range_, num):
    integers    = np.random.randint(0, range_, 2 * num)
    data        = struct.pack('<I' + 'I' * (2 * num), num, *integers)
    return data, integers[::2]

def generate_lookups(set_, num):
    np.random.shuffle(set_)
    data        = struct.pack('<I' + 'I' * num, num, *set_)
    return data, set_

def generate_removes(set_, num):
    np.random.shuffle(set_)
    data        = struct.pack('<I' + 'I' * num, num, *set_)
    return data, set_

def main(range_, num, i_part=None, l_part=None, r_part=None):
    random.seed(0)
    if i_part is None:
        data, integers = generate_inserts(range_, num)
        with open('inserts_sample.bin', 'wb') as writer:
            writer.write(data)
        with open('lookups_sample.bin', 'wb') as writer:
            writer.write(generate_lookups(integers, num)[0])
        with open('removes_sample.bin', 'wb') as writer:
            writer.write(generate_removes(integers, num)[0])
        return

    i_count, l_count, r_count = (num * i_part) / 100, (num * l_part) / 100, (num * r_part) / 100
    
    i_keys  = np.random.randint(0, range_, i_count)
    i_vals  = np.random.randint(0, range_, i_count)
    np.random.shuffle(i_keys)
    l_keys  = i_keys[:l_count]
    np.random.shuffle(i_keys)
    r_keys  = i_keys[:r_count]

    actions = []
    actions.extend([Insert(key, val) for key, val in zip(i_keys, i_vals)])
    actions.extend([Lookup(key) for key in l_keys])
    actions.extend([Remove(key) for key in r_keys])
    random.shuffle(actions)

    with open('{}_{}_{}_sample.bin'.format(i_part, l_part, r_part), 'wb') as writer:
        writer.write(struct.pack('<I', len(actions)))
        for action in actions:
            writer.write(action.pack(max_size))

if __name__ == '__main__':
    if len(sys.argv) not in (3, 6):
        print('Usage: {} <range> <count> [<insert-partition> <lookup-partition> <remove-partition>]'.format(sys.argv[0]))
        sys.exit(1)
    args = [int(arg) for arg in sys.argv[1:]]
    if len(sys.argv) > 3 and sum(args[-3:]) != 100:
        print('partitioning must be summed up to 100: {} + {} + {} = {total}'.format(*args[-3:], total=sum(args[-3:])))
        sys.exit(1)
    main(*args)
