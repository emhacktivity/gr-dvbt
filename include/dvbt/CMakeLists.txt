# Copyright 2011,2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Install public header files
########################################################################
install(FILES
    api.h
    test.h
    vector_pad.h
    reference_signals.h
    dvbt_config.h
    dvbt_map.h
    bit_inner_interleaver.h
    symbol_inner_interleaver.h
    inner_coder.h
    reed_solomon_enc.h
    energy_dispersal.h
    convolutional_interleaver.h
    test2.h
    demod_reference_signals.h
    dvbt_demap.h
    bit_inner_deinterleaver.h
    convolutional_deinterleaver.h
    energy_descramble.h
    reed_solomon.h
    reed_solomon_dec.h
    ofdm_sym_acquisition.h
    viterbi_decoder.h DESTINATION include/dvbt
)
