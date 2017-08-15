import struct
import random
import sys


def main(range_, num):
    # TODO seed.
    integers    = [random.randint(0, range_) for x in xrange(2 * num)]
    data        = struct.pack('<I' + 'I' * (2 * num), num, *integers)
    with open('sample.bin', 'wb') as writer:
        writer.write(data)

if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
