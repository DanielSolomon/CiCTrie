import struct
import random
import sys

def generate_inserts(range_, num):
    integers    = [random.randint(0, range_) for _ in xrange(2 * num)]
    data        = struct.pack('<I' + 'I' * (2 * num), num, *integers)
    return data, integers

def generate_lookups(set_, num):
    #integers    = [random.choice(set_) for _ in xrange(num)]
    integers    = [set_[i*2] for i in xrange(num)]
    data        = struct.pack('<I' + 'I' * num, num, *integers)
    return data, integers

def generate_removes(set_, num):
    #integers    = [random.choice(set_) for _ in xrange(num)]
    integers    = [set_[i*2] for i in xrange(num)]
    data        = struct.pack('<I' + 'I' * num, num, *integers)
    return data, integers

def main(range_, num):
    random.seed(0)
    data, integers = generate_inserts(range_, num)
    with open('inserts_sample.bin', 'wb') as writer:
        writer.write(data)
    with open('lookups_sample.bin', 'wb') as writer:
        writer.write(generate_lookups(integers, num)[0])
    with open('removes_sample.bin', 'wb') as writer:
        writer.write(generate_removes(integers, num)[0])

if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
