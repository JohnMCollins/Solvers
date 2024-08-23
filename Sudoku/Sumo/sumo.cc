//
// Copyright (c) Xi Software Ltd. 2004.
//
// sudoku.cc: created by John M Collins on Tue Nov 23 2004.
//----------------------------------------------------------------------
// $Header$
// $Log$
//----------------------------------------------------------------------
// C++ Version: 

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
using  namespace  std;

//==========================================================================

class	gridel  {
public:
	int	num;		// Or -1 if not found yet
	int	play;		// Play number

	gridel() : num(-1), play(0)  { }
};

gridel	grid[81];

//==========================================================================

int	playnum = 0,		// Number of moves
	num_left = 81,		// Number left
	niter = 1;		// Number of basic iterations

bool	timedout = false;	// Run out of time

//==========================================================================

//  Things to throw in various situations....

class	stuck  {		// No solution found - back up or quit
public:
	string	msg;
	stuck(const string &m) : msg(m)  { }
};

class	finished  {  };		// Found solution, exit

class	looping  {  };		// Exceeded CPU

//==========================================================================
// Class for remembering what possibilities are left in given row/box/column

class	possleft  {
public:
	unsigned  possmap;
	unsigned  nleft;

	possleft() : possmap(0x03fe), nleft(9)  { }
};

//  Class for holding a row / column / box-worth

class	posslist  {
public:
	possleft  poss[9];
	char		namebase; 		// For future....
	const  char	*nametype;

	posslist(const posslist &pl)  { *this = pl;  }
	posslist(const char nb, const char *nt) : namebase(nb), nametype(nt)  { }

	void	setoff(const int, const int);
	void	csetoff(const int, const int);
	unsigned  possibles(const int n)  {  return  poss[n].possmap;  }
	int	possnum(const int n)  {  return  poss[n].nleft;  }

	bool	isset(const int n, const int b)  {  return  possibles(n) & (1 << b);  }

	void	dispposs(ostringstream &, const int);
};

void	posslist::setoff(const int n, const int v)
{
	poss[n].possmap &= ~(1 << v);
	--poss[n].nleft;
}

void	posslist::csetoff(const int n, const int v)
{
	unsigned  vb = 1 << v;
	if  (!(poss[n].possmap & vb))  {
		ostringstream  ostr;
		ostr << "Duplicated " << v << " found looking at " << nametype << ' ' << n+1;
		throw  stuck(ostr.str());
	}
	poss[n].possmap &= ~vb;
	--poss[n].nleft;
}

//  Display in stream for when we're giving commentary

void	posslist::dispposs(ostringstream &ostr, const int n)
{
	ostr << ' ' << nametype << ' '<< n+1 << '[';
	for  (int bit = 1;  bit < 10;  bit++)
		if  (isset(n, bit))
			ostr << bit;
	ostr << ']';
}

//  Here are the records we've got to

posslist  rowposs('1', "row"), colposs('a', "col"), boxposs('A', "box");

//==========================================================================

//  Try to stop myself running wild

void	runcatch(int n)
{
	timedout = true;
}

void	set_limits()
{
	rlimit  limcpu;
	limcpu.rlim_cur = 10;
	limcpu.rlim_max = 20;
	setrlimit(RLIMIT_CPU, &limcpu);
	struct  sigaction  sa;
	sa.sa_handler = runcatch;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGXCPU, &sa, 0);
}

//==========================================================================

//  Display message

void	htmlmsg(const string &msg, const bool st = false)
{
	cout << msg << endl;
}

void	printboard(char *argv)
{
	ofstream  outf(argv);
	for  (int r = 0;  r < 81;  r += 9)  {
		for  (int c = 0;  c < 9;  c++)  {
			int	g = grid[r+c].num;
			if  (g > 0)  {
				outf << g;
				cout << g;
			}
			else  {
				outf << '.';
				cout << '.';
			}
		}
		outf << endl;
		cout << endl;
	}
}

//==========================================================================

// Get row/col/box from square

inline	int	row(const int sq)
{
	return  sq / 9;
}

inline	int	col(const int sq)
{
	return  sq % 9;
}

inline	int	box(const int sq)
{
	return  (sq / 27) * 3 + col(sq) / 3;
}

//==========================================================================

//  Fill grid from std input

