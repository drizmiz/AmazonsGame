﻿
#include "std.h"

#include "sio.h"
#include "searcher.h"

int main()
{
	using namespace std;
	using namespace amz;
	amz::initialize();
	amz::chess_game cg = sio::input(cin);
	const movement mm = cg.generate_next_move();
	cg.make_move(mm);
	sio::output(cout, mm);
	cout << "DEBUG Info: ";
	_Debug_evaluate_bz(cg.get_status(), _Color_rev(cg.get_color()));
	return 0;
}