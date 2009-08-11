/* mpn_sqrtrem -- square root and remainder

Copyright 1999, 2000, 2001, 2002, 2004, 2005 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA. */


/* Contributed by Paul Zimmermann.
   See "Karatsuba Square Root", reference in gmp.texi.  */


#include <stdio.h>
#include <stdlib.h>

#include "mpir.h"
#include "gmp-impl.h"
#include "longlong.h"

static mp_limb_t mpn_intdivrem (mp_ptr qp, mp_size_t qxn,
	    mp_ptr np, mp_size_t nn,
	    mp_srcptr dp, mp_size_t dn)
{
  ASSERT (qxn >= 0);
  ASSERT (nn >= dn);
  ASSERT (dn >= 1);
  ASSERT (dp[dn-1] & GMP_NUMB_HIGHBIT);
  ASSERT (! MPN_OVERLAP_P (np, nn, dp, dn));
  ASSERT (! MPN_OVERLAP_P (qp, nn-dn+qxn, np, nn) || qp==np+dn+qxn);
  ASSERT (! MPN_OVERLAP_P (qp, nn-dn+qxn, dp, dn));
  ASSERT_MPN (np, nn);
  ASSERT_MPN (dp, dn);

  if (dn == 1)
    {
      mp_limb_t ret;
      mp_ptr q2p;
      mp_size_t qn;
      TMP_DECL;

      TMP_MARK;
      q2p = (mp_ptr) TMP_ALLOC ((nn + qxn) * BYTES_PER_MP_LIMB);

      np[0] = mpn_divrem_1 (q2p, qxn, np, nn, dp[0]);
      qn = nn + qxn - 1;
      MPN_COPY (qp, q2p, qn);
      ret = q2p[qn];

      TMP_FREE;
      return ret;
    }
  else if (dn == 2)
    {
      return mpn_divrem_2 (qp, qxn, np, nn, dp);
    }
  else
    {
      mp_ptr rp, q2p;
      mp_limb_t qhl;
      mp_size_t qn;
      TMP_DECL;

      TMP_MARK;
      if (UNLIKELY (qxn != 0))
	{
	  mp_ptr n2p;
	  n2p = (mp_ptr) TMP_ALLOC ((nn + qxn) * BYTES_PER_MP_LIMB);
	  MPN_ZERO (n2p, qxn);
	  MPN_COPY (n2p + qxn, np, nn);
	  q2p = (mp_ptr) TMP_ALLOC ((nn - dn + qxn + 1) * BYTES_PER_MP_LIMB);
	  rp = (mp_ptr) TMP_ALLOC (dn * BYTES_PER_MP_LIMB);
	  mpn_tdiv_qr (q2p, rp, 0L, n2p, nn + qxn, dp, dn);
	  MPN_COPY (np, rp, dn);
	  qn = nn - dn + qxn;
	  MPN_COPY (qp, q2p, qn);
	  qhl = q2p[qn];
	}
      else
	{
	  q2p = (mp_ptr) TMP_ALLOC ((nn - dn + 1) * BYTES_PER_MP_LIMB);
	  rp = (mp_ptr) TMP_ALLOC (dn * BYTES_PER_MP_LIMB);
	  mpn_tdiv_qr (q2p, rp, 0L, np, nn, dp, dn);
	  MPN_COPY (np, rp, dn);	/* overwrite np area with remainder */
	  qn = nn - dn;
	  MPN_COPY (qp, q2p, qn);
	  qhl = q2p[qn];
	}
      TMP_FREE;
      return qhl;
    }
}




/* Square roots table.  Generated by the following program:
#include "mpir.h"
main(){mpz_t x;int i;mpz_init(x);for(i=64;i<256;i++){mpz_set_ui(x,256*i);
mpz_sqrt(x,x);mpz_out_str(0,10,x);printf(",");if(i%16==15)printf("\n");}}
*/
static const unsigned char approx_tab[192] =
  {
    128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,
    143,144,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
    156,157,158,159,160,160,161,162,163,163,164,165,166,167,167,168,
    169,170,170,171,172,173,173,174,175,176,176,177,178,178,179,180,
    181,181,182,183,183,184,185,185,186,187,187,188,189,189,190,191,
    192,192,193,193,194,195,195,196,197,197,198,199,199,200,201,201,
    202,203,203,204,204,205,206,206,207,208,208,209,209,210,211,211,
    212,212,213,214,214,215,215,216,217,217,218,218,219,219,220,221,
    221,222,222,223,224,224,225,225,226,226,227,227,228,229,229,230,
    230,231,231,232,232,233,234,234,235,235,236,236,237,237,238,238,
    239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,247,
    247,248,248,249,249,250,250,251,251,252,252,253,253,254,254,255
  };

#define HALF_NAIL (GMP_NAIL_BITS / 2)

