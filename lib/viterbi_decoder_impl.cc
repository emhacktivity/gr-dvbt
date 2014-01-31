/* -*- c++ -*- */
/*
 * Based on Phil Karn, KA9Q impl of Viterbi decoder
 * 2013 <Bogdan Diaconescu, yo3iiu@yo3iiu.ro>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * There are two implementatios of Viterbi algorithms:
 * - one based on Karn's implementation
 * - one based on Karn's with SSE2 vectorization
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "viterbi_decoder_impl.h"
#include <xmmintrin.h>
#include <stdio.h>
#include <sys/time.h>

//#define VITERBI_DEBUG 1

#ifdef VITERBI_DEBUG
#define PRINTF(a...) printf(a)
#else
#define PRINTF(a...)
#endif

// TODO - these variables should not be static/global
// but members of the class. DO the change when refactoring
// the viterbi decoder.
static __m128i metric0[4] __attribute__ ((aligned(16)));
static __m128i metric1[4] __attribute__ ((aligned(16)));
static __m128i path0[4] __attribute__ ((aligned(16)));
static __m128i path1[4] __attribute__ ((aligned(16)));

// For timing debug
static struct timeval tvs, tve;
static struct timezone tzs, tze;

namespace gr {
  namespace dvbt {

    viterbi_decoder::sptr
    viterbi_decoder::make(dvbt_constellation_t constellation, \
                dvbt_hierarchy_t hierarchy, dvbt_code_rate_t coderate, int K, int S0, int SK)
    {
      return gnuradio::get_initial_sptr (new viterbi_decoder_impl(constellation, hierarchy, coderate, K, S0, SK));
    }

    /*
     * The private constructor
     */
    viterbi_decoder_impl::viterbi_decoder_impl(dvbt_constellation_t constellation, \
                dvbt_hierarchy_t hierarchy, dvbt_code_rate_t coderate, int K, int S0, int SK)
      : block("viterbi_decoder",
          io_signature::make(1, 1, sizeof (unsigned char)),
          io_signature::make(1, 1, sizeof (unsigned char))),
      config(constellation, hierarchy, coderate, coderate),
      d_K (K),
      d_S0 (S0),
      d_SK (SK),
      d_state (S0)
    {
      //Determine k - input of encoder
      d_k = config.d_cr_k;
      //Determine n - output of encoder
      d_n = config.d_cr_n;
      //Determine m - constellation symbol size
      d_m = config.d_m;


      set_relative_rate (1.0);
      set_output_multiple (d_K/8);

      printf("Viterbi: k: %i\n", d_k);
      printf("Viterbi: n: %i\n", d_n);
      printf("Viterbi: m: %i\n", d_m);
      printf("Viterbi: K: %i\n", d_K);

      /*
       * We input n bytes, each carrying m bits => nm bits
       * The result after decoding is km bits, therefore km/8 bytes.
       *
       * out/in rate is therefore km/8n in bytes
       */

      assert((d_k * d_m) % (8 * d_n));

      set_relative_rate((d_k * d_m) / (8 * d_n));

      assert ((d_K * d_n) % d_m == 0);
      d_nsymbols = d_K * d_n / d_m;

      int amp = 100;
      float RATE=0.5;
      float ebn0 = 12.0;
      float esn0 = RATE*pow(10.0, ebn0/10);
      d_gen_met(mettab, amp, esn0, 0.0, 4);
      d_viterbi_chunks_init(state0);


      d_viterbi_chunks_init_sse2(metric0, path0);
    }

    /*
     * Our virtual destructor.
     */
    viterbi_decoder_impl::~viterbi_decoder_impl()
    {
    }

    void
    viterbi_decoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
       assert (noutput_items % d_K == 0);

       int input_required = noutput_items * 8 * d_n / (d_k * d_m);

       unsigned ninputs = ninput_items_required.size();
       for (unsigned int i = 0; i < ninputs; i++) {
         ninput_items_required[i] = input_required;
       }
    }

    int
    viterbi_decoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
        assert (input_items.size() == output_items.size());
        int nstreams = input_items.size();
        assert (noutput_items % d_K == 0);
        int nblocks = 8*noutput_items / d_K;

        // TODO - Allocate dynamically these buffers
        unsigned char in_bits[d_K * d_n * nblocks];

        gettimeofday(&tvs, &tzs);

        for (int m=0;m<nstreams;m++) {
          const unsigned char *in = (const unsigned char *) input_items[m];
          unsigned char *out = (unsigned char *) output_items[m];

          for (int n = 0; n < nblocks; n++) {

            /*
             * We receive the symbol (d_m bits/byte) in one byte (e.g. for QAM16 00001111).
             * Create a buffer of bytes containing just one bit/byte.
             */
            for (int count = 0, i = 0; i < d_nsymbols; i++)
            {
              for (int j = (d_m - 1); j >= 0; j--)
                in_bits[count++] = (in[(n * d_nsymbols) + i] >> j) & 1;

              //printf("in[%i]: %x\n", \
                //(n * d_nsymbols) + i, in[(n * d_nsymbols) + i]);
            }

            /*
             * Decode a block.
             */

            int out_count = 0;

            for (int count = 0, i = 0; i < (d_K * 2); i++)
            {
              if ((count % 4) == 3)
              {
                d_viterbi_butterfly2_sse2(&in_bits[i & 0xfffffffc], metric0, metric1, path0, path1);
                //d_viterbi_butterfly2(&in_bits[i & 0xfffffffc], mettab, state0, state1);

                if ((count > 0) && (count % 16) == 11)
                  d_viterbi_get_output_sse2(metric0, path0, &out[n*(d_K/8) + out_count++]);
                  //d_viterbi_get_output(state0, &out[n*(d_K/8) + out_count++]);
              }

              count++;
            }

            // TODO - Make Viterbi algorithm aware of puncturing matrix
          }
        }


        gettimeofday(&tve, &tze);
        PRINTF("VITERBI: nblocks: %i, Mbit/s out: %f\n", \
            nblocks, (float) (nblocks * d_K) / (float)(tve.tv_usec - tvs.tv_usec));

        // Tell runtime system how many input items we consumed on
        // each input stream.
        consume_each (noutput_items * 8 * d_n / (d_k * d_m));

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }

  } /* namespace dvbt */
} /* namespace gr */

