//
// Copyright (c) Xi Software Ltd. 1999.
//
// sokomatic.cc: created by John M Collins on Fri Dec 24 1999.
//----------------------------------------------------------------------
// $Header$
// $Log$
//----------------------------------------------------------------------
// C++ Version: 

#include <iostream.h>
#include <fstream.h>
#include <ctype.h>
#include <vector>
#include <string>

class	square  {
private:
	enum SQ  { NONE = 0, JEWEL = 0x1, TARGET = 0x40, WALL = 0x80,
		   NTROUGH = 0x2, STROUGH = 0x4, ETROUGH =0x8, WTROUGH = 0x10 };
	unsigned  char	datum;

public:
	square() { datum = NONE; }
	void	init_wall()  {  datum = WALL; }
	void	init_target()  {  datum = TARGET;  }
	void	init_jewel()  {  datum = JEWEL;  }
	void	init_targ_jewel() {  datum = SQ(TARGET|JEWEL);  }
	void	jewelon() { datum |= JEWEL; }
	void	jeweloff() { datum &= ~JEWEL; }
	bool	iswall() const { return  datum & WALL; }
	bool	istarget() const { return  datum & TARGET; }
	bool	hasjewel() const { return  datum & JEWEL; }
	bool	isoccupied() const { return  datum & (JEWEL|WALL); }
	bool	isempty() const { return  !isoccupied(); }
	bool	istrough() const  { return datum & SQ(NTROUGH|STROUGH|ETROUGH|WTROUGH);  }

	friend	void	findtroughs();
};

const	unsigned	MAXCOLROW = 64;

class	delta  {
private:
	short	mdx, mdy;	// One of these is always zero
public:
	delta(const int pdx, const int pdy)  : mdx(pdx), mdy(pdy)  { }
	int	dx() const  {  return  mdx; }
	int	dy() const  {  return  mdy; }
	delta	clockwise() const  {  return  mdy == 0? delta(0, mdx): delta(-mdy, 0);  }
	delta	anticlockwise() const  {  return  mdy == 0? delta(0, -mdx): delta(mdy, 0);  }
	delta	backward() const  {  return  delta(-mdx, -mdy);  }
};

class	coord  {
private:
	unsigned  comb_val;
public:
	coord()  { comb_val = 0xFFFFFFFF;  }
	coord(int xp, int yp)  { comb_val = (xp << 6) | yp;  }
	int	x() const  {  return comb_val >> 6;  }
	int	y() const  {  return comb_val & 63;  }
	unsigned  index() const  { return  comb_val;  }
	bool	operator == (const coord p) { return  comb_val == p.comb_val;  }
	bool	operator != (const coord p) { return  comb_val != p.comb_val;  }
	coord	operator + (const delta del) { return  coord(x()+del.dx(), y()+del.dy());  }
	delta	operator - (const coord b)  { return  delta(x()-b.x(), y()-b.y());  }
};

class	array  {
protected:
	square	area[MAXCOLROW*MAXCOLROW];
public:
	square	&sq(const coord c)  { return  area[c.index()];  }
	bool operator == (const array &p)  {  return  memcmp(area, p.area, sizeof(area)) == 0;  }
	bool operator != (const array &p)  {  return  memcmp(area, p.area, sizeof(area)) != 0;  }
};


class	arena : public array  {
private:
	int	n_jewels, n_jewelsleft;
public:
	void	initjewels(int j, int jl)  { n_jewels = j; n_jewelsleft = jl; }
	int	njewels() const { return  n_jewels; }
	int	nleft() const { return  n_jewelsleft; }
	void	incrleft() { n_jewelsleft++; }
	void	decrleft() { n_jewelsleft--; }
};

arena	workarea;

class	move  {
public:
	enum	movetype  { INVALID, WALK, PUSH, PUSHON, PUSHTO, PUSHOFF };
	movetype	typem;
	coord		from, to;
	move() { typem = INVALID; }
};

class	movelist  {
public:
	int	nvalid;
	move	list[4];	// There are at most 4 of course

	movelist() { nvalid = 0; }
	void	init(coord cf)  {
		list[0].from = list[1].from = list[2].from = list[3].from = cf;
	}
};

vector<move>	move_list, best_solution;
int	nsolutions = 0;

class	position  {
public:
	int	nmoves;
	coord	man;
	array	savedpos;
	bool 	operator == (position &);
	void	save_position(coord);
	bool	been_there();
	void	storehist();
};

bool	position::operator == (position &z)
{
	if  (nmoves < z.nmoves)
		return  false;
	return  man == z.man  &&  savedpos == z.savedpos;
}

void	position::save_position(coord me)
{
	nmoves = move_list.size();
	man = me;
	memcpy((void *) &savedpos, (void *) (array *) &workarea, sizeof(savedpos));
}

