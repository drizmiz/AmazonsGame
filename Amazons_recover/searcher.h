
#pragma once

// #ifndef __GNUC__
// #include <algorithm>		// for sort
// #include <chrono>		// for clock
// #endif

#include "game.h"
#include "eval.h"

namespace amz
{
	enum class phase
	{
		hash,
		gen_moves,
		rest
	};

	// �߷�����
	class chess_game::mm_sort
	{
	public:
		movement mm_hash;
		phase cur_phase;
		int mm_index;
		std::vector<movement> mms;

		chess_game& cg_parent;
		mm_sort(chess_game& cg, movement _mm_hash) noexcept :
			cg_parent(cg), mm_hash(_mm_hash), cur_phase(phase::hash), mms{}
		{}

		const movement next();
	};

	// �õ���һ���߷�
	const movement chess_game::mm_sort::next()
	{
		switch (cur_phase)
		{
			// ����break�ǹ����
			// �û�������
		case phase::hash:
			cur_phase = phase::gen_moves;
			if (!_Is_dft_move(mm_hash))
				return mm_hash;
			// ���������ŷ�
		case phase::gen_moves:
			cur_phase = phase::rest;
			mms = std::move(cg_parent._Get_all_possible_moves());
			std::sort(mms.begin(), mms.end(),
				[this](movement mm1, movement mm2) {
					return cg_parent.rt.probe_history(mm1) >
						cg_parent.rt.probe_history(mm2);
				}
			);
			mm_index = 0;
			// ��ʷ������
		case phase::rest:
			for (; mm_index < mms.size();)
			{
				movement mm = mms.at(mm_index);
				mm_index++;
				if (mm != mm_hash)
					return mm;
			}
			// û���ŷ���
		default:
			return dft_movement;
		}
	}

	inline bool chess_game::_Permit_null()
	{
		return turn_cnt <= 11;
	}

	constexpr int _null_cut = 2;
	constexpr int pvs_window = 1200;
	eval_t chess_game::_Alphabeta(int depth, eval_t alpha, eval_t beta, bool _no_null = false)
	{
		using namespace std::chrono;
		// �û����ѯ
		const auto [_val, mm_hash] = rt.probe_hash(this->get_status(), depth, alpha, beta);
		if (_val != unknown)
			return _val;
		// ��ʱ
		auto end = steady_clock::now();
		auto diff = duration_cast<std::chrono::milliseconds>(end - starttime).count();

		if (diff > deadspan)
		{
#ifdef __GNUC__
			return inf;
#else
			std::cout << "warning: TLE" << std::endl;
#endif
		}
		if (diff > timespan)
		{
			timespan = 0;
			return _Evaluate(this->get_status(), this->get_color(), turn_cnt);
		}
		// �������
		if (depth <= 0)
		{
			const eval_t val = _Evaluate(this->get_status(), this->get_color(), turn_cnt);
			// rt.record_hash(this->get_status(), , depth, val, node_f::pv);
			return val;
		}
		// ���Ųü�
		if (!_no_null && _Permit_null())
		{
			null_move();
			eval_t ev = -_Alphabeta(depth - 1 - _null_cut, -beta, -(beta - pvs_window), true);
			undo_null_move();
			if (ev >= beta)
				return ev;
		}
		// ��ʼ������
		node_f flag = node_f::alpha;
		eval_t eval_max = -inf;
		movement mm_best = dft_movement;

		mm_sort mmsrt(*this, mm_hash);

		for (movement mm; !_Is_dft_move(mm = mmsrt.next());)
		{
			make_move(mm);
			// PVS
			eval_t eval = -inf;
			if (eval_max == -inf)
				eval = -_Alphabeta(depth - 1, -beta, -alpha);
			else
			{
				eval = -_Alphabeta(depth - 1, -(alpha + pvs_window), -alpha);
				if ((eval > alpha) && (eval < beta))
					eval = -_Alphabeta(depth - 1, -beta, -alpha);
			}
			unmake_move(mm);

			if (eval > eval_max)
			{
				eval_max = eval;
				if (eval >= beta)
				{
					flag = node_f::beta;
					mm_best = mm;
					break;
				}
				if (eval > alpha)
				{
					flag = node_f::pv;
					mm_best = mm;
					alpha = eval;
				}
			}
		}
		// ��¼�û���
		rt.record_hash(this->get_status(), mm_best, depth, eval_max, flag);
		if (!_Is_dft_move(mm_best)) // not alpha node
			rt.record_history(mm_best, depth);
		return eval_max;
	}
	
	std::pair<eval_t, movement> chess_game::_Root_search(int depth)
	{
		movement mm_hash = dft_movement;
		// ��ʼ������
		eval_t eval_max = -inf;
		movement mm_best = dft_movement;

		mm_sort mmsrt(*this, mm_hash);

		for (movement mm; !_Is_dft_move(mm = mmsrt.next());)
		{
			make_move(mm);
			// PVS
			eval_t eval = -inf;
			if (eval_max == -inf)
				eval = -_Alphabeta(depth - 1, -inf, inf, true);
			else
			{
				eval = -_Alphabeta(depth - 1, -(eval_max + pvs_window), -eval_max);
				if (eval > eval_max)
					eval = -_Alphabeta(depth - 1, -inf, -eval_max, true);
			}
			unmake_move(mm);

			if (eval > eval_max)
			{
				eval_max = eval;
				mm_best = mm;
			}
		}
		// ��¼�û���
		rt.record_hash(this->get_status(), mm_best, depth, eval_max, node_f::pv);
		rt.record_history(mm_best, depth);
		return { eval_max,mm_best };
	}
	movement chess_game::_Search_till_timeout() // unit: ms
	{
		using namespace std::chrono;
		this->rt.clear();

		movement mm_best = dft_movement;
		eval_t eval = _Evaluate(this->get_status(), this->get_color(), turn_cnt);
		for (int i = 1; ; ++i)
		{
			auto res = _Root_search(i);
			mm_best = res.second;
			eval = res.first;
			if (timespan == 0)
			{
#ifndef __GNUC__
				std::cout << "��ȫ�����Ĳ�����" << i - 1 << std::endl;
#endif
				break;
			}
		}
		if (_Is_dft_move(mm_best))
		{

			// һ��̰��
			movement mm_hash = dft_movement;
			// ��ʼ������
			node_f flag = node_f::alpha;
			eval_t eval_max = -inf;

			mm_sort mmsrt(*this, mm_hash);

			for (movement mm; !_Is_dft_move(mm = mmsrt.next());)
			{
				make_move(mm);
				const eval_t cur_eval = -_Evaluate(this->get_status(), this->get_color(), turn_cnt);
				if (eval_max <= cur_eval)
				{
					eval_max = cur_eval;
					mm_best = mm;
				}
				unmake_move(mm);
			}
		}
		return mm_best;
	}
}