/* same as mpn_sqrtrem, but for size=1 and {np, 1} normalized */
static mp_size_t
mpn_sqrtrem1 (mp_ptr sp, mp_ptr rp, mp_srcptr np)
{
  mp_limb_t np0, s, r, q, u;
  int prec;

  ASSERT (np[0] >= GMP_NUMB_HIGHBIT / 2);
  ASSERT (GMP_LIMB_BITS >= 16);

  /* first compute a 8-bit approximation from the high 8-bits of np[0] */
  np0 = np[0] << GMP_NAIL_BITS;
  q = np0 >> (GMP_LIMB_BITS - 8);
  /* 2^6 = 64 <= q < 256 = 2^8 */
  s = approx_tab[q - 64];				/* 128 <= s < 255 */
  r = (np0 >> (GMP_LIMB_BITS - 16)) - s * s;		/* r <= 2*s */
  if (r > 2 * s)
    {
      r -= 2 * s + 1;
      s++;
    }

  prec = 8;
  np0 <<= 2 * prec;
  while (2 * prec < GMP_LIMB_BITS)
    {
      /* invariant: s has prec bits, and r <= 2*s */
      r = (r << prec) + (np0 >> (GMP_LIMB_BITS - prec));
      np0 <<= prec;
      u = 2 * s;
      q = r / u;
      u = r - q * u;
      s = (s << prec) + q;
      u = (u << prec) + (np0 >> (GMP_LIMB_BITS - prec));
      q = q * q;
      r = u - q;
      if (u < q)
	{
	  r += 2 * s - 1;
	  s --;
	}
      np0 <<= prec;
      prec = 2 * prec;
    }

  ASSERT (2 * prec == GMP_LIMB_BITS); /* GMP_LIMB_BITS must be a power of 2 */

  /* normalize back, assuming GMP_NAIL_BITS is even */
  ASSERT (GMP_NAIL_BITS % 2 == 0);
  sp[0] = s >> HALF_NAIL;
  u = s - (sp[0] << HALF_NAIL); /* s mod 2^HALF_NAIL */
  r += u * ((sp[0] << (HALF_NAIL + 1)) + u);
  r = r >> GMP_NAIL_BITS;

  if (rp != NULL)
    rp[0] = r;
  return r != 0 ? 1 : 0;
}


#define Prec (GMP_NUMB_BITS >> 1)

/* same as mpn_sqrtrem, but for size=2 and {np, 2} normalized
   return cc such that {np, 2} = sp[0]^2 + cc*2^GMP_NUMB_BITS + rp[0] */
static mp_limb_t
mpn_sqrtrem2 (mp_ptr sp, mp_ptr rp, mp_srcptr np)
{
  mp_limb_t qhl, q, u, np0;
  int cc;

  ASSERT (np[1] >= GMP_NUMB_HIGHBIT / 2);

  np0 = np[0];
  mpn_sqrtrem1 (sp, rp, np + 1);
  qhl = 0;
  while (rp[0] >= sp[0])
    {
      qhl++;
      rp[0] -= sp[0];
    }
  /* now rp[0] < sp[0] < 2^Prec */
  rp[0] = (rp[0] << Prec) + (np0 >> Prec);
  u = 2 * sp[0];
  q = rp[0] / u;
  u = rp[0] - q * u;
  q += (qhl & 1) << (Prec - 1);
  qhl >>= 1; /* if qhl=1, necessary q=0 as qhl*2^Prec + q <= 2^Prec */
  /* now we have (initial rp[0])<<Prec + np0>>Prec = (qhl<<Prec + q) * (2sp[0]) + u */
  sp[0] = ((sp[0] + qhl) << Prec) + q;
  cc = u >> Prec;
  rp[0] = ((u << Prec) & GMP_NUMB_MASK) + (np0 & (((mp_limb_t) 1 << Prec) - 1));
  /* subtract q * q or qhl*2^(2*Prec) from rp */
  cc -= mpn_sub_1 (rp, rp, 1, q * q) + qhl;
  /* now subtract 2*q*2^Prec + 2^(2*Prec) if qhl is set */
  if (cc < 0)
    {
      cc += sp[0] != 0 ? mpn_add_1 (rp, rp, 1, sp[0]) : 1;
      cc += mpn_add_1 (rp, rp, 1, --sp[0]);
    }

  return cc;
}

/* writes in {sp, n} the square root (rounded towards zero) of {np, 2n},
   and in {np, n} the low n limbs of the remainder, returns the high
   limb of the remainder (which is 0 or 1).
   Assumes {np, 2n} is normalized, i.e. np[2n-1] >= B/4
   where B=2^GMP_NUMB_BITS.  */