vector<position> position_history;

bool	position::been_there()
{
	for  (int i = 0;  i < position_history.size();  i++)
		if  (*this == position_history[i])
			return  true;
	return  false;
}

void	position::storehist()
{
	for  (int i = 0;  i < position_history.size();  i++)  {
		position  &ph = position_history[i];
		if  (man == ph.man  &&  savedpos == ph.savedpos)  {
			if  (nmoves < ph.nmoves)
				ph.nmoves = nmoves;
			return;
		}
	}
	position_history.push_back(*this);
}

bool	circles(coord pos)
{
	for  (int i = move_list.size()-1;  i >= 0;  i--)  {
		move	&m = move_list[i];
		if  (m.typem != move::WALK)
			return  false;
		if  (m.from == pos)
			return  true;
	}
	return  false;
}

bool	moveok(const delta del, move &res)
{
	coord	cpos = res.from;
	res.to = cpos + del;
	coord	cto = res.to;
	square	&resq = workarea.sq(cto);

	//  OK if the thing is emply. We check elsewhere that we visited it
	//  before without doing anything.
	
	if  (resq.isempty())  {
		//  Catch simple case of going back to where we came from.
		if  (circles(cpos))
			return  false;
		res.typem = move::WALK;
		return  true;
	}

	//  If the thing is a wall, we can't go there
	
	if  (resq.iswall())
		return  false;

	//  Otherwise we must be pushing a jewel
	//  We can't do that if the square beyond is blocked
	//  Also catch trough case

	coord	cbeyond = cto + del;
	square  &beyond = workarea.sq(cbeyond);

	if  (beyond.isoccupied() || beyond.istrough())
		return  false;
	
	// If pushing, classify according to whether we are just pushing,
	// pushing onto, or off a target

	if  (resq.istarget())
		res.typem = beyond.istarget()? move::PUSHON: move::PUSHOFF;
	else
		res.typem = beyond.istarget()? move::PUSHTO: move::PUSH;

	// Try to trap common case of running two adjacent to a wall or into a corner
	// which is never right except when it's on or to a target.
	// This won't catch pushing the thing to a target but clobbering one just against a
	// wall.

	if  (res.typem == move::PUSHON || res.typem == move::PUSHTO)
		return  true;

	coord	nxtbeyond = cbeyond + del;
	if  (!workarea.sq(nxtbeyond).iswall())
		return  true;
	if  (workarea.sq(nxtbeyond + del.clockwise()).iswall()  &&  workarea.sq(cbeyond + del.clockwise()).isoccupied())
		return  false;
	if  (workarea.sq(nxtbeyond + del.anticlockwise()).iswall()  &&  workarea.sq(cbeyond + del.anticlockwise()).isoccupied())
		return  false;
	return  true;
}

bool	listmoves(coord cpos, movelist &result)
{
	result.init(cpos);

	//  Try all 4 possible directions
	
	if  (moveok(delta(1, 0), result.list[result.nvalid]))
	     result.nvalid++;
	if  (moveok(delta(-1, 0), result.list[result.nvalid]))
	     result.nvalid++;
	if  (moveok(delta(0, 1), result.list[result.nvalid]))
	     result.nvalid++;
	if  (moveok(delta(0, -1), result.list[result.nvalid]))
	     result.nvalid++;
	return  result.nvalid > 0;
}

void	makemove(coord &pos, move &m)
{
	pos = m.to;
	switch  (m.typem)  {
	case  move::WALK:
		return;
	case  move::PUSHTO:
		workarea.decrleft();
		break;
	case  move::PUSHOFF:
		workarea.incrleft();
		break;
	}
	workarea.sq(pos).jeweloff();
	workarea.sq(pos + (m.to - m.from)).jewelon();
}

void	undomove(coord &pos, move &m)
{
	pos = m.from;
	switch  (m.typem)  {
	case  move::WALK:
		return;
	case  move::PUSHTO:
		workarea.incrleft();
		break;
	case  move::PUSHOFF:
		workarea.decrleft();
		break;
	}
	workarea.sq(m.to).jewelon();
	workarea.sq(m.to + (m.to - m.from)).jeweloff();
}

void	print_sol(vector<move> &sol)
{
	cout << "\nSolution: " << sol.size() << " moves\n";
	for  (int mcnt = 0;  mcnt < sol.size();  mcnt++)  {
		move	&mv = sol[mcnt];
		delta	del = mv.to - mv.from;
		char	p = del.dx()? ((del.dx() > 0)? 'e': 'w'): (del.dy() > 0)? 's': 'n';
		if  (mv.typem != move::WALK)
			p = toupper(p);
		cout << p;
		if  (mcnt % 74 == 73)
			cout << endl;
	}
	cout << endl;
}

