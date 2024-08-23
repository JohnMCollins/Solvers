//
// Copyright (c) Xi Software Ltd. 2001.
//
// nums.cc: created by John M Collins on Sat Dec 15 2001.
//----------------------------------------------------------------------
// $Header$
// $Log$
//----------------------------------------------------------------------
// C++ Version:

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <iostream>
using namespace std;

vector<int> resstack;

class  opcl  {
public:
	short	v;
	char	op;
	bool	isop;
	opcl(const char c) : v(0), op(c), isop(true)  { }
	opcl(const int n) : v(n), op('\0'), isop(false)  { }
};

class	resst  {
public:
	int	res;
	bool	isorig;
	resst(const int r, const bool iso) : res(r), isorig(iso)  { }
};

vector<opcl> opstack, bestans;
int	target, mindiff = 1000000;
bool	printall = false, extended = false, html = false;

ostream  &operator <<(ostream &ost, const resst &rs)
{
	if  (rs.isorig)  {
		if  (html)
			ost << "<strong>" << rs.res << "</strong>";
		else
			ost << '[' << rs.res << ']';
	}
	else
		ost << rs.res;
	return  ost;
}

int	ipower(const int a, const int b)
{
	int	result = 1;
	for  (int  c = 0;  c < b;  c++)  {
		int  nresult = result * a;
		if  (nresult < result)
			return  -1;
		result = nresult;
	}
	return  result;
}

int	ifact(const int a)
{
	int	result = 1;
	for  (int  c = 2;  c <= a;  c++)  {
		int  nresult = result * c;
		if  (nresult < result)
			return  -1;
		result = nresult;
	}
	return  result;
}

void	calculdisp()
{
	vector<resst> rstack;
	for  (vector<opcl>::const_iterator ops = opstack.begin(); ops != opstack.end(); ops++)  {
		const  opcl  &item = *ops;
		if  (item.isop)  {
			resst  rst = rstack.back();
			rstack.pop_back();
			resst  lst = rstack.back();
			rstack.pop_back();
			int  l = lst.res, r = rst.res;
			int  sum = -1;
			if  (html)
				cout << "<br/>\n";
			switch  (item.op)  {
			case  '+':
				sum = l + r;
				cout << lst << " + " << rst << " = " << sum << endl;
				break;
			case  '-':
				sum = l - r;
				cout << lst << " - " << rst << " = " << sum << endl;
				break;
			case  '*':
				sum = l * r;
				cout << lst << " * " << rst << " = " << sum << endl;
				break;
			case  '/':
				sum = l / r;
				cout << lst << " / " << rst << " = " << sum << endl;
				break;
			case  '^':
				sum = ipower(l,r);
				cout << lst << " ^ " << rst << " = " << sum << endl;
				break;
			case  'q':
				sum = int(.5 + sqrt(double(r)));
				rstack.push_back(lst);
				cout << "sqrt(" << rst << ") = " << sum << endl;
				break;
			case  '!':
				sum = ifact(r);
				rstack.push_back(lst);
				cout << rst << "! = " << sum << endl;
				break;
			}
			rstack.push_back(resst(sum, false));
		}
		else
			rstack.push_back(resst(item.v, true));
	}
	if  (html)
		cout << "<br/>\n";
	cout << "Result is " << rstack.back().res << endl;
}

void  checkans(const int sum)
{
	int  diff = sum - target;
	if  (diff < 0)
		diff = - diff;
	if  (diff < mindiff || (diff == mindiff && bestans.size() > opstack.size()))  {
		mindiff = diff;
		bestans = opstack;
	}
	if  (diff == 0  &&  printall)  {
		cout << "\n\nGot an answer:\n\n";
		calculdisp();
	}
}

vector<int> concat(vector<int> &a, vector<int> &b)
{
	vector<int> result;
	for  (vector<int>::const_iterator ar = a.begin();  ar != a.end();  ar++)
		result.push_back(*ar);
	for  (vector<int>::const_iterator br = b.begin();  br != b.end();  br++)
		result.push_back(*br);
	return  result;
}

