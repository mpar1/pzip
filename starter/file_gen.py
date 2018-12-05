#!/usr/bin/env python3

import random
import os
import os.path
import string
import sys
from argparse import ArgumentParser

char_set_dict = {
    'alpha': string.ascii_letters,
    'alpha_lower': string.ascii_lowercase,
    'alpha_upper': string.ascii_uppercase,
    'alpha_num': string.ascii_letters + string.digits,
    'num': string.digits,
    'hex_chars': string.hexdigits,
    'punct': string.punctuation,
    'whitespace': ' \n\t',
    'full': string.printable,
    'binary': 'binary'
}

distribution_list = ['gauss', 'uniform']

def char_generator(charset):
    if charset != 'binary':
        chars = char_set_dict[charset]
        while(True):
            yield random.choice(chars)
    else:
        while(True):
            yield random.getrandbits(8)

def run_length_generator(average_run, std, distribution):
    if distribution == 'gauss':
        while(True):
            yield round(random.gauss(average_run, std))
    elif distribution == 'uniform':
        while(True):
            yield round(random.uniform(1, average_run))

def gen_file(outfile, charset, average_run, std, distribution,
             file_len):
    c_gen = char_generator(charset)
    r_gen = run_length_generator(average_run,std,distribution)

    with open(outfile, 'wb') as f:
        written_bytes = 0
        while(written_bytes < file_len):
            character = next(c_gen)
            run_len = next(r_gen)
            if written_bytes + run_len > file_len:
                run_len = file_len - written_bytes
            written_bytes += run_len
            if charset == 'binary':
                f.write(character.to_bytes(1, 'little', signed=False)* run_len)
            else:
                f.write(bytes(character * run_len, 'utf-8'))
def main():
    parser = ArgumentParser()
    parser.add_argument('-c', '--charset', dest='charset',
                        type=str, default='alpha',
                        help=("The charset to use. "
                              "Valid options are: {}".format(list(char_set_dict.keys()))))
    parser.add_argument('-l', '--length', dest='file_len',
                        type=int, default=(10 ** 4),
                        help="Length of the output file in characters")
    parser.add_argument('-a', '--avgrun', dest='avg_run',
                        type=int, default=5,
                        help="Average run length")
    parser.add_argument('-d', '--distribution', dest='distribution',
                        type=str, default="gauss",
                        help="Distribution to draw lengths from. Valid options: {}".format(distribution_list))
    parser.add_argument('-o', '--output', dest='outfile', type=str)
    parser.add_argument('-mu', '--mu', dest='std', type=int, default=1,
                        help="The standard deviation")
    parser.add_argument('-s', '--seed', dest='seed', type=int,
                         default=(-1))

    args = parser.parse_args()

    if args.distribution not in distribution_list:
        parser.print_help()
        exit(0)

    if args.charset not in char_set_dict.keys():
        parser.print_help()
        exit(0)

    if args.outfile is None:
        print("Missing output file")
        parser.print_help()
        exit(1)

    if args.seed != -1:
        random.seed(a=args.seed)

    gen_file(args.outfile, args.charset, args.avg_run,
                args.std, args.distribution,args.file_len)

    print('Wrote to \'{}\''.format(args.outfile))

if __name__ == '__main__': main()