void	trymove(move &m)
{
	coord	cpos;
	makemove(cpos, m);
	move_list.push_back(m);
	if  (workarea.nleft() == 0)  {
		print_sol(move_list);
		if  (nsolutions++ == 0  ||  move_list.size() < best_solution.size())
			best_solution = move_list;
	}
	else  {
		position  npos;
		npos.save_position(cpos);
		if  (!npos.been_there())  {
			npos.storehist();
			movelist  nxtp;
			if  (listmoves(cpos, nxtp))  {
				for  (int i = 0;  i < nxtp.nvalid;  i++)
					trymove(nxtp.list[i]);
			}
		}
	}
	move_list.pop_back();
	undomove(cpos, m);
}

void	findtroughs()
{
	//  Look for "South Troughs"

	for  (int sr = 1;  sr < MAXCOLROW-1;  sr++)  {
		for  (int sc = 1;  sc < MAXCOLROW-1;  sc++)  {
			square	&curr = workarea.sq(coord(sc, sr));
			if  (curr.iswall() || curr.istarget())
				continue;
			if  (!workarea.sq(coord(sc,sr+1)).iswall())
				continue;
			square  &prev = workarea.sq(coord(sc-1, sr));
			if  (prev.datum & square::STROUGH)  {
				curr.datum |= square::STROUGH;
				continue;
			}
			if  (!prev.iswall())
				continue;
			for  (int nsc = sc+1;  nsc < MAXCOLROW-1;  nsc++)  {
				square  &nxt = workarea.sq(coord(nsc, sr));
				if  (nxt.iswall())  {
					curr.datum |= square::STROUGH;
					goto  cont2s;
				}
				if  (nxt.istarget())
					goto  cont2s;
				if  (!workarea.sq(coord(nsc, sr+1)).iswall())
					goto  cont2s;
			}
		cont2s:
			;
		}
	}

	//  Ditto for "North Troughs"

	for  (int sr = MAXCOLROW-2;  sr > 0;  sr--)  {
		for  (int sc = 1;  sc < MAXCOLROW-1;  sc++)  {
			square	&curr = workarea.sq(coord(sc, sr));
			if  (curr.iswall() || curr.istarget())
				continue;
			if  (!workarea.sq(coord(sc,sr-1)).iswall())
				continue;
			square  &prev = workarea.sq(coord(sc-1, sr));
			if  (prev.datum & square::NTROUGH)  {
				curr.datum |= square::NTROUGH;
				continue;
			}
			if  (!prev.iswall())
				continue;
			for  (int nsc = sc+1;  nsc < MAXCOLROW-1;  nsc++)  {
				square  &nxt = workarea.sq(coord(nsc, sr));
				if  (nxt.iswall())  {
					curr.datum |= square::NTROUGH;
					goto  cont2n;
				}
				if  (nxt.istarget())
					goto  cont2n;
				if  (!workarea.sq(coord(nsc, sr-1)).iswall())
					goto  cont2n;
			}
		cont2n:
			;
		}
	}
					

	//  Ditto for "East Troughs"

	for  (int sc = 1;  sc < MAXCOLROW-1;  sc++)  {
		for  (int sr = 1;  sr < MAXCOLROW-1;  sr++)  {
			square	&curr = workarea.sq(coord(sc, sr));
			if  (curr.iswall() || curr.istarget())
				continue;
			if  (!workarea.sq(coord(sc+1,sr)).iswall())
				continue;
			square  &prev = workarea.sq(coord(sc, sr-1));
			if  (prev.datum & square::ETROUGH)  {
				curr.datum |= square::ETROUGH;
				continue;
			}
			if  (!prev.iswall())
				continue;
			for  (int nsr = sr+1;  nsr < MAXCOLROW-1;  nsr++)  {
				square  &nxt = workarea.sq(coord(sc, nsr));
				if  (nxt.iswall())  {
					curr.datum |= square::ETROUGH;
					goto  cont2e;
				}
				if  (nxt.istarget())
					goto  cont2e;
				if  (!workarea.sq(coord(sc+1, nsr)).iswall())
					goto  cont2e;
			}
		cont2e:
			;
		}
	}

	//  Finally for "West Troughs"

	for  (int sc = MAXCOLROW-2;  sc > 0;  sc--)  {
		for  (int sr = 1;  sr < MAXCOLROW-1;  sr++)  {
			square	&curr = workarea.sq(coord(sc, sr));
			if  (curr.iswall() || curr.istarget())
				continue;
			if  (!workarea.sq(coord(sc-1,sr)).iswall())
				continue;
			square  &prev = workarea.sq(coord(sc, sr-1));
			if  (prev.datum & square::WTROUGH)  {
				curr.datum |= square::WTROUGH;
				continue;
			}
			if  (!prev.iswall())
				continue;
			for  (int nsr = sr+1;  nsr < MAXCOLROW-1;  nsr++)  {
				square  &nxt = workarea.sq(coord(sc, nsr));
				if  (nxt.iswall())  {
					curr.datum |= square::WTROUGH;
					goto  cont2w;
				}
				if  (nxt.istarget())
					goto  cont2w;
				if  (!workarea.sq(coord(sc-1, nsr)).iswall())
					goto  cont2w;
			}
		cont2w:
			;
		}
	}

	for  (int sr = 0;  sr < MAXCOLROW;  sr++)  {
		int	had = 0;
		for  (int sc = 0;  sc < MAXCOLROW;  sc++)
			if  (workarea.sq(coord(sc, sr)).datum != square::NONE)  {
			     had++;
			     break;
			}
		if  (!had)
			break;
		for  (int sc = 0; sc < MAXCOLROW; sc++)  {
			square  &curr = workarea.sq(coord(sc, sr));
			if  (curr.iswall())
				cerr << '#';
			else  if  (curr.istrough())
				cerr << 'T';
			else
				cerr << ' ';
		}
		cerr << endl;
	}
}

