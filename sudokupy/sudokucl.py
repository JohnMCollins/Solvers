"""Classes for Sudoku solvers"""

import re
import sys

class problem(Exception):
    """Throw this if we have problems"""
    pass

class cell:
    """Representation of Cell"""
    
    def __init__(self, row, col):
        self.contents = set(range(1,10))
        self.possibles = 9
        self.known = False
        self.row = row
        self.col = col
        self.rc = (row, col)
        self.mg = (row//3, col//3)
        
    def set_known(self, value):
        """Once we know a value, set it"""
        self.contents = value
        self.possibles = 1
        self.known = True
    
    def pr(self):
        """Get string reprsentation"""
        if self.known:
            return f'{self.contents:>11d}'
        l = sorted(list(self.contents))
        retstr = "["
        for p in l:
            retstr += chr(ord('0')+p)
        retstr += ']'
        return f'{retstr:>11s}'
    
    def pp(self):
        """Just output single character either 1 to 9 or ."""
        if self.known:
            print(f"{self.contents}", end='')
        else:
            print(".", end='')

class sgrid:
    """Representation of Sudoku grid"""

    def __init__(self):
        
        self.complete = False
        self.layout = []
        for row in range(0,9):
            icols = []
            for col in range(0,9):
                icols.append(cell(row+1, col+1))
            self.layout.append(icols)
        self.setup_rcm()
    
    def setup_rcm(self):
        """Set up rows/ cols and minigrids from layout.
        Separate this from init as we need to do it again for Samuri"""
        
        self.rows = []
        for row in range(0,9):
            cvals = []
            for col in self.layout[row]:
                cvals.append(col)
            self.rows.append(cvals)
        self.cols = []
        for col in range(0,9):
            rvals = []
            for row in range(0,9):
                rvals.append(self.layout[row][col])
            self.cols.append(rvals)
        self.minigrids = []
        for bigrow in range(0,9,3):
            for bigcol in range(0,9,3):
                mg = []
                for row in range(0,3):
                    for col in range(0,3):
                        mg.append(self.layout[bigrow+row][bigcol+col])
                self.minigrids.append(mg)
    
    def init_row(self, rownum, entries):
        """Set up row number from data"""
        
        if  len(entries) > 9:
            raise problem(f"Too long a line {len(entries)} in {entries} for row {rownum}")
        
        for n, ch in enumerate(entries):
            try:
                self.layout[rownum][n].set_known('123456789'.index(ch)+1)
            except ValueError:
                pass
    
    def check_dup(self, lst, descr):
        """Check for duplicate vale in thing described by descr in given list
           return false if OK, otherwise report and return true"""
        
        errors = 0
        vals = [item.contents for item in lst if item.known]
        if len(vals) > 1 and len(vals) != len(set(vals)):
            vdict = {}
            for v in set(vals):
                vdict[v] = []
            for item in lst:
                if item.known:
                    vdict[item.contents].append(item)
            for val, ocl in vdict.items():
                if  len(ocl) > 1:
                    print("Duplicate", val, "in", descr)
                    for item in ocl:
                        print("\tRow:", item.row, "col", item.col)
                    errors += 1
        return  errors != 0
           
    def check_data(self, extra = ""):
        """Check data is correct with no duplicates in rows, columns, minigrid"""
        
        errors = 0
        for row in self.rows:
            if self.check_dup(row, "row"+extra):
                errors += 1
        for col in self.cols:
            if self.check_dup(col, "col"+extra):
                errors += 1
        for mg in self.minigrids:
            if self.check_dup(mg, "minigrid"+extra):
                errors += 1
        if errors != 0:
            raise problem("Aborting due to errors")
        
    def loadfile(self, file):
        """Load problem file into layout"""
        with open(file, "rt") as fin:
            rownum = 0
            for line in fin:
                self.init_row(rownum, line.strip())
                rownum += 1
                if rownum > 9:
                    break
        self.check_data()

    def prin(self):
        """Print out current"""
        if self.complete:
            for row in self.rows:
                for col in row:
                    print(f"{col.contents}", end='')
                print()
        else:
            for row in self.rows:
                for col in row:
                    print(col.pr(), end=' ')
                print()

    def apply_known_comp(self, comp):
        """Apply known items in rows/cols/minigrids"""
        changes = 0
        for subcomp in comp:
            knownvals = []
            for item in subcomp:
                if  item.known:
                    knownvals.append(item.contents)
            for kv in knownvals:
                for item in subcomp:
                    if not item.known:
                        try:
                            item.contents.remove(kv)
                            print(f"Removed {kv} from {item.row},{item.col} giving", item.contents)
                            changes += 1
                            if len(item.contents) == 0:
                                raise problem("Problem here")
                        except KeyError:
                            pass
        return  changes

    def apply_known(self):
        """Adjust sets in as yet unknown celss to exclude known values"""
        return  self.apply_known_comp(self.rows) +\
                self.apply_known_comp(self.cols) +\
                self.apply_known_comp(self.minigrids)
    
    def count_possible(self):
        """Count how many are possible for each cell and note if blocked
        or any new resolved"""
        changes = 0
        unresolved = 0
        for row in self.rows:
            for item in row:
                if not item.known:
                    item.possibles = len(item.contents)
                    if item.possibles == 0:
                        raise problem(f"No soluction row {item.row} col {item.col}")
                    elif item.possibles == 1:
                        item.set_known(item.contents.pop())
                        changes += 1
                    else:
                        unresolved += 1
        if  unresolved == 0:
            self.complete = True
        return  changes
    
    def apply_onlyone_comp(self, comp):
        """Look for row/col/minigrid where only one place is possible and
        adjust. Return changes made"""
        changes = 0
        for subcomp in comp:
            dc = {}
            for n in range(1,10):
                dc[n] = []
            for item in subcomp:
                if item.known:
                    dc[item.contents].append(item)
                else:
                    for s in item.contents:
                        dc[s].append(item)
            for n, v in dc.items():
                if len(v) == 1:
                    c = v[0]
                    if  c.known:
                        continue
                    print(f"Setting cell row {c.row} col {c.col} to {n}", file=sys.stderr)
                    c.set_known(n)
                    changes += 1
        return  changes
    
    def apply_onlyone(self):
        """Look everywhere for just one value if possible and adjust"""
        return  self.apply_onlyone_comp(self.rows) +\
                self.apply_onlyone_comp(self.cols) +\
                self.apply_onlyone_comp(self.minigrids)
                
    def apply_pair_comp(self, comp):
        """Note situation where we've got 2 cells with a pair of identical contents and eliminate those from other cells"""
        changes = 0
        for subcomp in comp:
            pairs = set()
            for item in subcomp:
                if item.possibles == 2:
                    pairs.add(item)
            while  len(pairs) >= 2:
                nxt = pairs.pop()
                for p in pairs:
                    if nxt.contents == p.contents:
                        for item in subcomp:
                            if item.known or item == nxt or item == p:
                                continue
                            item.contents -= nxt.contents
                            if  len(item.contents) != item.possibles:
                                print("Item.contents now", item.contents, "possibles", item.possibles)
                                changes += 1
        return  changes > 0
    
    def apply_pair(self):
        """Look for situations where we've got pairs and can eliminae other cell contents"""
        if  self.apply_pair_comp(self.rows):
            return  True
        if  self.apply_pair_comp(self.cols):
            return  True
        if  self.apply_pair_comp(self.minigrids):
            return  True
        return  False
    
    def solve(self):
        """Try to generate solution"""
        
        while 1:
            while  self.apply_known() > 0  or  self.count_possible() > 0:
                pass
            if  self.complete:
                return  True
            if  self.apply_onlyone() != 0:
                while  self.apply_onlyone() != 0:
                    pass
                continue
            if  self.apply_pair():
                continue
            return  self.complete

class Samuri:
    """Samuri sudokus"""
    
    def __init__(self):
        self.topleft = sgrid()
        self.topright = sgrid()
        self.bottomleft = sgrid()
        self.bottomright = sgrid()
        self.middle = sgrid()
        self.complete = False
        
        # Now share corners of middle grid
        
        for row in range(0, 3):
            for col in range(0,3):
                self.middle.layout[row][col] = self.topleft.layout[row+6][col+6]
                self.middle.layout[row][col+6] = self.topright.layout[row+6][col]
                self.middle.layout[row+6][col] = self.bottomleft.layout[row][col+6]
                self.middle.layout[row+6][col+6] = self.bottomright.layout[row][col]
        
        # Redo this to reset overlaps
        
        self.middle.setup_rcm()
    
    def loadfile(self, file):
        """Load problem file into layout"""
        with open(file, "rt") as fin:
            rownum = 0
            for line in fin:
                line = line.strip()
                if  rownum < 6:
                    bits = line.split()
                    if  len(bits) != 2:
                        raise problem(f"Row {rownum} of {line} is not 2 pieces")
                    self.topleft.init_row(rownum, bits[0])
                    self.topright.init_row(rownum, bits[1])
                elif rownum < 9:
                    if len(line) < 16:
                        raise problem("Expecting row {rownum} to be at least 16 chars")
                    self.topleft.init_row(rownum, line[:9])
                    self.middle.init_row(rownum-6, line[6:15])
                    self.topright.init_row(rownum, line[12:])
                elif rownum < 12:
                    self.middle.init_row(rownum-6, line)
                elif rownum < 15:
                    if len(line) < 16:
                        raise problem("Expecting row {rownum} to be at least 16 chars")
                    self.bottomleft.init_row(rownum-12, line[:9])
                    self.middle.init_row(rownum-6, line[6:15])
                    self.bottomright.init_row(rownum-12, line[12:])
                else:
                    bits = line.split()
                    if  len(bits) != 2:
                        raise problem(f"Row {rownum} of {line} is not 2 pieces")
                    self.bottomleft.init_row(rownum-12, bits[0])
                    self.bottomright.init_row(rownum-12, bits[1])
                    
                rownum += 1
                if rownum >= 21:
                    break
        self.check_data()

    def check_data(self):
        """Check all data consistent"""
        self.topleft.check_data(" top left")
        self.topright.check_data(" top right")
        self.bottomleft.check_data(" bottom left")
        self.bottomright.check_data(" bottom right")
        self.middle.check_data(" middle")
    
    def prin(self):
        """Print out grid"""
        for rownum in range(0, 21):
            if rownum < 6:
                for col in self.topleft.rows[rownum]:
                    col.pp()
                print(" " * 3, end='')
                for col in self.topright.rows[rownum]:
                    col.pp()
            elif rownum < 9:
                for col in self.topleft.rows[rownum]:
                    col.pp()
                for col in self.middle.rows[rownum-6][3:6]:
                    col.pp()
                for col in self.topright.rows[rownum]:
                    col.pp()
            elif rownum < 12:
                print(" " * 6, end='')
                for col in self.middle.rows[rownum-6]:
                    col.pp()
            elif rownum < 15:
                for col in self.bottomleft.rows[rownum-12]:
                    col.pp()
                for col in self.middle.rows[rownum-6][3:6]:
                    col.pp()
                for col in self.bottomright.rows[rownum-12]:
                    col.pp()
            else:
                for col in self.bottomleft.rows[rownum-12]:
                    col.pp()
                print(" " * 3, end='')
                for col in self.bottomright.rows[rownum-12]:
                    col.pp()
            print()
    
    def solve(self):
        """Try to solve whole Samuri"""
        
        escape = 10
        
        while not self.complete:
            escape -= 1
            if  escape < 0:
                return  False
            if not self.topleft.complete:
                self.topleft.solve()
            if not self.topright.complete:
                self.topright.solve()
            if not self.bottomright.complete:
                self.bottomright.solve()
            if not self.bottomleft.complete:
                self.bottomleft.solve()
            if not self.middle.complete:
                self.middle.solve()
            self.complete = self.topleft.complete and self.topright.complete and self.middle.complete and self.bottomleft.complete and self.bottomright.complete

killercombs = (None, dict(), dict(), dict(), dict(), dict(), dict(), dict(), dict(), dict())

for bits in range(1, 512):
    comb = []
    for n in range(0, 9):
        if bits & (1 << n):
            comb.append(n+1)
    v = sum(comb)
    l = len(comb)
    if v not in killercombs[l]:
        killercombs[l][v] = []
    killercombs[l][v].append(set(comb))
    
reshape = re.compile('(\d+):([a-i1-9]+)', re.I)
rescoords = re.compile('([a-i][1-9])', re.I)

class Kshape:
    """Killer shape"""
    
    def __init__(self):
        self.length = 0
        self.total = 0
        self.possibles = []
        self.cells = []
    
    def init_shape(self, data):
        """Initialise shape from string"""
        if m := reshape.search(data):
            tot = int(m.group(1))
            if  tot <= 0 or tot > 45:
                raise problem(f"Total {tot} in data {data} should be between 1 and 45")
            self.total = tot
            pos =  m.group(2).upper()
            if len(pos) % 2 != 0:
                raise problem("Expecting coords to be multiple of 2")
            for coord in rescoords.split(pos):
                if len(coord) != 2:
                    continue
                print(f'coord is {coord}')
                self.cells.append(('ABCDEFGHI'.index(coord[0]),'123456789'.index(coord[1])))
        else:
            raise problem(f"Could not decode {data}")
        self.length = len(self.cells)
        if self.length == 0 or self.length > 9:
            raise problem(f'Length from {data} is wrong should be 1 to 9')
    
    def ready_shape(self, layout):
        """Now ready to set contents of shape appropriately"""
        combs = killercombs[self.length][self.total]
        if len(combs) == 0:
            raise problem(f'No combinations for length {self.length} total {self.total}')
        for c in combs:
            self.possibles.append(c.copy())
        self.cells = sorted(self.cells, key=lambda x: x[0]*10+x[1])
        newcells = []
        for r, c in self.cells:
            newcells.append(layout[r][c])
        self.cells = newcells
    
    def possibles_in(self):
        """Get all the values which can be in the shape"""
        r = set()
        for c in self.possibles:
            r |= c
        return  r
    
    def values_in(self):
        """Get all the values which are in the shape"""
        r = set()
        for c in self.cells:
            if c.known:
                r.add(c.contents)
            else:
                r |= c.contents
        return  r
    
    def same_comp(self, compname):
        """Return thing if shape is all in same row/col/minigrid"""
        compnum = getattr(self.cells[0], compname)
        for c in self.cells[1:]:
            if getattr(c, compname) != compnum:
                return None
        return  compnum
    
    def list_used_rc(self):
        """List r/c pair used by shape"""
        r = set()
        for c in self.cells:
            r.add(c.rc)
        return  r
    

class Killer(sgrid):
    """Representation of killer shape"""
    
    def __init__(self):
        super().__init__()
        self.shapes = []

    def loadfile(self, file):
        """Load problem file into layout"""
        with open(file, "rt") as fin:
            for line in fin:
                line = line.strip()
                if len(line) == 0:
                    continue
                newshape = Kshape()
                newshape.init_shape(line)
                self.shapes.append(newshape)
        self.check_data()
        for s in self.shapes:
            s.ready_shape(self.layout)
    
    def check_data(self):
        """Check consistency of input data"""
        had = [[False]*9 for r in range(0,9)]
        errors = 0
        total = 0
        for s in self.shapes:
            total += s.total
            for r, c in s.cells:
                if had[r][c]:
                    print(f'Duplicated entry {"abcdefghi"[r]}{c+1}', file=sys.stderr)
                    errors += 1
                else:
                    had[r][c] = True
        for r in range(0,9):
            for c in range(0,9):
                if not had[r][c]:
                    print(f'Omitted entry {"abcdefghi"[r]}{c+1}', file=sys.stderr)
                    errors += 1
        
        if total != 405:
            print(f"Expected total of shapes to be 405 not {total}", file=sys.stderr)
            errors += 1
            
        if errors != 0:
            raise problem("Aborting due to errors")
    
    def apply_shapes_initial(self):
        """First cut is to prune down the cell contents where possible to values in shapes"""
        
        for s in self.shapes:
            possvals = s.possibles_in()
            for c in s.cells:
                if not c.known:
                    c.contents &= possvals

    def remove_from_single_element(self):
        """Remove values for other items in rows/cols/minigrids where shape occup single row/col/minigrid"""
        for s in self.shapes:
            for cellcomp, gridcomp in (('row', 'rows'), ('col', 'cols'), ('mg', 'minigrids')):
                if num := s.same_comp(cellcomp):
                    shaperowscols = s.list_used_rc()
                    vals = s.values_in()
                    if len(vals) != s.length:
                        continue
                    for sublist in getattr(self, gridcomp):
                        for cl in sublist:
                            if not cl.known and cl.rc not in shaperowscols:
                                cl.contents -= vals
