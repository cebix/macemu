/*
 *  basic-blockinfo.hpp - PowerPC basic block information
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef BASIC_BLOCKINFO_H
#define BASIC_BLOCKINFO_H

struct basic_block_info
{
	typedef basic_block_info block_info;
	static const int MAX_TARGETS = 2;

	struct dependency
	{
		block_info *	source;
		block_info *	target;
		dependency *	next;
		dependency **	prev_p;
	};

	uintptr				pc;
	uintptr				end_pc;
	int32				count;
	uint32				size;
	uint32				c1;
	uint32				c2;

	// List of blocks we depend on
	dependency			dep[MAX_TARGETS];

	// List of blocks that depends on this block
	dependency *		deplist;

	void init(uintptr start_pc);
	void remove_dep(dependency *d);
	void remove_deps();
	void create_jmpdep(block_info *tbi, int i);
	void maybe_create_jmpdep(block_info *tbi);
	bool intersect(uintptr start, uintptr end);
};

inline void
basic_block_info::init(uintptr start_pc)
{
	pc = start_pc;
	deplist = NULL;
	for (int i = 0; i < MAX_TARGETS; i++) {
		dep[i].source = NULL;
		dep[i].target = NULL;
		dep[i].next = NULL;
		dep[i].prev_p = NULL;
	}
}

inline void
basic_block_info::remove_dep(dependency *d)
{
	if (d->prev_p)
		*(d->prev_p) = d->next;
	if (d->next)
		d->next->prev_p = d->prev_p;
	d->prev_p = NULL;
	d->next = NULL;
}

inline void
basic_block_info::remove_deps()
{
	for (int i = 0; i < MAX_TARGETS; i++)
		remove_dep(&dep[i]);
}

inline void
basic_block_info::create_jmpdep(block_info *tbi, int i)
{
	dep[i].source = this;
	dep[i].target = tbi;
	dep[i].next = tbi->deplist;
	if (dep[i].next)
		dep[i].next->prev_p = &(dep[i].next);
	dep[i].prev_p = &(tbi->deplist);
	tbi->deplist = &(dep[i]);
}

inline void
basic_block_info::maybe_create_jmpdep(block_info *tbi)
{
	for (int i = 0; i < MAX_TARGETS && dep[i].target != tbi; i++) {
		if (dep[i].source == NULL) {
			create_jmpdep(tbi, i);
			break;
		}
	}
}

inline bool
basic_block_info::intersect(uintptr start, uintptr end)
{
	return (pc >= start && pc < end) || (end_pc >= start && end_pc < end);
}

#endif /* BASIC_BLOCKINFO_H */