static mp_limb_t
mpn_dc_sqrtrem (mp_ptr sp, mp_ptr np, mp_size_t n)
{
  mp_limb_t q;			/* carry out of {sp, n} */
  int c, b;			/* carry out of remainder */
  mp_size_t l, h;

  ASSERT (np[2 * n - 1] >= GMP_NUMB_HIGHBIT / 2);

  if (n == 1)
    c = mpn_sqrtrem2 (sp, np, np);
  else
    {
      l = n / 2;
      h = n - l;
      q = mpn_dc_sqrtrem (sp + l, np + 2 * l, h);
      if (q != 0)
        mpn_sub_n (np + 2 * l, np + 2 * l, sp + l, h);
      q += mpn_intdivrem (sp, 0, np + l, n, sp + l, h);
      c = sp[0] & 1;
      mpn_rshift1 (sp, sp, l);
      sp[l - 1] |= (q << (GMP_NUMB_BITS - 1)) & GMP_NUMB_MASK;
      q >>= 1;
      if (c != 0)
        c = mpn_add_n (np + l, np + l, sp + l, h);
      mpn_sqr_n (np + n, sp, l);
      b = q + mpn_sub_n (np, np, np + n, 2 * l);
      c -= (l == h) ? b : mpn_sub_1 (np + 2 * l, np + 2 * l, 1, (mp_limb_t) b);
      q = mpn_add_1 (sp + l, sp + l, h, q);

      if (c < 0)
        {
          c += mpn_addmul_1 (np, sp, n, CNST_LIMB(2)) + 2 * q;
          c -= mpn_sub_1 (np, np, n, CNST_LIMB(1));
          q -= mpn_sub_1 (sp, sp, n, CNST_LIMB(1));
        }
    }

  return c;
}


mp_size_t
mpn_sqrtrem (mp_ptr sp, mp_ptr rp, mp_srcptr np, mp_size_t nn)
{
  mp_limb_t *tp, s0[1], cc, high, rl;
  int c;
  mp_size_t rn, tn;
  TMP_DECL;

  ASSERT (nn >= 0);
  ASSERT_MPN (np, nn);

  /* If OP is zero, both results are zero.  */
  if (nn == 0)
    return 0;

  ASSERT (np[nn - 1] != 0);
  ASSERT (rp == NULL || MPN_SAME_OR_SEPARATE_P (np, rp, nn));
  ASSERT (rp == NULL || ! MPN_OVERLAP_P (sp, (nn + 1) / 2, rp, nn));
  ASSERT (! MPN_OVERLAP_P (sp, (nn + 1) / 2, np, nn));

  high = np[nn - 1];
  if (nn == 1 && (high & GMP_NUMB_HIGHBIT))
    return mpn_sqrtrem1 (sp, rp, np);
  count_leading_zeros (c, high);
  c -= GMP_NAIL_BITS;

  c = c / 2; /* we have to shift left by 2c bits to normalize {np, nn} */
  tn = (nn + 1) / 2; /* 2*tn is the smallest even integer >= nn */

  TMP_MARK;
  if (nn % 2 != 0 || c > 0)
    {
      tp = TMP_ALLOC_LIMBS (2 * tn);
      tp[0] = 0;	     /* needed only when 2*tn > nn, but saves a test */
      if (c != 0)
	mpn_lshift (tp + 2 * tn - nn, np, nn, 2 * c);
      else
	MPN_COPY (tp + 2 * tn - nn, np, nn);
      rl = mpn_dc_sqrtrem (sp, tp, tn);
      /* We have 2^(2k)*N = S^2 + R where k = c + (2tn-nn)*GMP_NUMB_BITS/2,
	 thus 2^(2k)*N = (S-s0)^2 + 2*S*s0 - s0^2 + R where s0=S mod 2^k */
      c += (nn % 2) * GMP_NUMB_BITS / 2;		/* c now represents k */
      s0[0] = sp[0] & (((mp_limb_t) 1 << c) - 1);	/* S mod 2^k */
      rl += mpn_addmul_1 (tp, sp, tn, 2 * s0[0]);	/* R = R + 2*s0*S */
      cc = mpn_submul_1 (tp, s0, 1, s0[0]);
      rl -= (tn > 1) ? mpn_sub_1 (tp + 1, tp + 1, tn - 1, cc) : cc;
      mpn_rshift (sp, sp, tn, c);
      tp[tn] = rl;
      if (rp == NULL)
	rp = tp;
      c = c << 1;
      if (c < GMP_NUMB_BITS)
	tn++;
      else
	{
	  tp++;
	  c -= GMP_NUMB_BITS;
	}
      if (c != 0)
	mpn_rshift (rp, tp, tn, c);
      else
	MPN_COPY_INCR (rp, tp, tn);
      rn = tn;
    }
  else
    {
      if (rp == NULL)
	rp = TMP_ALLOC_LIMBS (nn);
      if (rp != np)
	MPN_COPY (rp, np, nn);
      rn = tn + (rp[tn] = mpn_dc_sqrtrem (sp, rp, tn));
    }

  MPN_NORMALIZE (rp, rn);

  TMP_FREE;
  return rn;
}
