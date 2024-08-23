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
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
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
	nheur = 0,		// Number of heur calls
	niter = 1;		// Number of basic iterations

bool	html = false,		// Output in HTML
	explain = false,	// Give commentary
	timedout = false;	// Run out of time

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

//  Html output and messages

void	htmlheader()
{
	if  (!html)
		return;
	cout << "content-type: text/html\n\n<html>\n<head><title>Sudoku answer</title></head>\n<body>\n";
	cout << "<h1>Sudoku result</h1>\n";
}

void	htmlfooter()
{
	if  (html)
		cout << "</body>\n</html>\n";
}

//  Display message

void	htmlmsg(const string &msg, const bool st = false)
{
	if  (html)  {
		cout << "<p>";
		if  (st)
			cout << "<strong>";
	}
	cout << msg;
	if  (html)  {
		if  (st)
			cout << "</strong>";
		cout << "</p>";
	}
	cout << endl;
}

void	printboard()
{
	if  (num_left != 0)  {
		if  (html)
			cout << "<p>No solution found after " << niter << " iterations and " << nheur << " heuristic entries.</p>\n";
		else
			cout << "No solution found after " << niter << " iterations and " << nheur << " heuristic entries.\n";
	}
	if  (html)  {
		cout << "<div align=center>\n<table border=3>\n";
		for  (int br = 0;  br < 81;  br += 27)  {
			cout << "<tr>\n";
			for  (int bc = 0;  bc < 9;  bc += 3)  {
				cout << "<td><table border=1>\n";
				for  (int r = 0;  r < 27;  r += 9)  {
					cout << "<tr>\n";
					for  (int c = 0;  c < 3;  c++)
						cout << "<td>" << grid[br+bc+r+c].num << "</td>\n";
					cout << "</tr>\n";
				}
				cout << "</td></table>\n";
			}
			cout << "</tr>\n";
		}
		cout << "</table></div>\n";
		cout << "<p>Solution took " << niter << " iterations";
		if  (nheur > 0)
			cout << " and " << nheur << " heuristic entries.";
		cout << "</p>\n";
	}
	else  {
		for  (int r = 0;  r < 81;  r += 9)  {
			for  (int c = 0;  c < 9;  c++)  {
				int	g = grid[r+c].num;
				if  (g > 0)
					cout << g;
				else
					cout << '.';
			}
			cout << endl;
		}
		cout << "Solution took " << niter << " iterations";
		if  (nheur > 0)
			cout << " and " << nheur << " heuristic entries.";
		cout << endl;
	}
	htmlfooter();
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

void	fillgrid()
{
	for  (int r = 0;  r < 81;  r += 9)  {
		for  (int c = 0;  c < 9;  c++)  {
		        char  ch;
			if  (!cin.get(ch))
				return;
			if  (ch == '\n')
				break;
			if  (isdigit(ch)  &&  ch != '0')  {
				grid[r+c].num = ch - '0';
				num_left--;
			}
		}
		char  ch;
		if  (!cin.get(ch))
			return;
		if  (ch != '\n')
			return;
	}
}

//  Fill grid from command line args or from QUERY_STRING

void	fillgrid(char **argv)
{
	for  (int r = 0;  r < 81;  r += 9)  {
		char	*arg = *argv++;
		if  (!arg)
			return;
		for  (int c = 0;  c < 9;  c++)  {
			int  ch = *arg++;
			if  (!ch)
				break;
			if  (isdigit(ch) && ch != '0')  {
				grid[r+c].num = ch - '0';
				num_left--;
			}
		}
	}
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
		if  (explain)  {
			ostringstream  ostr;
			ostr << playnum << ':';
			rowposs.dispposs(ostr, r);
			colposs.dispposs(ostr, c);
			boxposs.dispposs(ostr, b);
			ostr << " only possible " << bt;
			ostr << " (Scanning " << msg << ')';
			htmlmsg(ostr.str());
		}
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
	if  (explain)  {
		ostringstream  ostr;
		ostr << playnum << ": Looking at " << descr << ' ' << n+1;
		ostr << " only possible square for " << bit << " is row " << row(sq)+1 << " col " << col(sq)+1 << " box " << box(sq)+1;
		htmlmsg(ostr.str());
	}
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

//  Try possibilities with backup

void	heur(const int fromsq, const int level)
{
	nheur++;

	for  (int trialstart = fromsq+1;  trialstart < 81;  trialstart++)  {

		unsigned  minbits = 100;
		int	trysq = -1;

		for  (int n = trialstart;  n < 81;  n++)  {
			if  (grid[n].num > 0)
				continue;
			int  rown = row(n), coln = col(n), boxn = box(n);
			unsigned  inter = rowposs.possibles(rown) & colposs.possibles(coln) & boxposs.possibles(boxn);
			if  (inter == 0)
				throw  stuck("h");
			unsigned  cb = countbits(inter);
			if    (cb < minbits)  {
				minbits = cb;
				trysq = n;
				if  (minbits <= 2) // Won't do better than that by definition
					break;
			}
		}
		if  (trysq < 0)
			break;

		int  rown = row(trysq), coln = col(trysq), boxn = box(trysq);
		if  (explain)  {
			ostringstream ostr;
			ostr << "At trial level " << level+1 << ", " << minbits << " possible at row " << rown+1 << " col " << coln+1;
			htmlmsg(ostr.str());
		}
		unsigned  inter = rowposs.possibles(rown) & colposs.possibles(coln) & boxposs.possibles(boxn);

		for  (int bit = 1;  bit < 10;  bit++)
			if  (inter & (1 << bit))  {
				gridel savegrid[81];
				memcpy(savegrid, grid, sizeof(savegrid));
				posslist rp(rowposs), cp(colposs), bp(boxposs);
				int  save_play = playnum, save_left = num_left;
				grid[trysq].num = bit;
				grid[trysq].play = ++playnum;
				rowposs.setoff(rown, bit);
				colposs.setoff(coln, bit);
				boxposs.setoff(boxn, bit);
				if  (explain)  {
					ostringstream  ostr;
					ostr << playnum << ": Trial level " << level+1 << ", trying " << bit << " in row " << rown+1 << " col " << coln+1;
					htmlmsg(ostr.str());
				}
				if  (--num_left <= 0)	 // Very unlikely!!
					throw  finished();
				try  {
					iterate();
					heur(trysq, level + 1);
					if  (explain)
						htmlmsg("No solution found", true);
				}
				catch  (stuck s)  {
					if  (explain)
						htmlmsg(s.msg, true);
				}
				memcpy(grid, savegrid, sizeof(grid));
				rowposs = rp;
				colposs = cp;
				boxposs = bp;
				playnum = save_play;
				num_left = save_left;
				if  (explain)  {
					ostringstream  ostr;
					ostr << "Backing up to after move " << playnum;
					htmlmsg(ostr.str(), true);
				}
			}
	}
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
	// Arguments or query string handling

	char	*qs = getenv("QUERY_STRING");
	if  (qs)  {
		html = true;
		vector <char *> av;
		av.push_back(argv[0]);
		char	*cp;
		while  ((cp = strchr(qs, '&')))  {
			*cp = '\0';
			av.push_back(stracpy(qs));
			qs = cp + 1;
		}
		av.push_back(stracpy(qs));
		char	**r = new char *[av.size()+1];
		argv = r;
		argc = av.size();
		for  (vector<char *>::const_iterator ap = av.begin(); ap != av.end(); ap++)
			*r++ = *ap;
		*r = 0;
	}

	//  -e means explain steps
	if  (argc > 1)  {
		if  (strcmp(argv[1], "-e") == 0)  {
			explain = true;
			argc--;
			argv++;
		}
	}

	//  Fill board from stdin or arguments

	if  (argc < 2)
		fillgrid();
	else  {
		if  (argc != 10)  {
			cerr << "Usage: sudoku r1 .. r9\n";
			return  1;
		}
		fillgrid(++argv);
	}

	//  Check board isn't too empty

	if  (num_left > 63)  {
		if  (html)  {
			htmlheader();
			cout << "<p>";
		}
		cout << "Please put some numbers in!" << endl;
		if  (html)  {
			cout << "</p>\n";
			htmlfooter();
			return  0;
		}
		return  9;
	}

	//  Fill in possibles.
	//  May detect errors here

	try  {
		fillposs();
	}
	catch  (stuck s)  {
		if  (html)  {
			htmlheader();
			cout << "<p>";
		}
		cout << s.msg << endl;
		if  (html)  {
			cout << "</p>\n";
			htmlfooter();
			return  0;
		}
		return  10;
	}

	//  Set up limit catch
	//  Output header

	set_limits();
	htmlheader();

	try {
		iterate();
		heur(-1, 0);
	}
	catch  (finished)  {
		printboard();
		return  0;
	}
	catch  (stuck s)  {
		htmlmsg(s.msg);
		htmlfooter();
		return  html? 0: 2;
	}
	catch  (looping)  {
		htmlmsg("Sorry - looping on that data");
		htmlfooter();
		return  html? 0: 3;
	}
	printboard();
	return  0;
}
