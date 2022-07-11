#pragma once
#include <dsp/processor.h>

namespace dsp {
    //block that extracts symbols from DQPSK constellation
    class DQPSKSymbolExtractor : public Processor<complex_t, uint8_t> {
        using base_type = Processor<complex_t, uint8_t>;
    public:
        DQPSKSymbolExtractor() {}

        DQPSKSymbolExtractor(stream<complex_t>* in) {init(in);}

        int run() {
            count = base_type::_in->read();
            if(count < 0) { return -1; }

            for(int i = 0; i < count; i++) {
//                 complex_t sym = _in->readBuf[i];
                complex_t sym_c = base_type::_in->readBuf[i];
//                 complex_t z, sym_rot;
//                 z.re = cosf(-FL_M_PI / 4.0f);
//                 z.im = sinf(-FL_M_PI / 4.0f);
//                 sym_rot = sym_c * z;
                uint8_t bits = ((sym_c.im<0)<<1) | (sym_c.re<0);
//                 float sym = std::atan2(sym_c.im, sym_c.re);
//                 uint8_t bits;
//                 if(sym > 2) {
//                     bits = 0b01; // re<0;im>0
//                 } else if(sym > 0) {
//                     bits = 0b00; // re>0;im>0
//                 } else if(sym < -2) {
//                     bits = 0b11; // re<0;im<0
//                 } else {
//                     bits = 0b10; // re>0;im<0
//                 }
                //spdlog::info("re={0}, im={1}, atan={2}, data={3}, rotated re={4}, im={5}, data={6}", sym_c.re, sym_c.im, sym, bits, sym_rot.re, sym_rot.im, ((sym_rot.im<0)<<1) | (sym_rot.re<0));
//                 if (fabs(sym.re) > fabs(sym.im)) {  // Horizontal axis is the major axis
//                     if (sym.re > 0.0) {
//                         bits = 0;  // No rotation
//                     } else {
//                         bits = 3; // +- pi rotation
//                     }
//                 } else if (sym.im > 0.0 ) {
//                     bits = 1;  // +pi rotation
//                 } else {
//                     bits = 2;  // -pi rotation
//                 }
                base_type::out.writeBuf[i] = bits;
            }
            base_type::_in->flush();
            if(!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        int count;
    };

    //Perform differential decode(because it's Dqpsk)
    class DifferentialDecoder : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        DifferentialDecoder() {}

        DifferentialDecoder(stream<complex_t>* in) {init(in);}

        int run() {
            count = base_type::_in->read();
            if(count < 0) { return -1; }

            for(int i = 0; i < count; i++) {
                //out.writeBuf[i] = (_in->readBuf[i] - prev_sample + 4) % 4;
//                 complex_t rotation {cosf(rotation_coeff), sinf(rotation_coeff)};
//                 sym = sym * rotation;
//                 std::complex conj_res = std::conj(std::complex<float> (prev_sample.re, prev_sample.im));
//                 out.writeBuf[i] = sym * complex_t {conj_res.real(), conj_res.imag()};
                complex_t x, y;
                x.re = prev_sample.re;
                x.im = -(prev_sample.im);
                y = base_type::_in->readBuf[i] * x;
                prev_sample = base_type::_in->readBuf[i];
                y = y * rotation_coeff; //rotate constellation by -pi/4
                //float sym = std::atan2(y.im, y.re);
                //filter = filter * (1.0f - filter_val) + (sym - 0.0f)*filter_val;
                //out.writeBuf[i] = dsp::complex_t {sym, 0.0f};
                base_type::out.writeBuf[i] = y;
            }

            base_type::_in->flush();
            if(!base_type::out.swap(count)) { return -1; }
            return count;
        }

    private:
        int count;
         //float filter = 0;
         //float filter_val=0.0001;
        complex_t rotation_coeff = complex_t {cosf(-FL_M_PI / 4.0f), sinf(-FL_M_PI / 4.0f)};
        complex_t prev_sample;
    };

    //Unpack 2bits/byte to 1, because tetra-rx needs it
    class BitUnpacker : public Processor<uint8_t, uint8_t> {
        using base_type = Processor<uint8_t, uint8_t>;
    public:
        BitUnpacker() {}

        BitUnpacker(stream<uint8_t>* in) { init(in); }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }
            for(int i = 0; i < count; i++) {
                uint8_t inByte = base_type::_in->readBuf[i];

                base_type::out.readBuf[(i * 2) + 1] = inByte & 0b01;
                base_type::out.readBuf[(i * 2)] = (inByte & 0b10) >> 1;
            }
            base_type::_in->flush();
            if(!base_type::out.swap(count * 2)) {
                return -1;
            }
            return count;
        }

    };
    
}
