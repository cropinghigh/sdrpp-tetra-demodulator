#pragma once
#include <dsp/block.h>

namespace dsp {
    
    //block that extracts symbols from DQPSK constellation
    class DQPSKSymbolExtractor : public generic_block<DQPSKSymbolExtractor> {
    public:
        DQPSKSymbolExtractor() {}

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<DQPSKSymbolExtractor>::registerInput(_in);
            generic_block<DQPSKSymbolExtractor>::registerOutput(&out);
            generic_block<DQPSKSymbolExtractor>::_block_init = true;
        }

        int run() {
            count = _in->read();
            if(count < 0) { return -1; }

            for(int i = 0; i < count; i++) {
//                 complex_t sym = _in->readBuf[i];
                complex_t sym_c = _in->readBuf[i];
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
                out.writeBuf[i] = bits;
            }
            _in->flush();
            if(!out.swap(count)) { return -1; }
            return count;
        }

        stream<uint8_t> out;

    private:
        int count;
        stream<complex_t>* _in;
        //stream<float>* _in;
    };

    //Perform differential decode(because it's Dqpsk)
    class DifferentialDecoder : public generic_block<DifferentialDecoder> {
    public:
        DifferentialDecoder() {}

        void init(stream<complex_t>* in) {
            _in = in;
            generic_block<DifferentialDecoder>::registerInput(_in);
            generic_block<DifferentialDecoder>::registerOutput(&out);
            generic_block<DifferentialDecoder>::_block_init = true;
        }

        void setInput(stream<complex_t>* in) {
            assert(generic_block<DifferentialDecoder>::_block_init);
            generic_block<DifferentialDecoder>::tempStop();
            generic_block<DifferentialDecoder>::unregisterInput(_in);
            _in = in;
            generic_block<DifferentialDecoder>::registerInput(_in);
            generic_block<DifferentialDecoder>::tempStart();
        }

        int run() {
            count = _in->read();
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
                y = _in->readBuf[i] * x;
                prev_sample = _in->readBuf[i];
                y = y * rotation_coeff; //rotate constellation by -pi/4
                //float sym = std::atan2(y.im, y.re);
                //filter = filter * (1.0f - filter_val) + (sym - 0.0f)*filter_val;
                //out.writeBuf[i] = dsp::complex_t {sym, 0.0f};
                out.writeBuf[i] = y;
            }

            _in->flush();
            if(!out.swap(count)) { return -1; }
            return count;
        }

        stream<complex_t> out;

    private:
        int count;
         //float filter = 0;
         //float filter_val=0.0001;
        complex_t rotation_coeff = complex_t {cosf(-FL_M_PI / 4.0f), sinf(-FL_M_PI / 4.0f)};
        complex_t prev_sample;
        stream<complex_t>* _in;
    };

    //Unpack 2bits/byte to 1, because tetra-rx needs it
    class BitUnpacker : public generic_block<BitUnpacker> {
    public:
        BitUnpacker() {}

        BitUnpacker(stream<uint8_t>* in) { init(in); }

        void init(stream<uint8_t>* in) {
            _in = in;

            generic_block<BitUnpacker>::registerInput(_in);
            generic_block<BitUnpacker>::registerOutput(&out);
            generic_block<BitUnpacker>::_block_init = true;
        }

        void setInput(stream<uint8_t>* in) {
            assert(generic_block<BitUnpacker>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<BitUnpacker>::ctrlMtx);
            generic_block<BitUnpacker>::tempStop();
            generic_block<BitUnpacker>::unregisterInput(_in);
            _in = in;
            generic_block<BitUnpacker>::registerInput(_in);
            generic_block<BitUnpacker>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }
            for(int i = 0; i < count; i++) {
                uint8_t inByte = _in->readBuf[i];

                out.readBuf[(i * 2) + 1] = inByte & 0b01;
                out.readBuf[(i * 2)] = (inByte & 0b10) >> 1;
            }
            _in->flush();
            if(!out.swap(count * 2)) {
                return -1;
            }
            return count;
        }

        stream<uint8_t> out;

    private:

        stream<uint8_t>* _in;

    };
    
}
