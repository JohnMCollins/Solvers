//
// Copyright (c) Xi Software Ltd. 1999.
//
// sokomatic.cc: created by John M Collins on Fri Dec 24 1999.
//----------------------------------------------------------------------
// $Header$
// $Log$
//----------------------------------------------------------------------
// C++ Version: 

#include <iostream>
#include <fstream>
#include <cctype>
#include <vector>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

using  namespace  std;

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

class	bmap  {
	unsigned  long	bits[MAXCOLROW*MAXCOLROW/32];
public:
	bmap()  {
		memset((void *) bits, '\0', sizeof(bits));
	}
	void	seton(coord c)
	{
		unsigned  cv = c.index();
		bits[cv >> 5] |= 1 << (cv & 31);
	}
	bool	ison(coord c)
	{
		unsigned  cv = c.index();
		return  bits[cv >> 5] & (1 << (cv & 31));
	}
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
	enum	movetype  { INVALID,
			    PUSH,	// Push no targets involved
			    PUSHON,	// Push from one target to another
			    PUSHTO, 	// Push onto target
			    PUSHOFF 	// Push off target
	};
	movetype	typem;
	coord		man_init, from, to;
	move() { typem = INVALID; }
};

typedef	vector<move>	movelist;

movelist	move_list, best_solution;
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

void	find_reachable(coord from, bmap &result)
{
	result.seton(from);
	int	settings;
	do  {
		settings = 0;
		for  (int sr = 1;  sr < MAXCOLROW-1;  sr++)
			for  (int sc = 1;  sc < MAXCOLROW-1;  sc++)  {
				coord	here(sc, sr);
				if  (result.ison(here))
					continue;
				if  (workarea.sq(here).isoccupied())
					continue;
				if  (result.ison(coord(sc+1, sr))  ||
				     result.ison(coord(sc-1, sr))  ||
				     result.ison(coord(sc, sr+1))  ||
				     result.ison(coord(sc, sr-1)))  {
					result.seton(here);
					settings++;
				}
			}
	}  while  (settings != 0);
}

inline	void	look_around(int vis[MAXCOLROW][MAXCOLROW], int col, int row, int cnt)
{
	if  (!vis[col][row]  &&  workarea.sq(coord(col, row)).isempty())
		vis[col][row] = cnt;
}

void	find_path(coord from, coord to, vector<coord> &clist)
{
	int	fx = from.x(), tx = to.x(), fy = from.y(), ty = to.y();

	int	visited[MAXCOLROW][MAXCOLROW];
	memset((void *) visited, 0, sizeof(visited));
	visited[tx][ty] = 1;
	int	pcnt = 1, cnt = 2;
	for  (;;)  {
		for  (int sc = 1;  sc < MAXCOLROW-1;  sc++)
			for  (int sr = 1;  sr < MAXCOLROW-1;  sr++)
				if  (visited[sc][sr] == pcnt)  {
					if  (sc == fx  &&  sr == fy)
						goto  dun;
					look_around(visited, sc-1, sr, cnt);
					look_around(visited, sc+1, sr, cnt);
					look_around(visited, sc, sr-1, cnt);
					look_around(visited, sc, sr+1, cnt);
				}
		cnt++;
		pcnt++;
	}
 dun:
	int	endx = fx, endy = fy;
	clist.push_back(coord(endx, endy));
	while  (--pcnt > 0)  {
		if  (visited[endx-1][endy] == pcnt)
			endx--;
		else  if  (visited[endx+1][endy] == pcnt)
			endx++;
		else  if  (visited[endx][endy-1] == pcnt)
			endy--;
		else  if  (visited[endx][endy+1] == pcnt)
			endy++;
		else  {
			cerr << "Panic lost place\n";
			exit(19);
		}
		clist.push_back(coord(endx, endy));
	}
}

//  This only considers pushes

bool	moveok(move &m)
{
	delta	del = m.to - m.from;
	
	// Try to trap common case of running two adjacent to a wall or into a corner
	// which is never right except when it's on or to a target.
	// This won't catch pushing the thing to a target but clobbering one just against a
	// wall.

	if  (!workarea.sq(m.from + del).iswall())
		return  true;
	coord	adj = m.from + del.clockwise();
	if  (workarea.sq(adj).iswall())
		return  false;
	if  (workarea.sq(adj).hasjewel() && workarea.sq(adj+del).iswall())
		return  false;
	adj = m.from + del.anticlockwise();
	if  (workarea.sq(adj).iswall())
		return  false;
	if  (workarea.sq(adj).hasjewel() && workarea.sq(adj+del).iswall())
		return  false;
	return  true;
}

void	pushmove(coord manpos, coord from, coord to, movelist &result)
{
	move	them;
	them.man_init = manpos;
	them.from = from;
	them.to = to;
	if  (workarea.sq(to).istarget())  {
		them.typem = workarea.sq(from).istarget()? move::PUSHON: move::PUSHTO;
		result.push_back(them);
	}
	else  {
		them.typem = workarea.sq(from).istarget()? move::PUSHOFF: move::PUSH;
		if  (moveok(them))
			result.push_back(them);
	}
}