void	solve(coord initial)
{
	position ipos;
	ipos.save_position(initial);	
	position_history.push_back(ipos);
	movelist  nxtp;
	if  (!listmoves(initial, nxtp))  {
		cerr << "There don\'t seem to be any starting moves\n";
		exit(10);
	}
	for  (int i = 0;  i < nxtp.nvalid;  i++)
		trymove(nxtp.list[i]);
	position_history.pop_back();

	if  (nsolutions <= 0)  {
		cerr << "Found no solutions!\n";
		exit(11);
	}
	cout << "Found " << nsolutions << " solution(s)\nLeast moves (" << best_solution.size() << ")\n";
	print_sol(best_solution);
}

bool	read_lin(ifstream &ifl, string &buf)
{
	int col = 1;
	
	for  (;;)  {
		int	inch;
		inch = ifl.get();
		switch  (inch)  {
		case  '\r':
			continue;
		case  EOF:
			return  col > 1;
		case  '\n':
			return  true;
		case  '\t':
			do  {
				buf += ' ';
				col++;
			}  while  (col & 7);
			continue;
		case  '*':
		case  '$':
		case  ' ':
		case  '#':
		case  '@':
		case  '.':
		case  '+':
			buf += char(inch);
			col++;
			continue;
		default:
			return  false;
		}
	}
}

void	readpuzz(char *fn, coord &man)
{
	ifstream  ifl;
	ifl.open(fn);
	if  (!ifl.good())  {
		cerr << "Cannot open " << fn << endl;
		exit(3);
	}
	vector<string> lines;
	int	maxl = 0;
	for  (;;)  {
		string	inl;
		if  (!read_lin(ifl, inl))
			break;
		lines.push_back(inl);
		if  (inl.length() > maxl)
			maxl = inl.length();
	}
	ifl.close();
	if  (lines.size() <= 0)  {
		cerr << "Sorry did not read any data\n";
		exit(4);
	}

	if  (maxl >= MAXCOLROW  ||  lines.size() >= MAXCOLROW)  {
		cerr << "Sorry dimensions exceed limit\n";
		exit(25);
	}
	int  jewels = 0, jewelsleft = 0;
	bool	hadman = false;
	
	for  (int rowc = 0;  rowc < lines.size();  rowc++)  {
		string  row = lines[rowc];
		for  (int colc = 0;  colc < row.length();  colc++)  {
			square	&sqr = workarea.sq(coord(colc, rowc));
			switch  (row[colc])  {
			case  '@':
				if  (hadman)  {
					cerr << "Two men?\n";
					exit(5);
				}
				hadman = true;
				man = coord(colc, rowc);
				continue;
			case  '+':
				if  (hadman)  {
					cerr << "Two men?\n";
					exit(5);
				}
				hadman = true;
				man = coord(colc, rowc);
				sqr.init_target();
				continue;
			case  '.':
				sqr.init_target();
				continue;
			case  '$':
				sqr.init_jewel();
				jewels++;
				jewelsleft++;
				continue;
			case  '*':
				sqr.init_targ_jewel();
				jewels++;
				continue;
			case  '#':
				sqr.init_wall();
				continue;
			}
		}
	}
	if  (!hadman)  {
		cerr << "No man?\n";
		exit(6);
	}
	workarea.initjewels(jewels, jewelsleft);
}

main(int argc, char **argv)
{
	if  (argc != 2)  {
		cerr << "Usage: sokomatic fn\n";
		return  1;
	}
	coord	man;
	readpuzz(argv[1], man);
	findtroughs();
	solve(man);
	return  0;
}
