#!/usr/bin/env python3

import os, re
from prjxray import util


def noprefix(tag):
    n = tag.find('.')
    assert n > 0
    return tag[n + 1:]


def getprefix(tag):
    n = tag.find('.')
    assert n > 0
    return tag[0:n]


def load_pipfile(pipfile, verbose=False):
    '''Returns a set of tags containing real tile type prefixes (ex: INT_L)'''
    todos = set()
    tile_type = None
    with open(pipfile, "r") as f:
        # INT_L.WW2BEG0.SR1BEG_S0
        for line in f:
            tag = line.strip()
            prefix_line = getprefix(tag)
            if tile_type is None:
                tile_type = prefix_line
            else:
                assert tile_type == prefix_line
            todos.add(tag)
    return todos, tile_type


def maketodo(
        pipfile, dbfile, intre, exclude_re=None, not_endswith=None,
        verbose=False):
    '''
    db files start with INT., but pipfile lines start with INT_L
    Normalize by removing before the first dot
    050-intpips doesn't care about contents, but most fuzzers use the tile type prefix
    '''

    todos, tile_type = load_pipfile(pipfile, verbose=verbose)
    verbose and print('%s: %u entries' % (pipfile, len(todos)))
    verbose and print("pipfile todo sample: %s" % list(todos)[0])

    if 0 and verbose:
        print("TODOs")
        for todo in sorted(list(todos)):
            print('  %s' % todo)

    verbose and print('Pre db %s: %u entries' % (dbfile, len(todos)))
    # Allow against empty db
    if os.path.exists(dbfile):
        verbose and print("Loading %s" % dbfile)
        with open(dbfile, "r") as f:
            # INT.BYP_ALT0.BYP_BOUNCE_N3_3 !22_07 !23_07 !25_07 21_07 24_07
            for line in f:
                tag, _bits, mode = util.parse_db_line(line.strip())
                # Only count resolved entries
                if mode:
                    continue
                # INT.BLAH => INT_L.BLAH
                tag = tile_type + '.' + noprefix(tag)

                # bipips works on a subset
                if tag in todos:
                    todos.remove(tag)
                else:
                    verbose and print(
                        "WARNING: couldnt remove %s (line %s)" % (tag, line))
    else:
        verbose and print("WARNING: dbfile doesnt exist: %s" % dbfile)
    verbose and print('Post db %s: %u entries' % (dbfile, len(todos)))
    drops = 0
    lines = 0
    for line in todos:
        include = re.match(intre, line) is not None

        if include and not_endswith is not None:
            include = not line.endswith(not_endswith)

        if include and exclude_re is not None:
            include = re.match(exclude_re, line) is None

        if include:
            print(line)
        else:
            drops += 1
        lines += 1
    verbose and print('Print %u entries w/ %u drops' % (lines, drops))


def run(
        build_dir,
        db_dir,
        pip_dir,
        intre,
        sides,
        l,
        r,
        pip_type,
        seg_type,
        exclude_re=None,
        not_endswith=None,
        verbose=False):
    if db_dir is None:
        db_dir = "%s/%s" % (
            os.getenv("XRAY_DATABASE_DIR"), os.getenv("XRAY_DATABASE"))

    if pip_dir is None:
        pip_dir = "%s/piplist/build/%s" % (
            os.getenv("XRAY_FUZZERS_DIR"), pip_type)

    assert intre, "RE is required"

    for side in sides:
        if side == "l" and not l:
            continue

        if side == "r" and not r:
            continue

        if side != "":
            side = "_" + side

        maketodo(
            "%s/%s%s.txt" % (pip_dir, pip_type, side),
            "%s/segbits_%s%s.db" % (db_dir, seg_type, side),
            intre,
            exclude_re=exclude_re,
            not_endswith=not_endswith,
            verbose=verbose)


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Print list of known but unsolved PIPs")

    parser.add_argument('--verbose', action='store_true', help='')
    parser.add_argument('--build-dir', default="build", help='')
    parser.add_argument('--db-dir', default=None, help='')
    parser.add_argument('--pip-dir', default=None, help='')
    parser.add_argument('--re', required=True, help='')
    parser.add_argument('--exclude-re', required=False, default=None, help='')
    parser.add_argument('--pip-type', default="pips_int", help='')
    parser.add_argument('--seg-type', default="int", help='')
    parser.add_argument('--sides', default="l,r", help='')
    util.add_bool_arg(parser, '--l', default=True, help='')
    util.add_bool_arg(parser, '--r', default=True, help='')
    parser.add_argument(
        '--not-endswith', help='Drop lines if they end with this')
    args = parser.parse_args()

    run(
        build_dir=args.build_dir,
        db_dir=args.db_dir,
        pip_dir=args.pip_dir,
        intre=args.re,
        exclude_re=args.exclude_re,
        sides=args.sides.split(','),
        l=args.l,
        r=args.r,
        pip_type=args.pip_type,
        seg_type=args.seg_type,
        not_endswith=args.not_endswith,
        verbose=args.verbose)


if __name__ == '__main__':
    main()