void	listmoves(coord cpos, movelist &result)
{
	bmap	reachable;
	find_reachable(cpos, reachable);
	for  (int  sc = 1;  sc < MAXCOLROW-1;  sc++)  {
		for  (int  sr = 1;  sr < MAXCOLROW-1;  sr++)  {
			coord	here(sc, sr);
			if  (!workarea.sq(here).hasjewel())
				continue;
			coord	leftp(sc-1, sr);
			coord	rightp(sc+1, sr);
			if  (workarea.sq(leftp).isempty()  &&  workarea.sq(rightp).isempty())  {
				if  (reachable.ison(leftp)  &&  !workarea.sq(rightp).istrough())
					pushmove(cpos, here, rightp, result);
				if  (reachable.ison(rightp)  &&  !workarea.sq(leftp).istrough())
					pushmove(cpos, here, leftp, result);
			}
			coord	upp(sc, sr-1);
			coord	downp(sc, sr+1);
			if  (workarea.sq(upp).isempty()  &&  workarea.sq(downp).isempty())  {
				if  (reachable.ison(upp)  &&  !workarea.sq(downp).istrough())
					pushmove(cpos, here, downp, result);
				if  (reachable.ison(downp)  &&  !workarea.sq(upp).istrough())
					pushmove(cpos, here, upp, result);
			}
		}
	}
}

void	makemove(coord &pos, const move &m)
{
	pos = m.from;
	switch  (m.typem)  {
	case  move::PUSHTO:
		workarea.decrleft();
		break;
	case  move::PUSHOFF:
		workarea.incrleft();
		break;
	}
	workarea.sq(m.from).jeweloff();
	workarea.sq(m.to).jewelon();
}

void	undomove(coord &pos, const move &m)
{
	pos = m.man_init;
	switch  (m.typem)  {
	case  move::PUSHTO:
		workarea.incrleft();
		break;
	case  move::PUSHOFF:
		workarea.decrleft();
		break;
	}
	workarea.sq(m.from).jewelon();
	workarea.sq(m.to).jeweloff();
}

void	print_sol(movelist &sol)
{
	cout << "\nSolution: " << sol.size() << " pushes\n";
	for  (int mcnt = 0;  mcnt < sol.size();  mcnt++)  {
		move	&mv = sol[mcnt];
		delta	del = mv.to - mv.from;
		char	p = del.dx()? ((del.dx() > 0)? 'E': 'W'): (del.dy() > 0)? 'S': 'N';
		cout << p;
		if  (mcnt % 74 == 73)
			cout << endl;
	}
	cout << endl;
}

void	print_solall(movelist &sol)
{
	int	colcnt = 0;
	
	for  (int pcnt = 0;  pcnt < sol.size();  pcnt++)  {
		move	&mv = sol[pcnt];
		delta	dep = mv.to - mv.from;
		coord	manto = mv.from + dep.backward();
		vector<coord> wlist;
		find_path(mv.man_init, manto, wlist);
		for  (int  mp = 1;  mp < wlist.size();  mp++)  {
			delta	del = wlist[mp] - wlist[mp-1];
			char	p = del.dx() == 0? (del.dy() > 0? 's': 'n') : del.dx() > 0? 'e': 'w';
			cout << p;
			colcnt++;
			if  (colcnt % 74 == 73)
				cout << endl;
		}
		char	p = dep.dx() == 0? (dep.dy() > 0? 'S': 'N') : dep.dx() > 0? 'E': 'W';
		cout << p;
		colcnt++;
		if  (colcnt % 74 == 73)
			cout << endl;
		makemove(manto, mv);
	}
	cout << "\nTotal: " << colcnt << " moves " << sol.size() << " pushes\n";
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
			movelist nxtp;
			listmoves(cpos, nxtp);
			while  (nxtp.size() != 0)  {
				trymove(nxtp[nxtp.size() - 1]);
				nxtp.pop_back();
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
}

void	solve(coord initial)
{
	position ipos;
	ipos.save_position(initial);	
	position_history.push_back(ipos);
	movelist nxtp;
	listmoves(initial, nxtp);
	if  (nxtp.size() == 0)  {
		cerr << "There don\'t seem to be any starting moves\n";
		exit(10);
	}
	do  {
		trymove(nxtp[nxtp.size() - 1]);
		nxtp.pop_back();
	}  while  (nxtp.size() != 0);
	position_history.pop_back();

	if  (nsolutions <= 0)  {
		cerr << "Found no solutions!\n";
		exit(11);
	}
	cout << "Found " << nsolutions << " solution(s)\nLeast pushes (" << best_solution.size() << ")\n";
	print_solall(best_solution);
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
