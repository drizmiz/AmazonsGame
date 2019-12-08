
#pragma once

#include "game.h"
#include "eval.h"

namespace amz
{
	enum class phase
	{
		hash,
		killer1,
		killer2,
		gen_moves,
		rest
	};

	// �߷�����
	class chess_game::mm_sort
	{
	public:
		movement mm_hash, mm_killer1, mm_killer2;
		phase cur_phase;
		int mm_index;
		std::vector<movement> mms;

		chess_game& cg_parent;
		mm_sort(chess_game& cg, movement _mm_hash) noexcept :
			cg_parent(cg), mm_hash(_mm_hash), cur_phase(phase::hash), mms{}
		{
			mm_killer1 = cg.rt.killer_moves[cg.distance][0];
			mm_killer2 = cg.rt.killer_moves[cg.distance][1];
		}

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
			cur_phase = phase::killer1;
			mms = std::move(cg_parent._Get_all_possible_moves());
			std::sort(mms.begin(), mms.end(),
				[this](movement mm1, movement mm2) {
					return cg_parent.rt.probe_history(mm1) >
						cg_parent.rt.probe_history(mm2);
				}
			);
			mm_index = 0;
			// killer ����
		case phase::killer1:
			cur_phase = phase::killer2;
			if (mm_killer1 != mm_hash && !_Is_dft_move(mm_killer1))
				if (std::find(mms.begin(), mms.end(), mm_killer1) != mms.end())
					return mm_killer1;
		case phase::killer2:
			cur_phase = phase::rest;
			if (mm_killer2 != mm_hash && !_Is_dft_move(mm_killer2))
				if (std::find(mms.begin(), mms.end(), mm_killer2) != mms.end())
					return mm_killer2;
			// ��ʷ������
		case phase::rest:
			for (; mm_index < mms.size(); )
			{
				movement mm = mms[mm_index];
				++mm_index;
				if (mm != mm_hash && mm != mm_killer1 && mm != mm_killer2)
					return mm;
			}
			// û���ŷ���
		default:
			return dft_movement;
		}
	}

	inline bool chess_game::_Permit_null()
	{
		return turn_cnt <= 12;
	}

	constexpr int _null_cut = 1;
	int pvs_window = 100;
	constexpr int limit_depth = 56;
	eval_t chess_game::_Alphabeta(int depth, eval_t alpha, eval_t beta, bool _no_null = false)
	{
		pvs_window = 25000 * eval_adj::quick_pow_s(2, -turn_cnt);
		if (pvs_window <= 1000)pvs_window = 1000;

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
#ifdef _BOTZONE_ONLINE
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
		if (depth <= 0 || distance > limit_depth)
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
			_Set_best_move(mm_best, depth);
		return eval_max;
	}

	void chess_game::_Set_best_move(movement mm_best,int depth)
	{
		rt.record_history(mm_best, depth);
		movement* lp_killers = rt.killer_moves[distance];
		if (lp_killers[0] != mm_best)
		{
			lp_killers[1] = lp_killers[0];
			lp_killers[0] = mm_best;
		}
	}

	std::pair<eval_t, movement> chess_game::_Root_search(int depth,movement lastmove)
	{
		const movement& mm_hash = lastmove;
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
		_Set_best_move(mm_best, depth);
		return { eval_max,mm_best };
	}
	
	movement chess_game::_Search_till_timeout()
	{
		using namespace std::chrono;

		movement mm_best = dft_movement;
		eval_t eval = _Evaluate(this->get_status(), this->get_color(), turn_cnt);

		distance = 0;

		for (int i = 1; ; ++i)
		{
			auto res = _Root_search(i, mm_best);
			mm_best = res.second;
			eval = res.first;
			if (timespan == 0)
			{
#ifndef _BOTZONE_ONLINE
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