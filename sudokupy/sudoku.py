#! /usr/bin/env python

import sys
import sudokucl

for arg in sys.argv[1:]:
    G = sudokucl.sgrid()
    try:
        G.loadfile(arg)
        if G.solve():
            print("Solution of", arg, "complete")
            print()
            G.prin()
            print()
        else:
            print("Did not solve", arg)
            print()
            G.prin()
    except  FileNotFoundError:
        print("Could not open", arg, file=sys.stderr)
    except  sudokucl.problem as e:
        print("No solution for", arg, e.arg[0])

    