void  tryposs(vector<int> numsleft)
{
	if  (mindiff == 0  &&  bestans.size() != 0  &&  bestans.size() < opstack.size())
		return;

	vector<int> todo = numsleft, done;

	for  (vector<int>::const_iterator rr = numsleft.begin();  rr != numsleft.end();  rr++)  {
		int  r = todo.back();
		todo.pop_back();
		resstack.push_back(r);
		opcl  rc(r);
		opstack.push_back(rc);
		tryposs(concat(done, todo));
		done.push_back(r);
		opstack.pop_back();
		resstack.pop_back();
	}

	if  (resstack.size() > 1)  {
		int  r = resstack.back();
		resstack.pop_back();
		int  l = resstack.back();
		resstack.pop_back();
		if  (l != 0  &&  r != 0)  {
			{
				int  sum = l + r;
				opcl pl('+');
				opstack.push_back(pl);
				if  (resstack.size() == 0)
					checkans(sum);
				resstack.push_back(sum);
				tryposs(numsleft);
				opstack.pop_back();
				resstack.pop_back();
			}
			if  (l > r)  {
				int  sum = l - r;
				opcl minus('-');
				opstack.push_back(minus);
				if  (resstack.size() == 0)
					checkans(sum);
				resstack.push_back(sum);
				tryposs(numsleft);
				opstack.pop_back();
				resstack.pop_back();
			}
			if  (l != 1 && r != 1)  {
				int  sum = l * r;
				opcl times('*');
				opstack.push_back(times);
				if  (resstack.size() == 0)
					checkans(sum);
				resstack.push_back(sum);
				tryposs(numsleft);
				opstack.pop_back();
				resstack.pop_back();
				if  (l % r == 0)  {
					int  sum = l / r;
					opcl div('/');
					opstack.push_back(div);
					if  (resstack.size() == 0)
						checkans(sum);
					resstack.push_back(sum);
					tryposs(numsleft);
					opstack.pop_back();
					resstack.pop_back();
				}
			}
			if  (extended && l > 1 && r > 1)  {
				int  sum = ipower(l,r);
				if  (sum > 0)  {
					opcl pwr('^');
					opstack.push_back(pwr);
					if  (resstack.size() == 0)
						checkans(sum);
					resstack.push_back(sum);
					tryposs(numsleft);
					opstack.pop_back();
					resstack.pop_back();
				}
			}
		}
		resstack.push_back(l);
		resstack.push_back(r);
	}
	if  (extended  &&  resstack.size() > 0  &&  resstack.back() > 2)  {
		int	r = resstack.back();
		resstack.pop_back();
		int    sum = ifact(r);
		if  (sum > 0)  {
			opcl  sqr('!');
			opstack.push_back(sqr);
			if  (resstack.size() == 0)
				checkans(sum);
			resstack.push_back(sum);
			tryposs(numsleft);
			opstack.pop_back();
			resstack.pop_back();
		}
		sum = int(.5 + sqrt(double(r)));
		if  (r == sum*sum)  {
			opcl  sqr('q');
			opstack.push_back(sqr);
			if  (resstack.size() == 0)
				checkans(sum);
			resstack.push_back(sum);
			tryposs(numsleft);
			opstack.pop_back();
			resstack.pop_back();
		}
		resstack.push_back(r);
	}
}

void	htmlheader(vector<int> &nlist)
{
	if  (!html)
		return;
	cout << "content-type: text/html\n\n<html>\n<head><title>Numbers game best result</title></head>\n<body>\n";
	cout << "<h1>Target = " << target << "</h1>\n";
	cout << "<h2>Using numbers:";
	for  (vector<int>::const_iterator cv = nlist.begin();  cv != nlist.end(); cv++)
		cout << ' ' << *cv;
	if  (extended)
		cout << " <i>(and cheating!)</i>";
	cout << "</h2>\n<p>\n";
}

void	htmlfooter()
{
	if  (html)
		cout << "</p>\n</body>\n</html>\n";
}

char	*stracpy(const char *m)
{
	char	*result = new char [strlen(m) + 1];
	strcpy(result, m);
	return  result;
}

int  main(int argc, char **argv)
{
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
	if  (argc < 3)  {
	usage:
		cerr << "Usage: nums [-a] target num1 num2 ...\n";
		return  1;
	}
	if  (argv[1][0] == '-')  {
		char  *arg = argv[1];
		while  (*++arg)
			switch  (*arg)  {
			case  'a':
				printall = true;
				continue;
			case  'e':
				extended = true;
				continue;
			case  'h':
				html = true;
				continue;
			}
		argv++;
		argc--;
	}
	target = atoi(argv[1]);
	if  (target <= 0)
		goto  usage;

	vector<int> nlist;
	for  (int  c = 2;  c < argc;  c++)
		nlist.push_back(atoi(argv[c]));

	for  (vector<int>::const_iterator rr = nlist.begin();  rr != nlist.end();  rr++)  {
		if  (*rr <= 0)  {
			htmlheader(nlist);
			cout << "Cannot have zero or negatives in list\n";
			htmlfooter();
			return  0;
		}
		
		if  (*rr == target)  {
			htmlheader(nlist);
			cout << "Answer " << target << " already in list\n";
			htmlfooter();
			return  0;
		}
	}

	tryposs(nlist);

	if  (bestans.size() > 0)  {
		htmlheader(nlist);
		if  (!html)
			cout << "Best answer for " << target << " .....\n";
		opstack = bestans;
		calculdisp();
		htmlfooter();
	}
	else  {
		htmlheader(nlist);
		cout << "I didn\'t find any answers\n";
		htmlfooter();
	}

	return  0;
}


