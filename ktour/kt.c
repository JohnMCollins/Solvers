/*
 * Copyright (c) Xi Software Ltd. 1998.
 *
 * kt.c: created by John M Collins on Tue Apr 21 1998.
 *----------------------------------------------------------------------
 * $Header$
 * $Log$
 *----------------------------------------------------------------------
 */

#include <stdio.h>

unsigned   char	 board[8][8];

void	print()
{
	int	i, j;

	for  (i = 7;  i >= 0;  i--)  {
		printf("%d:", i+1);
		for  (j = 0;  j < 8;  j++)
			printf(" %2d", board[i][j]);
		putchar('\n');
	}
	printf("  ");
	for  (j = 0;  j < 8;  j++)
		printf(" %c ", j + 'A');
	putchar('\n');
}

void	try(int level, int row, int col)
{
	if  (row < 0  ||  row > 7  ||  col < 0  ||  col > 7  ||  board[row][col] != 0)
		return;
	board[row][col] = level;
	if  (level == 64)
		print();
	else  {
		if  (level > 61)
			printf("Level %d\n", level);
		level++;
		try(level, row+1, col+2);
		try(level, row-1, col+2);
		try(level, row+1, col-2);
		try(level, row-1, col-2);
		try(level, row+2, col+1);
		try(level, row-2, col+1);
		try(level, row+2, col-1);
		try(level, row-2, col-1);
		level--;
	}
	board[row][col] = 0;
}

main()
{
	try(1, 0, 0);
	return  0;
}
