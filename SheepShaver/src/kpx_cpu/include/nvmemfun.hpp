/*
 *  nvmemfun.hpp - Non-virtual member function wrappers
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

#ifndef NVMEMFUN_H
#define NVMEMFUN_H

#include <functional>

#if defined __GNUC__
#define HAVE_FAST_NV_MEM_FUN 1
#define MEM_FUN_WORDS  2
#if defined __GXX_ABI_VERSION /* GCC >= 3.0 */
#define MEM_FUN_OFFSET 0
#else
#define MEM_FUN_OFFSET 1
#endif
#endif

#if defined __ICC
#define HAVE_FAST_NV_MEM_FUN 1
#define MEM_FUN_WORDS  2
#define MEM_FUN_OFFSET 0 /* GNU C++ ABI v3 */
#endif

#if defined __EDG__  && defined __sgi
#define HAVE_FAST_NV_MEM_FUN 1
#define MEM_FUN_WORDS  3
#define MEM_FUN_OFFSET 2
#endif

#if HAVE_FAST_NV_MEM_FUN

template< class PMF, class PF >
inline PF nv_mem_fun_of(PMF pmf) {
	if (pmf == 0)
		return 0;
	union { PMF pmf; uintptr p[MEM_FUN_WORDS]; } x;
	x.pmf = pmf;
	return (PF)x.p[MEM_FUN_OFFSET];
}

template< class R, class T >
class nv_mem_fun_t : public std::unary_function<T, R> {
	typedef R (T::*pmf_t)();
	typedef R (*pf_t)(T *);
	pf_t pf;
public:
	nv_mem_fun_t(pmf_t pmf) : pf(nv_mem_fun_of<pmf_t, pf_t>(pmf)) {}
	R operator()(T *p) const { return (*pf)(p); }
	pf_t ptr() const { return pf; }
};

template< class R, class T >
class const_nv_mem_fun_t : public std::unary_function<T, R> {
	typedef R (T::*pmf_t)();
	typedef R (*pf_t)(T *);
	pf_t const pf;
public:
	const_nv_mem_fun_t(pmf_t const pmf) : pf(nv_mem_fun_of<pmf_t, pf_t>(pmf)) {}
	R operator()(const T *p) const { return (*pf)(p); }
	pf_t ptr() const { return pf; }
};

template< class R, class T, class A >
class nv_mem_fun1_t : public std::binary_function<T, A, R> {
	typedef R (T::*pmf_t)(A);
	typedef R (*pf_t)(T *, A x);
	pf_t pf;
public:
	nv_mem_fun1_t(pmf_t pmf) : pf(nv_mem_fun_of<pmf_t, pf_t>(pmf)) {}
	R operator()(T *p, A x) const { return (*pf)(p, x); }
	pf_t ptr() const { return pf; }
};

template< class R, class T, class A >
class const_nv_mem_fun1_t : public std::binary_function<T, A, R> {
	typedef R (T::*pmf_t)(A);
	typedef R (*pf_t)(T *, A x);
	pf_t const pf;
public:
	const_nv_mem_fun1_t(pmf_t const pmf) : pf(nv_mem_fun_of<pmf_t, pf_t>(pmf)) {}
	R operator()(const T *p, A x) const { return (*pf)(p, x); }
	pf_t ptr() const { return pf; }
};

#else

template< class R, class T >
class nv_mem_fun_t : public std::unary_function<T, R> {
	typedef R (T::*pmf_t)();
	pmf_t pf;
public:
	nv_mem_fun_t(R (T::*pmf)()) : pf(pmf) {}
	R operator()(T *p) const { return (p->*pf)(); }
	pmf_t ptr() const { return pf; }
};

template< class R, class T >
class const_nv_mem_fun_t : public std::unary_function<T, R> {
	typedef R (T::*pmf_t)() const;
	pmf_t pf;
public:
	const_nv_mem_fun_t(R (T::*pmf)() const) : pf(pmf) {}
	R operator()(const T *p) const { return (p->*pf)(); }
	pmf_t ptr() const { return pf; }
};

template< class R, class T, class A >
class nv_mem_fun1_t : public std::binary_function<T, A, R> {
	typedef R (T::*pmf_t)(A);
	pmf_t pf;
public:
	nv_mem_fun1_t(R (T::*pmf)(A)) : pf(pmf) {}
	R operator()(T *p, A x) const { return (p->*pf)(x); }
	pmf_t ptr() const { return pf; }
};

template< class R, class T, class A >
class const_nv_mem_fun1_t : public std::binary_function<T, A, R> {
	typedef R (T::*pmf_t)(A) const;
	pmf_t pf;
public:
	const_nv_mem_fun1_t(R (T::*pmf)(A) const) : pf(pmf) {}
	R operator()(const T *p, A x) const { return (p->*pf)(x); }
	pmf_t ptr() const { return pf; }
};

#endif

template< class R, class T >
inline nv_mem_fun_t<R, T> nv_mem_fun(R (T::*pmf)()) {
	return nv_mem_fun_t<R, T>(pmf);
}

template< class R, class T >
inline const_nv_mem_fun_t<R, T> nv_mem_fun(R (T::*pmf)() const) {
	return const_nv_mem_fun_t<R, T>(pmf);
}

template< class R, class T, class A >
inline nv_mem_fun1_t<R, T, A> nv_mem_fun(R (T::*pmf)(A)) {
	return nv_mem_fun1_t<R, T, A>(pmf);
}

template< class R, class T, class A >
inline const_nv_mem_fun1_t<R, T, A> nv_mem_fun(R (T::*pmf)(A) const) {
	return const_nv_mem_fun1_t<R, T, A>(pmf);
}

#endif /* NVMEMFUN_H */