void	fillgrid(istream &istr)
{
	for  (int r = 0;  r < 81;  r += 9)  {
		for  (int c = 0;  c < 9;  c++)  {
		        char  ch;
			if  (!istr.get(ch))
				return;
			if  (ch == '\n')
				break;
			if  (isdigit(ch)  &&  ch != '0')  {
				grid[r+c].num = ch - '0';
				num_left--;
			}
		}
		char  ch;
		if  (!istr.get(ch))
			return;
		if  (ch != '\n')
			return;
	}
}

//  Fill grid from command line args or from QUERY_STRING

void	fillgrid(char *argv)
{
	ifstream  istr(argv);

	if  (istr.fail())  {
		cerr << "Cannot open " << argv << endl;
		exit(20);
	}
	fillgrid(istr);
}

//  Set up "rowposs" etc and check initial board doesn't have duplicates

void	fillposs()
{
	for  (int sq = 0;  sq < 81;  sq++)
		if  (grid[sq].num > 0)  {
			rowposs.csetoff(row(sq), grid[sq].num);
			colposs.csetoff(col(sq), grid[sq].num);
			boxposs.csetoff(box(sq), grid[sq].num);
		}
}

//==========================================================================

//  If exactly one bit is set in n, return that bit number
//  Otherwise return zero

int  whichbit(unsigned n)
{
	int  result = 0;
	for  (int b = 1;  b < 10;  b++)
		if  (n & (1 << b))  {
			if  (result)
				return  0;
			result = b;
		}

	return  result;
}

//  Look at given square and see if we can deduce something

int	checkinter(const int sq, const int r, const int c, const int b, const char *msg)
{
	unsigned  inter = rowposs.possibles(r) & colposs.possibles(c) & boxposs.possibles(b);
	if  (inter == 0)  {
		throw  stuck("No solution found");
	}
	if  (int  bt = whichbit(inter))  {
		grid[sq].num = bt;
		grid[sq].play = ++playnum;
		rowposs.setoff(r, bt);
		colposs.setoff(c, bt);
		boxposs.setoff(b, bt);
		if  (--num_left <= 0)
			throw  finished();
		return  sq;
	}
	return  -1;
}
	
//  Look for possible placables in given row and return square number placed or -1
//  (We don't currently use the result).

int	searchrow(const int rownum)
{
	int	sq = rownum * 9;
	for  (int c = 0;  c < 9;  sq++, c++)  {
		if  (grid[sq].num > 0)
			continue;
		int  itc = checkinter(sq, rownum, col(sq), box(sq), "rows");
		if  (itc >= 0)
			return  itc;
	}
	return  -1;
}

//  Ditto column

int	searchcol(const int colnum)
{
	int	sq = colnum;
	for  (int r = 0;  r < 9;  sq += 9, r++)  {
		if  (grid[sq].num > 0)
			continue;
		int  itc = checkinter(sq, row(sq), colnum, box(sq), "cols");
		if  (itc >= 0)
			return  itc;
	}
	return  -1;
}

//  Ditto box

int	searchbox(const int boxnum)
{
	int  r = (boxnum / 3) * 3;
	int  c = (boxnum % 3) * 3;
	int  oc = c;
	int  sq = r * 9 + c;

	for  (int rr = 0;  rr < 3;  sq += 6, r++, rr++)  {
		for  (int cc = 0;  cc < 3;  sq++, c++, cc++)  {
			if  (grid[sq].num > 0)
				continue;
			int  itc = checkinter(sq, r, c, boxnum, "boxes");
			if  (itc >= 0)
				return  itc;
		}
		c = oc;
	}
	return  -1;
}

//  Report where we have the only place a number can go in a row/column/box

int	foundres(const int sq, const int bit, const char *descr, const int n)
{
	grid[sq].num = bit;
	grid[sq].play = ++playnum;
	rowposs.setoff(row(sq), bit);
	colposs.setoff(col(sq), bit);
	boxposs.setoff(box(sq), bit);
	if  (--num_left <= 0)
		throw  finished();
	return  sq;
}

//  Look for the only places a number might go in a row

int	researchrow(const int rownum)
{
	unsigned  tocome = rowposs.possibles(rownum);
	for  (int bit = 1;  bit < 10;  bit++)  {
		unsigned  msk = 1 << bit;
		if  (!(msk & tocome))
			continue;
		int  wsq = -1, countcols = 0;
		int	sq = rownum * 9;
		for  (int c = 0;  c < 9;  sq++, c++)  {
			if  (grid[sq].num > 0)
				continue;
			if  (msk & colposs.possibles(c) & boxposs.possibles(box(sq)))  {
				wsq = sq;
				countcols++;
			}
		}
		if  (countcols <= 0)
			throw  stuck("no solution searching rows");
		if  (countcols == 1)
			return  foundres(wsq, bit, "row", rownum);
	}
	return	-1;
}

//  Look for the only places a number might go in a column

int	researchcol(const int colnum)
{
	unsigned  tocome = colposs.possibles(colnum);
	for  (int bit = 1;  bit < 10;  bit++)  {
		unsigned  msk = 1 << bit;
		if  (!(msk & tocome))
			continue;
		int  wsq = -1, countrows = 0;
		int  sq = colnum;
		for  (int r = 0;  r < 9;  sq += 9, r++)  {
			if  (grid[sq].num > 0)
				continue;
			if  (msk & rowposs.possibles(r) & boxposs.possibles(box(sq)))  {
				wsq = sq;
				countrows++;
			}
		}
		if  (countrows <= 0)
			throw  stuck("no solution searching cols");
		if  (countrows == 1)
			return  foundres(wsq, bit, "col", colnum);
	}
	return  -1;
}

//  Look for the only places a number might go in a box

int	researchbox(const int boxnum)
{
	unsigned  tocome = boxposs.possibles(boxnum);
	for  (int bit = 1;  bit < 10;  bit++)  {
		unsigned  msk = 1 << bit;
		if  (!(msk & tocome))
			continue;
		int  wsq = -1, counthad = 0;
		int  r = (boxnum / 3) * 3;
		int  c = (boxnum % 3) * 3;
		int  oc = c;
		int  sq = r * 9 + c;
		for  (int rr = 0;  rr < 3;  sq += 6, r++, rr++)  {
			for  (int cc = 0;  cc < 3;  sq++, c++, cc++)  {
				if  (grid[sq].num > 0)
					continue;
				if  (msk & rowposs.possibles(r) & colposs.possibles(c))  {
					wsq = sq;
					counthad++;
				}
			}
			c = oc;
		}
		if  (counthad <= 0)
			throw  stuck("no solution searching boxes");
		if  (counthad == 1)
			return  foundres(wsq, bit, "box", boxnum);
	}
	return  -1;
}

//================================================================================

// Main iteration....

void	iterate()
{
	int	nl;
	do  {
		nl = num_left;
		for  (int  n = 0;  n < 9;  n++)  {
			searchrow(n);
			searchcol(n);
			searchbox(n);
		}
		for  (int  n = 0;  n < 9;  n++)  {
			researchrow(n);
			researchcol(n);
			researchbox(n);
		}
		niter++;
		if  (timedout)
			throw  looping();
	}  while  (nl != num_left);
}

unsigned  countbits(unsigned n)
{
	unsigned  result = 0;
	
	while  (n != 0)  {
		result++;
		n &= n - 1;
	}
	return  result;
}

//================================================================================

char	*stracpy(const char *m)
{
	char	*result = new char [strlen(m) + 1];
	strcpy(result, m);
	return  result;
}
			
int	main(int argc, char **argv)
{
	//  Fill board from stdin or arguments
	
	if  (argc < 2)
		fillgrid(cin);
	else
		fillgrid(argv[1]);

	//  Fill in possibles.
	//  May detect errors here

	try  {
		fillposs();
	}
	catch  (stuck s)  {
		cerr << s.msg << endl;
		return  10;
	}

	//  Set up limit catch
	//  Output header
	
	set_limits();

	int	orig_numleft = num_left;
	try {
		iterate();
	}
	catch  (finished)  {
		printboard(argv[1]);
		return  0;
	}
	catch  (stuck s)  {
		cerr << s.msg << endl;
		return  2;
	}
	catch  (looping)  {
		cerr << "Sorry - looping on that data" << endl;
		return  3;
	}

	if  (num_left != orig_numleft)  {
		printboard(argv[1]);
		return  0;
	}
	return  1;
}
